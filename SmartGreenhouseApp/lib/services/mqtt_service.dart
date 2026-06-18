import 'dart:async';
import 'dart:convert';

import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';

import '../models.dart';

class MqttEnvelope {
  const MqttEnvelope({
    required this.topic,
    required this.payload,
  });

  final String topic;
  final Map<String, dynamic> payload;
}

class GreenhouseMqttService {
  GreenhouseMqttService();

  final StreamController<MqttEnvelope> _messages =
      StreamController<MqttEnvelope>.broadcast();

  MqttServerClient? _client;
  MqttSettings? _settings;

  Stream<MqttEnvelope> get messages => _messages.stream;

  bool get isConnected =>
      _client?.connectionStatus?.state == MqttConnectionState.connected;

  Future<void> connect(MqttSettings settings) async {
    await disconnect();
    _settings = settings;

    final client = MqttServerClient.withPort(
      settings.host,
      settings.clientId,
      settings.port,
    );
    client.logging(on: false);
    client.keepAlivePeriod = 30;
    client.connectTimeoutPeriod = 3000;
    client.autoReconnect = true;
    client.resubscribeOnAutoReconnect = true;
    client.onConnected = () => _subscribe(settings);
    client.onAutoReconnect = () {};
    client.onAutoReconnected = () => _subscribe(settings);

    final connectionMessage = MqttConnectMessage()
        .withClientIdentifier(settings.clientId)
        .startClean();
    client.connectionMessage = connectionMessage;

    _client = client;
    try {
      await client.connect(
        settings.username.isEmpty ? null : settings.username,
        settings.password.isEmpty ? null : settings.password,
      );
    } on Object {
      client.disconnect();
      rethrow;
    }

    if (client.connectionStatus?.state != MqttConnectionState.connected) {
      final status = client.connectionStatus;
      client.disconnect();
      throw StateError('MQTT не подключился: ${status?.returnCode}');
    }

    _subscribe(settings);
    client.updates?.listen(_handleMessages);
  }

  Future<void> disconnect() async {
    _client?.disconnect();
    _client = null;
  }

  Future<void> dispose() async {
    await disconnect();
    await _messages.close();
  }

  void _subscribe(MqttSettings settings) {
    final client = _client;
    if (client == null) return;

    client.subscribe('${settings.baseTopic}/telemetry/#', MqttQos.atLeastOnce);
    client.subscribe('${settings.baseTopic}/status/#', MqttQos.atLeastOnce);
    client.subscribe('${settings.baseTopic}/ack', MqttQos.atLeastOnce);
  }

  void _handleMessages(List<MqttReceivedMessage<MqttMessage>> events) {
    for (final event in events) {
      final message = event.payload;
      if (message is! MqttPublishMessage) continue;

      final payload = MqttPublishPayload.bytesToStringAsString(
        message.payload.message,
      );
      try {
        final decoded = jsonDecode(payload);
        if (decoded is Map<String, dynamic>) {
          _messages.add(MqttEnvelope(topic: event.topic, payload: decoded));
        }
      } on FormatException {
        _messages.add(MqttEnvelope(
          topic: event.topic,
          payload: <String, dynamic>{'raw': payload},
        ));
      }
    }
  }

  void publishCommand(
    String command,
    Map<String, dynamic> params, {
    MqttQos qos = MqttQos.atLeastOnce,
  }) {
    final settings = _settings;
    final client = _client;
    if (settings == null || client == null || !isConnected) {
      throw StateError('MQTT не подключён');
    }

    final payload = <String, dynamic>{
      'requestId': _requestId(command),
      'source': 'flutter-app',
      'ts': moscowTimestamp(),
      'params': params,
    };

    final builder = MqttClientPayloadBuilder();
    builder.addString(jsonEncode(payload));
    client.publishMessage(
      '${settings.baseTopic}/cmd/$command',
      qos,
      builder.payload!,
    );
  }

  String _requestId(String command) {
    final safeCommand = command.replaceAll('/', '-');
    return '$safeCommand-${DateTime.now().millisecondsSinceEpoch}';
  }
}

String moscowTimestamp() {
  final moscow = DateTime.now().toUtc().add(const Duration(hours: 3));
  String two(int value) => value.toString().padLeft(2, '0');
  String three(int value) => value.toString().padLeft(3, '0');
  return '${moscow.year}-${two(moscow.month)}-${two(moscow.day)}'
      'T${two(moscow.hour)}:${two(moscow.minute)}:${two(moscow.second)}'
      '.${three(moscow.millisecond)}+03:00';
}

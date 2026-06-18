import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';

import '../models.dart';
import '../services/mqtt_service.dart';

class GreenhouseController extends ChangeNotifier {
  GreenhouseController({
    GreenhouseMqttService? mqttService,
  }) : _mqttService = mqttService ?? GreenhouseMqttService();

  final GreenhouseMqttService _mqttService;
  StreamSubscription<MqttEnvelope>? _mqttSubscription;

  GreenhouseState state = GreenhouseState.initial();
  MqttSettings settings = MqttSettings.defaults();
  ConnectionStatus connectionStatus = ConnectionStatus.disconnected;

  Future<void> initialize() async {
    await _loadSettings();
    await _loadZoneNames();
    _mqttSubscription = _mqttService.messages.listen(_handleMqttEnvelope);
    notifyListeners();
  }

  Future<void> disposeController() async {
    await _mqttSubscription?.cancel();
    await _mqttService.dispose();
  }

  Future<void> connect() async {
    connectionStatus = ConnectionStatus.connecting;
    state.lastError = null;
    notifyListeners();

    try {
      await _mqttService.connect(settings);
      connectionStatus = ConnectionStatus.connected;
      requestSensors();
    } on Object catch (error) {
      connectionStatus = ConnectionStatus.error;
      state.lastError = _humanError(error.toString());
    }
    notifyListeners();
  }

  Future<void> disconnect() async {
    await _mqttService.disconnect();
    connectionStatus = ConnectionStatus.disconnected;
    notifyListeners();
  }

  Future<void> updateSettings(MqttSettings next) async {
    settings = next;
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString('mqtt.host', next.host);
    await prefs.setInt('mqtt.port', next.port);
    await prefs.setString('mqtt.username', next.username);
    await prefs.setString('mqtt.password', next.password);
    await prefs.setString('mqtt.clientId', next.clientId);
    await prefs.setString('mqtt.deviceId', next.deviceId);
    notifyListeners();
  }

  Future<void> renameZone(int zone, String name) async {
    final safeName = name.trim().isEmpty ? 'Грядка $zone' : name.trim();
    state.zones = state.zones
        .map(
            (item) => item.index == zone ? item.copyWith(name: safeName) : item)
        .toList();

    final prefs = await SharedPreferences.getInstance();
    await prefs.setString('zone.$zone.name', safeName);
    notifyListeners();
  }

  void requestSensors() {
    _publish('sensors/get', <String, dynamic>{
      'fields': <String>['all'],
    });
  }

  void waterZone(int zone, {int durationSec = 60, bool force = false}) {
    _publish('irrigation/start', <String, dynamic>{
      'zones': <int>[zone],
      'durationSec': durationSec,
      'force': force,
    });
  }

  void waterAll({int zoneDurationSec = 60, bool force = false}) {
    _publish('irrigation/start', <String, dynamic>{
      'zones': 'all',
      'strategy': 'sequential',
      'zoneDurationSec': zoneDurationSec,
      'force': force,
    });
  }

  void stopIrrigation({int? zone}) {
    _publish('irrigation/stop', <String, dynamic>{
      if (zone == null) 'zones': 'all' else 'zones': <int>[zone],
    });
  }

  void setVentilation(List<int> windows, String action,
      {int durationSec = 60}) {
    _publish('vent/set', <String, dynamic>{
      'windows': windows,
      'action': action,
      if (action != 'stop') 'durationSec': durationSec,
    });
  }

  void setModes({
    required String irrigation,
    required String ventilation,
  }) {
    _publish('automation/mode/set', <String, dynamic>{
      'irrigation': irrigation,
      'ventilation': ventilation,
    });
  }

  void updateSafety(AutomationConfig config) {
    _publish('automation/safety/set', <String, dynamic>{
      'enabled': config.safetyEnabled,
      'blockOutsideAllowedTime': config.blockOutsideAllowedTime,
      'allowedTime': <String, dynamic>{
        'start': config.safetyStart,
        'end': config.safetyEnd,
      },
      'blockIfSoilAbovePct': config.blockIfSoilAbovePct,
      'blockIfAirHumidityAbovePct': config.blockIfAirHumidityAbovePct,
      'blockIfAirTempBelowC': config.blockIfAirTempBelowC,
      'allowManualForce': config.allowManualForce,
    });
  }

  void updateIrrigationAutomation(AutomationConfig config) {
    _publish('automation/irrigation/set', <String, dynamic>{
      'enabled': config.irrigationMode == 'sensor' ||
          config.irrigationMode == 'hybrid',
      'mode': config.irrigationMode,
      'strategy': 'sequential',
      'allowedTime': <String, dynamic>{
        'start': config.safetyStart,
        'end': config.safetyEnd,
      },
      'zones': <String, dynamic>{
        for (var zone = 1; zone <= 6; zone++)
          '$zone': <String, dynamic>{
            'minMoisturePct': config.minMoisturePct,
            'targetMoisturePct': config.targetMoisturePct,
            'maxDurationSec': config.maxIrrigationDurationSec,
          },
      },
    });
  }

  void updateVentilationAutomation(AutomationConfig config) {
    _publish('automation/ventilation/set', <String, dynamic>{
      'enabled': config.ventilationMode == 'sensor' ||
          config.ventilationMode == 'hybrid',
      'mode': config.ventilationMode,
      'windows': <int>[1, 2],
      'openIf': <String, dynamic>{
        'temperatureAboveC': config.ventOpenTempC,
        'humidityAbovePct': config.ventOpenHumidityPct,
      },
      'closeIf': <String, dynamic>{
        'temperatureBelowC': config.ventCloseTempC,
        'humidityBelowPct': config.ventCloseHumidityPct,
      },
      'openDurationSec': config.ventOpenDurationSec,
      'closeDurationSec': config.ventCloseDurationSec,
      'minOpenTimeMin': 10,
      'cooldownMin': 5,
      'sensorSource': 'average',
    });
  }

  void setIrrigationSchedule(List<IrrigationScheduleItem> items) {
    _publish('schedule/irrigation/set', <String, dynamic>{
      'enabled': items.any((item) => item.enabled),
      'timezone': 'Europe/Moscow',
      'items': [
        for (var i = 0; i < items.length; i++) items[i].toJson(i + 1),
      ],
    });
  }

  void setVentilationSchedule(List<VentilationScheduleItem> items) {
    _publish('schedule/ventilation/set', <String, dynamic>{
      'enabled': items.any((item) => item.enabled),
      'timezone': 'Europe/Moscow',
      'items': [
        for (var i = 0; i < items.length; i++) items[i].toJson(i + 1),
      ],
    });
  }

  void syncTimeFromNtp() {
    _publish('time/sync', <String, dynamic>{});
  }

  void _publish(String command, Map<String, dynamic> params) {
    try {
      state.lastError = null;
      _mqttService.publishCommand(command, params);
    } on Object catch (error) {
      state.lastError = _humanError(error.toString());
      notifyListeners();
    }
  }

  Future<void> _loadSettings() async {
    final prefs = await SharedPreferences.getInstance();
    settings = MqttSettings(
      host: prefs.getString('mqtt.host') ?? settings.host,
      port: prefs.getInt('mqtt.port') ?? settings.port,
      username: prefs.getString('mqtt.username') ?? settings.username,
      password: prefs.getString('mqtt.password') ?? settings.password,
      clientId: prefs.getString('mqtt.clientId') ?? settings.clientId,
      deviceId: prefs.getString('mqtt.deviceId') ?? settings.deviceId,
    );
  }

  Future<void> _loadZoneNames() async {
    final prefs = await SharedPreferences.getInstance();
    state.zones = state.zones
        .map((zone) => zone.copyWith(
              name: prefs.getString('zone.${zone.index}.name') ?? zone.name,
            ))
        .toList();
  }

  void _handleMqttEnvelope(MqttEnvelope envelope) {
    final topic = envelope.topic;
    final payload = envelope.payload;

    if (topic.endsWith('/telemetry/snapshot')) {
      _applyAir(payload['air']);
      _applySoil(payload['soil']);
      _applyState(payload['state']);
      state.lastTelemetryAt = DateTime.now();
    } else if (topic.endsWith('/telemetry/air')) {
      _applyAir(payload);
      state.lastTelemetryAt = DateTime.now();
    } else if (topic.endsWith('/telemetry/soil')) {
      _applySoil(payload);
      state.lastTelemetryAt = DateTime.now();
    } else if (topic.endsWith('/telemetry/state')) {
      _applyState(payload);
      state.lastTelemetryAt = DateTime.now();
    } else if (topic.endsWith('/telemetry/config')) {
      _applyConfig(payload);
    } else if (topic.endsWith('/status/availability')) {
      state.availability = _asString(payload['status']) ?? 'unknown';
    } else if (topic.endsWith('/ack')) {
      final status = _asString(payload['status']) ?? 'unknown';
      final message = _asString(payload['message']) ?? '';
      state.lastAck = '$status: $message';
      if (status == 'rejected' || status == 'failed') {
        state.lastError = _humanAckError(payload);
      }
    }

    notifyListeners();
  }

  void _applyAir(Object? value) {
    final data = _asMap(value);
    if (data == null) return;

    final sensors = data['sensors'];
    if (sensors is! List) return;

    final parsed = sensors
        .whereType<Map>()
        .map((item) => AirSensor(
              id: _asString(item['id']) ?? 'air',
              temperatureC: _asDouble(item['temperatureC']),
              humidityPct: _asDouble(item['humidityPct']),
              status: _asString(item['status']) ?? 'unknown',
            ))
        .toList();

    if (parsed.isNotEmpty) {
      state.airSensors = parsed;
    }
  }

  void _applySoil(Object? value) {
    final data = _asMap(value);
    final zones = _asMap(data?['zones']);
    if (zones == null) return;

    state.zones = state.zones.map((zone) {
      final zoneData = _asMap(zones['${zone.index}']);
      if (zoneData == null) return zone;

      return zone.copyWith(
        raw: _asInt(zoneData['raw']),
        moisturePct: _asInt(zoneData['moisturePct']),
        status: _asString(zoneData['status']) ?? zone.status,
      );
    }).toList();
  }

  void _applyState(Object? value) {
    final data = _asMap(value);
    if (data == null) return;

    final irrigation = _asMap(data['irrigation']);
    if (irrigation != null) {
      state.irrigation = IrrigationState(
        active: irrigation['active'] == true,
        activeZones: _asIntList(irrigation['activeZones']),
        mode: _asString(irrigation['mode']) ?? state.irrigation.mode,
        remainingSec: _asInt(irrigation['remainingSec']) ?? 0,
      );
    }

    final valves = _asMap(data['valves']);
    if (valves != null) {
      state.zones = state.zones.map((zone) {
        return zone.copyWith(valveOn: valves['${zone.index}'] == 'on');
      }).toList();
    }

    final ventilation = _asMap(data['ventilation']);
    final windows = _asMap(ventilation?['windows']);
    if (windows != null) {
      state.windows = List<WindowStatus>.generate(2, (index) {
        final windowIndex = index + 1;
        final windowData = _asMap(windows['$windowIndex']);
        return WindowStatus(
          index: windowIndex,
          state: _asString(windowData?['state']) ?? 'unknown',
          remainingSec: _asInt(windowData?['remainingSec']) ?? 0,
        );
      });
    }
  }

  void _applyConfig(Object? value) {
    final data = _asMap(value);
    if (data == null) return;

    final modes = _asMap(data['modes']);
    final safety = _asMap(_asMap(data['safety'])?['irrigation']);
    final allowedTime = _asMap(safety?['allowedTime']);
    final automation = _asMap(data['automation']);
    final irrigation = _asMap(automation?['irrigation']);
    final irrigationZones = _asMap(irrigation?['zones']);
    final firstZone = _asMap(irrigationZones?['1']);
    final ventilation = _asMap(automation?['ventilation']);
    final openIf = _asMap(ventilation?['openIf']);
    final closeIf = _asMap(ventilation?['closeIf']);

    state.automation = state.automation.copyWith(
      irrigationMode:
          _asString(modes?['irrigation']) ?? state.automation.irrigationMode,
      ventilationMode:
          _asString(modes?['ventilation']) ?? state.automation.ventilationMode,
      safetyEnabled:
          safety?['enabled'] is bool ? safety!['enabled'] as bool : null,
      blockOutsideAllowedTime: safety?['blockOutsideAllowedTime'] is bool
          ? safety!['blockOutsideAllowedTime'] as bool
          : null,
      safetyStart:
          _asString(allowedTime?['start']) ?? state.automation.safetyStart,
      safetyEnd: _asString(allowedTime?['end']) ?? state.automation.safetyEnd,
      blockIfSoilAbovePct: _asInt(safety?['blockIfSoilAbovePct']),
      blockIfAirHumidityAbovePct: _asInt(safety?['blockIfAirHumidityAbovePct']),
      blockIfAirTempBelowC: _asInt(safety?['blockIfAirTempBelowC']),
      allowManualForce: safety?['allowManualForce'] is bool
          ? safety!['allowManualForce'] as bool
          : null,
      minMoisturePct: _asInt(firstZone?['minMoisturePct']),
      targetMoisturePct: _asInt(firstZone?['targetMoisturePct']),
      maxIrrigationDurationSec: _asInt(firstZone?['maxDurationSec']),
      ventOpenTempC: _asInt(openIf?['temperatureAboveC']),
      ventOpenHumidityPct: _asInt(openIf?['humidityAbovePct']),
      ventCloseTempC: _asInt(closeIf?['temperatureBelowC']),
      ventCloseHumidityPct: _asInt(closeIf?['humidityBelowPct']),
      ventOpenDurationSec: _asInt(ventilation?['openDurationSec']),
      ventCloseDurationSec: _asInt(ventilation?['closeDurationSec']),
    );
  }
}

String _humanAckError(Map<String, dynamic> payload) {
  final code = _asString(payload['errorCode']);
  final message = _asString(payload['message']) ?? '';
  if (code == 'BAD_PARAM') return 'Команда содержит неверные параметры.';
  if (code == 'BUSY') return 'ESP32 занята выполнением другой команды.';
  if (code == 'SAFETY_LOCKOUT') {
    return 'Полив запрещён защитой: ${_humanError(message)}';
  }
  return _humanError(message.isEmpty ? 'Команда не выполнена.' : message);
}

String _humanError(String value) {
  var text = value;
  final replacements = <String, String>{
    'State lock timeout': 'ESP32 занята обработкой команды.',
    'BUSY': 'ESP32 занята.',
    'BAD_PARAM': 'Неверные параметры команды.',
    'SAFETY_LOCKOUT': 'Команда заблокирована защитой.',
    'No valid irrigation zones': 'Нет доступных зон для полива.',
    'Invalid irrigation zone': 'Неверный номер зоны полива.',
    'Soil moisture is above safety limit':
        'Влажность почвы выше безопасного предела.',
    'Air humidity is above safety limit':
        'Влажность воздуха выше безопасного предела.',
    'Air temperature is too low for irrigation':
        'Температура воздуха слишком низкая для полива.',
    'Irrigation outside allowed time': 'Сейчас полив запрещён по времени.',
    'Missing windows array': 'Не выбраны окна для проветривания.',
    'Window must be 1 or 2': 'Окно должно быть северным или южным.',
    'Invalid ventilation action': 'Неверная команда проветривания.',
    'Invalid schedule': 'Неверное расписание.',
    'Client is not connected': 'Нет подключения к MQTT.',
    'Connection refused': 'MQTT-сервер отклонил подключение.',
    'Failed host lookup': 'Не удалось найти MQTT-сервер.',
  };

  replacements.forEach((from, to) {
    text = text.replaceAll(from, to);
  });
  if (text.trim().isEmpty) return 'Неизвестная ошибка.';
  return text;
}

Map<String, dynamic>? _asMap(Object? value) {
  if (value is Map<String, dynamic>) return value;
  if (value is Map) {
    return value.map((key, val) => MapEntry(key.toString(), val));
  }
  return null;
}

String? _asString(Object? value) => value?.toString();

int? _asInt(Object? value) {
  if (value is int) return value;
  if (value is double) return value.round();
  if (value is num) return value.toInt();
  return int.tryParse(value?.toString() ?? '');
}

double? _asDouble(Object? value) {
  if (value is double) return value;
  if (value is num) return value.toDouble();
  return double.tryParse(value?.toString() ?? '');
}

List<int> _asIntList(Object? value) {
  if (value is! List) return const <int>[];
  return value.map(_asInt).whereType<int>().toList();
}

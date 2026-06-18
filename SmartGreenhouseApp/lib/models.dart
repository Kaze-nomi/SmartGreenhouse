enum ConnectionStatus {
  disconnected,
  connecting,
  connected,
  error,
}

class MqttSettings {
  const MqttSettings({
    required this.host,
    required this.port,
    required this.username,
    required this.password,
    required this.clientId,
    required this.deviceId,
  });

  factory MqttSettings.defaults() {
    return const MqttSettings(
      host: 'm7.wqtt.ru',
      port: 22028,
      username: '',
      password: '',
      clientId: 'greenhouse-mobile',
      deviceId: 'esp32-greenhouse-01',
    );
  }

  final String host;
  final int port;
  final String username;
  final String password;
  final String clientId;
  final String deviceId;

  String get baseTopic => 'greenhouse/v1/$deviceId';

  MqttSettings copyWith({
    String? host,
    int? port,
    String? username,
    String? password,
    String? clientId,
    String? deviceId,
  }) {
    return MqttSettings(
      host: host ?? this.host,
      port: port ?? this.port,
      username: username ?? this.username,
      password: password ?? this.password,
      clientId: clientId ?? this.clientId,
      deviceId: deviceId ?? this.deviceId,
    );
  }
}

class AirSensor {
  const AirSensor({
    required this.id,
    this.temperatureC,
    this.humidityPct,
    this.status = 'unknown',
  });

  final String id;
  final double? temperatureC;
  final double? humidityPct;
  final String status;
}

class SoilZone {
  const SoilZone({
    required this.index,
    required this.name,
    this.raw,
    this.moisturePct,
    this.status = 'unknown',
    this.valveOn = false,
  });

  final int index;
  final String name;
  final int? raw;
  final int? moisturePct;
  final String status;
  final bool valveOn;

  SoilZone copyWith({
    String? name,
    int? raw,
    int? moisturePct,
    String? status,
    bool? valveOn,
  }) {
    return SoilZone(
      index: index,
      name: name ?? this.name,
      raw: raw ?? this.raw,
      moisturePct: moisturePct ?? this.moisturePct,
      status: status ?? this.status,
      valveOn: valveOn ?? this.valveOn,
    );
  }
}

class WindowStatus {
  const WindowStatus({
    required this.index,
    this.state = 'unknown',
    this.remainingSec = 0,
  });

  final int index;
  final String state;
  final int remainingSec;
}

class IrrigationState {
  const IrrigationState({
    this.active = false,
    this.activeZones = const <int>[],
    this.mode = 'manual',
    this.remainingSec = 0,
  });

  final bool active;
  final List<int> activeZones;
  final String mode;
  final int remainingSec;
}

class AutomationConfig {
  const AutomationConfig({
    this.irrigationMode = 'manual',
    this.ventilationMode = 'manual',
    this.safetyEnabled = true,
    this.blockOutsideAllowedTime = true,
    this.safetyStart = '06:00',
    this.safetyEnd = '21:00',
    this.blockIfSoilAbovePct = 78,
    this.blockIfAirHumidityAbovePct = 88,
    this.blockIfAirTempBelowC = 14,
    this.allowManualForce = false,
    this.minMoisturePct = 50,
    this.targetMoisturePct = 62,
    this.maxIrrigationDurationSec = 60,
    this.ventOpenTempC = 28,
    this.ventOpenHumidityPct = 78,
    this.ventCloseTempC = 24,
    this.ventCloseHumidityPct = 68,
    this.ventOpenDurationSec = 60,
    this.ventCloseDurationSec = 60,
  });

  final String irrigationMode;
  final String ventilationMode;
  final bool safetyEnabled;
  final bool blockOutsideAllowedTime;
  final String safetyStart;
  final String safetyEnd;
  final int blockIfSoilAbovePct;
  final int blockIfAirHumidityAbovePct;
  final int blockIfAirTempBelowC;
  final bool allowManualForce;
  final int minMoisturePct;
  final int targetMoisturePct;
  final int maxIrrigationDurationSec;
  final int ventOpenTempC;
  final int ventOpenHumidityPct;
  final int ventCloseTempC;
  final int ventCloseHumidityPct;
  final int ventOpenDurationSec;
  final int ventCloseDurationSec;

  AutomationConfig copyWith({
    String? irrigationMode,
    String? ventilationMode,
    bool? safetyEnabled,
    bool? blockOutsideAllowedTime,
    String? safetyStart,
    String? safetyEnd,
    int? blockIfSoilAbovePct,
    int? blockIfAirHumidityAbovePct,
    int? blockIfAirTempBelowC,
    bool? allowManualForce,
    int? minMoisturePct,
    int? targetMoisturePct,
    int? maxIrrigationDurationSec,
    int? ventOpenTempC,
    int? ventOpenHumidityPct,
    int? ventCloseTempC,
    int? ventCloseHumidityPct,
    int? ventOpenDurationSec,
    int? ventCloseDurationSec,
  }) {
    return AutomationConfig(
      irrigationMode: irrigationMode ?? this.irrigationMode,
      ventilationMode: ventilationMode ?? this.ventilationMode,
      safetyEnabled: safetyEnabled ?? this.safetyEnabled,
      blockOutsideAllowedTime:
          blockOutsideAllowedTime ?? this.blockOutsideAllowedTime,
      safetyStart: safetyStart ?? this.safetyStart,
      safetyEnd: safetyEnd ?? this.safetyEnd,
      blockIfSoilAbovePct: blockIfSoilAbovePct ?? this.blockIfSoilAbovePct,
      blockIfAirHumidityAbovePct:
          blockIfAirHumidityAbovePct ?? this.blockIfAirHumidityAbovePct,
      blockIfAirTempBelowC: blockIfAirTempBelowC ?? this.blockIfAirTempBelowC,
      allowManualForce: allowManualForce ?? this.allowManualForce,
      minMoisturePct: minMoisturePct ?? this.minMoisturePct,
      targetMoisturePct: targetMoisturePct ?? this.targetMoisturePct,
      maxIrrigationDurationSec:
          maxIrrigationDurationSec ?? this.maxIrrigationDurationSec,
      ventOpenTempC: ventOpenTempC ?? this.ventOpenTempC,
      ventOpenHumidityPct: ventOpenHumidityPct ?? this.ventOpenHumidityPct,
      ventCloseTempC: ventCloseTempC ?? this.ventCloseTempC,
      ventCloseHumidityPct: ventCloseHumidityPct ?? this.ventCloseHumidityPct,
      ventOpenDurationSec: ventOpenDurationSec ?? this.ventOpenDurationSec,
      ventCloseDurationSec: ventCloseDurationSec ?? this.ventCloseDurationSec,
    );
  }
}

class IrrigationScheduleItem {
  const IrrigationScheduleItem({
    required this.time,
    this.enabled = true,
    this.allZones = true,
    this.zones = const <int>[],
    this.durationSec = 60,
  });

  final String time;
  final bool enabled;
  final bool allZones;
  final List<int> zones;
  final int durationSec;

  Map<String, dynamic> toJson(int index) {
    return <String, dynamic>{
      'id': 'water-$index',
      'enabled': enabled,
      'time': time,
      'zones': allZones ? 'all' : zones,
      'durationSec': durationSec,
      'zoneDurationSec': durationSec,
    };
  }
}

class VentilationScheduleItem {
  const VentilationScheduleItem({
    required this.time,
    this.enabled = true,
    this.windows = const <int>[1, 2],
    this.action = 'open',
    this.durationSec = 60,
  });

  final String time;
  final bool enabled;
  final List<int> windows;
  final String action;
  final int durationSec;

  Map<String, dynamic> toJson(int index) {
    return <String, dynamic>{
      'id': 'vent-$index',
      'enabled': enabled,
      'time': time,
      'windows': windows,
      'action': action,
      'durationSec': durationSec,
    };
  }
}

class GreenhouseState {
  GreenhouseState.initial()
      : zones = List<SoilZone>.generate(
          6,
          (index) => SoilZone(index: index + 1, name: 'Грядка ${index + 1}'),
        ),
        airSensors = const <AirSensor>[
          AirSensor(id: 'air1'),
          AirSensor(id: 'air2'),
        ],
        windows = List<WindowStatus>.generate(
          2,
          (index) => WindowStatus(index: index + 1),
        );

  List<SoilZone> zones;
  List<AirSensor> airSensors;
  List<WindowStatus> windows;
  IrrigationState irrigation = const IrrigationState();
  AutomationConfig automation = const AutomationConfig();
  String availability = 'unknown';
  String? lastAck;
  String? lastError;
  DateTime? lastTelemetryAt;

  double? get averageTemperature {
    final values = airSensors
        .map((sensor) => sensor.temperatureC)
        .whereType<double>()
        .toList();
    if (values.isEmpty) return null;
    return values.reduce((a, b) => a + b) / values.length;
  }

  double? get averageHumidity {
    final values = airSensors
        .map((sensor) => sensor.humidityPct)
        .whereType<double>()
        .toList();
    if (values.isEmpty) return null;
    return values.reduce((a, b) => a + b) / values.length;
  }
}

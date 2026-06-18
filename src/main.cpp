#define MQTT_MAX_PACKET_SIZE 4096

#include <Adafruit_SHT31.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <RTClib.h>
#include <WiFi.h>
#include <Wire.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <math.h>
#include <sys/time.h>
#include <time.h>

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

#ifndef MQTT_BROKER_ADDRESS
#define MQTT_BROKER_ADDRESS "127.0.0.1"
#endif

#ifndef MQTT_PORT
#define MQTT_PORT 0
#endif

#ifndef MQTT_USER
#define MQTT_USER ""
#endif

#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD ""
#endif

#ifndef MQTT_CLIENT_ID
#define MQTT_CLIENT_ID "esp32-greenhouse-01"
#endif

#ifndef DEVICE_ID
#define DEVICE_ID "esp32-greenhouse-01"
#endif

#ifndef NTP_SERVER_1
#define NTP_SERVER_1 "pool.ntp.org"
#endif

#ifndef NTP_SERVER_2
#define NTP_SERVER_2 "time.google.com"
#endif

#ifndef NTP_SYNC_INTERVAL_MIN
#define NTP_SYNC_INTERVAL_MIN 360
#endif

#ifndef RELAY_ACTIVE_LOW
#define RELAY_ACTIVE_LOW 1
#endif

constexpr uint8_t SOIL_COUNT = 6;
constexpr uint8_t AIR_COUNT = 2;
constexpr uint8_t WINDOW_COUNT = 2;
constexpr uint8_t MAX_SCHEDULE_ITEMS = 8;
constexpr uint8_t MAX_QUEUE_ITEMS = 16;
constexpr const char *MOSCOW_TZ = "MSK-3";
constexpr const char *MOSCOW_TIMEZONE_NAME = "Europe/Moscow";
constexpr uint32_t COMMAND_DEBOUNCE_MS = 900;
constexpr uint32_t REQUEST_ID_CACHE_MS = 60000;

const uint8_t SOIL_PINS[SOIL_COUNT] = {36, 39, 34, 35, 32, 33};
const uint8_t VALVE_RELAY_PINS[SOIL_COUNT] = {19, 18, 5, 17, 16, 4};
const uint8_t WINDOW_RELAY_PINS[WINDOW_COUNT][2] = {
  {25, 26},
  {27, 14},
};
const uint8_t SHT30_ADDR[AIR_COUNT] = {0x44, 0x45};

const int SOIL_DRY_RAW[SOIL_COUNT] = {3000, 3000, 3000, 3000, 3000, 3000};
const int SOIL_WET_RAW[SOIL_COUNT] = {1200, 1200, 1200, 1200, 1200, 1200};

constexpr uint8_t RELAY_ON_LEVEL = RELAY_ACTIVE_LOW ? LOW : HIGH;
constexpr uint8_t RELAY_OFF_LEVEL = RELAY_ACTIVE_LOW ? HIGH : LOW;

struct AirReading {
  float temperatureC = NAN;
  float humidityPct = NAN;
  bool ok = false;
};

struct SoilReading {
  int raw = 0;
  int moisturePct = 0;
  bool ok = false;
};

enum class WindowState {
  Open,
  Closed,
  Opening,
  Closing,
  Stopped,
  Unknown,
};

struct WindowRuntime {
  WindowState state = WindowState::Unknown;
  uint32_t actionUntilMs = 0;
  uint32_t openedAtMs = 0;
};

struct IrrigationRuntime {
  int queueZones[MAX_QUEUE_ITEMS] = {};
  uint16_t queueDurations[MAX_QUEUE_ITEMS] = {};
  int queueTargets[MAX_QUEUE_ITEMS] = {};
  bool queueForces[MAX_QUEUE_ITEMS] = {};
  uint8_t queueLen = 0;
  uint8_t queuePos = 0;
  int currentZone = -1;
  uint32_t currentUntilMs = 0;
  bool currentForce = false;
  uint32_t lastWateredMs[SOIL_COUNT] = {};
};

struct IrrigationAutoZone {
  int minMoisturePct = 50;
  int targetMoisturePct = 62;
  uint16_t maxDurationSec = 60;
};

struct IrrigationAutoConfig {
  bool enabled = false;
  String mode = "manual";
  IrrigationAutoZone zones[SOIL_COUNT];
  uint16_t cooldownMin = 30;
  String allowedStart = "06:00";
  String allowedEnd = "21:00";
  String strategy = "sequential";
};

struct IrrigationSafetyConfig {
  bool enabled = true;
  bool blockOutsideAllowedTime = true;
  String allowedStart = "06:00";
  String allowedEnd = "21:00";
  int blockIfSoilAbovePct = 78;
  float blockIfAirHumidityAbovePct = 88.0f;
  float blockIfAirTempBelowC = 14.0f;
  bool allowManualForce = false;
};

struct VentAutoConfig {
  bool enabled = false;
  String mode = "manual";
  int targetWindows[WINDOW_COUNT] = {1, 2};
  uint8_t targetWindowCount = WINDOW_COUNT;
  float openTempC = 28.0f;
  float openHumidityPct = 78.0f;
  float closeTempC = 24.0f;
  float closeHumidityPct = 68.0f;
  uint16_t openDurationSec = 60;
  uint16_t closeDurationSec = 60;
  uint16_t minOpenTimeMin = 10;
  uint16_t cooldownMin = 5;
  String sensorSource = "average";
  uint32_t lastActionMs = 0;
};

struct ScheduleItem {
  String id;
  bool enabled = false;
  uint8_t dayMask = 0x7f;
  uint8_t hour = 0;
  uint8_t minute = 0;
  bool allZones = false;
  int zones[SOIL_COUNT] = {};
  uint8_t zoneCount = 0;
  uint16_t durationSec = 60;
  uint16_t zoneDurationSec = 60;
  int skipIfMoistureAbovePct = -1;
  int windows[WINDOW_COUNT] = {};
  uint8_t windowCount = 0;
  String action = "open";
  uint16_t actionDurationSec = 60;
  int lastRunKey = -1;
};

struct ScheduleConfig {
  bool enabled = false;
  ScheduleItem items[MAX_SCHEDULE_ITEMS];
  uint8_t count = 0;
};

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Preferences preferences;
Adafruit_SHT31 sht30Sensors[AIR_COUNT];
RTC_DS3231 rtcDs3231;
RTC_DS1307 rtcDs1307;
SemaphoreHandle_t stateMutex = nullptr;
SemaphoreHandle_t mqttMutex = nullptr;

enum class RtcDevice {
  None,
  DS3231,
  DS1307,
};

AirReading air[AIR_COUNT];
SoilReading soil[SOIL_COUNT];
WindowRuntime windows[WINDOW_COUNT];
IrrigationRuntime irrigation;
IrrigationAutoConfig irrigationAuto;
IrrigationSafetyConfig irrigationSafety;
VentAutoConfig ventAuto;
ScheduleConfig irrigationSchedule;
ScheduleConfig ventSchedule;
bool sht30Ready[AIR_COUNT] = {};
RtcDevice rtcDevice = RtcDevice::None;

String mqttHost = MQTT_BROKER_ADDRESS;
uint16_t mqttPort = MQTT_PORT;
String deviceId = DEVICE_ID;
uint32_t lastSensorReadMs = 0;
uint32_t lastTelemetryMs = 0;
uint32_t lastAutomationMs = 0;
String lastCommandFingerprint;
uint32_t lastCommandFingerprintMs = 0;
String lastRequestId;
uint32_t lastRequestIdMs = 0;
uint32_t lastScheduleCheckMs = 0;
uint32_t lastNtpAttemptMs = 0;
uint32_t lastMqttConnectAttemptMs = 0;
bool ntpConfigured = false;
bool ntpEverSynced = false;
String lastIrrigationBlockReason;

bool syncTimeFromNtp(bool force);
void averageAir(float &temperature, float &humidity, bool &ok);
bool isNowWithinTimeWindow(const String &startValue, const String &endValue);

bool lockRecursive(SemaphoreHandle_t mutex, TickType_t timeout = pdMS_TO_TICKS(1000)) {
  return mutex == nullptr || xSemaphoreTakeRecursive(mutex, timeout) == pdTRUE;
}

void unlockRecursive(SemaphoreHandle_t mutex) {
  if (mutex != nullptr) {
    xSemaphoreGiveRecursive(mutex);
  }
}

bool lockState(TickType_t timeout = pdMS_TO_TICKS(1000)) {
  return lockRecursive(stateMutex, timeout);
}

void unlockState() {
  unlockRecursive(stateMutex);
}

bool lockMqtt(TickType_t timeout = pdMS_TO_TICKS(1000)) {
  return lockRecursive(mqttMutex, timeout);
}

void unlockMqtt() {
  unlockRecursive(mqttMutex);
}

String baseTopic() {
  return "greenhouse/v1/" + deviceId;
}

bool isReadOnlyCommand(const String &command) {
  return command.endsWith("/get") || command == "sensors/get" || command == "time/get";
}

bool rejectRepeatedCommand(const String &requestId, const String &command,
                           JsonObject params, String &reason) {
  if (isReadOnlyCommand(command)) {
    return false;
  }

  uint32_t nowMs = millis();
  if (requestId.length() > 0 && requestId == lastRequestId &&
      nowMs - lastRequestIdMs < REQUEST_ID_CACHE_MS) {
    reason = "Duplicate requestId";
    return true;
  }

  String paramsJson;
  serializeJson(params, paramsJson);
  String fingerprint = command + "|" + paramsJson;
  if (fingerprint == lastCommandFingerprint &&
      nowMs - lastCommandFingerprintMs < COMMAND_DEBOUNCE_MS) {
    reason = "Repeated command too fast";
    return true;
  }

  if (requestId.length() > 0) {
    lastRequestId = requestId;
    lastRequestIdMs = nowMs;
  }
  lastCommandFingerprint = fingerprint;
  lastCommandFingerprintMs = nowMs;
  return false;
}

const char *windowStateToString(WindowState state) {
  switch (state) {
    case WindowState::Open:
      return "open";
    case WindowState::Closed:
      return "closed";
    case WindowState::Opening:
      return "opening";
    case WindowState::Closing:
      return "closing";
    case WindowState::Stopped:
      return "stopped";
    default:
      return "unknown";
  }
}

void parseMqttEndpoint() {
  mqttHost = MQTT_BROKER_ADDRESS;
  mqttHost.trim();

  if (mqttHost.startsWith("mqtt://")) {
    mqttHost = mqttHost.substring(7);
  }

  int slash = mqttHost.indexOf('/');
  if (slash >= 0) {
    mqttHost = mqttHost.substring(0, slash);
  }

  int colon = mqttHost.lastIndexOf(':');
  if (colon > 0 && colon < static_cast<int>(mqttHost.length()) - 1) {
    String portPart = mqttHost.substring(colon + 1);
    bool numeric = true;
    for (uint16_t i = 0; i < portPart.length(); i++) {
      if (!isDigit(portPart[i])) {
        numeric = false;
        break;
      }
    }
    if (numeric) {
      mqttPort = static_cast<uint16_t>(portPart.toInt());
      mqttHost = mqttHost.substring(0, colon);
    }
  }
}

String isoNow() {
  time_t now = time(nullptr);
  struct tm tmNow;
  localtime_r(&now, &tmNow);

  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d+03:00",
           tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday,
           tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);
  return String(buffer);
}

bool isSystemTimeValid() {
  return time(nullptr) > 1700000000;
}

bool readRtc(time_t &result) {
  if (rtcDevice == RtcDevice::None) {
    return false;
  }

  DateTime dateTime = rtcDevice == RtcDevice::DS1307 ? rtcDs1307.now() : rtcDs3231.now();

  struct tm tmValue = {};
  tmValue.tm_year = dateTime.year() - 1900;
  tmValue.tm_mon = dateTime.month() - 1;
  tmValue.tm_mday = dateTime.day();
  tmValue.tm_hour = dateTime.hour();
  tmValue.tm_min = dateTime.minute();
  tmValue.tm_sec = dateTime.second();
  tmValue.tm_isdst = -1;

  if (dateTime.year() < 2024 || dateTime.month() < 1 || dateTime.month() > 12) {
    return false;
  }

  result = mktime(&tmValue);
  return result > 0;
}

bool writeRtc(time_t value) {
  if (rtcDevice == RtcDevice::None) {
    return false;
  }

  struct tm tmValue;
  localtime_r(&value, &tmValue);

  DateTime dateTime(tmValue.tm_year + 1900, tmValue.tm_mon + 1, tmValue.tm_mday,
                    tmValue.tm_hour, tmValue.tm_min, tmValue.tm_sec);

  if (rtcDevice == RtcDevice::DS1307) {
    rtcDs1307.adjust(dateTime);
  } else {
    rtcDs3231.adjust(dateTime);
  }

  time_t verify;
  return readRtc(verify) && abs(static_cast<long>(verify - value)) <= 2;
}

bool parseIsoLocal(const char *value, time_t &result) {
  if (!value) {
    return false;
  }

  int year, month, day, hour, minute, second;
  if (sscanf(value, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6) {
    return false;
  }

  struct tm tmValue = {};
  tmValue.tm_year = year - 1900;
  tmValue.tm_mon = month - 1;
  tmValue.tm_mday = day;
  tmValue.tm_hour = hour;
  tmValue.tm_min = minute;
  tmValue.tm_sec = second;
  tmValue.tm_isdst = -1;
  result = mktime(&tmValue);
  return result > 0;
}

void setSystemTime(time_t value) {
  timeval tv = {
    .tv_sec = value,
    .tv_usec = 0,
  };
  settimeofday(&tv, nullptr);
}

bool readSht30(uint8_t index, AirReading &reading) {
  if (index >= AIR_COUNT || !sht30Ready[index]) {
    reading.ok = false;
    return false;
  }

  float temperature = sht30Sensors[index].readTemperature();
  float humidity = sht30Sensors[index].readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    reading.ok = false;
    return false;
  }

  reading.temperatureC = temperature;
  reading.humidityPct = humidity;
  reading.ok = true;
  return true;
}

int readSoilRaw(uint8_t pin) {
  long sum = 0;
  constexpr uint8_t samples = 32;
  for (uint8_t i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delayMicroseconds(800);
  }
  return static_cast<int>(sum / samples);
}

int rawToMoisturePct(uint8_t index, int raw) {
  int dry = SOIL_DRY_RAW[index];
  int wet = SOIL_WET_RAW[index];
  if (dry == wet) {
    return 0;
  }

  int pct = (dry - raw) * 100 / (dry - wet);
  return constrain(pct, 0, 100);
}

void readAllSensors() {
  for (uint8_t i = 0; i < AIR_COUNT; i++) {
    readSht30(i, air[i]);
  }

  for (uint8_t i = 0; i < SOIL_COUNT; i++) {
    soil[i].raw = readSoilRaw(SOIL_PINS[i]);
    soil[i].moisturePct = rawToMoisturePct(i, soil[i].raw);
    soil[i].ok = true;
  }
}

void writeRelay(uint8_t pin, bool on) {
  digitalWrite(pin, on ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL);
}

void setValve(uint8_t zone, bool on) {
  if (zone < 1 || zone > SOIL_COUNT) {
    return;
  }
  writeRelay(VALVE_RELAY_PINS[zone - 1], on);
}

void stopAllValves() {
  for (uint8_t zone = 1; zone <= SOIL_COUNT; zone++) {
    setValve(zone, false);
  }
}

void stopWindow(uint8_t window) {
  if (window < 1 || window > WINDOW_COUNT) {
    return;
  }
  uint8_t index = window - 1;
  writeRelay(WINDOW_RELAY_PINS[index][0], false);
  writeRelay(WINDOW_RELAY_PINS[index][1], false);
  windows[index].state = WindowState::Stopped;
  windows[index].actionUntilMs = 0;
}

void startWindowAction(uint8_t window, const String &action, uint16_t durationSec) {
  if (window < 1 || window > WINDOW_COUNT) {
    return;
  }

  uint8_t index = window - 1;
  writeRelay(WINDOW_RELAY_PINS[index][0], false);
  writeRelay(WINDOW_RELAY_PINS[index][1], false);
  delay(400);

  if (action == "open") {
    writeRelay(WINDOW_RELAY_PINS[index][0], true);
    writeRelay(WINDOW_RELAY_PINS[index][1], false);
    windows[index].state = WindowState::Opening;
    windows[index].openedAtMs = millis();
    windows[index].actionUntilMs = millis() + durationSec * 1000UL;
  } else if (action == "close") {
    writeRelay(WINDOW_RELAY_PINS[index][0], false);
    writeRelay(WINDOW_RELAY_PINS[index][1], true);
    windows[index].state = WindowState::Closing;
    windows[index].actionUntilMs = millis() + durationSec * 1000UL;
  } else {
    windows[index].state = WindowState::Stopped;
    windows[index].actionUntilMs = 0;
  }
}

void updateWindows() {
  uint32_t nowMs = millis();
  for (uint8_t i = 0; i < WINDOW_COUNT; i++) {
    if (windows[i].actionUntilMs == 0 || static_cast<int32_t>(nowMs - windows[i].actionUntilMs) < 0) {
      continue;
    }

    writeRelay(WINDOW_RELAY_PINS[i][0], false);
    writeRelay(WINDOW_RELAY_PINS[i][1], false);
    if (windows[i].state == WindowState::Opening) {
      windows[i].state = WindowState::Open;
    } else if (windows[i].state == WindowState::Closing) {
      windows[i].state = WindowState::Closed;
    } else {
      windows[i].state = WindowState::Stopped;
    }
    windows[i].actionUntilMs = 0;
  }
}

void clearIrrigationQueue() {
  irrigation.queueLen = 0;
  irrigation.queuePos = 0;
}

bool irrigationForceAllowed(bool force) {
  return force && irrigationSafety.allowManualForce;
}

bool isIrrigationBlocked(uint8_t zone, bool force, String &reason) {
  if (!irrigationSafety.enabled || irrigationForceAllowed(force)) {
    return false;
  }
  if (zone < 1 || zone > SOIL_COUNT) {
    reason = "Invalid irrigation zone";
    return true;
  }

  if (irrigationSafety.blockOutsideAllowedTime &&
      !isNowWithinTimeWindow(irrigationSafety.allowedStart, irrigationSafety.allowedEnd)) {
    reason = "Irrigation is blocked outside allowed time window";
    return true;
  }

  if (soil[zone - 1].ok && soil[zone - 1].moisturePct >= irrigationSafety.blockIfSoilAbovePct) {
    reason = "Soil moisture safety limit reached";
    return true;
  }

  float temperature, humidity;
  bool airOk;
  averageAir(temperature, humidity, airOk);
  if (airOk && humidity >= irrigationSafety.blockIfAirHumidityAbovePct) {
    reason = "Air humidity safety limit reached";
    return true;
  }
  if (airOk && temperature <= irrigationSafety.blockIfAirTempBelowC) {
    reason = "Air temperature is too low for irrigation";
    return true;
  }

  return false;
}

bool enqueueIrrigation(uint8_t zone, uint16_t durationSec, int targetPct, bool force = false) {
  if (zone < 1 || zone > SOIL_COUNT || irrigation.queueLen >= MAX_QUEUE_ITEMS) {
    return false;
  }

  String blockReason;
  if (isIrrigationBlocked(zone, force, blockReason)) {
    lastIrrigationBlockReason = blockReason;
    return false;
  }

  irrigation.queueZones[irrigation.queueLen] = zone;
  irrigation.queueDurations[irrigation.queueLen] = durationSec;
  irrigation.queueTargets[irrigation.queueLen] = targetPct;
  irrigation.queueForces[irrigation.queueLen] = force;
  irrigation.queueLen++;
  return true;
}

void startNextIrrigationZone() {
  if (irrigation.currentZone != -1 || irrigation.queuePos >= irrigation.queueLen) {
    return;
  }

  uint8_t zone = irrigation.queueZones[irrigation.queuePos];
  uint16_t durationSec = irrigation.queueDurations[irrigation.queuePos];
  irrigation.currentZone = zone;
  irrigation.currentUntilMs = millis() + durationSec * 1000UL;
  irrigation.currentForce = irrigation.queueForces[irrigation.queuePos];
  setValve(zone, true);
}

void stopCurrentIrrigationZone() {
  if (irrigation.currentZone == -1) {
    return;
  }

  setValve(irrigation.currentZone, false);
  irrigation.lastWateredMs[irrigation.currentZone - 1] = millis();
  irrigation.currentZone = -1;
  irrigation.currentUntilMs = 0;
  irrigation.currentForce = false;
  irrigation.queuePos++;
}

void stopIrrigationZones(JsonVariant zonesValue) {
  if (zonesValue.is<const char *>() && String(zonesValue.as<const char *>()) == "all") {
    if (irrigation.currentZone != -1) {
      stopCurrentIrrigationZone();
    }
    stopAllValves();
    clearIrrigationQueue();
    return;
  }

  if (!zonesValue.is<JsonArray>()) {
    return;
  }

  JsonArray zones = zonesValue.as<JsonArray>();
  for (JsonVariant value : zones) {
    uint8_t zone = value.as<uint8_t>();
    if (zone >= 1 && zone <= SOIL_COUNT) {
      if (irrigation.currentZone == zone) {
        stopCurrentIrrigationZone();
      } else {
        setValve(zone, false);
      }
    }
  }
}

void updateIrrigation() {
  if (irrigation.currentZone != -1) {
    int queueIndex = irrigation.queuePos;
    int target = queueIndex < irrigation.queueLen ? irrigation.queueTargets[queueIndex] : -1;
    bool targetReached = target >= 0 && soil[irrigation.currentZone - 1].ok &&
                         soil[irrigation.currentZone - 1].moisturePct >= target;
    bool timeout = static_cast<int32_t>(millis() - irrigation.currentUntilMs) >= 0;
    String blockReason;
    bool safetyBlocked = isIrrigationBlocked(irrigation.currentZone, irrigation.currentForce, blockReason);

    if (targetReached || timeout || safetyBlocked) {
      if (safetyBlocked) {
        lastIrrigationBlockReason = blockReason;
      }
      stopCurrentIrrigationZone();
    }
  }

  if (irrigation.currentZone == -1 && irrigation.queuePos < irrigation.queueLen) {
    startNextIrrigationZone();
  }

  if (irrigation.currentZone == -1 && irrigation.queuePos >= irrigation.queueLen) {
    clearIrrigationQueue();
  }
}

bool publishJson(const String &topic, JsonDocument &doc, bool retained = false) {
  String payload;
  serializeJson(doc, payload);

  if (!lockMqtt(pdMS_TO_TICKS(2000))) {
    return false;
  }
  bool ok = mqttClient.connected() && mqttClient.publish(topic.c_str(), payload.c_str(), retained);
  unlockMqtt();
  return ok;
}

void publishAck(const String &requestId, const String &command, const String &status,
                const String &message, const char *errorCode = nullptr) {
  StaticJsonDocument<512> doc;
  doc["requestId"] = requestId;
  doc["command"] = command;
  doc["status"] = status;
  doc["ts"] = isoNow();
  doc["message"] = message;
  if (errorCode) {
    doc["errorCode"] = errorCode;
  }
  publishJson(baseTopic() + "/ack", doc);
}

void fillAirJson(JsonObject root) {
  JsonArray sensors = root.createNestedArray("sensors");
  float tempSum = 0.0f;
  float humSum = 0.0f;
  uint8_t okCount = 0;

  for (uint8_t i = 0; i < AIR_COUNT; i++) {
    JsonObject item = sensors.createNestedObject();
    item["id"] = i == 0 ? "air1" : "air2";
    item["type"] = "SHT30";
    item["i2cAddress"] = i == 0 ? "0x44" : "0x45";
    item["status"] = air[i].ok ? "ok" : "unavailable";
    if (air[i].ok) {
      item["temperatureC"] = roundf(air[i].temperatureC * 10.0f) / 10.0f;
      item["humidityPct"] = roundf(air[i].humidityPct * 10.0f) / 10.0f;
      tempSum += air[i].temperatureC;
      humSum += air[i].humidityPct;
      okCount++;
    }
  }

  JsonObject average = root.createNestedObject("average");
  if (okCount > 0) {
    average["temperatureC"] = roundf((tempSum / okCount) * 10.0f) / 10.0f;
    average["humidityPct"] = roundf((humSum / okCount) * 10.0f) / 10.0f;
  }
}

void fillSoilJson(JsonObject root) {
  JsonObject zones = root.createNestedObject("zones");
  for (uint8_t i = 0; i < SOIL_COUNT; i++) {
    JsonObject zone = zones.createNestedObject(String(i + 1));
    zone["raw"] = soil[i].raw;
    zone["moisturePct"] = soil[i].moisturePct;
    zone["status"] = soil[i].ok ? "ok" : "unavailable";
  }
}

void fillStateJson(JsonObject root) {
  JsonObject irrigationObj = root.createNestedObject("irrigation");
  irrigationObj["active"] = irrigation.currentZone != -1;
  JsonArray activeZones = irrigationObj.createNestedArray("activeZones");
  if (irrigation.currentZone != -1) {
    activeZones.add(irrigation.currentZone);
    uint32_t remaining = irrigation.currentUntilMs > millis() ? (irrigation.currentUntilMs - millis()) / 1000UL : 0;
    irrigationObj["remainingSec"] = remaining;
  } else {
    irrigationObj["remainingSec"] = 0;
  }
  irrigationObj["mode"] = irrigationAuto.mode;

  JsonObject valves = root.createNestedObject("valves");
  for (uint8_t zone = 1; zone <= SOIL_COUNT; zone++) {
    valves[String(zone)] = irrigation.currentZone == zone ? "on" : "off";
  }

  JsonObject ventilation = root.createNestedObject("ventilation");
  JsonObject win = ventilation.createNestedObject("windows");
  for (uint8_t i = 0; i < WINDOW_COUNT; i++) {
    JsonObject item = win.createNestedObject(String(i + 1));
    item["state"] = windowStateToString(windows[i].state);
    item["remainingSec"] = windows[i].actionUntilMs > millis() ? (windows[i].actionUntilMs - millis()) / 1000UL : 0;
  }
}

void publishAirTelemetry() {
  if (!lockState()) {
    return;
  }
  StaticJsonDocument<1536> doc;
  doc["ts"] = isoNow();
  fillAirJson(doc.as<JsonObject>());
  unlockState();
  publishJson(baseTopic() + "/telemetry/air", doc);
}

void publishSoilTelemetry() {
  if (!lockState()) {
    return;
  }
  StaticJsonDocument<1536> doc;
  doc["ts"] = isoNow();
  fillSoilJson(doc.as<JsonObject>());
  unlockState();
  publishJson(baseTopic() + "/telemetry/soil", doc);
}

void publishStateTelemetry() {
  if (!lockState()) {
    return;
  }
  StaticJsonDocument<1536> doc;
  doc["ts"] = isoNow();
  fillStateJson(doc.as<JsonObject>());
  unlockState();
  publishJson(baseTopic() + "/telemetry/state", doc, true);
}

void publishConfigTelemetry() {
  if (!lockState()) {
    return;
  }
  DynamicJsonDocument doc(4096);
  doc["ts"] = isoNow();
  doc["timezone"] = MOSCOW_TIMEZONE_NAME;

  JsonObject modes = doc.createNestedObject("modes");
  modes["irrigation"] = irrigationAuto.mode;
  modes["ventilation"] = ventAuto.mode;

  JsonObject schedules = doc.createNestedObject("schedules");
  schedules["irrigationEnabled"] = irrigationSchedule.enabled;
  schedules["irrigationItems"] = irrigationSchedule.count;
  schedules["ventilationEnabled"] = ventSchedule.enabled;
  schedules["ventilationItems"] = ventSchedule.count;

  JsonObject safety = doc.createNestedObject("safety");
  JsonObject irrigationSafetyObj = safety.createNestedObject("irrigation");
  irrigationSafetyObj["enabled"] = irrigationSafety.enabled;
  irrigationSafetyObj["blockOutsideAllowedTime"] = irrigationSafety.blockOutsideAllowedTime;
  JsonObject safetyAllowedTime = irrigationSafetyObj.createNestedObject("allowedTime");
  safetyAllowedTime["start"] = irrigationSafety.allowedStart;
  safetyAllowedTime["end"] = irrigationSafety.allowedEnd;
  irrigationSafetyObj["blockIfSoilAbovePct"] = irrigationSafety.blockIfSoilAbovePct;
  irrigationSafetyObj["blockIfAirHumidityAbovePct"] = irrigationSafety.blockIfAirHumidityAbovePct;
  irrigationSafetyObj["blockIfAirTempBelowC"] = irrigationSafety.blockIfAirTempBelowC;
  irrigationSafetyObj["allowManualForce"] = irrigationSafety.allowManualForce;
  if (lastIrrigationBlockReason.length() > 0) {
    irrigationSafetyObj["lastBlockReason"] = lastIrrigationBlockReason;
  }

  JsonObject automation = doc.createNestedObject("automation");
  automation["irrigationEnabled"] = irrigationAuto.enabled;
  automation["ventilationEnabled"] = ventAuto.enabled;

  JsonObject irrigation = automation.createNestedObject("irrigation");
  irrigation["enabled"] = irrigationAuto.enabled;
  irrigation["mode"] = irrigationAuto.mode;
  irrigation["cooldownMin"] = irrigationAuto.cooldownMin;
  irrigation["strategy"] = irrigationAuto.strategy;
  JsonObject allowedTime = irrigation.createNestedObject("allowedTime");
  allowedTime["start"] = irrigationAuto.allowedStart;
  allowedTime["end"] = irrigationAuto.allowedEnd;
  JsonObject zones = irrigation.createNestedObject("zones");
  for (uint8_t zone = 1; zone <= SOIL_COUNT; zone++) {
    char key[2] = {static_cast<char>('0' + zone), '\0'};
    JsonObject zoneCfg = zones.createNestedObject(key);
    zoneCfg["minMoisturePct"] = irrigationAuto.zones[zone - 1].minMoisturePct;
    zoneCfg["targetMoisturePct"] = irrigationAuto.zones[zone - 1].targetMoisturePct;
    zoneCfg["maxDurationSec"] = irrigationAuto.zones[zone - 1].maxDurationSec;
  }

  JsonObject ventilation = automation.createNestedObject("ventilation");
  ventilation["enabled"] = ventAuto.enabled;
  ventilation["mode"] = ventAuto.mode;
  JsonArray targetWindows = ventilation.createNestedArray("windows");
  for (uint8_t i = 0; i < ventAuto.targetWindowCount; i++) {
    targetWindows.add(ventAuto.targetWindows[i]);
  }
  JsonObject openIf = ventilation.createNestedObject("openIf");
  openIf["temperatureAboveC"] = ventAuto.openTempC;
  openIf["humidityAbovePct"] = ventAuto.openHumidityPct;
  JsonObject closeIf = ventilation.createNestedObject("closeIf");
  closeIf["temperatureBelowC"] = ventAuto.closeTempC;
  closeIf["humidityBelowPct"] = ventAuto.closeHumidityPct;
  ventilation["openDurationSec"] = ventAuto.openDurationSec;
  ventilation["closeDurationSec"] = ventAuto.closeDurationSec;
  ventilation["minOpenTimeMin"] = ventAuto.minOpenTimeMin;
  ventilation["cooldownMin"] = ventAuto.cooldownMin;
  ventilation["sensorSource"] = ventAuto.sensorSource;

  unlockState();
  publishJson(baseTopic() + "/telemetry/config", doc, true);
}

void publishSnapshot() {
  if (!lockState()) {
    return;
  }
  DynamicJsonDocument doc(4096);
  doc["ts"] = isoNow();
  JsonObject airObj = doc.createNestedObject("air");
  fillAirJson(airObj);
  JsonObject soilObj = doc.createNestedObject("soil");
  fillSoilJson(soilObj);
  JsonObject stateObj = doc.createNestedObject("state");
  fillStateJson(stateObj);
  unlockState();
  publishJson(baseTopic() + "/telemetry/snapshot", doc, true);
}

void publishAvailability(const char *status) {
  StaticJsonDocument<256> doc;
  doc["status"] = status;
  doc["ts"] = isoNow();
  doc["fwVersion"] = "0.1.0";
  doc["mqttContract"] = "greenhouse.mqtt.v1";
  publishJson(baseTopic() + "/status/availability", doc, true);
}

void publishTimeTelemetry(const char *source, const char *rtcStatus) {
  StaticJsonDocument<256> doc;
  doc["ts"] = isoNow();
  doc["timezone"] = MOSCOW_TIMEZONE_NAME;
  doc["source"] = source;
  doc["rtcStatus"] = rtcStatus;
  publishJson(baseTopic() + "/telemetry/time", doc);
}

uint8_t dayToBit(const String &day) {
  if (day == "sun") return 1 << 0;
  if (day == "mon") return 1 << 1;
  if (day == "tue") return 1 << 2;
  if (day == "wed") return 1 << 3;
  if (day == "thu") return 1 << 4;
  if (day == "fri") return 1 << 5;
  if (day == "sat") return 1 << 6;
  return 0;
}

bool parseClock(const String &value, uint8_t &hour, uint8_t &minute) {
  int parsedHour, parsedMinute;
  if (sscanf(value.c_str(), "%d:%d", &parsedHour, &parsedMinute) != 2) {
    return false;
  }
  if (parsedHour < 0 || parsedHour > 23 || parsedMinute < 0 || parsedMinute > 59) {
    return false;
  }
  hour = parsedHour;
  minute = parsedMinute;
  return true;
}

int minutesOfDay(const String &value) {
  uint8_t hour, minute;
  if (!parseClock(value, hour, minute)) {
    return -1;
  }
  return hour * 60 + minute;
}

bool isNowWithinTimeWindow(const String &startValue, const String &endValue) {
  int start = minutesOfDay(startValue);
  int end = minutesOfDay(endValue);
  if (start < 0 || end < 0) {
    return true;
  }

  time_t now = time(nullptr);
  struct tm tmNow;
  localtime_r(&now, &tmNow);
  int current = tmNow.tm_hour * 60 + tmNow.tm_min;

  if (start <= end) {
    return current >= start && current <= end;
  }
  return current >= start || current <= end;
}

void parseDayMask(JsonVariant daysValue, ScheduleItem &item) {
  item.dayMask = 0;
  if (!daysValue.is<JsonArray>()) {
    item.dayMask = 0x7f;
    return;
  }

  for (JsonVariant dayValue : daysValue.as<JsonArray>()) {
    item.dayMask |= dayToBit(String(dayValue.as<const char *>()));
  }
  if (item.dayMask == 0) {
    item.dayMask = 0x7f;
  }
}

void parseZones(JsonVariant zonesValue, ScheduleItem &item) {
  item.allZones = false;
  item.zoneCount = 0;

  if (zonesValue.is<const char *>() && String(zonesValue.as<const char *>()) == "all") {
    item.allZones = true;
    return;
  }

  if (!zonesValue.is<JsonArray>()) {
    return;
  }

  for (JsonVariant zoneValue : zonesValue.as<JsonArray>()) {
    uint8_t zone = zoneValue.as<uint8_t>();
    if (zone >= 1 && zone <= SOIL_COUNT && item.zoneCount < SOIL_COUNT) {
      item.zones[item.zoneCount++] = zone;
    }
  }
}

void parseWindows(JsonVariant windowsValue, ScheduleItem &item) {
  item.windowCount = 0;
  if (!windowsValue.is<JsonArray>()) {
    return;
  }

  for (JsonVariant windowValue : windowsValue.as<JsonArray>()) {
    uint8_t window = windowValue.as<uint8_t>();
    if (window >= 1 && window <= WINDOW_COUNT && item.windowCount < WINDOW_COUNT) {
      item.windows[item.windowCount++] = window;
    }
  }
}

bool parseSchedule(JsonObject params, ScheduleConfig &schedule, bool ventilation) {
  schedule.enabled = params["enabled"] | false;
  schedule.count = 0;

  JsonArray items = params["items"].as<JsonArray>();
  if (items.isNull()) {
    return true;
  }

  for (JsonObject input : items) {
    if (schedule.count >= MAX_SCHEDULE_ITEMS) {
      break;
    }

    ScheduleItem &item = schedule.items[schedule.count];
    item = ScheduleItem();
    item.id = input["id"] | "";
    item.enabled = input["enabled"] | true;
    parseDayMask(input["days"], item);

    String timeValue = input["time"] | "00:00";
    if (!parseClock(timeValue, item.hour, item.minute)) {
      return false;
    }

    if (ventilation) {
      parseWindows(input["windows"], item);
      item.action = input["action"] | "open";
      item.actionDurationSec = input["durationSec"] | 60;
    } else {
      parseZones(input["zones"], item);
      item.durationSec = input["durationSec"] | 60;
      item.zoneDurationSec = input["zoneDurationSec"] | item.durationSec;
      item.skipIfMoistureAbovePct = input["skipIfMoistureAbovePct"] | -1;
    }

    schedule.count++;
  }

  return true;
}

bool saveConfigJson(const char *key, JsonObject params) {
  String payload;
  serializeJson(params, payload);
  return preferences.putString(key, payload) == payload.length();
}

bool loadConfigJson(const char *key, JsonDocument &doc) {
  if (!preferences.isKey(key)) {
    return false;
  }

  String payload = preferences.getString(key, "");
  if (payload.length() == 0) {
    return false;
  }

  doc.clear();
  return deserializeJson(doc, payload) == DeserializationError::Ok;
}

bool isValidAutomationMode(const String &mode) {
  return mode == "manual" || mode == "schedule" || mode == "sensor" || mode == "hybrid";
}

bool isValidVentSensorSource(const String &source) {
  return source == "average" || source == "air1" || source == "air2" ||
         source == "maxTemperature" || source == "maxHumidity";
}

bool applyModeParams(JsonObject params) {
  String irrigationMode = params["irrigation"] | irrigationAuto.mode;
  String ventilationMode = params["ventilation"] | ventAuto.mode;

  if (!isValidAutomationMode(irrigationMode) || !isValidAutomationMode(ventilationMode)) {
    return false;
  }

  irrigationAuto.mode = irrigationMode;
  irrigationAuto.enabled = irrigationMode == "sensor" || irrigationMode == "hybrid";
  ventAuto.mode = ventilationMode;
  ventAuto.enabled = ventilationMode == "sensor" || ventilationMode == "hybrid";
  return true;
}

bool applyIrrigationAutomationParams(JsonObject params) {
  IrrigationAutoConfig next = irrigationAuto;
  next.enabled = params["enabled"] | next.enabled;
  next.mode = params["mode"] | next.mode;
  next.cooldownMin = params["cooldownMin"] | next.cooldownMin;
  next.allowedStart = params["allowedTime"]["start"] | next.allowedStart;
  next.allowedEnd = params["allowedTime"]["end"] | next.allowedEnd;
  next.strategy = params["strategy"] | next.strategy;

  uint8_t hour, minute;
  if (!isValidAutomationMode(next.mode) ||
      !parseClock(next.allowedStart, hour, minute) ||
      !parseClock(next.allowedEnd, hour, minute)) {
    return false;
  }

  JsonObject zones = params["zones"].as<JsonObject>();
  if (!zones.isNull()) {
    for (uint8_t zone = 1; zone <= SOIL_COUNT; zone++) {
      JsonObject cfg = zones[String(zone)];
      if (cfg.isNull()) {
        continue;
      }
      next.zones[zone - 1].minMoisturePct = cfg["minMoisturePct"] | next.zones[zone - 1].minMoisturePct;
      next.zones[zone - 1].targetMoisturePct = cfg["targetMoisturePct"] | next.zones[zone - 1].targetMoisturePct;
      next.zones[zone - 1].maxDurationSec = cfg["maxDurationSec"] | next.zones[zone - 1].maxDurationSec;
    }
  }

  irrigationAuto = next;
  return true;
}

bool applyIrrigationSafetyParams(JsonObject params) {
  IrrigationSafetyConfig next = irrigationSafety;
  next.enabled = params["enabled"] | next.enabled;
  next.blockOutsideAllowedTime = params["blockOutsideAllowedTime"] | next.blockOutsideAllowedTime;
  next.allowedStart = params["allowedTime"]["start"] | next.allowedStart;
  next.allowedEnd = params["allowedTime"]["end"] | next.allowedEnd;
  next.blockIfSoilAbovePct = params["blockIfSoilAbovePct"] | next.blockIfSoilAbovePct;
  next.blockIfAirHumidityAbovePct = params["blockIfAirHumidityAbovePct"] | next.blockIfAirHumidityAbovePct;
  next.blockIfAirTempBelowC = params["blockIfAirTempBelowC"] | next.blockIfAirTempBelowC;
  next.allowManualForce = params["allowManualForce"] | next.allowManualForce;

  uint8_t hour, minute;
  if (next.blockIfSoilAbovePct < 0 || next.blockIfSoilAbovePct > 100 ||
      next.blockIfAirHumidityAbovePct < 0.0f || next.blockIfAirHumidityAbovePct > 100.0f ||
      next.blockIfAirTempBelowC < -30.0f || next.blockIfAirTempBelowC > 40.0f ||
      !parseClock(next.allowedStart, hour, minute) ||
      !parseClock(next.allowedEnd, hour, minute)) {
    return false;
  }

  irrigationSafety = next;
  return true;
}

bool parseVentTargetWindows(JsonVariant windowsValue, VentAutoConfig &config) {
  if (windowsValue.isNull()) {
    return true;
  }
  if (!windowsValue.is<JsonArray>()) {
    return false;
  }

  int parsedWindows[WINDOW_COUNT] = {};
  uint8_t parsedCount = 0;
  for (JsonVariant value : windowsValue.as<JsonArray>()) {
    uint8_t window = value.as<uint8_t>();
    if (window < 1 || window > WINDOW_COUNT || parsedCount >= WINDOW_COUNT) {
      return false;
    }
    parsedWindows[parsedCount++] = window;
  }

  if (parsedCount == 0) {
    return false;
  }

  config.targetWindowCount = parsedCount;
  for (uint8_t i = 0; i < parsedCount; i++) {
    config.targetWindows[i] = parsedWindows[i];
  }
  return true;
}

bool applyVentilationAutomationParams(JsonObject params) {
  VentAutoConfig next = ventAuto;
  next.enabled = params["enabled"] | next.enabled;
  next.mode = params["mode"] | next.mode;
  next.openTempC = params["openIf"]["temperatureAboveC"] | next.openTempC;
  next.openHumidityPct = params["openIf"]["humidityAbovePct"] | next.openHumidityPct;
  next.closeTempC = params["closeIf"]["temperatureBelowC"] | next.closeTempC;
  next.closeHumidityPct = params["closeIf"]["humidityBelowPct"] | next.closeHumidityPct;
  next.openDurationSec = params["openDurationSec"] | next.openDurationSec;
  next.closeDurationSec = params["closeDurationSec"] | next.closeDurationSec;
  next.minOpenTimeMin = params["minOpenTimeMin"] | next.minOpenTimeMin;
  next.cooldownMin = params["cooldownMin"] | next.cooldownMin;
  next.sensorSource = params["sensorSource"] | next.sensorSource;

  if (!isValidAutomationMode(next.mode) ||
      !isValidVentSensorSource(next.sensorSource) ||
      !parseVentTargetWindows(params["windows"], next)) {
    return false;
  }

  ventAuto = next;
  return true;
}

void loadPersistedConfig() {
  DynamicJsonDocument doc(6144);

  if (loadConfigJson("mode", doc)) {
    applyModeParams(doc.as<JsonObject>());
  }

  if (loadConfigJson("schIrr", doc)) {
    parseSchedule(doc.as<JsonObject>(), irrigationSchedule, false);
  }

  if (loadConfigJson("schVent", doc)) {
    parseSchedule(doc.as<JsonObject>(), ventSchedule, true);
  }

  if (loadConfigJson("autoIrr", doc)) {
    applyIrrigationAutomationParams(doc.as<JsonObject>());
  }

  if (loadConfigJson("safeIrr", doc)) {
    applyIrrigationSafetyParams(doc.as<JsonObject>());
  }

  if (loadConfigJson("autoVent", doc)) {
    applyVentilationAutomationParams(doc.as<JsonObject>());
  }
}

void startIrrigationFromZones(JsonVariant zonesValue, uint16_t durationSec, uint16_t zoneDurationSec, int targetPct, bool force = false) {
  clearIrrigationQueue();
  lastIrrigationBlockReason = "";

  if (zonesValue.is<const char *>() && String(zonesValue.as<const char *>()) == "all") {
    for (uint8_t zone = 1; zone <= SOIL_COUNT; zone++) {
      enqueueIrrigation(zone, zoneDurationSec, targetPct, force);
    }
  } else if (zonesValue.is<JsonArray>()) {
    for (JsonVariant zoneValue : zonesValue.as<JsonArray>()) {
      enqueueIrrigation(zoneValue.as<uint8_t>(), durationSec, targetPct, force);
    }
  }

  startNextIrrigationZone();
}

void runIrrigationScheduleItem(ScheduleItem &item) {
  clearIrrigationQueue();
  lastIrrigationBlockReason = "";

  if (item.allZones) {
    for (uint8_t zone = 1; zone <= SOIL_COUNT; zone++) {
      if (item.skipIfMoistureAbovePct >= 0 && soil[zone - 1].moisturePct >= item.skipIfMoistureAbovePct) {
        continue;
      }
      enqueueIrrigation(zone, item.zoneDurationSec, -1);
    }
  } else {
    for (uint8_t i = 0; i < item.zoneCount; i++) {
      uint8_t zone = item.zones[i];
      if (item.skipIfMoistureAbovePct >= 0 && soil[zone - 1].moisturePct >= item.skipIfMoistureAbovePct) {
        continue;
      }
      enqueueIrrigation(zone, item.durationSec, -1);
    }
  }

  startNextIrrigationZone();
}

void runVentScheduleItem(ScheduleItem &item) {
  for (uint8_t i = 0; i < item.windowCount; i++) {
    if (item.action == "stop") {
      stopWindow(item.windows[i]);
    } else {
      startWindowAction(item.windows[i], item.action, item.actionDurationSec);
    }
  }
}

void evaluateSchedule(ScheduleConfig &schedule, bool ventilation) {
  if (!schedule.enabled) {
    return;
  }

  time_t now = time(nullptr);
  if (now < 1700000000) {
    return;
  }

  struct tm tmNow;
  localtime_r(&now, &tmNow);
  uint8_t todayBit = 1 << tmNow.tm_wday;
  int runKey = tmNow.tm_yday * 1440 + tmNow.tm_hour * 60 + tmNow.tm_min;

  for (uint8_t i = 0; i < schedule.count; i++) {
    ScheduleItem &item = schedule.items[i];
    if (!item.enabled || !(item.dayMask & todayBit)) {
      continue;
    }
    if (item.hour == tmNow.tm_hour && item.minute == tmNow.tm_min && item.lastRunKey != runKey) {
      if (ventilation) {
        runVentScheduleItem(item);
      } else {
        if (irrigation.currentZone != -1 || irrigation.queueLen > 0) {
          item.lastRunKey = runKey;
          continue;
        }
        runIrrigationScheduleItem(item);
      }
      item.lastRunKey = runKey;
    }
  }
}

bool modeAllowsSensors(const String &mode) {
  return mode == "sensor" || mode == "hybrid";
}

bool modeAllowsSchedule(const String &mode) {
  return mode == "schedule" || mode == "hybrid";
}

void evaluateIrrigationAutomation() {
  if (!irrigationAuto.enabled || !modeAllowsSensors(irrigationAuto.mode) || irrigation.currentZone != -1) {
    return;
  }
  if (!isNowWithinTimeWindow(irrigationAuto.allowedStart, irrigationAuto.allowedEnd)) {
    return;
  }

  uint32_t nowMs = millis();
  clearIrrigationQueue();
  lastIrrigationBlockReason = "";
  for (uint8_t zone = 1; zone <= SOIL_COUNT; zone++) {
    IrrigationAutoZone &cfg = irrigationAuto.zones[zone - 1];
    uint32_t cooldownMs = static_cast<uint32_t>(irrigationAuto.cooldownMin) * 60UL * 1000UL;
    if (irrigation.lastWateredMs[zone - 1] != 0 && nowMs - irrigation.lastWateredMs[zone - 1] < cooldownMs) {
      continue;
    }
    if (soil[zone - 1].ok && soil[zone - 1].moisturePct < cfg.minMoisturePct) {
      enqueueIrrigation(zone, cfg.maxDurationSec, cfg.targetMoisturePct);
    }
  }
  startNextIrrigationZone();
}

void averageAir(float &temperature, float &humidity, bool &ok) {
  float tempSum = 0.0f;
  float humSum = 0.0f;
  uint8_t count = 0;
  for (uint8_t i = 0; i < AIR_COUNT; i++) {
    if (!air[i].ok) {
      continue;
    }
    tempSum += air[i].temperatureC;
    humSum += air[i].humidityPct;
    count++;
  }
  ok = count > 0;
  temperature = ok ? tempSum / count : NAN;
  humidity = ok ? humSum / count : NAN;
}

bool airByIndex(uint8_t index, float &temperature, float &humidity) {
  if (index >= AIR_COUNT || !air[index].ok) {
    return false;
  }

  temperature = air[index].temperatureC;
  humidity = air[index].humidityPct;
  return true;
}

bool airByMaximum(bool temperatureMode, float &temperature, float &humidity) {
  bool found = false;
  uint8_t selected = 0;
  float bestValue = temperatureMode ? -1000.0f : -1.0f;

  for (uint8_t i = 0; i < AIR_COUNT; i++) {
    if (!air[i].ok) {
      continue;
    }

    float value = temperatureMode ? air[i].temperatureC : air[i].humidityPct;
    if (!found || value > bestValue) {
      selected = i;
      bestValue = value;
      found = true;
    }
  }

  return found && airByIndex(selected, temperature, humidity);
}

bool selectVentAir(float &temperature, float &humidity) {
  if (ventAuto.sensorSource == "air1") {
    return airByIndex(0, temperature, humidity);
  }
  if (ventAuto.sensorSource == "air2") {
    return airByIndex(1, temperature, humidity);
  }
  if (ventAuto.sensorSource == "maxTemperature") {
    return airByMaximum(true, temperature, humidity);
  }
  if (ventAuto.sensorSource == "maxHumidity") {
    return airByMaximum(false, temperature, humidity);
  }

  bool ok;
  averageAir(temperature, humidity, ok);
  return ok;
}

void evaluateVentAutomation() {
  if (!ventAuto.enabled || !modeAllowsSensors(ventAuto.mode)) {
    return;
  }

  uint32_t nowMs = millis();
  uint32_t cooldownMs = static_cast<uint32_t>(ventAuto.cooldownMin) * 60UL * 1000UL;
  if (ventAuto.lastActionMs != 0 && nowMs - ventAuto.lastActionMs < cooldownMs) {
    return;
  }

  float temperature, humidity;
  if (!selectVentAir(temperature, humidity)) {
    return;
  }

  bool anyOpeningOrOpen = false;
  for (uint8_t i = 0; i < ventAuto.targetWindowCount; i++) {
    uint8_t window = ventAuto.targetWindows[i];
    uint8_t index = window - 1;
    anyOpeningOrOpen = anyOpeningOrOpen || windows[index].state == WindowState::Open || windows[index].state == WindowState::Opening;
  }

  if (temperature > ventAuto.openTempC || humidity > ventAuto.openHumidityPct) {
    for (uint8_t i = 0; i < ventAuto.targetWindowCount; i++) {
      startWindowAction(ventAuto.targetWindows[i], "open", ventAuto.openDurationSec);
    }
    ventAuto.lastActionMs = nowMs;
    return;
  }

  uint32_t minOpenMs = static_cast<uint32_t>(ventAuto.minOpenTimeMin) * 60UL * 1000UL;
  bool canClose = true;
  for (uint8_t i = 0; i < ventAuto.targetWindowCount; i++) {
    uint8_t index = ventAuto.targetWindows[i] - 1;
    if (windows[index].openedAtMs != 0 && nowMs - windows[index].openedAtMs < minOpenMs) {
      canClose = false;
    }
  }

  if (anyOpeningOrOpen && canClose && temperature < ventAuto.closeTempC && humidity < ventAuto.closeHumidityPct) {
    for (uint8_t i = 0; i < ventAuto.targetWindowCount; i++) {
      startWindowAction(ventAuto.targetWindows[i], "close", ventAuto.closeDurationSec);
    }
    ventAuto.lastActionMs = nowMs;
  }
}

void handleSensorsGet(const String &requestId, const String &command) {
  readAllSensors();
  publishAirTelemetry();
  publishSoilTelemetry();
  publishStateTelemetry();
  publishSnapshot();
  publishAck(requestId, command, "done", "Sensor data published");
}

void handleVentSet(const String &requestId, const String &command, JsonObject params) {
  String action = params["action"] | "";
  if (action != "open" && action != "close" && action != "stop") {
    publishAck(requestId, command, "rejected", "Invalid ventilation action", "BAD_PARAM");
    return;
  }

  JsonArray windowsArray = params["windows"].as<JsonArray>();
  if (windowsArray.isNull()) {
    publishAck(requestId, command, "rejected", "Missing windows array", "BAD_PARAM");
    return;
  }

  uint16_t durationSec = params["durationSec"] | 60;
  for (JsonVariant value : windowsArray) {
    uint8_t window = value.as<uint8_t>();
    if (window < 1 || window > WINDOW_COUNT) {
      publishAck(requestId, command, "rejected", "Window must be 1 or 2", "BAD_PARAM");
      return;
    }
    if (action == "stop") {
      stopWindow(window);
    } else {
      startWindowAction(window, action, durationSec);
    }
  }

  publishStateTelemetry();
  publishAck(requestId, command, "accepted", "Ventilation command accepted");
}

void handleIrrigationStart(const String &requestId, const String &command, JsonObject params) {
  JsonVariant zones = params["zones"];
  if (zones.isNull()) {
    publishAck(requestId, command, "rejected", "Missing zones", "BAD_PARAM");
    return;
  }

  uint16_t durationSec = params["durationSec"] | 60;
  uint16_t zoneDurationSec = params["zoneDurationSec"] | durationSec;
  bool force = params["force"] | false;
  startIrrigationFromZones(zones, durationSec, zoneDurationSec, -1, force);

  if (irrigation.queueLen == 0 && irrigation.currentZone == -1) {
    publishAck(requestId, command, "rejected",
               lastIrrigationBlockReason.length() > 0 ? lastIrrigationBlockReason : "No valid irrigation zones",
               lastIrrigationBlockReason.length() > 0 ? "SAFETY_LOCKOUT" : "BAD_PARAM");
    return;
  }

  publishStateTelemetry();
  publishAck(requestId, command, "accepted", "Irrigation started");
}

void handleIrrigationStop(const String &requestId, const String &command, JsonObject params) {
  JsonVariant zones = params["zones"];
  if (zones.isNull()) {
    if (irrigation.currentZone != -1) {
      stopCurrentIrrigationZone();
    }
    stopAllValves();
    clearIrrigationQueue();
  } else {
    stopIrrigationZones(zones);
  }

  publishStateTelemetry();
  publishAck(requestId, command, "done", "Irrigation stopped");
}

void handleTimeSet(const String &requestId, const String &command, JsonObject params) {
  const char *datetime = params["datetime"];
  time_t parsed;
  if (!parseIsoLocal(datetime, parsed)) {
    publishAck(requestId, command, "rejected", "Invalid datetime", "BAD_PARAM");
    return;
  }

  setSystemTime(parsed);
  bool writeRtcRequested = params["writeRtc"] | false;
  bool rtcOk = !writeRtcRequested || writeRtc(parsed);
  publishTimeTelemetry("mqtt", rtcOk ? "ok" : "write_failed");
  publishAck(requestId, command, rtcOk ? "done" : "failed", rtcOk ? "Time updated" : "RTC write failed",
             rtcOk ? nullptr : "HW_ERROR");
}

void handleTimeGet(const String &requestId, const String &command) {
  time_t rtcValue;
  bool rtcOk = readRtc(rtcValue);
  publishTimeTelemetry(rtcOk ? "rtc" : "system", rtcOk ? "ok" : "unavailable");
  publishAck(requestId, command, "done", "Time published");
}

void handleTimeSync(const String &requestId, const String &command) {
  bool ok = syncTimeFromNtp(true);
  publishAck(requestId, command, ok ? "done" : "failed",
             ok ? "Time synchronized from NTP" : "NTP synchronization failed",
             ok ? nullptr : "TIMEOUT");
}

void handleModeSet(const String &requestId, const String &command, JsonObject params) {
  if (!applyModeParams(params)) {
    publishAck(requestId, command, "rejected", "Invalid automation mode", "BAD_PARAM");
    return;
  }

  bool saved = saveConfigJson("mode", params);
  publishConfigTelemetry();
  publishAck(requestId, command, saved ? "done" : "failed",
             saved ? "Automation modes updated" : "Automation modes updated, but NVS save failed",
             saved ? nullptr : "HW_ERROR");
}

void handleScheduleSet(const String &requestId, const String &command, JsonObject params, bool ventilation) {
  bool ok = parseSchedule(params, ventilation ? ventSchedule : irrigationSchedule, ventilation);
  if (!ok) {
    publishAck(requestId, command, "rejected", "Invalid schedule", "BAD_PARAM");
    return;
  }

  bool saved = saveConfigJson(ventilation ? "schVent" : "schIrr", params);
  publishConfigTelemetry();
  publishAck(requestId, command, saved ? "done" : "failed",
             saved ? "Schedule updated" : "Schedule updated, but NVS save failed",
             saved ? nullptr : "HW_ERROR");
}

void handleAutomationIrrigationSet(const String &requestId, const String &command, JsonObject params) {
  if (!applyIrrigationAutomationParams(params)) {
    publishAck(requestId, command, "rejected", "Invalid irrigation automation parameters", "BAD_PARAM");
    return;
  }

  bool saved = saveConfigJson("autoIrr", params);
  publishConfigTelemetry();
  publishAck(requestId, command, saved ? "done" : "failed",
             saved ? "Irrigation automation updated" : "Irrigation automation updated, but NVS save failed",
             saved ? nullptr : "HW_ERROR");
}

void handleAutomationSafetySet(const String &requestId, const String &command, JsonObject params) {
  if (!applyIrrigationSafetyParams(params)) {
    publishAck(requestId, command, "rejected", "Invalid irrigation safety parameters", "BAD_PARAM");
    return;
  }

  bool saved = saveConfigJson("safeIrr", params);
  publishConfigTelemetry();
  publishAck(requestId, command, saved ? "done" : "failed",
             saved ? "Irrigation safety updated" : "Irrigation safety updated, but NVS save failed",
             saved ? nullptr : "HW_ERROR");
}

void handleAutomationVentilationSet(const String &requestId, const String &command, JsonObject params) {
  if (!applyVentilationAutomationParams(params)) {
    publishAck(requestId, command, "rejected", "Invalid ventilation automation parameters", "BAD_PARAM");
    return;
  }

  bool saved = saveConfigJson("autoVent", params);
  publishConfigTelemetry();
  publishAck(requestId, command, saved ? "done" : "failed",
             saved ? "Ventilation automation updated" : "Ventilation automation updated, but NVS save failed",
             saved ? nullptr : "HW_ERROR");
}

void handleMqttCommand(const String &topic, const String &payload) {
  String prefix = baseTopic() + "/cmd/";
  if (!topic.startsWith(prefix)) {
    return;
  }

  String command = topic.substring(prefix.length());
  DynamicJsonDocument doc(6144);
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    publishAck("", command, "rejected", "Bad JSON", "BAD_JSON");
    return;
  }

  String requestId = doc["requestId"] | "";
  JsonObject params = doc["params"].as<JsonObject>();

  if (!lockState(pdMS_TO_TICKS(3000))) {
    publishAck(requestId, command, "failed", "State lock timeout", "BUSY");
    return;
  }

  String repeatReason;
  if (rejectRepeatedCommand(requestId, command, params, repeatReason)) {
    publishAck(requestId, command, "rejected", repeatReason, "BUSY");
    unlockState();
    return;
  }

  if (command == "sensors/get") {
    handleSensorsGet(requestId, command);
  } else if (command == "vent/set") {
    handleVentSet(requestId, command, params);
  } else if (command == "irrigation/start") {
    handleIrrigationStart(requestId, command, params);
  } else if (command == "irrigation/stop") {
    handleIrrigationStop(requestId, command, params);
  } else if (command == "time/set") {
    handleTimeSet(requestId, command, params);
  } else if (command == "time/get") {
    handleTimeGet(requestId, command);
  } else if (command == "time/sync") {
    handleTimeSync(requestId, command);
  } else if (command == "automation/mode/set") {
    handleModeSet(requestId, command, params);
  } else if (command == "automation/mode/get") {
    publishConfigTelemetry();
    publishAck(requestId, command, "done", "Configuration published");
  } else if (command == "schedule/irrigation/set") {
    handleScheduleSet(requestId, command, params, false);
  } else if (command == "schedule/irrigation/get") {
    publishConfigTelemetry();
    publishAck(requestId, command, "done", "Irrigation schedule published");
  } else if (command == "schedule/ventilation/set") {
    handleScheduleSet(requestId, command, params, true);
  } else if (command == "schedule/ventilation/get") {
    publishConfigTelemetry();
    publishAck(requestId, command, "done", "Ventilation schedule published");
  } else if (command == "automation/irrigation/set") {
    handleAutomationIrrigationSet(requestId, command, params);
  } else if (command == "automation/irrigation/get") {
    publishConfigTelemetry();
    publishAck(requestId, command, "done", "Irrigation automation published");
  } else if (command == "automation/safety/set") {
    handleAutomationSafetySet(requestId, command, params);
  } else if (command == "automation/safety/get") {
    publishConfigTelemetry();
    publishAck(requestId, command, "done", "Irrigation safety published");
  } else if (command == "automation/ventilation/set") {
    handleAutomationVentilationSet(requestId, command, params);
  } else if (command == "automation/ventilation/get") {
    publishConfigTelemetry();
    publishAck(requestId, command, "done", "Ventilation automation published");
  } else {
    publishAck(requestId, command, "rejected", "Unknown command", "BAD_PARAM");
  }

  unlockState();
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  String message;
  message.reserve(length);
  for (unsigned int i = 0; i < length; i++) {
    message += static_cast<char>(payload[i]);
  }
  handleMqttCommand(String(topic), message);
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.printf("Connecting WiFi SSID: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nWiFi connected: %s\n", WiFi.localIP().toString().c_str());
}

void connectMqtt() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (!lockMqtt(pdMS_TO_TICKS(100))) {
    return;
  }

  if (mqttClient.connected()) {
    unlockMqtt();
    return;
  }

  uint32_t nowMs = millis();
  if (lastMqttConnectAttemptMs != 0 && nowMs - lastMqttConnectAttemptMs < 5000UL) {
    unlockMqtt();
    return;
  }
  lastMqttConnectAttemptMs = nowMs;

  String clientId = MQTT_CLIENT_ID;
  if (clientId.length() == 0) {
    clientId = deviceId + "-" + String(random(0xffff), HEX);
  }

  Serial.printf("Connecting MQTT: %s:%u\n", mqttHost.c_str(), mqttPort);
  vTaskDelay(pdMS_TO_TICKS(1));

  bool connected;
  if (strlen(MQTT_USER) > 0) {
    connected = mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD);
  } else {
    connected = mqttClient.connect(clientId.c_str());
  }

  if (!connected) {
    Serial.printf("MQTT connection failed, state=%d\n", mqttClient.state());
    unlockMqtt();
    return;
  }

  bool subscribed = mqttClient.subscribe((baseTopic() + "/cmd/#").c_str(), 1);
  unlockMqtt();
  lastMqttConnectAttemptMs = 0;

  Serial.printf("MQTT connected, subscribed=%s\n", subscribed ? "yes" : "no");
  publishAvailability("online");
  publishConfigTelemetry();
  publishSnapshot();
}

void setupPins() {
  for (uint8_t i = 0; i < SOIL_COUNT; i++) {
    pinMode(VALVE_RELAY_PINS[i], OUTPUT);
    writeRelay(VALVE_RELAY_PINS[i], false);
  }

  for (uint8_t i = 0; i < WINDOW_COUNT; i++) {
    pinMode(WINDOW_RELAY_PINS[i][0], OUTPUT);
    pinMode(WINDOW_RELAY_PINS[i][1], OUTPUT);
    writeRelay(WINDOW_RELAY_PINS[i][0], false);
    writeRelay(WINDOW_RELAY_PINS[i][1], false);
  }

  analogReadResolution(12);
  for (uint8_t i = 0; i < SOIL_COUNT; i++) {
    analogSetPinAttenuation(SOIL_PINS[i], ADC_11db);
  }
}

void setupI2CDevices() {
  for (uint8_t i = 0; i < AIR_COUNT; i++) {
    sht30Ready[i] = sht30Sensors[i].begin(SHT30_ADDR[i]);
    Serial.printf("SHT30 #%u at 0x%02X: %s\n", i + 1, SHT30_ADDR[i],
                  sht30Ready[i] ? "ready" : "not found");
  }

  if (rtcDs3231.begin(&Wire)) {
    rtcDevice = RtcDevice::DS3231;
    Serial.println("RTC DS3231-compatible device ready");
  } else if (rtcDs1307.begin(&Wire)) {
    rtcDevice = RtcDevice::DS1307;
    Serial.println("RTC DS1307 ready");
  } else {
    rtcDevice = RtcDevice::None;
    Serial.println("RTC not found");
  }
}

void setupTime() {
  setenv("TZ", MOSCOW_TZ, 1);
  tzset();

  time_t rtcValue;
  if (readRtc(rtcValue)) {
    setSystemTime(rtcValue);
    Serial.println("Time source: RTC");
  } else {
    Serial.println("RTC unavailable, time will be set by MQTT/NTP later");
  }
}

bool syncTimeFromNtp(bool force) {
  uint32_t nowMs = millis();
  uint32_t intervalMs = static_cast<uint32_t>(NTP_SYNC_INTERVAL_MIN) * 60UL * 1000UL;

  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  if (!force && ntpEverSynced && nowMs - lastNtpAttemptMs < intervalMs) {
    return true;
  }

  if (!force && !ntpEverSynced && lastNtpAttemptMs != 0 && nowMs - lastNtpAttemptMs < 60000UL) {
    return false;
  }

  lastNtpAttemptMs = nowMs;

  if (!ntpConfigured) {
    configTzTime(MOSCOW_TZ, NTP_SERVER_1, NTP_SERVER_2);
    ntpConfigured = true;
  }

  struct tm tmNow;
  bool ok = false;
  for (uint8_t attempt = 0; attempt < 20; attempt++) {
    if (getLocalTime(&tmNow, 250)) {
      ok = true;
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }

  if (!ok || !isSystemTimeValid()) {
    Serial.println("NTP sync failed");
    return false;
  }

  ntpEverSynced = true;
  time_t syncedTime = time(nullptr);
  bool rtcOk = writeRtc(syncedTime);
  Serial.printf("NTP time synced, RTC write: %s\n", rtcOk ? "ok" : "failed");
  publishTimeTelemetry("ntp", rtcOk ? "ok" : "write_failed");
  return true;
}

void mqttTask(void *pvParameters) {
  (void)pvParameters;

  for (;;) {
    connectWiFi();
    connectMqtt();

    if (lockMqtt(pdMS_TO_TICKS(1000))) {
      mqttClient.loop();
      unlockMqtt();
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void timeTask(void *pvParameters) {
  (void)pvParameters;

  for (;;) {
    if (WiFi.status() == WL_CONNECTED && lockState(pdMS_TO_TICKS(8000))) {
      syncTimeFromNtp(false);
      unlockState();
    }

    vTaskDelay(pdMS_TO_TICKS(60000));
  }
}

void sensorTask(void *pvParameters) {
  (void)pvParameters;

  for (;;) {
    if (lockState(pdMS_TO_TICKS(3000))) {
      readAllSensors();
      lastSensorReadMs = millis();
      unlockState();
    }

    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

void controlTask(void *pvParameters) {
  (void)pvParameters;

  for (;;) {
    uint32_t nowMs = millis();

    if (lockState(pdMS_TO_TICKS(3000))) {
      updateWindows();
      updateIrrigation();

      if (nowMs - lastAutomationMs >= 10000) {
        evaluateIrrigationAutomation();
        evaluateVentAutomation();
        lastAutomationMs = nowMs;
      }

      if (nowMs - lastScheduleCheckMs >= 1000) {
        if (modeAllowsSchedule(irrigationAuto.mode)) {
          evaluateSchedule(irrigationSchedule, false);
        }
        if (modeAllowsSchedule(ventAuto.mode)) {
          evaluateSchedule(ventSchedule, true);
        }
        lastScheduleCheckMs = nowMs;
      }

      unlockState();
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void telemetryTask(void *pvParameters) {
  (void)pvParameters;

  vTaskDelay(pdMS_TO_TICKS(3000));

  for (;;) {
    publishAirTelemetry();
    publishSoilTelemetry();
    publishStateTelemetry();
    publishSnapshot();
    lastTelemetryMs = millis();

    vTaskDelay(pdMS_TO_TICKS(30000));
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  stateMutex = xSemaphoreCreateRecursiveMutex();
  mqttMutex = xSemaphoreCreateRecursiveMutex();
  if (stateMutex == nullptr || mqttMutex == nullptr) {
    Serial.println("Failed to create FreeRTOS mutexes");
    abort();
  }

  parseMqttEndpoint();
  setupPins();
  Wire.begin(21, 22);
  setupI2CDevices();
  preferences.begin("greenhouse", false);
  setupTime();
  loadPersistedConfig();
  readAllSensors();

  mqttClient.setServer(mqttHost.c_str(), mqttPort);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(4096);
  mqttClient.setSocketTimeout(2);
  mqttClient.setKeepAlive(30);

  xTaskCreatePinnedToCore(mqttTask, "mqttTask", 16384, nullptr, 1, nullptr, 1);
  xTaskCreatePinnedToCore(timeTask, "timeTask", 6144, nullptr, 2, nullptr, 1);
  xTaskCreatePinnedToCore(sensorTask, "sensorTask", 4096, nullptr, 2, nullptr, 1);
  xTaskCreatePinnedToCore(controlTask, "controlTask", 8192, nullptr, 2, nullptr, 1);
  xTaskCreatePinnedToCore(telemetryTask, "telemetryTask", 12288, nullptr, 1, nullptr, 1);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}

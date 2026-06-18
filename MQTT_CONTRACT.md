# MQTT-контракт умной теплицы

Базовый топик:

```text
greenhouse/v1/{deviceId}
```

Для текущей теплицы можно использовать:

```text
greenhouse/v1/esp32-greenhouse-01
```

Все payload отправляются в JSON. Топики и поля JSON оставлены на английском, чтобы прошивка и клиентское приложение были проще и стабильнее.

## Общие правила

Команды от приложения к ESP32:

```text
greenhouse/v1/{deviceId}/cmd/{command}
```

Ответы ESP32 на команды:

```text
greenhouse/v1/{deviceId}/ack
```

Телеметрия ESP32:

```text
greenhouse/v1/{deviceId}/telemetry/{type}
```

Состояние доступности:

```text
greenhouse/v1/{deviceId}/status/availability
```

Рекомендуемые MQTT-настройки:

| Тип сообщения | QoS | Retain |
|---|---:|---:|
| `cmd/*` | 1 | false |
| `ack` | 1 | false |
| `telemetry/snapshot` | 1 | true |
| `telemetry/state` | 1 | true |
| `telemetry/air`, `telemetry/soil` | 0 или 1 | false |
| `status/availability` | 1 | true |

## Формат команды

```json
{
  "requestId": "a2b4b7d8-0001",
  "source": "mobile-app",
  "ts": "2026-06-17T14:00:00+03:00",
  "params": {}
}
```

`requestId` обязателен. ESP32 возвращает его в `ack`, чтобы приложение связало ответ с командой.

## Формат ack

Топик:

```text
greenhouse/v1/{deviceId}/ack
```

```json
{
  "requestId": "a2b4b7d8-0001",
  "command": "vent/set",
  "status": "accepted",
  "ts": "2026-06-17T14:00:01+03:00",
  "message": "Command accepted"
}
```

`status`:

```text
accepted | rejected | done | failed
```

Типовые `errorCode` для `rejected` и `failed`:

```text
BAD_JSON
BAD_PARAM
BUSY
SAFETY_LOCKOUT
SENSOR_UNAVAILABLE
HW_ERROR
TIMEOUT
```

Пример ошибки:

```json
{
  "requestId": "a2b4b7d8-0002",
  "command": "irrigation/start",
  "status": "rejected",
  "errorCode": "BAD_PARAM",
  "message": "Zone must be between 1 and 6"
}
```

## Получить данные датчиков

Топик:

```text
greenhouse/v1/{deviceId}/cmd/sensors/get
```

Получить всё:

```json
{
  "requestId": "req-001",
  "source": "mobile-app",
  "ts": "2026-06-17T14:00:00+03:00",
  "params": {
    "fields": ["all"]
  }
}
```

Получить только температуру и влажность воздуха:

```json
{
  "requestId": "req-002",
  "source": "mobile-app",
  "ts": "2026-06-17T14:00:00+03:00",
  "params": {
    "fields": ["air"]
  }
}
```

Получить влажность почвы:

```json
{
  "requestId": "req-003",
  "source": "mobile-app",
  "ts": "2026-06-17T14:00:00+03:00",
  "params": {
    "fields": ["soil"]
  }
}
```

Допустимые `fields`:

```text
all | air | soil | state
```

После команды ESP32 публикует актуальные данные в:

```text
greenhouse/v1/{deviceId}/telemetry/snapshot
```

## Открыть проветривание

Одна общая команда управляет первым окном, вторым окном или обоими.

Топик:

```text
greenhouse/v1/{deviceId}/cmd/vent/set
```

Открыть первое окно:

```json
{
  "requestId": "vent-001",
  "source": "mobile-app",
  "ts": "2026-06-17T14:00:00+03:00",
  "params": {
    "windows": [1],
    "action": "open",
    "durationSec": 60
  }
}
```

Открыть второе окно:

```json
{
  "requestId": "vent-002",
  "source": "mobile-app",
  "ts": "2026-06-17T14:00:00+03:00",
  "params": {
    "windows": [2],
    "action": "open",
    "durationSec": 60
  }
}
```

Открыть оба окна:

```json
{
  "requestId": "vent-003",
  "source": "mobile-app",
  "ts": "2026-06-17T14:00:00+03:00",
  "params": {
    "windows": [1, 2],
    "action": "open",
    "durationSec": 60
  }
}
```

Закрыть или остановить:

```json
{
  "requestId": "vent-004",
  "source": "mobile-app",
  "ts": "2026-06-17T14:00:00+03:00",
  "params": {
    "windows": [1, 2],
    "action": "stop"
  }
}
```

`action`:

```text
open | close | stop
```

Правила безопасности для прошивки:

```text
Window 1: K7/K8, GPIO25/GPIO26
Window 2: K9/K10, GPIO27/GPIO14
```

ESP32 не должна одновременно включать оба направления одного привода. Перед сменой направления нужна пауза 300-500 мс.

## Запустить полив

Топик:

```text
greenhouse/v1/{deviceId}/cmd/irrigation/start
```

Полив всей теплицы:

```json
{
  "requestId": "water-001",
  "source": "mobile-app",
  "ts": "2026-06-17T14:00:00+03:00",
  "params": {
    "zones": "all",
    "strategy": "sequential",
    "zoneDurationSec": 60,
    "force": false
  }
}
```

Полив одной секции:

```json
{
  "requestId": "water-002",
  "source": "mobile-app",
  "ts": "2026-06-17T14:00:00+03:00",
  "params": {
    "zones": [3],
    "durationSec": 60,
    "force": false
  }
}
```

Полив нескольких секций:

```json
{
  "requestId": "water-003",
  "source": "mobile-app",
  "ts": "2026-06-17T14:00:00+03:00",
  "params": {
    "zones": [1, 4, 6],
    "strategy": "sequential",
    "zoneDurationSec": 45
  }
}
```

По умолчанию `zones: "all"` выполняется последовательно, а не одновременным включением всех клапанов.

Перед открытием клапана ESP32 всегда проверяет safety-ограничения полива. Если зона слишком влажная, воздух слишком влажный, слишком холодно или текущее московское время вне разрешённого окна, команда отклоняется с `errorCode: "SAFETY_LOCKOUT"`. Поле `force: true` работает только если в safety-настройках включено `allowManualForce: true`; по умолчанию ручной обход запрещён.

Соответствие зон и GPIO:

```text
Zone 1 -> K1 -> GPIO19
Zone 2 -> K2 -> GPIO18
Zone 3 -> K3 -> GPIO5
Zone 4 -> K4 -> GPIO17
Zone 5 -> K5 -> GPIO16
Zone 6 -> K6 -> GPIO4
```

## Остановить полив

Топик:

```text
greenhouse/v1/{deviceId}/cmd/irrigation/stop
```

Остановить весь полив:

```json
{
  "requestId": "water-stop-001",
  "source": "mobile-app",
  "ts": "2026-06-17T14:00:00+03:00",
  "params": {
    "zones": "all"
  }
}
```

Остановить конкретную секцию:

```json
{
  "requestId": "water-stop-002",
  "source": "mobile-app",
  "ts": "2026-06-17T14:00:00+03:00",
  "params": {
    "zones": [3]
  }
}
```

## Время контроллера / RTC

ESP32 использует время для расписаний. При старте контроллер читает DS3231/DS1307, а после подключения Wi-Fi синхронизирует системное время через NTP и записывает его обратно в RTC.

NTP - Network Time Protocol. Это протокол синхронизации времени по сети: ESP32 подключается к NTP-серверу, получает точное текущее время, переводит его в московское локальное время и обновляет свои системные часы.

Все времена в MQTT-командах трактуются как московское время. Клиентское приложение не передаёт timezone. Если пользователь находится в другом часовом поясе, он всё равно указывает время теплицы по Москве: например `10:00` означает `10:00` в Москве.

Поле `timezone` встречается только в исходящей телеметрии как справочная информация о фиксированной зоне контроллера.

Установить время:

```text
greenhouse/v1/{deviceId}/cmd/time/set
```

```json
{
  "requestId": "time-001",
  "source": "mobile-app",
  "ts": "2026-06-17T14:00:00+03:00",
  "params": {
    "datetime": "2026-06-17T14:00:00+03:00",
    "writeRtc": true
  }
}
```

Получить время:

```text
greenhouse/v1/{deviceId}/cmd/time/get
```

```json
{
  "requestId": "time-002",
  "source": "mobile-app",
  "ts": "2026-06-17T14:00:00+03:00",
  "params": {}
}
```

Принудительно синхронизировать время через Wi-Fi/NTP:

```text
greenhouse/v1/{deviceId}/cmd/time/sync
```

```json
{
  "requestId": "time-003",
  "source": "mobile-app",
  "ts": "2026-06-17T14:00:00+03:00",
  "params": {}
}
```

Если синхронизация успешна, ESP32 устанавливает системное время, записывает его в RTC и публикует `telemetry/time` с `source: "ntp"`.

Ответ публикуется в `ack` и затем в:

```text
greenhouse/v1/{deviceId}/telemetry/time
```

```json
{
  "ts": "2026-06-17T14:00:00+03:00",
  "timezone": "Europe/Moscow",
  "source": "rtc",
  "rtcStatus": "ok"
}
```

## Режимы автоматики

У каждого подсистемного блока есть отдельный режим:

```text
manual   - только ручные команды;
schedule - работа по расписанию;
sensor   - работа по состоянию датчиков;
hybrid   - расписание + датчики.
```

Установить режимы:

```text
greenhouse/v1/{deviceId}/cmd/automation/mode/set
```

```json
{
  "requestId": "mode-001",
  "source": "mobile-app",
  "ts": "2026-06-17T14:00:00+03:00",
  "params": {
    "irrigation": "hybrid",
    "ventilation": "sensor"
  }
}
```

Получить режимы:

```text
greenhouse/v1/{deviceId}/cmd/automation/mode/get
```

ESP32 публикует актуальные режимы в:

```text
greenhouse/v1/{deviceId}/telemetry/config
```

## Расписание полива

Установить расписание:

```text
greenhouse/v1/{deviceId}/cmd/schedule/irrigation/set
```

Ежедневный полив всей теплицы по зонам:

```json
{
  "requestId": "schedule-water-001",
  "source": "mobile-app",
  "ts": "2026-06-17T14:00:00+03:00",
  "params": {
    "enabled": true,
    "items": [
      {
        "id": "morning-all",
        "enabled": true,
        "days": ["mon", "tue", "wed", "thu", "fri", "sat", "sun"],
        "time": "07:30",
        "zones": "all",
        "strategy": "sequential",
        "zoneDurationSec": 60,
        "skipIfMoistureAbovePct": 70
      }
    ]
  }
}
```

Полив отдельных секций по расписанию:

```json
{
  "requestId": "schedule-water-002",
  "source": "mobile-app",
  "ts": "2026-06-17T14:00:00+03:00",
  "params": {
    "enabled": true,
    "items": [
      {
        "id": "zone-3-evening",
        "enabled": true,
        "days": ["mon", "wed", "fri"],
        "time": "19:00",
        "zones": [3],
        "durationSec": 90
      }
    ]
  }
}
```

Получить расписание полива:

```text
greenhouse/v1/{deviceId}/cmd/schedule/irrigation/get
```

## Расписание проветривания

Установить расписание:

```text
greenhouse/v1/{deviceId}/cmd/schedule/ventilation/set
```

Открыть оба окна утром и закрыть вечером:

```json
{
  "requestId": "schedule-vent-001",
  "source": "mobile-app",
  "ts": "2026-06-17T14:00:00+03:00",
  "params": {
    "enabled": true,
    "items": [
      {
        "id": "open-morning",
        "enabled": true,
        "days": ["mon", "tue", "wed", "thu", "fri", "sat", "sun"],
        "time": "09:00",
        "windows": [1, 2],
        "action": "open",
        "durationSec": 60
      },
      {
        "id": "close-evening",
        "enabled": true,
        "days": ["mon", "tue", "wed", "thu", "fri", "sat", "sun"],
        "time": "21:00",
        "windows": [1, 2],
        "action": "close",
        "durationSec": 60
      }
    ]
  }
}
```

Получить расписание проветривания:

```text
greenhouse/v1/{deviceId}/cmd/schedule/ventilation/get
```

## Автополив по датчикам

Установить пороги:

```text
greenhouse/v1/{deviceId}/cmd/automation/irrigation/set
```

```json
{
  "requestId": "auto-water-001",
  "source": "mobile-app",
  "ts": "2026-06-17T14:00:00+03:00",
  "params": {
    "enabled": true,
    "mode": "sensor",
    "zones": {
      "1": { "minMoisturePct": 50, "targetMoisturePct": 62, "maxDurationSec": 60 },
      "2": { "minMoisturePct": 50, "targetMoisturePct": 62, "maxDurationSec": 60 },
      "3": { "minMoisturePct": 50, "targetMoisturePct": 62, "maxDurationSec": 60 },
      "4": { "minMoisturePct": 50, "targetMoisturePct": 62, "maxDurationSec": 60 },
      "5": { "minMoisturePct": 50, "targetMoisturePct": 62, "maxDurationSec": 60 },
      "6": { "minMoisturePct": 50, "targetMoisturePct": 62, "maxDurationSec": 60 }
    },
    "cooldownMin": 30,
    "allowedTime": {
      "start": "06:00",
      "end": "21:00"
    },
    "strategy": "sequential"
  }
}
```

Логика прошивки:

```text
1. Если зона ниже minMoisturePct, она попадает в очередь полива.
2. Полив идёт по одной зоне за раз.
3. Полив зоны останавливается по maxDurationSec или при достижении targetMoisturePct.
4. После полива зоны действует cooldownMin.
5. Если режим hybrid, расписание может запускать полив, но skipIfMoistureAbovePct не даёт переливать влажную зону.
6. Safety-защита проверяется перед стартом зоны и во время полива.
```

Получить настройки автополива:

```text
greenhouse/v1/{deviceId}/cmd/automation/irrigation/get
```

## Safety-защита полива

Safety-защита применяется ко всем источникам полива: ручная команда, расписание и автоматика по датчикам.

Установить защитные пороги:

```text
greenhouse/v1/{deviceId}/cmd/automation/safety/set
```

```json
{
  "requestId": "safety-water-001",
  "source": "mobile-app",
  "ts": "2026-06-17T14:00:00+03:00",
  "params": {
    "enabled": true,
    "blockOutsideAllowedTime": true,
    "allowedTime": {
      "start": "06:00",
      "end": "21:00"
    },
    "blockIfSoilAbovePct": 78,
    "blockIfAirHumidityAbovePct": 88,
    "blockIfAirTempBelowC": 14,
    "allowManualForce": false
  }
}
```

Логика прошивки:

```text
1. Ночью полив запрещён, если blockOutsideAllowedTime = true.
2. Если влажность почвы зоны >= blockIfSoilAbovePct, клапан этой зоны не открывается.
3. Если средняя влажность воздуха >= blockIfAirHumidityAbovePct, любой полив блокируется.
4. Если средняя температура воздуха <= blockIfAirTempBelowC, любой полив блокируется.
5. Ручной force может обойти safety только если allowManualForce = true.
```

Получить safety-настройки:

```text
greenhouse/v1/{deviceId}/cmd/automation/safety/get
```

## Автопроветривание по датчикам

Установить пороги:

```text
greenhouse/v1/{deviceId}/cmd/automation/ventilation/set
```

```json
{
  "requestId": "auto-vent-001",
  "source": "mobile-app",
  "ts": "2026-06-17T14:00:00+03:00",
  "params": {
    "enabled": true,
    "mode": "sensor",
    "windows": [1, 2],
    "openIf": {
      "temperatureAboveC": 28.0,
      "humidityAbovePct": 78
    },
    "closeIf": {
      "temperatureBelowC": 24.0,
      "humidityBelowPct": 68
    },
    "openDurationSec": 60,
    "closeDurationSec": 60,
    "minOpenTimeMin": 10,
    "cooldownMin": 5,
    "sensorSource": "average"
  }
}
```

`sensorSource`:

```text
average | air1 | air2 | maxTemperature | maxHumidity
```

Логика прошивки:

```text
1. Если температура или влажность выше порога openIf, окна открываются.
2. Если температура и влажность ниже порогов closeIf, окна закрываются.
3. Между открытием и закрытием действует minOpenTimeMin.
4. Перед сменой направления привода обязательна пауза 300-500 мс.
5. Если режим hybrid, расписание может открывать/закрывать окна, а датчики могут корректировать состояние при перегреве или высокой влажности.
```

Получить настройки автопроветривания:

```text
greenhouse/v1/{deviceId}/cmd/automation/ventilation/get
```

## Публикация конфигурации

После любой команды `*/set` ESP32 публикует подтверждённую конфигурацию:

```text
greenhouse/v1/{deviceId}/telemetry/config
```

```json
{
  "ts": "2026-06-17T14:00:00+03:00",
  "timezone": "Europe/Moscow",
  "modes": {
    "irrigation": "hybrid",
    "ventilation": "sensor"
  },
  "schedules": {
    "irrigationEnabled": true,
    "ventilationEnabled": true
  },
  "safety": {
    "irrigation": {
      "enabled": true,
      "blockOutsideAllowedTime": true,
      "allowedTime": { "start": "06:00", "end": "21:00" },
      "blockIfSoilAbovePct": 78,
      "blockIfAirHumidityAbovePct": 88,
      "blockIfAirTempBelowC": 14,
      "allowManualForce": false
    }
  },
  "automation": {
    "irrigationEnabled": true,
    "ventilationEnabled": true,
    "irrigation": {
      "enabled": true,
      "mode": "hybrid",
      "cooldownMin": 30,
      "strategy": "sequential",
      "allowedTime": { "start": "06:00", "end": "21:00" },
      "zones": {
        "1": { "minMoisturePct": 50, "targetMoisturePct": 62, "maxDurationSec": 60 }
      }
    },
    "ventilation": {
      "enabled": true,
      "mode": "sensor",
      "windows": [1, 2],
      "openIf": { "temperatureAboveC": 28.0, "humidityAbovePct": 78 },
      "closeIf": { "temperatureBelowC": 24.0, "humidityBelowPct": 68 },
      "openDurationSec": 60,
      "closeDurationSec": 60,
      "minOpenTimeMin": 10,
      "cooldownMin": 5,
      "sensorSource": "average"
    }
  }
}
```

## Телеметрия воздуха

Топик:

```text
greenhouse/v1/{deviceId}/telemetry/air
```

```json
{
  "ts": "2026-06-17T14:00:00+03:00",
  "sensors": [
    {
      "id": "air1",
      "type": "SHT30",
      "i2cAddress": "0x44",
      "temperatureC": 24.3,
      "humidityPct": 58.1,
      "status": "ok"
    },
    {
      "id": "air2",
      "type": "SHT30",
      "i2cAddress": "0x45",
      "temperatureC": 25.0,
      "humidityPct": 60.4,
      "status": "ok"
    }
  ],
  "average": {
    "temperatureC": 24.7,
    "humidityPct": 59.3
  }
}
```

## Телеметрия влажности почвы

Топик:

```text
greenhouse/v1/{deviceId}/telemetry/soil
```

```json
{
  "ts": "2026-06-17T14:00:00+03:00",
  "zones": {
    "1": { "raw": 2180, "moisturePct": 47, "status": "ok" },
    "2": { "raw": 2350, "moisturePct": 41, "status": "ok" },
    "3": { "raw": 1900, "moisturePct": 58, "status": "ok" },
    "4": { "raw": 2600, "moisturePct": 33, "status": "ok" },
    "5": { "raw": 2100, "moisturePct": 50, "status": "ok" },
    "6": { "raw": 2250, "moisturePct": 45, "status": "ok" }
  }
}
```

`raw` - значение ADC ESP32 после усреднения. `moisturePct` - значение после калибровки сухо/влажно.

## Состояние исполнительных устройств

Топик:

```text
greenhouse/v1/{deviceId}/telemetry/state
```

```json
{
  "ts": "2026-06-17T14:00:00+03:00",
  "irrigation": {
    "active": true,
    "activeZones": [3],
    "mode": "manual",
    "remainingSec": 42
  },
  "valves": {
    "1": "off",
    "2": "off",
    "3": "on",
    "4": "off",
    "5": "off",
    "6": "off"
  },
  "ventilation": {
    "windows": {
      "1": { "state": "opening", "remainingSec": 8 },
      "2": { "state": "stopped", "remainingSec": 0 }
    }
  }
}
```

Состояния окна:

```text
open | closed | opening | closing | stopped | unknown
```

## Полный снимок

Топик:

```text
greenhouse/v1/{deviceId}/telemetry/snapshot
```

`snapshot` объединяет последние версии `air`, `soil` и `state`. ESP32 публикует его:

```text
1. при старте;
2. после команды sensors/get;
3. после изменения реле;
4. периодически, например раз в 30 секунд.
```

## Availability

Топик:

```text
greenhouse/v1/{deviceId}/status/availability
```

При подключении ESP32 публикует retained-сообщение:

```json
{
  "status": "online",
  "ts": "2026-06-17T14:00:00+03:00",
  "fwVersion": "0.1.0",
  "mqttContract": "greenhouse.mqtt.v1"
}
```

Last Will ESP32:

```json
{
  "status": "offline"
}
```

## Хранение настроек на ESP32

`.env` не загружается на плату как файл. PlatformIO читает `.env` при сборке, и значения Wi-Fi/MQTT попадают в прошивку как compile-time параметры. Чтобы изменить MQTT-брокер или пароль Wi-Fi, нужно изменить `.env`, пересобрать и заново загрузить прошивку.

Расписания и настройки автоматики сохраняются на самой ESP32 в NVS через `Preferences`. Они переживают перезагрузку платы:

```text
mode     - режимы полива и проветривания
schIrr   - расписание полива
schVent  - расписание проветривания
autoIrr  - настройки автополива по датчикам
autoVent - настройки автопроветривания по датчикам
```

Время хранится в RTC DS3231/DS1307. При старте ESP32 читает RTC и устанавливает системное время. После подключения Wi-Fi ESP32 синхронизируется через NTP и обновляет RTC. Если Wi-Fi/интернет недоступен, расписания продолжают работать по RTC. Если RTC недоступен, время нужно задать через MQTT или дождаться успешной NTP-синхронизации.


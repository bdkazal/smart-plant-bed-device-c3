# Smart Plant Bed Device C3

ESP32-C3 Super Mini firmware for the **Biztola Smart Plant Bed** controller.

This repo is the C3 replacement/port of the original ESP32 DevKit Plant Bed firmware. The product logic stays the same, but the implementation is adapted for the ESP32-C3 Super Mini: single-core CPU, lower power/heat target, fewer usable GPIO pins, native USB serial, and careful cooperative loop timing.

Current stable firmware:

```text
smart-plant-bed-c3-m12-0.2
```

## Current status

Working now:

- ESP32-C3 Super Mini PlatformIO firmware
- Native USB serial monitor support
- Wi-Fi station mode with reduced TX power
- Laravel API authentication with `X-DEVICE-KEY`
- Heartbeat
- Config fetch
- Command polling
- Device state sync
- Dashboard `valve_on` / `valve_off`
- Duration-based auto-stop
- Physical manual watering button
- Wi-Fi status LED
- Valve/watering output on GPIO5
- Soil moisture sensor on GPIO1
- Soil sensor disconnected/N/A detection
- DHT11 temperature/humidity on GPIO2
- Sensor readings upload to Laravel
- Cached Laravel config in ESP32 NVS/Preferences
- Offline auto-watering fallback using cached config
- Time sync from NTP and Laravel UTC
- DS3231 RTC UTC backup
- RTC update skipped when drift is within 5 seconds
- Offline schedule fallback using cached schedules and valid local time
- Multiple offline schedules in the same test session

Not enabled yet:

- OLED display
- OLED/display button
- Wi-Fi setup portal
- Wi-Fi reset provisioning flow
- Production enclosure/power finalization

## Important design rule

Laravel is the primary controller when reachable.

Local firmware automation is **fallback-only**:

```text
Laravel reachable:
  Laravel controls commands, config, state, and automation.

Laravel not reachable:
  firmware may use cached config + local sensors + RTC/NTP/Laravel time for safe fallback behavior.
```

Local auto-watering and local schedule watering are both fallback-only. They do not run while Laravel is recently reachable.

## Hardware target

```text
Board: ESP32-C3 Super Mini
PlatformIO board: esp32-c3-devkitm-1
Framework: Arduino
```

## Pin map

| Function | GPIO | Notes |
| --- | ---: | --- |
| Valve / MOSFET input | GPIO5 | Active HIGH |
| Watering indicator LED | GPIO5 | Mirrors valve signal through resistor |
| Wi-Fi status LED | GPIO6 | Active HIGH |
| Manual watering button | GPIO3 | `INPUT_PULLUP`, press connects to GND |
| Soil moisture ADC | GPIO1 | Capacitive sensor v1.2, 100k pulldown recommended |
| DHT11 data | GPIO2 | Temperature/humidity reporting only |
| DS3231 SDA | GPIO8 | I2C |
| DS3231 SCL | GPIO9 | I2C |

See [`Docs/HARDWARE_PIN_MAP.md`](Docs/HARDWARE_PIN_MAP.md) for wiring details.

## Laravel API endpoints used

```http
POST /api/device/heartbeat
GET  /api/device/config?device_uuid=...
POST /api/device/readings
GET  /api/device/commands?device_uuid=...
POST /api/device/commands/{id}/ack
POST /api/device/state
```

See [`Docs/API_AND_RUNTIME.md`](Docs/API_AND_RUNTIME.md) for request behavior and intervals.

## Time and RTC model

The firmware keeps system time as UTC epoch internally and uses Laravel timezone config for local time display/schedule comparison.

Priority:

```text
1. NTP
2. Laravel server_time_utc
3. DS3231 RTC UTC backup
4. no valid time
```

DS3231 stores **UTC**, not Bangladesh local time.

See [`Docs/OFFLINE_TIME_AND_RTC.md`](Docs/OFFLINE_TIME_AND_RTC.md).

## Local secrets setup

Copy the example file:

```bash
cp include/DeviceSecrets.example.h include/DeviceSecrets.h
```

Edit `include/DeviceSecrets.h`:

```cpp
#define WIFI_SSID "Your WiFi Name"
#define WIFI_PASSWORD "Your WiFi Password"

#define API_BASE_URL "http://192.168.0.xxx:8000"
#define DEVICE_UUID "Your Laravel device UUID"
#define DEVICE_API_KEY "Your Laravel device API key"
```

Use your Mac LAN IP for `API_BASE_URL`, not `localhost`.

## Build, upload, monitor

```bash
git pull
pio run
pio run -t upload
pio device monitor -b 115200
```

Expected boot highlights:

```text
Firmware version: smart-plant-bed-c3-m12-0.2
Time sync initialized.
Initializing DS3231 RTC...
Device storage initialized.
Cached config loaded from flash.
Valve OFF - safe boot default
Wi-Fi connected.
Heartbeat sent successfully.
Config fetched successfully.
Device state synced successfully.
Sensor reading uploaded successfully.
```

## Current test checklist

See [`Docs/TESTING_CHECKLIST.md`](Docs/TESTING_CHECKLIST.md).

Minimum checks before continuing:

- RTC restores time on reboot
- RTC write is skipped when drift is within 5 seconds
- Laravel config cache is not rewritten when unchanged
- Sensor disconnected shows N/A/null, not false 0
- Offline auto fallback works only when Laravel is not reachable
- Offline schedule fallback works only when Laravel is not reachable
- Multiple offline schedules can run correctly
- Manual button can start and stop local watering
- Dashboard commands still work after all local modules are enabled

## Next milestone

```text
M13 — production hardening and remaining device features
```

Good candidates:

```text
OLED display/status UI
Wi-Fi setup portal
local offline event sync to Laravel after reconnect
final power/enclosure validation
```

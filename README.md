# Smart Plant Bed Device C3

ESP32-C3 Super Mini firmware for the **Biztola Smart Plant Bed** controller.

This repo is the C3 replacement/port of the original ESP32 DevKit Plant Bed firmware. The product logic follows the old Plant Bed behavior, but the implementation is adapted for the ESP32-C3 Super Mini: single-core CPU, lower power/heat target, fewer usable GPIO pins, native USB serial, RTC backup, OLED status UI, Wi-Fi setup portal, and careful cooperative loop timing.

Current stable firmware:

```text
smart-plant-bed-c3-m14-0.1-wifi-setup
```

## Current status

Working now:

- ESP32-C3 Super Mini PlatformIO firmware
- Native USB serial monitor support
- Wi-Fi station mode with reduced TX power
- Stored Wi-Fi credentials in ESP32 Preferences/NVS
- Wi-Fi setup hotspot portal
- GPIO7 boot-time Wi-Fi reset button
- Wi-Fi reset clears Wi-Fi credentials only, not cached Laravel config
- Setup hotspot skips `DeviceSecrets` after a Wi-Fi reset request
- Setup portal page loads immediately and scans Wi-Fi after page load
- Setup portal supports scanned Wi-Fi, manual SSID/hidden network, password show/hide, wrong-password retry, save, and restart
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
- Time sync from NTP, Laravel UTC, and DS3231 RTC UTC backup
- RTC update skipped when drift is within 5 seconds
- Offline schedule fallback using cached schedules and valid local time
- Multiple offline schedules in the same test session
- OLED boot logo
- OLED boot/status/home/schedule/watering/done/critical dry pages
- OLED Wi-Fi reset/setup status pages
- OLED schedule page shows weekday + HH:MM for the next schedule
- GPIO4 OLED wake/next button
- Modular old Plant Bed-style source structure

Not enabled yet:

- Production enclosure/power finalization
- Offline action history sync back to Laravel after reconnect

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

Manual button control remains available locally even when Laravel is not reachable.

## Architecture

The firmware is split into old Plant Bed-style modules:

```text
main.cpp
  setup/loop orchestration only

DeviceApi
  Laravel/device API workflow
  heartbeat, config fetch, readings upload, state sync, command poll, command ACK

ApiClient
  low-level API base URL and device headers

AppConfig
  config state, config parsing, cache loading, config summary

WiFiMan
  stored Wi-Fi connection, development fallback, Laravel reachability, online/offline timing intervals

SetupPortal
  PlantBed-Setup hotspot, setup web page, Wi-Fi scan, credential test, save and restart

WifiReset
  GPIO7 boot-time Wi-Fi credential reset and setup-portal request

CommandHandler
  dashboard command execution: valve_on / valve_off

ValveController
  valve GPIO, watering runtime, auto-stop, command completion

SensorReader
  soil moisture and DHT11 readings

TimeSync / RtcClock
  NTP, Laravel UTC, DS3231 UTC backup, local time conversion

ScheduleConfig / LocalAutomation
  cached schedule parsing and fallback-only local automation

DisplayManager
  OLED boot logo, reset/setup/status pages, schedule page, watering page, critical dry page, GPIO4 wake/next button

DeviceStorage
  stored Wi-Fi credentials + cached Laravel config in Preferences/NVS
```

See [`Docs/ARCHITECTURE.md`](Docs/ARCHITECTURE.md) for more detail.

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
| OLED wake/next button | GPIO4 | `INPUT_PULLUP`, press connects to GND |
| Wi-Fi reset/setup button | GPIO7 | `INPUT_PULLUP`, hold during boot for 3 seconds |
| Soil moisture ADC | GPIO1 | Capacitive sensor v1.2, 100k pulldown recommended |
| DHT11 data | GPIO2 | Temperature/humidity reporting only |
| DS3231 SDA | GPIO8 | I2C shared with OLED |
| DS3231 SCL | GPIO9 | I2C shared with OLED |
| OLED SDA | GPIO8 | I2C shared with DS3231 |
| OLED SCL | GPIO9 | I2C shared with DS3231 |

See [`Docs/HARDWARE_PIN_MAP.md`](Docs/HARDWARE_PIN_MAP.md) for wiring details.

## Wi-Fi setup and reset

Normal development boot:

```text
stored Wi-Fi exists:
  connect using stored Wi-Fi

stored Wi-Fi missing:
  fall back to DeviceSecrets Wi-Fi for development
```

Wi-Fi reset boot:

```text
1. Power off device
2. Hold GPIO7 button to GND
3. Power on while holding GPIO7
4. Keep holding for 3 seconds
5. Device clears stored Wi-Fi only
6. Cached Laravel config remains
7. Device restarts into PlantBed-Setup hotspot
```

Setup hotspot:

```text
SSID: PlantBed-Setup
Password: plantbed123
URL: http://192.168.4.1
```

The setup page loads immediately, then scans Wi-Fi after page load. If the password is wrong, the same page stays open and shows an error. If the password is correct, credentials are saved and the device restarts.

See [`Docs/WIFI_SETUP_AND_RESET.md`](Docs/WIFI_SETUP_AND_RESET.md).

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

## OLED behavior

Startup sequence:

```text
1. Biztola boot logo
2. Booting information / Wi-Fi reset/setup page if needed
3. Home page for 10 seconds
4. If soil is critically dry, critical dry page stays awake
```

Wi-Fi reset/setup OLED pages:

```text
Wi-Fi Reset
Keep holding
3 seconds

Wi-Fi Setup
PlantBed-Setup
192.168.4.1
```

Normal behavior:

```text
GPIO4 button:
  cycles home page / schedule page
  wakes OLED for 15 seconds

Schedule page:
  shows schedule state
  shows next schedule as weekday + HH:MM
  shows current HH:MM

Watering active:
  watering page stays awake while watering

Watering done:
  done page shows, then sleeps

Critical dry:
  critical dry page stays awake
  GPIO4 can temporarily override it to view normal pages
```

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

`DeviceSecrets` Wi-Fi is a development fallback only. After GPIO7 Wi-Fi reset, the firmware skips `DeviceSecrets` and goes directly to setup hotspot mode.

## Build, upload, monitor

```bash
git pull
pio run
pio run -t upload
pio device monitor -b 115200
```

Expected boot highlights:

```text
Firmware version: smart-plant-bed-c3-m14-0.1-wifi-setup
Time sync initialized.
Initializing DS3231 RTC...
Device storage initialized.
OLED display manager initialized.
Checking Wi-Fi reset button...
Cached config loaded from flash.
Valve OFF - safe boot default
Stored Wi-Fi credentials found.
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
- OLED boot logo appears
- OLED home page sleeps after 10 seconds
- GPIO4 cycles OLED home/schedule pages
- OLED schedule page shows weekday + HH:MM for next schedule
- Watering page appears while valve is active
- Critical dry page appears and can be temporarily overridden by GPIO4
- GPIO7 boot hold clears Wi-Fi credentials only
- GPIO7 reset starts setup hotspot without trying `DeviceSecrets`
- PlantBed-Setup hotspot starts
- Setup portal loads immediately
- Setup portal scans Wi-Fi after page load
- Wrong password stays on same setup page
- Correct password saves Wi-Fi and restarts
- Stored Wi-Fi is used on the next boot

## Next milestone

```text
M15 — production hardening and stability testing
```

Good candidates:

```text
final power/enclosure validation
long-duration stability test
local offline event sync to Laravel after reconnect
customer-facing setup polish
```
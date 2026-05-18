# Smart Plant Bed C3 Firmware Architecture

This document describes the current ESP32-C3 firmware structure after the M14 Wi-Fi setup/provisioning milestone.

The goal is to keep the C3 firmware close to the original Plant Bed firmware style:

```text
main.cpp should orchestrate.
Modules should own behavior.
Network/API/config/display/automation logic should not live directly in main.cpp.
```

## Current firmware

```text
smart-plant-bed-c3-m14-0.1-wifi-setup
```

## Core rule

Laravel is the primary controller when reachable.

Local firmware logic is fallback-only unless it is a direct physical/local action, such as the manual watering button or Wi-Fi setup/reset.

```text
Laravel reachable:
  Laravel owns config, commands, readings, state, and automation decisions.

Laravel not recently reachable:
  firmware may use cached config, local sensors, RTC/NTP time, and local safety rules.
```

Wi-Fi setup/reset is local-only and does not clear Laravel config.

## Module map

### main.cpp

Owns the high-level firmware lifecycle only:

```text
setup()
loop()
runStartupApiTasks()
handleSensorReadingCycle()
updateLocalControls()
logWifiStatusIfNeeded()
initializeOfflineLocalRuntime()
```

It should not contain large JSON parsing, API request construction, command business logic, or display page drawing.

Important boot order:

```text
beginTimeSync()
beginDeviceStorage()
beginDisplayManager()
checkWifiResetOnBoot()
consumeWifiSetupPortalRequest()
loadCachedConfigOnBoot()
initialize local modules
connect Wi-Fi or start setup portal
```

OLED is initialized before Wi-Fi reset check so reset/setup messages can be shown on the display.

### DeviceApi.h / DeviceApi.cpp

Owns Laravel/device API workflow:

```text
beginDeviceApi()
printBootInfo()
sendHeartbeat()
fetchConfig()
sendSensorReading()
syncDeviceState()
syncDeviceStateIfServerReachable()
pollCommands()
ackCommand()
```

It uses `ApiClient` for URL/header helper logic.

### ApiClient.h / ApiClient.cpp

Small low-level helper for:

```text
API base URL
X-DEVICE-KEY header
shared device HTTP headers
```

It should not own product behavior.

### AppConfig.h / AppConfig.cpp

Owns Laravel config state and parsing:

```text
configDeviceName
configTimezone
configWateringMode
configTimezoneOffsetMinutes
configMaxWateringDurationSeconds
configCooldownMinutes
configLocalManualDurationSeconds
configScheduleCount
hasSoilMoistureThreshold
configSoilMoistureThreshold
hasLoadedCachedConfig
```

Also owns:

```text
applyConfigObject()
parseConfigResponse()
parseCachedConfigObjectJson()
loadCachedConfigOnBoot()
extractConfigJsonForCache()
printConfigSummary()
getLocalManualDurationSeconds()
```

### WiFiMan.h / WiFiMan.cpp

Owns normal Wi-Fi connection and Laravel reachability state:

```text
connectWifi()
connectWifiUsingConfig()
connectWithCredentials()
updateWiFiReconnect()
isWifiConnected()
isServerRecentlyReachable()
markServerResult()
markServerUnavailable()
heartbeatIntervalForCurrentReachability()
commandPollIntervalForCurrentReachability()
configFetchIntervalForCurrentReachability()
```

Behavior:

```text
stored Wi-Fi exists:
  connect with stored Wi-Fi

stored Wi-Fi missing:
  use DeviceSecrets Wi-Fi as development fallback

Wi-Fi reset setup request exists:
  main.cpp skips connectWifi() and starts setup portal directly
```

Important behavior:

```text
HTTP success marks Laravel reachable.
HTTP connection failure marks Laravel not confirmed.
Offline intervals are slower to avoid blocking local controls.
Non-blocking reconnect keeps local controls responsive.
```

### SetupPortal.h / SetupPortal.cpp

Owns Wi-Fi setup hotspot and web page:

```text
startSetupPortal()
handleSetupPortal()
isSetupPortalActive()
```

Setup hotspot:

```text
SSID: PlantBed-Setup
Password: plantbed123
URL: http://192.168.4.1
```

Routes:

```http
GET  /
GET  /networks
POST /save
```

Behavior:

```text
page loads immediately
Wi-Fi scan starts after page load
manual SSID supports hidden/unstable networks
password show/hide button is available
wrong password keeps same page open
correct password saves Wi-Fi and restarts
```

### WifiReset.h / WifiReset.cpp

Owns boot-time Wi-Fi reset button behavior:

```text
checkWifiResetOnBoot()
```

GPIO:

```text
GPIO7 ---- button ---- GND
INPUT_PULLUP
hold during boot for 3 seconds
```

Reset behavior:

```text
clear wifi_ssid
clear wifi_pass
keep cached Laravel config
set one-shot wifi_setup flag
restart into setup portal
```

### CommandHandler.h / CommandHandler.cpp

Owns Laravel command execution behavior:

```text
handleCommand()
commandDurationSeconds()
```

Currently supports:

```text
valve_on
valve_off
```

Unsupported command types are ACKed as failed.

### ValveController.h / ValveController.cpp

Owns valve runtime state:

```text
safe boot valve OFF
start/stop watering
manual watering
command watering
local fallback watering
watering duration auto-stop
command completion ACK coordination
watering OLED page trigger
```

GPIO5 is the valve output and also physically mirrors the watering LED.

### SensorReader.h / SensorReader.cpp

Owns sensors:

```text
soil moisture ADC on GPIO1
DHT11 temperature/humidity on GPIO2
soil sensor disconnected/N/A detection
soil raw-to-percent conversion
```

Important rule:

```text
Sensor reads update cached display data and may trigger critical dry display.
Sensor reads must not repeatedly wake the normal OLED home page.
```

### DisplayManager.h / DisplayManager.cpp

Owns OLED UI:

```text
boot logo
boot status
Wi-Fi reset/setup pages
home/status page
schedule page
watering page
done page
critical dry page
GPIO4 wake/next button
sleep timers
```

Current behavior:

```text
Boot logo:       2.5 sec
Boot status:     during startup
Wi-Fi reset:     visible while holding GPIO7 during boot
Wi-Fi setup:     shows PlantBed-Setup and 192.168.4.1
Home page:       10 sec
GPIO4 pages:     15 sec
Schedule page:   next schedule shows weekday + HH:MM
Watering page:   stay awake while watering
Done page:       10 sec
Critical dry:    stay awake
```

GPIO4 can temporarily override the critical dry page so the user can view normal pages.

`beginDisplayManager()` has a duplicate-initialization guard to prevent OLED flicker if it is called twice.

### TimeSync.h / TimeSync.cpp

Owns system time and local time conversion:

```text
NTP sync
Laravel UTC sync
Laravel local timestamp sync fallback
POSIX timezone config
current local time string
ISO day-of-week
valid time checks
```

Priority:

```text
1. NTP
2. Laravel server_time_utc
3. DS3231 RTC UTC backup
4. no valid time
```

### RtcClock.h / RtcClock.cpp

Owns DS3231 RTC:

```text
initialize DS3231
load UTC backup time
compare drift
write UTC when drift exceeds threshold
```

Current rule:

```text
RTC write is skipped if DS3231 UTC is already within 5 seconds of system UTC.
```

### ScheduleConfig.h / ScheduleConfig.cpp

Owns cached watering schedule parsing and access:

```text
parseScheduleConfigs()
getScheduleConfigCount()
getScheduleConfigAt()
```

Schedules are used by local fallback only when Laravel is not recently reachable.

### LocalAutomation.h / LocalAutomation.cpp

Owns fallback-only local automation:

```text
local auto watering fallback
local schedule watering fallback
cooldown protection
server reachability guard
valid time guard
```

Important rule:

```text
Local automation does not run while Laravel is recently reachable.
```

### DeviceStorage.h / DeviceStorage.cpp

Owns stored Wi-Fi credentials and cached Laravel config in ESP32 Preferences/NVS:

```text
beginDeviceStorage()
loadStoredDeviceConfig()
saveWifiCredentials()
clearStoredWifiCredentials()
requestWifiSetupPortalOnNextBoot()
consumeWifiSetupPortalRequest()
loadCachedConfigJson()
saveCachedConfigJsonIfChanged()
```

Storage keys:

```text
wifi_ssid
wifi_pass
wifi_setup
cfg_json
```

Important behavior:

```text
Wi-Fi reset clears wifi_ssid and wifi_pass only.
cfg_json cached Laravel config is not cleared.
Flash write is skipped when cached config is unchanged.
Preference keys are checked before remove() to avoid harmless NOT_FOUND logs.
```

### StatusLed.h / StatusLed.cpp

Owns Wi-Fi status LED behavior on GPIO6.

### ManualButton.h / ManualButton.cpp

Owns physical manual button behavior on GPIO3.

```text
press while idle: start local watering
press while watering: stop watering
```

## Loop responsiveness

The loop is cooperative. Long blocking work should be avoided.

Important patterns:

```text
updateLocalControls() runs before and after network tasks.
HTTP timeouts are short.
Offline API retry intervals are slower.
Manual button and display button must keep working when Laravel is offline.
Setup portal mode handles web clients and keeps local controls alive.
```

## HTTP timing

Current HTTP limits:

```text
connect timeout: 1000 ms
response timeout: 1500 ms
```

When Laravel resets or refuses a connection, the firmware marks Laravel as not confirmed and continues local operation.

## Tested behavior after M14

Confirmed on ESP32-C3 hardware:

```text
PlatformIO build passes
RTC restore works
NTP sync works
Laravel heartbeat works
Laravel config fetch/cache works
Device state sync works
Sensor upload works
Command polling works
Dashboard valve_on works
Dashboard valve_off works
Manual button works
OLED GPIO4 button works
OLED reset/setup pages work
Laravel connection reset does not freeze local controls
Manual watering works while Laravel is not confirmed
GPIO7 Wi-Fi reset clears Wi-Fi only
DeviceSecrets is skipped after reset request
PlantBed-Setup hotspot starts
Setup page loads immediately
Wi-Fi scan fills dropdown
Wrong password stays on setup page
Correct password saves Wi-Fi and restarts
Stored Wi-Fi is used after restart
OLED duplicate initialization guard prevents flicker
```

## Future development notes

Planned or possible future modules/features:

```text
OfflineEventLog
OfflineEventSync
PowerManager
ProductionDiagnostics
CustomerSetupPolish
```

Offline action history sync is intentionally not implemented yet. It requires persistent event storage, replay validation, duplicate prevention, Laravel-side UI handling, and careful consistency rules.

# Testing Checklist — Smart Plant Bed C3

Use this checklist after pulling a new firmware milestone.

Current stable target:

```text
smart-plant-bed-c3-m11-0.4
```

## Build and upload

```bash
git pull
pio run
pio run -t upload
pio device monitor -b 115200
```

Expected build result:

```text
[SUCCESS]
```

## Boot checklist

Expected boot logs:

```text
Time sync initialized.
Initializing DS3231 RTC...
Device storage initialized.
Valve OFF - safe boot default
Status LEDs initialized.
Manual watering button initialized.
Sensor reader initialized.
Local automation initialized.
Firmware version: smart-plant-bed-c3-m11-0.4
Wi-Fi connected.
```

Pass conditions:

- No boot loop.
- Valve is OFF before Wi-Fi work.
- DS3231 initializes or fails safely.
- Cached config loads if available.

## Time and RTC tests

### Test 1: RTC restore after reboot

Reset ESP32-C3 after DS3231 has already been synced once.

Expected:

```text
DS3231 RTC ready UTC: ...
System time loaded from DS3231 UTC: ...
Time source: RTC backup.
RTC restored local time: ...
```

### Test 2: NTP/Laravel time sync

Expected when internet/NTP works:

```text
NTP time synced: ...
Time=NTP ...
```

Expected when NTP fails but Laravel works:

```text
System time synced from LARAVEL_UTC: ...
Time=LARAVEL_UTC ...
```

### Test 3: RTC drift skip

Expected when RTC is already close to system UTC:

```text
DS3231 UTC drift seconds: 0
RTC update skipped: DS3231 already within 5 seconds of system UTC.
```

## Laravel API tests

Laravel command:

```bash
php artisan serve --host=0.0.0.0 --port=8000
```

Expected firmware logs:

```text
Heartbeat sent successfully.
Config fetched successfully.
Device state synced successfully.
No pending command.
Sensor reading uploaded successfully.
```

## Config cache tests

### Test 1: unchanged config skip

After repeated boot/fetch with unchanged config:

```text
Config cache unchanged. Flash write skipped.
```

### Test 2: cached config load

Expected after boot:

```text
Cached config loaded from flash.
Config summary:
  watering_mode: ...
  soil_moisture_threshold: ...
  max_watering_duration_seconds: ...
```

## Soil sensor tests

### Connected sensor

Expected:

```text
Soil moisture raw: ...
Soil moisture %: 0-100
POST /api/device/readings
soil_moisture: 0-100
```

### Disconnected sensor

Expected:

```text
Soil moisture: unavailable / sensor disconnected
soil_moisture:null
```

Important:

```text
0 = valid very dry reading
null = sensor unavailable
```

## DHT11 tests

Expected when working:

```text
Temperature C: ...
Humidity %: ...
```

Expected if disconnected or unstable:

```text
Temperature C: unavailable
Humidity %: unavailable
```

Firmware should still upload soil data even if DHT11 fails.

## Dashboard command tests

### valve_on

Expected:

```text
Command found: #... type=valve_on
Valve ON - dashboard command
Command #... marked as acknowledged
Watering will auto-stop after seconds: ...
```

After duration:

```text
Watering duration completed.
Valve OFF - duration completed
Command #... marked as executed
```

### valve_off

Expected:

```text
Command found: #... type=valve_off
Valve OFF - dashboard stop command
Valve OFF command executed.
```

## Manual button tests

Button wiring:

```text
GPIO3 ---- button ---- GND
```

Press while idle:

```text
Manual watering button pressed.
Starting local watering.
Valve ON - manual button
```

Press while watering:

```text
Manual watering button pressed.
Stopping watering from physical button.
Valve OFF - manual button stop
```

## Offline fallback auto test

Set Laravel config:

```text
watering_mode = auto
soil_moisture_threshold = 35
max_watering_duration_seconds = 30
cooldown_minutes = 10
```

Then stop Laravel or make Laravel unreachable.

Expected:

```text
Laravel not reachable. Sensor reading kept local for fallback automation.
Local fallback auto watering triggered.
Valve ON - local auto fallback
Watering duration completed.
Valve OFF - duration completed
```

Pass condition:

- Fallback auto runs only when Laravel is not recently reachable.
- It does not run while Laravel is reachable.

## Local schedule fallback status

Current expected message:

```text
Local schedule watering is disabled until schedule fallback read-only testing passes.
```

Schedule fallback should not turn valve ON yet.

## Known monitor behavior

PlatformIO Serial Monitor can appear stuck if terminal text is selected. This is copy/selection mode, not firmware freeze.

Click away or clear the selection to resume auto-scrolling.

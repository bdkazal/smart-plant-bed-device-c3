# ESP32-C3 Super Mini Migration Plan

This repository is not a new product. It is the ESP32-C3 Super Mini hardware migration of the existing Biztola Smart Plant Bed / Plant Buddy firmware.

## Source-of-truth order

Use these references in this order:

1. `biztola-iot-platform` branch `feature/iot-platform-foundation`
   - source of truth for Laravel API, command lifecycle, device type rules, platform behavior, and customer-facing meaning.
2. `smart-plant-bed-device`
   - source of truth for Smart Plant Bed firmware behavior.
   - Plant Bed is a timed-action watering device.
3. `smart-fountain-device`
   - reference for ESP32-C3 stability style only.
   - Do not copy Smart Fountain product behavior into Plant Bed.
4. `smart-plant-bed-device-c3`
   - clean C3-compatible implementation of the same Smart Plant Bed behavior.

## Product identity

The C3 device must remain a Smart Plant Bed timed-action device.

It must keep:

- `valve_on` / `valve_off` command behavior
- timed watering runtime
- Laravel watering log meaning
- Plant Bed config shape
- Plant Bed sensor/readings direction
- offline/local-first direction

It must not become a Smart Fountain-style persistent-state device.

## Command lifecycle rule

For Plant Bed / timed-action devices:

```text
executed = the watering action completed
```

For `valve_on`, this means:

```text
valve opened
watering ran for duration
valve closed
the run is complete
```

Correct `valve_on` flow:

1. Receive command.
2. Validate `duration_seconds`.
3. If already watering, mark new command failed.
4. ACK command as `acknowledged`.
5. Set GPIO valve output ON.
6. Store active command ID, start time, and duration.
7. Keep loop responsive while watering runs.
8. When duration completes, set valve output OFF.
9. Mark original command as `executed`.
10. Sync final state when state sync is implemented.

Correct `valve_off` flow:

1. Receive command.
2. ACK stop command as `acknowledged`.
3. Set GPIO valve output OFF immediately.
4. If a `valve_on` command was active, close it as `executed`.
5. Mark `valve_off` as `executed`.
6. Clear watering runtime.

## ESP32-C3 constraints

ESP32 DevKit V1 and ESP32-C3 Super Mini are not equivalent boards.

Important C3 constraints:

- single-core chip
- fewer safe GPIO choices
- native USB serial sensitivity
- GPIO9 can be boot-related on many C3 boards
- more sensitive to blocking loop work
- heat/power behavior matters more than speed
- Wi-Fi, HTTP, display, sensors, and local controls must not fight each other

C3 firmware priorities:

1. safe valve OFF default
2. stable Wi-Fi
3. short network blocking time
4. responsive local controls
5. correct command lifecycle
6. gradual sensor/display/RTC additions
7. offline fallback only after config/time/cache are stable

## C3 pin map baseline

Use the C3 pin profile from the old Plant Bed repo as the baseline.

| Function | GPIO | Notes |
| --- | ---: | --- |
| Valve / LR7843 input | GPIO5 | Active HIGH |
| Watering LED | GPIO5 | Physically mirrors valve through resistor |
| Wi-Fi status LED | GPIO6 | Future milestone |
| Wi-Fi reset button | GPIO7 | External button to GND; do not use GPIO9/BOOT |
| Soil moisture ADC | GPIO1 | Needs C3 recalibration |
| DHT11 data | GPIO2 | Display/reporting only later |
| Manual watering button | GPIO3 | `INPUT_PULLUP`, to GND |
| OLED wake/status button | GPIO4 | `INPUT_PULLUP`, to GND |
| I2C SDA | GPIO8 | OLED + RTC later |
| I2C SCL | GPIO9 | OLED + RTC later; test boot carefully |

Do not casually change this map unless testing shows a real conflict.

## C3 Wi-Fi rule

Use the proven Plant Bed / Smart Fountain C3-friendly radio setup:

```cpp
WiFi.mode(WIFI_STA);
WiFi.setTxPower(WIFI_POWER_8_5dBm);
WiFi.begin(...);
```

Do not force:

```cpp
WiFi.setSleep(false);
```

unless there is a measured stability problem that cannot be solved another way.

## Runtime timing direction

Match the old Plant Bed direction:

- heartbeat every 15 seconds while server is reachable
- command polling every 5 seconds while server is reachable
- config refresh every 60 seconds while server is reachable
- slower retry intervals when Laravel is not reachable
- local controls and watering updates must run often

Old Plant Bed uses very short HTTP timeouts:

```text
HTTP connect timeout: 1000 ms
HTTP response timeout: 1500 ms
server reachable window: 15000 ms
server very recent window: 5000 ms
```

The C3 repo should move toward this model before adding manual buttons, display, sensors, or offline fallback.

## Milestone strategy

### Completed / current foundation

- C3-only PlatformIO project
- Wi-Fi station mode
- Laravel heartbeat
- config fetch
- command polling
- GPIO5 valve output
- basic timed watering runtime
- Smart Fountain-style C3 Wi-Fi heat fix

### Next recommended cleanup before hardware expansion

1. Align network timeout/reachability handling with old Plant Bed.
2. Add `/api/device/state` sync for Plant Bed fields:
   - `device_type`
   - `firmware_version`
   - `operation_state`
   - `valve_state`
   - `watering_state`
   - `last_completed_command_id`
3. Ensure state sync happens:
   - after valve ON starts
   - after valve OFF completes
   - after duration auto-stop completes
4. Only then test real valve hardware.

### Later milestones

Add features in this order:

1. Manual watering button on GPIO3.
2. Wi-Fi status LED on GPIO6.
3. Soil moisture ADC on GPIO1 with C3 calibration and disconnected detection.
4. Reading upload to Laravel.
5. Local auto fallback only after sensor and cached config are stable.
6. Time sync and RTC/OLED I2C hardware.
7. Offline schedule fallback using cached config + valid time.
8. Setup portal / provisioning flow last, after station mode and local controls are proven.

## Do-not-do list

Do not:

- copy Smart Fountain persistent-state command behavior into Plant Bed
- mark `valve_on` executed immediately after turning GPIO on
- add OLED, RTC, DHT11, soil sensor, setup portal, and buttons in one step
- use long blocking HTTP calls once local controls exist
- hardcode Bangladesh-only time behavior
- store local wall-clock time in RTC
- use GPIO9 as Wi-Fi reset button
- power valve/solenoid from ESP32-C3
- connect 5V to ESP32-C3 GPIO/I2C lines

## Current engineering rule

Before each C3 code change, answer:

```text
Is this preserving old Smart Plant Bed behavior?
Is this compatible with ESP32-C3 constraints?
Does this keep local safety responsive?
Does this match the Laravel foundation branch contract?
```

If any answer is unclear, stop and review before coding.

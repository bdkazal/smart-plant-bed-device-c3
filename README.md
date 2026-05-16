# Smart Plant Bed Device C3

Clean ESP32-C3-only firmware for the Smart Plant Bed device.

This repository intentionally starts small. Milestone 1 proved stable ESP32-C3 Wi-Fi station mode. Milestone 2 proved Laravel heartbeat. Milestone 3 proved config fetch and command polling. Milestone 4 now adds safe GPIO5 valve control.

## Current scope: Milestone 4.3

Included now:

- ESP32-C3 Super Mini / `esp32-c3-devkitm-1` PlatformIO environment
- Arduino framework
- Native USB serial flags for ESP32-C3
- Fixed local Wi-Fi credentials through `include/DeviceSecrets.h`
- Smart Fountain-style C3 Wi-Fi:
  - `WiFi.mode(WIFI_STA)`
  - `WiFi.setTxPower(WIFI_POWER_8_5dBm)`
  - no forced `WiFi.setSleep(false)`
- 15 second Wi-Fi connection timeout
- 10 second Wi-Fi retry interval
- 20 ms cooperative loop delay
- Minimal `ApiClient`
- `POST /api/device/heartbeat` every 15 seconds
- `GET /api/device/config?device_uuid=...` every 60 seconds
- `GET /api/device/commands?device_uuid=...` every 5 seconds
- `POST /api/device/commands/{id}/ack` helper
- GPIO5 valve output, active HIGH
- safe valve OFF on boot
- `valve_on` command support
- `valve_off` command support
- duration-based auto-stop for `valve_on`
- watering LED mirrors valve if physically connected to GPIO5 through resistor

Not included yet:

- sensors
- OLED
- RTC
- setup portal
- AP+STA scanning
- automatic local watering
- schedule fallback execution

## Milestone 4 valve behavior

`valve_on` behavior:

1. Firmware receives command.
2. Firmware validates duration.
3. Firmware ACKs command as `acknowledged`.
4. GPIO5 goes HIGH.
5. Device keeps watering active until duration completes.
6. GPIO5 goes LOW automatically.
7. Firmware marks the original `valve_on` command as `executed`.

`valve_off` behavior:

1. Firmware receives stop command.
2. Firmware ACKs stop command as `acknowledged`.
3. GPIO5 goes LOW immediately.
4. Any active `valve_on` command is closed as `executed`.
5. Stop command is marked as `executed`.

This matches the original Plant Bed runtime idea: a watering command is not marked executed until watering actually ends.

## Milestone 4 safety rule

First Milestone 4 upload/test should be done with the MOSFET/valve disconnected. Confirm serial logs and command ACK flow first. After that passes, connect GPIO5 to the LR7843 input and test with the valve power side safely wired.

GPIO5 behavior:

| State | GPIO5 |
| --- | --- |
| Valve OFF | LOW |
| Valve ON | HIGH |

## Hardware target

ESP32-C3 Super Mini only.

Planned pin map:

| Function | GPIO | Notes |
| --- | ---: | --- |
| Valve / LR7843 MOSFET input | GPIO5 | Active HIGH |
| Watering LED | GPIO5 | Same valve signal through 330 ohm resistor |
| Wi-Fi status LED | GPIO6 | Future milestone |
| Soil moisture ADC | GPIO1 | Future milestone, with 100k pulldown to GND |
| DHT11 data | GPIO2 | Future milestone |
| Manual watering button | GPIO3 | Future milestone, to GND, `INPUT_PULLUP` |
| OLED wake/status button | GPIO4 | Future milestone, to GND, `INPUT_PULLUP` |
| Wi-Fi reset button | GPIO7 | Future milestone, to GND, `INPUT_PULLUP` |
| I2C SDA | GPIO8 | OLED + RTC later |
| I2C SCL | GPIO9 | OLED + RTC later |

## Safety notes

- ESP32-C3 GPIO pins are not 5V tolerant.
- LR7843 input must be driven from ESP32-C3 GPIO logic only, not from 5V.
- Use common GND between ESP32-C3 and valve power/MOSFET side.
- Do not power the valve from the ESP32-C3 3.3V pin.
- Do not connect DS1307 I2C pullups to 5V.
- DS1307 module VCC may be 5V, but SDA/SCL pullups must be to 3.3V only.
- Do not enable setup portal/AP+STA until normal station Wi-Fi, heartbeat, config fetch, command polling, and valve control are stable.

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

`include/DeviceSecrets.h` is ignored by Git.

Use your Mac LAN IP for `API_BASE_URL`, not `localhost`, because the ESP32-C3 is a separate device on the network.

## Build, upload, and test

From the repo root:

```bash
git pull
pio run
pio run -t upload
pio device monitor -b 115200
```

Expected boot output should include:

```text
Biztola Smart Plant Bed ESP32-C3 starting...
Firmware version: smart-plant-bed-c3-m4-0.3
Valve OFF - safe boot default
Valve GPIO: 5
Wi-Fi connected.
Config fetched successfully.
No pending command.
```

When you send a dashboard watering command, expected output should include:

```text
Command found: #... type=valve_on
Valve ON command duration_seconds: ...
Command #... marked as acknowledged
Valve ON - dashboard command
Watering will auto-stop after seconds: ...
```

After the duration completes, expected output should include:

```text
Watering duration completed.
Valve OFF - duration completed
Command #... marked as executed
Valve ON command completed and executed: #...
```

When you send a stop command during active watering, expected output should include:

```text
Command found: #... type=valve_off
Command #... marked as acknowledged
Valve OFF - dashboard stop command
Closing interrupted valve_on command: #...
Command #... marked as executed
Valve OFF command executed.
```

## Next milestone

Milestone 5 should add soil moisture sensor first:

- GPIO1 ADC input
- disconnected sensor handling
- raw ADC logging
- calibration values for ESP32-C3
- readings upload only after local reading is stable

Do not add DHT11/OLED/RTC before soil moisture is stable.

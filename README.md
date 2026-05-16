# Smart Plant Bed Device C3

Clean ESP32-C3-only firmware for the Smart Plant Bed device.

This repository intentionally starts small. Milestone 1 proved stable ESP32-C3 Wi-Fi station mode. Milestone 2 proved Laravel heartbeat. Milestone 3 now adds Laravel config fetch and command polling, but still does not control hardware.

## Current scope: Milestone 3

Included now:

- ESP32-C3 Super Mini / `esp32-c3-devkitm-1` PlatformIO environment
- Arduino framework
- Native USB serial flags for ESP32-C3
- Fixed local Wi-Fi credentials through `include/DeviceSecrets.h`
- `WiFi.mode(WIFI_STA)`
- `WiFi.setTxPower(WIFI_POWER_8_5dBm)`
- 15 second Wi-Fi connection timeout
- 10 second Wi-Fi retry interval
- 20 ms cooperative loop delay
- Minimal `ApiClient`
- `POST /api/device/heartbeat` every 15 seconds
- `GET /api/device/config?device_uuid=...` every 60 seconds
- `GET /api/device/commands?device_uuid=...` every 5 seconds
- `POST /api/device/commands/{id}/ack` helper
- 7 second HTTP timeout
- Serial summaries for config and commands

Not included yet:

- valve GPIO control
- sensors
- OLED
- RTC
- setup portal
- AP+STA scanning

## Milestone 3 command safety

Milestone 3 is API-only. If the firmware receives `valve_on` or `valve_off`, it logs the command and marks it as `failed` with a message saying valve GPIO control is not enabled yet.

This is intentional. It prevents Laravel watering logs from showing a fake successful watering before GPIO5 valve control exists.

## Hardware target

ESP32-C3 Super Mini only.

Planned future pin map:

| Function | GPIO | Notes |
| --- | ---: | --- |
| Valve / LR7843 MOSFET input | GPIO5 | Active HIGH |
| Watering LED | GPIO5 | Same valve signal through 330 ohm resistor |
| Wi-Fi status LED | GPIO6 | Through 330 ohm resistor |
| Soil moisture ADC | GPIO1 | With 100k pulldown to GND |
| DHT11 data | GPIO2 | Future milestone |
| Manual watering button | GPIO3 | To GND, `INPUT_PULLUP` |
| OLED wake/status button | GPIO4 | To GND, `INPUT_PULLUP` |
| Wi-Fi reset button | GPIO7 | To GND, `INPUT_PULLUP` |
| I2C SDA | GPIO8 | OLED + RTC later |
| I2C SCL | GPIO9 | OLED + RTC later |

## Safety notes

- ESP32-C3 GPIO pins are not 5V tolerant.
- Do not connect DS1307 I2C pullups to 5V.
- DS1307 module VCC may be 5V, but SDA/SCL pullups must be to 3.3V only.
- Do not connect all modules during early milestones. Test the bare C3 first.
- Do not enable setup portal/AP+STA until normal station Wi-Fi, heartbeat, config fetch, and command polling are stable.

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

Expected serial output should include:

```text
Biztola Smart Plant Bed ESP32-C3 starting...
Firmware version: smart-plant-bed-c3-m3-0.1
Connecting Wi-Fi: ...
Wi-Fi connected.
POST http://.../api/device/heartbeat
POST HTTP status: 200
GET http://.../api/device/config?device_uuid=...
GET HTTP status: 200
Config summary:
GET http://.../api/device/commands?device_uuid=...
GET HTTP status: 200
No pending command.
Wi-Fi OK. IP=... RSSI=... dBm Laravel=reachable
```

If you send a dashboard watering command during Milestone 3, expected output should include:

```text
Command found: #... type=valve_on
Milestone 3 safety: valve command received but GPIO valve control is not enabled yet.
POST http://.../api/device/commands/.../ack
POST HTTP status: 200
Command #... marked as failed
```

## Next milestone

Milestone 4 should add valve control only:

- GPIO5 output
- safe default OFF on boot
- `valve_on` command support
- `valve_off` command support
- command ACK flow: `acknowledged` then `executed` or `failed`
- watering LED mirrors valve because it is physically tied to GPIO5

Do not add sensors/OLED/RTC in Milestone 4.

# Smart Plant Bed Device C3

Clean ESP32-C3-only firmware for the Smart Plant Bed device.

This repository intentionally starts small. The first target was stable ESP32-C3 Wi-Fi station mode. Milestone 2 now adds Laravel heartbeat only, without sensors, OLED, RTC, valve control, command polling, config fetch, or setup portal.

## Current scope: Milestone 2

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
- Serial boot diagnostics
- Serial Wi-Fi status logs
- Minimal `ApiClient`
- `POST /api/device/heartbeat`
- `X-DEVICE-KEY` header
- 7 second HTTP timeout
- 15 second heartbeat interval

Not included yet:

- config fetch
- command polling
- valve control
- sensors
- OLED
- RTC
- setup portal
- AP+STA scanning

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
- Do not enable setup portal/AP+STA until normal station Wi-Fi and heartbeat are stable.

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
Firmware version: smart-plant-bed-c3-m2-0.1
Connecting Wi-Fi: ...
Wi-Fi connected.
IP address: ...
POST http://.../api/device/heartbeat
Heartbeat HTTP status: 200
Heartbeat sent successfully.
Wi-Fi OK. IP=... RSSI=... dBm Laravel=reachable
```

If upload works but serial monitor is blank, press the board reset button once while the monitor is open.

## Next milestone

Milestone 3 should add config fetch and command polling only:

- `GET /api/device/config?device_uuid=...`
- compact config cache
- `GET /api/device/commands?device_uuid=...`
- command ACK helper
- no valve hardware action until Milestone 4

Keep Milestone 3 small so API parsing problems and valve hardware problems are easy to separate.

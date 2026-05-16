# Smart Plant Bed Device C3

Clean ESP32-C3-only firmware for the Smart Plant Bed device.

This repository intentionally starts small. The first target is stable ESP32-C3 Wi-Fi station mode before adding Laravel API calls, sensors, OLED, RTC, buttons, or customer setup portal.

## Milestone 1 scope

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

Not included yet:

- Laravel API
- heartbeat
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
- Do not connect all modules during Milestone 1. Test the bare C3 first.
- Do not enable setup portal/AP+STA until normal station Wi-Fi is stable.

## Local secrets setup

Copy the example file:

```bash
cp include/DeviceSecrets.example.h include/DeviceSecrets.h
```

Edit `include/DeviceSecrets.h`:

```cpp
#define WIFI_SSID "Your WiFi Name"
#define WIFI_PASSWORD "Your WiFi Password"
```

`include/DeviceSecrets.h` is ignored by Git.

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
Firmware version: smart-plant-bed-c3-m1-0.1
Connecting Wi-Fi: ...
Wi-Fi connected.
IP address: ...
Wi-Fi OK. IP=... RSSI=... dBm
```

If upload works but serial monitor is blank, press the board reset button once while the monitor is open.

## Next milestone

Milestone 2 should add Laravel heartbeat only:

- `API_BASE_URL`
- `DEVICE_UUID`
- `DEVICE_API_KEY`
- shared device headers
- `POST /api/device/heartbeat`
- no sensors, no config polling, no valve yet

Keep Milestone 2 small so Wi-Fi instability and API problems are easy to separate.

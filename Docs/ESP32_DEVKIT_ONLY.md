# ESP32 DevKit Only Branch

This branch is for the classic ESP32 DevKit prototype only.

The main branch remains the ESP32-C3 Super Mini firmware. This branch intentionally switches the build target and hardware wiring to ESP32 DevKit.

## Build

```bash
pio run -e esp32dev
```

## Upload

```bash
pio run -e esp32dev -t upload
```

## Monitor

```bash
pio device monitor -b 115200
```

## ESP32 DevKit pin map

| Function | GPIO |
|---|---:|
| Valve / MOSFET / relay output | GPIO26 |
| Wi-Fi status LED | GPIO27 |
| Watering status LED | GPIO14 |
| Wi-Fi reset / BOOT button | GPIO0 |
| Soil moisture ADC | GPIO34 |
| Manual watering button | GPIO25 |
| OLED wake / next button | GPIO33 |
| DHT11 data | GPIO32 |
| OLED + DS3231 SDA | GPIO21 |
| OLED + DS3231 SCL | GPIO22 |

## Notes

- This branch does not try to support ESP32-C3.
- `platformio.ini` uses `default_envs = esp32dev`.
- The ESP32 build excludes the C3 display and valve source files and uses ESP32-specific replacements:
  - `src/DisplayManagerEsp32.cpp`
  - `src/ValveControllerEsp32.cpp`

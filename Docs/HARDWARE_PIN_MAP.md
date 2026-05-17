# Hardware Pin Map — ESP32-C3 Smart Plant Bed

Current target board:

```text
ESP32-C3 Super Mini
PlatformIO board: esp32-c3-devkitm-1
Framework: Arduino
```

## Pin map

| Function | GPIO | Direction | Active mode | Notes |
| --- | ---: | --- | --- | --- |
| Valve / MOSFET input | GPIO5 | Output | HIGH = ON | Drives LR7843/AO3400-style MOSFET input |
| Watering LED | GPIO5 | Output mirror | HIGH = ON | Optional LED mirrors valve signal through resistor |
| Wi-Fi status LED | GPIO6 | Output | HIGH = ON | Current status indicator |
| Manual watering button | GPIO3 | Input | Press = LOW | `INPUT_PULLUP`, button to GND |
| Soil moisture ADC | GPIO1 | Analog input | n/a | Capacitive soil sensor v1.2 |
| DHT11 data | GPIO2 | Digital input | n/a | Temperature/humidity reporting only |
| DS3231 SDA | GPIO8 | I2C | n/a | RTC I2C SDA |
| DS3231 SCL | GPIO9 | I2C | n/a | RTC I2C SCL |

## Wiring notes

### Valve / MOSFET

```text
ESP32-C3 GPIO5  -> MOSFET gate/input
ESP32-C3 GND    -> MOSFET/power supply common GND
Valve +         -> external valve supply +
Valve -         -> MOSFET switched output
```

Rules:

- Do not power the valve from the ESP32-C3 3.3V pin.
- Use common GND between ESP32-C3 and external valve supply.
- GPIO5 is logic only.
- Firmware sets valve OFF at boot before Wi-Fi/API work.

### Manual watering button

```text
GPIO3 ---- button ---- GND
```

Firmware:

```text
pinMode(GPIO3, INPUT_PULLUP)
```

Behavior:

```text
released = HIGH
pressed  = LOW
```

### Soil moisture sensor

Selected sensor profile:

```text
Capacitive soil moisture sensor v1.2
ADC pin: GPIO1
100k pulldown to GND recommended
```

Measured calibration on current C3 board:

```text
unplugged data pin: ~197–209
wet soil:           ~1741–1753
dry soil:           ~2362–2371
air:                ~2795–2829
```

Firmware constants:

```text
SOIL_DISCONNECTED_RAW_MAX = 800
SOIL_WET_RAW              = 1750
SOIL_DRY_RAW              = 2365
```

Important behavior:

```text
raw < 800  -> sensor unavailable -> Laravel soil_moisture = null
raw valid  -> convert to 0–100%
```

### DHT11

```text
DHT11 VCC  -> 3.3V
DHT11 GND  -> GND
DHT11 DATA -> GPIO2
```

If the module has no onboard pull-up, add:

```text
DATA -> 10kΩ -> 3.3V
```

DHT11 is for reporting only. It does not control watering.

### DS3231 RTC

```text
DS3231 VCC -> 3.3V
DS3231 GND -> GND
DS3231 SDA -> GPIO8
DS3231 SCL -> GPIO9
```

Rules:

- DS3231 stores UTC time.
- Firmware applies Laravel timezone offset for local time.
- Do not connect I2C pullups to 5V.
- Use 3.3V-safe module/pullups.

## Power notes

The ESP32-C3 Super Mini onboard regulator can be weak for multiple modules.

Recommended:

- Use a stable external 3.3V module for sensors/RTC/LEDs when needed.
- Keep all grounds common.
- Avoid drawing valve or high-current loads from ESP32-C3 3.3V.
- Use low-current LEDs where possible.

## Current known-good status

Working on current prototype:

```text
GPIO5 valve/watering LED
GPIO6 Wi-Fi LED
GPIO3 manual button
GPIO1 soil sensor
GPIO2 DHT11
GPIO8/GPIO9 DS3231 RTC
```

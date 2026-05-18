# Wi-Fi Setup and Reset

This document describes the M14 Wi-Fi provisioning behavior for the ESP32-C3 Smart Plant Bed firmware.

Current firmware:

```text
smart-plant-bed-c3-m14-0.1-wifi-setup
```

## Purpose

Wi-Fi setup/reset is only for Wi-Fi provisioning problems.

It does **not** clear:

```text
cached Laravel config
schedule config
timezone config
soil threshold config
device UUID/API key from firmware secrets
```

It clears only:

```text
stored Wi-Fi SSID
stored Wi-Fi password
```

## Hardware button

```text
GPIO7 ---- button ---- GND
```

Firmware mode:

```text
INPUT_PULLUP
released = HIGH
pressed  = LOW
```

## How to reset Wi-Fi

The Wi-Fi reset button is checked only during boot.

Correct method:

```text
1. Power off / unplug device
2. Hold GPIO7 button
3. Power on while holding GPIO7
4. Keep holding for 3 seconds
5. Release after Wi-Fi reset is confirmed
```

Alternative method if already powered:

```text
1. Hold GPIO7 button
2. While holding, press board RESET/EN
3. Keep holding GPIO7 for 3 seconds during boot
```

Holding GPIO7 while the firmware is already running does not reset Wi-Fi.

## Reset flow

```text
Hold GPIO7 during boot
  clear wifi_ssid
  clear wifi_pass
  keep cfg_json cached Laravel config
  set one-shot wifi_setup flag
  restart

Next boot
  consume wifi_setup flag
  skip stored Wi-Fi
  skip DeviceSecrets Wi-Fi
  start PlantBed-Setup hotspot directly
```

## Setup hotspot

```text
SSID: PlantBed-Setup
Password: plantbed123
URL: http://192.168.4.1
```

## Setup page behavior

The setup page is designed to load immediately.

```text
GET /
  returns setup page immediately

browser loads page
  JavaScript starts Wi-Fi scan after page load

GET /networks
  scans Wi-Fi and fills the dropdown

POST /save
  tests submitted Wi-Fi credentials
```

If Wi-Fi scan fails, the page still works using manual SSID.

If the password is wrong:

```text
same page remains open
error message is shown
password field is focused again
user can retry
```

If the password is correct:

```text
Wi-Fi credentials are saved
setup form is hidden
success message is shown
device restarts
```

## Manual SSID field

The manual SSID field is kept intentionally.

Use it when:

```text
Wi-Fi scan fails
network list is unstable
router SSID is hidden
user knows exact SSID
```

## DeviceSecrets behavior

Normal development boot:

```text
stored Wi-Fi exists:
  connect using stored Wi-Fi

stored Wi-Fi missing:
  fall back to DeviceSecrets Wi-Fi
```

After GPIO7 Wi-Fi reset:

```text
skip DeviceSecrets Wi-Fi
start setup hotspot directly
```

This keeps development easy while still making customer setup behavior correct.

## OLED behavior

During reset hold:

```text
Wi-Fi Reset
Keep holding
3 seconds
```

If released early:

```text
Wi-Fi Reset
Cancelled
Released early
```

After confirmed reset:

```text
Wi-Fi Setup
Starting AP
PlantBed-Setup
```

When hotspot is ready:

```text
Wi-Fi Setup
PlantBed-Setup
192.168.4.1
```

After Wi-Fi is saved:

```text
Wi-Fi Saved
Restarting
Please wait
```

## Expected logs

Reset confirmed:

```text
Wi-Fi reset confirmed.
Stored Wi-Fi credentials cleared.
Cached Laravel config was not cleared.
Wi-Fi setup portal requested for next boot.
Saved Wi-Fi cleared. Cached Laravel config kept. Restarting into setup portal...
```

Setup portal boot:

```text
Wi-Fi setup portal request consumed.
Wi-Fi setup was requested. Starting setup portal without trying saved/development Wi-Fi.
Starting setup portal...
Setup hotspot SSID: PlantBed-Setup
Setup hotspot password: plantbed123
Setup portal URL: http://192.168.4.1
```

Successful save:

```text
Testing submitted Wi-Fi credentials...
Submitted Wi-Fi credentials worked.
Wi-Fi credentials saved to flash.
Wi-Fi setup portal request cleared.
```

Next boot:

```text
Loading stored Wi-Fi config...
Stored Wi-Fi credentials found.
Trying stored Wi-Fi credentials...
Wi-Fi connected.
```

## Tested behavior

Confirmed on ESP32-C3 hardware:

```text
PlatformIO build passes
GPIO7 boot reset works
Wi-Fi credentials clear only Wi-Fi keys
cached Laravel config remains
setup request flag is consumed on next boot
DeviceSecrets is skipped after Wi-Fi reset
PlantBed-Setup hotspot starts
setup page loads immediately
Wi-Fi scan fills dropdown
wrong password stays on setup page
correct password saves Wi-Fi and restarts
stored Wi-Fi is used after restart
Laravel reconnect works after stored Wi-Fi boot
OLED reset/setup pages show during boot/setup
OLED duplicate initialization is guarded
```

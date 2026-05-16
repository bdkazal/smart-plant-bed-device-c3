#pragma once

// Copy this file to include/DeviceSecrets.h and fill in your local values.
// DeviceSecrets.h is ignored by Git and must not be committed.

// ===== Wi-Fi config =====
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// ===== Laravel API config =====
// Use your Mac LAN IP, not localhost, when testing from ESP32-C3.
#define API_BASE_URL "http://192.168.0.xxx:8000"
#define DEVICE_UUID "YOUR_DEVICE_UUID"
#define DEVICE_API_KEY "YOUR_DEVICE_API_KEY"

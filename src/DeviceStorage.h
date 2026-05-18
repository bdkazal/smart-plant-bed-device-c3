#pragma once

#include <Arduino.h>

struct StoredDeviceConfig
{
  String wifiSsid;
  String wifiPassword;
  bool hasWifiCredentials = false;
};

void beginDeviceStorage();

StoredDeviceConfig loadStoredDeviceConfig();
bool saveWifiCredentials(const String &ssid, const String &password);
void clearStoredWifiCredentials();
void requestWifiSetupPortalOnNextBoot();
bool consumeWifiSetupPortalRequest();

String loadCachedConfigJson();
bool hasCachedConfigJson();
bool saveCachedConfigJsonIfChanged(const String &configJson);

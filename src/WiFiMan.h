#pragma once

#include <Arduino.h>

#include "DeviceStorage.h"

bool isWifiConnected();
bool isServerRecentlyReachable();
void markServerResult(int statusCode);
void markServerUnavailable();

unsigned long heartbeatIntervalForCurrentReachability();
unsigned long commandPollIntervalForCurrentReachability();
unsigned long configFetchIntervalForCurrentReachability();

bool connectWithCredentials(const String &ssid, const String &password);
bool connectWifiUsingConfig(const StoredDeviceConfig &storedConfig);
void connectWifi();
void updateWiFiReconnect();

#pragma once

#include <Arduino.h>

bool isWifiConnected();
bool isServerRecentlyReachable();
void markServerResult(int statusCode);
void markServerUnavailable();

unsigned long heartbeatIntervalForCurrentReachability();
unsigned long commandPollIntervalForCurrentReachability();
unsigned long configFetchIntervalForCurrentReachability();

void connectWifi();

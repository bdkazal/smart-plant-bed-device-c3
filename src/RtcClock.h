#pragma once

#include <Arduino.h>

void beginRtcClock();

bool isRtcAvailable();
bool isRtcTimeValid();

// Loads DS3231 UTC time into the ESP32-C3 system clock when the RTC has valid time.
bool loadSystemTimeFromRtc();

// Saves the current ESP32-C3 system UTC time into the DS3231 after successful NTP/Laravel sync.
bool saveSystemTimeToRtc();

String getRtcStatusText();

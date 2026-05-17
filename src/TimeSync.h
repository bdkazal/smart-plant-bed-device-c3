#pragma once

#include <Arduino.h>

void beginTimeSync();
void syncTimeFromNtp(const String &timezoneName, int timezoneOffsetMinutes = 360);

// Preferred Laravel fallback time source.
// Expected format: ISO-8601 UTC, for example 2026-05-12T15:54:23+00:00 or 2026-05-12T15:54:23Z.
bool syncTimeFromLaravelUtcTimestamp(const String &timestamp);

// Legacy fallback for older Laravel responses.
// Expected format: YYYY-MM-DD HH:MM:SS in the configured local timezone.
bool syncTimeFromLaravelTimestamp(const String &timestamp);

bool isTimeReady();
String getTimeSourceText();

int getCurrentDayOfWeekIso();
String getCurrentDateString();
String getCurrentTimeString();
String getCurrentHourMinuteString();

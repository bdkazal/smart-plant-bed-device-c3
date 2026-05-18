#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

extern String configDeviceName;
extern String configTimezone;
extern String configWateringMode;
extern int configTimezoneOffsetMinutes;
extern int configMaxWateringDurationSeconds;
extern int configCooldownMinutes;
extern int configLocalManualDurationSeconds;
extern int configScheduleCount;
extern bool hasSoilMoistureThreshold;
extern int configSoilMoistureThreshold;
extern bool hasLoadedCachedConfig;

bool applyConfigObject(JsonObject config);
bool parseConfigResponse(const String &response);
bool parseCachedConfigObjectJson(const String &configJson);
void loadCachedConfigOnBoot();
String extractConfigJsonForCache(const String &response);
void printConfigSummary();
int getLocalManualDurationSeconds();

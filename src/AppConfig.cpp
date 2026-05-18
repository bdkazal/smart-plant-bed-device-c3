#include "AppConfig.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include "DeviceStorage.h"
#include "ScheduleConfig.h"
#include "TimeSync.h"
#include "ValveController.h"

String serverTimeUtc = "";
String serverTimeLocal = "";
String configDeviceName = "";
String configTimezone = "Asia/Dhaka";
String configWateringMode = "schedule";
int configTimezoneOffsetMinutes = 360;
int configMaxWateringDurationSeconds = 30;
int configCooldownMinutes = 60;
int configLocalManualDurationSeconds = 30;
int configScheduleCount = 0;
bool hasSoilMoistureThreshold = false;
int configSoilMoistureThreshold = 0;
bool hasLoadedCachedConfig = false;

void printConfigSummary()
{
  Serial.println("Config summary:");
  Serial.print("  device_name: ");
  Serial.println(configDeviceName.length() ? configDeviceName : "missing");
  Serial.print("  timezone: ");
  Serial.println(configTimezone);
  Serial.print("  timezone_offset_minutes: ");
  Serial.println(configTimezoneOffsetMinutes);
  Serial.print("  watering_mode: ");
  Serial.println(configWateringMode);
  Serial.print("  soil_moisture_threshold: ");
  if (hasSoilMoistureThreshold)
  {
    Serial.println(configSoilMoistureThreshold);
  }
  else
  {
    Serial.println("null");
  }
  Serial.print("  max_watering_duration_seconds: ");
  Serial.println(configMaxWateringDurationSeconds);
  Serial.print("  cooldown_minutes: ");
  Serial.println(configCooldownMinutes);
  Serial.print("  local_manual_duration_seconds: ");
  Serial.println(configLocalManualDurationSeconds);
  Serial.print("  schedules_count: ");
  Serial.println(configScheduleCount);
  Serial.print("  server_time_utc: ");
  Serial.println(serverTimeUtc.length() ? serverTimeUtc : "missing");
  Serial.print("  server_time_local: ");
  Serial.println(serverTimeLocal.length() ? serverTimeLocal : "missing");
  Serial.print("  time_ready: ");
  Serial.println(isTimeReady() ? "yes" : "no");
  Serial.print("  time_source: ");
  Serial.println(getTimeSourceText());
  Serial.print("  local_time: ");
  String localTime = getCurrentTimeString();
  Serial.println(localTime.length() ? localTime : "missing");
  Serial.print("  valve_state: ");
  Serial.println(isValveOpen() ? "on" : "off");
  Serial.print("  watering_active: ");
  Serial.println(isWateringActive() ? "yes" : "no");
}

bool applyConfigObject(JsonObject config)
{
  if (config.isNull())
  {
    Serial.println("Config JSON missing config object.");
    return false;
  }

  configDeviceName = config["device_name"] | "";
  configTimezone = config["timezone"] | "Asia/Dhaka";
  configTimezoneOffsetMinutes = config["timezone_offset_minutes"] | 360;
  configWateringMode = config["watering_mode"] | "schedule";
  configMaxWateringDurationSeconds = config["max_watering_duration_seconds"] | 30;
  configCooldownMinutes = config["cooldown_minutes"] | 60;
  configLocalManualDurationSeconds = config["local_manual_duration_seconds"] | 30;

  hasSoilMoistureThreshold = !config["soil_moisture_threshold"].isNull();
  configSoilMoistureThreshold = config["soil_moisture_threshold"] | 0;

  JsonArray schedules = config["schedules"].as<JsonArray>();
  parseScheduleConfigs(schedules);
  configScheduleCount = getScheduleConfigCount();

  return true;
}

bool parseConfigResponse(const String &response)
{
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, response);

  if (error)
  {
    Serial.print("Config JSON parse failed: ");
    Serial.println(error.c_str());
    return false;
  }

  JsonObject config = doc["config"].as<JsonObject>();

  serverTimeUtc = doc["server_time_utc"] | "";
  serverTimeLocal = doc["server_time_local"] | "";

  if (!applyConfigObject(config))
  {
    return false;
  }

  syncTimeFromLaravelUtcTimestamp(serverTimeUtc);
  syncTimeFromLaravelTimestamp(serverTimeLocal);

  printConfigSummary();
  return true;
}

bool parseCachedConfigObjectJson(const String &configJson)
{
  if (configJson.length() == 0)
  {
    Serial.println("No cached config JSON to parse.");
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, configJson);

  if (error)
  {
    Serial.print("Failed to parse cached config JSON: ");
    Serial.println(error.c_str());
    return false;
  }

  if (!applyConfigObject(doc.as<JsonObject>()))
  {
    return false;
  }

  syncTimeFromNtp(configTimezone, configTimezoneOffsetMinutes);

  serverTimeUtc = "";
  serverTimeLocal = "";

  Serial.println("Cached config loaded from flash.");
  printConfigSummary();
  return true;
}

void loadCachedConfigOnBoot()
{
  String cachedConfigJson = loadCachedConfigJson();

  if (cachedConfigJson.length() == 0)
  {
    Serial.println("No cached Laravel config found in flash.");
    hasLoadedCachedConfig = false;
    return;
  }

  hasLoadedCachedConfig = parseCachedConfigObjectJson(cachedConfigJson);
}

String extractConfigJsonForCache(const String &response)
{
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, response);

  if (error)
  {
    Serial.print("Failed to parse config response for cache extraction: ");
    Serial.println(error.c_str());
    return "";
  }

  JsonObject config = doc["config"].as<JsonObject>();

  if (config.isNull())
  {
    Serial.println("Cannot extract cache config: config object missing.");
    return "";
  }

  String configJson;
  serializeJson(config, configJson);
  return configJson;
}

int getLocalManualDurationSeconds()
{
  if (configLocalManualDurationSeconds > 0)
  {
    return configLocalManualDurationSeconds;
  }

  return 30;
}

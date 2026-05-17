#include "LocalAutomation.h"

#include <Arduino.h>

#include "ValveController.h"

extern bool isServerRecentlyReachable();
extern String configWateringMode;
extern bool hasSoilMoistureThreshold;
extern int configSoilMoistureThreshold;
extern int configMaxWateringDurationSeconds;
extern int configCooldownMinutes;
extern int configScheduleCount;

unsigned long lastLocalAutoWateringAt = 0;

bool isLocalAutoModeEnabled()
{
  return configWateringMode == "auto";
}

bool isLocalScheduleModeEnabled()
{
  return configWateringMode == "schedule";
}

bool hasUsableLocalAutoConfig()
{
  return hasSoilMoistureThreshold &&
         configSoilMoistureThreshold > 0 &&
         configMaxWateringDurationSeconds > 0;
}

bool localAutoCooldownPassed()
{
  if (lastLocalAutoWateringAt == 0)
  {
    return true;
  }

  unsigned long cooldownMs = (unsigned long)configCooldownMinutes * 60UL * 1000UL;

  return millis() - lastLocalAutoWateringAt >= cooldownMs;
}

void beginLocalAutomation()
{
  lastLocalAutoWateringAt = 0;

  Serial.println();
  Serial.println("Local automation initialized.");
  Serial.println("Local auto watering is fallback-only when Laravel is not reachable.");
  Serial.println("Local schedule watering is disabled until schedule fallback read-only testing passes.");
}

void updateLocalAutomation(const SensorReading &reading)
{
  if (isServerRecentlyReachable())
  {
    return;
  }

  if (!hasUsableLocalAutoConfig())
  {
    Serial.println("Local auto skipped: no usable cached config.");
    return;
  }

  if (!isLocalAutoModeEnabled())
  {
    return;
  }

  if (!reading.hasSoilMoisture)
  {
    Serial.println("Local auto skipped: soil moisture reading unavailable.");
    return;
  }

  if (isWateringActive())
  {
    return;
  }

  if (!localAutoCooldownPassed())
  {
    return;
  }

  if (reading.soilMoisturePercent > configSoilMoistureThreshold)
  {
    return;
  }

  Serial.println();
  Serial.println("Local fallback auto watering triggered.");
  Serial.print("Soil moisture %: ");
  Serial.println(reading.soilMoisturePercent);
  Serial.print("Threshold %: ");
  Serial.println(configSoilMoistureThreshold);
  Serial.print("Duration seconds: ");
  Serial.println(configMaxWateringDurationSeconds);

  startLocalAutoWatering(configMaxWateringDurationSeconds);

  lastLocalAutoWateringAt = millis();
}

void updateLocalScheduleFallback()
{
  if (isServerRecentlyReachable())
  {
    return;
  }

  if (!isLocalScheduleModeEnabled())
  {
    return;
  }

  if (configScheduleCount <= 0)
  {
    Serial.println("Local schedule skipped: no usable cached schedule config.");
    return;
  }

  Serial.println("Local schedule skipped: read-only schedule fallback test is not implemented yet.");
}

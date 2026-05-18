#include "LocalAutomation.h"

#include <Arduino.h>

#include "ScheduleConfig.h"
#include "TimeSync.h"
#include "ValveController.h"

extern bool isServerRecentlyReachable();
extern String configWateringMode;
extern bool hasSoilMoistureThreshold;
extern int configSoilMoistureThreshold;
extern int configMaxWateringDurationSeconds;
extern int configCooldownMinutes;
extern int configScheduleCount;

unsigned long lastLocalAutoWateringAt = 0;
String lastTriggeredScheduleKey = "";

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

bool hasUsableLocalScheduleConfig()
{
  return configScheduleCount > 0 && getScheduleConfigCount() > 0;
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

bool isScheduleDue(const WateringScheduleConfig &schedule, int currentDayOfWeek, const String &currentTime)
{
  if (!schedule.isEnabled)
  {
    return false;
  }

  if (schedule.dayOfWeek != currentDayOfWeek)
  {
    return false;
  }

  if (schedule.timeOfDay.length() < 5 || currentTime.length() < 5)
  {
    return false;
  }

  String scheduleHourMinute = schedule.timeOfDay.substring(0, 5);
  String currentHourMinute = currentTime.substring(0, 5);

  return scheduleHourMinute == currentHourMinute;
}

String makeScheduleTriggerKey(const WateringScheduleConfig &schedule, const String &currentDate)
{
  return currentDate + "#" + String(schedule.id) + "#" + schedule.timeOfDay;
}

void beginLocalAutomation()
{
  lastLocalAutoWateringAt = 0;
  lastTriggeredScheduleKey = "";

  Serial.println();
  Serial.println("Local automation initialized.");
  Serial.println("Local auto watering is fallback-only when Laravel is not reachable.");
  Serial.println("Local schedule watering is fallback-only when Laravel is not reachable and valid time is available.");
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

  if (!hasUsableLocalScheduleConfig())
  {
    Serial.println("Local schedule skipped: no usable cached schedule config.");
    return;
  }

  if (!isTimeReady())
  {
    Serial.println("Local schedule skipped: time is not ready.");
    return;
  }

  if (isWateringActive())
  {
    return;
  }

  int currentDayOfWeek = getCurrentDayOfWeekIso();
  String currentDate = getCurrentDateString();
  String currentTime = getCurrentTimeString();

  if (currentDayOfWeek == 0 || currentDate.length() == 0 || currentTime.length() == 0)
  {
    Serial.println("Local schedule skipped: current date/time unavailable.");
    return;
  }

  for (int i = 0; i < getScheduleConfigCount(); i++)
  {
    WateringScheduleConfig schedule = getScheduleConfigAt(i);

    if (!isScheduleDue(schedule, currentDayOfWeek, currentTime))
    {
      continue;
    }

    String triggerKey = makeScheduleTriggerKey(schedule, currentDate);

    if (triggerKey == lastTriggeredScheduleKey)
    {
      return;
    }

    if (schedule.durationSeconds <= 0)
    {
      Serial.println("Local schedule skipped: invalid duration.");
      lastTriggeredScheduleKey = triggerKey;
      return;
    }

    Serial.println();
    Serial.println("Local fallback schedule watering triggered.");
    Serial.print("Schedule ID: ");
    Serial.println(schedule.id);
    Serial.print("Day of week: ");
    Serial.println(schedule.dayOfWeek);
    Serial.print("Scheduled time: ");
    Serial.println(schedule.timeOfDay);
    Serial.print("Current date: ");
    Serial.println(currentDate);
    Serial.print("Current time: ");
    Serial.println(currentTime);
    Serial.print("Duration seconds: ");
    Serial.println(schedule.durationSeconds);

    startLocalScheduleWatering(schedule.durationSeconds);

    lastTriggeredScheduleKey = triggerKey;

    return;
  }
}

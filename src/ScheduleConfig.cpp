#include "ScheduleConfig.h"

WateringScheduleConfig scheduleConfigs[MAX_WATERING_SCHEDULES];
int scheduleConfigCount = 0;

void resetScheduleConfigs()
{
  for (int i = 0; i < MAX_WATERING_SCHEDULES; i++)
  {
    scheduleConfigs[i] = WateringScheduleConfig();
  }

  scheduleConfigCount = 0;
}

bool parseScheduleEnabled(JsonObject schedule)
{
  if (!schedule["is_enabled"].isNull())
  {
    return schedule["is_enabled"] | false;
  }

  if (!schedule["enabled"].isNull())
  {
    return schedule["enabled"] | false;
  }

  return false;
}

String parseScheduleTime(JsonObject schedule)
{
  const char *timeOfDay = schedule["time_of_day"] | "";

  if (strlen(timeOfDay) > 0)
  {
    return String(timeOfDay);
  }

  const char *time = schedule["time"] | "";

  return String(time);
}

int parseScheduleDuration(JsonObject schedule)
{
  int duration = schedule["duration_seconds"] | 0;

  if (duration > 0)
  {
    return duration;
  }

  return schedule["watering_duration_seconds"] | 0;
}

void parseScheduleConfigs(JsonArray schedules)
{
  resetScheduleConfigs();

  if (schedules.isNull())
  {
    return;
  }

  for (JsonObject schedule : schedules)
  {
    if (scheduleConfigCount >= MAX_WATERING_SCHEDULES)
    {
      Serial.println("Schedule config limit reached. Extra schedules ignored.");
      break;
    }

    WateringScheduleConfig parsed;
    parsed.id = schedule["id"] | 0;
    parsed.isEnabled = parseScheduleEnabled(schedule);
    parsed.dayOfWeek = schedule["day_of_week"] | 0;
    parsed.timeOfDay = parseScheduleTime(schedule);
    parsed.durationSeconds = parseScheduleDuration(schedule);

    scheduleConfigs[scheduleConfigCount] = parsed;
    scheduleConfigCount++;
  }
}

int getScheduleConfigCount()
{
  return scheduleConfigCount;
}

WateringScheduleConfig getScheduleConfigAt(int index)
{
  if (index < 0 || index >= scheduleConfigCount)
  {
    return WateringScheduleConfig();
  }

  return scheduleConfigs[index];
}

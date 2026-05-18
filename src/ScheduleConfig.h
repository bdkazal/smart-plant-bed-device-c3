#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

static const int MAX_WATERING_SCHEDULES = 10;

struct WateringScheduleConfig
{
  int id = 0;
  bool isEnabled = false;
  int dayOfWeek = 0;
  String timeOfDay = "";
  int durationSeconds = 0;
};

void resetScheduleConfigs();
void parseScheduleConfigs(JsonArray schedules);

int getScheduleConfigCount();
WateringScheduleConfig getScheduleConfigAt(int index);

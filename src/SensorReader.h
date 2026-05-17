#pragma once

struct SensorReading
{
  bool hasSoilMoisture = false;
  int soilMoistureRaw = 0;
  int soilMoisturePercent = 0;
};

void beginSensorReader();
SensorReading readSensors();

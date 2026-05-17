#pragma once

struct SensorReading
{
  bool hasSoilMoisture = false;
  int soilMoistureRaw = 0;
};

void beginSensorReader();
SensorReading readSensors();

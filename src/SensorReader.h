#pragma once

struct SensorReading
{
  bool hasSoilMoisture = false;
  int soilMoistureRaw = 0;
  int soilMoisturePercent = 0;

  bool hasTemperature = false;
  float temperatureC = 0.0;

  bool hasHumidity = false;
  float humidityPercent = 0.0;
};

void beginSensorReader();
SensorReading readSensors();

#include "SensorReader.h"

#include <Arduino.h>
#include <DHT.h>

#include "DisplayManager.h"
#include "PinConfig.h"

static const int DHT_TYPE = DHT11;
DHT dht(DHT_SENSOR_PIN, DHT_TYPE);

void beginSensorReader()
{
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  dht.begin();
}

int clampPercent(int value)
{
  if (value < 0)
  {
    return 0;
  }
  if (value > 100)
  {
    return 100;
  }
  return value;
}

int readSoilMoistureRaw()
{
  const int sampleCount = 10;
  long total = 0;
  for (int i = 0; i < sampleCount; i++)
  {
    total += analogRead(SOIL_MOISTURE_PIN);
    delay(5);
  }
  return total / sampleCount;
}

bool isSoilMoistureSensorAvailable(int rawValue)
{
  return rawValue >= SOIL_DISCONNECTED_RAW_MAX;
}

int convertSoilRawToPercent(int rawValue)
{
  return clampPercent(map(rawValue, SOIL_DRY_RAW, SOIL_WET_RAW, 0, 100));
}

SensorReading readSensors()
{
  SensorReading reading;

  int soilRaw = readSoilMoistureRaw();
  reading.soilMoistureRaw = soilRaw;
  reading.hasSoilMoisture = isSoilMoistureSensorAvailable(soilRaw);

  if (reading.hasSoilMoisture)
  {
    reading.soilMoisturePercent = convertSoilRawToPercent(soilRaw);
  }

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  reading.hasTemperature = !isnan(temperature);
  reading.hasHumidity = !isnan(humidity);

  if (reading.hasTemperature)
  {
    reading.temperatureC = temperature;
  }
  if (reading.hasHumidity)
  {
    reading.humidityPercent = humidity;
  }

  displaySetLatestSensorReading(reading);
  displayShowCriticalIfNeeded();
  return reading;
}

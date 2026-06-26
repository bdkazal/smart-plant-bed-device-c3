#include "SensorReader.h"

#include <Arduino.h>
#include <DHT.h>

#include "DisplayManager.h"

static const int SOIL_MOISTURE_PIN = 34;
static const int DHT_SENSOR_PIN = 32;
static const int DHT_TYPE = DHT11;

DHT dht(DHT_SENSOR_PIN, DHT_TYPE);

// ESP32 DevKit calibration from the original prototype with 100k pulldown.
static const int SOIL_DISCONNECTED_RAW_MAX = 800;
static const int SOIL_WET_RAW = 1000;
static const int SOIL_DRY_RAW = 1550;

void beginSensorReader()
{
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  dht.begin();

  Serial.println();
  Serial.println("Sensor reader initialized.");
  Serial.print("Soil moisture ADC GPIO: ");
  Serial.println(SOIL_MOISTURE_PIN);
  Serial.print("DHT11 data GPIO: ");
  Serial.println(DHT_SENSOR_PIN);
  Serial.println("Soil sensor profile: ESP32 DevKit capacitive sensor, 100k pulldown on ADC pin");
  Serial.print("Soil disconnected raw max: ");
  Serial.println(SOIL_DISCONNECTED_RAW_MAX);
  Serial.print("Soil wet raw: ");
  Serial.println(SOIL_WET_RAW);
  Serial.print("Soil dry raw: ");
  Serial.println(SOIL_DRY_RAW);
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
  int percent = map(rawValue, SOIL_DRY_RAW, SOIL_WET_RAW, 0, 100);
  return clampPercent(percent);
}

float readTemperatureC()
{
  float temperature = dht.readTemperature();

  if (isnan(temperature))
  {
    Serial.println("Warning: failed to read DHT11 temperature.");
    return NAN;
  }

  return temperature;
}

float readHumidityPercent()
{
  float humidity = dht.readHumidity();

  if (isnan(humidity))
  {
    Serial.println("Warning: failed to read DHT11 humidity.");
    return NAN;
  }

  return humidity;
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

  float temperature = readTemperatureC();
  float humidity = readHumidityPercent();

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

  Serial.println();
  Serial.println("Sensor reading:");
  Serial.print("Soil moisture raw: ");
  Serial.println(soilRaw);

  if (reading.hasSoilMoisture)
  {
    Serial.print("Soil moisture %: ");
    Serial.println(reading.soilMoisturePercent);
  }
  else
  {
    Serial.println("Soil moisture: unavailable / sensor disconnected");
  }

  if (reading.hasTemperature)
  {
    Serial.print("Temperature C: ");
    Serial.println(reading.temperatureC);
  }
  else
  {
    Serial.println("Temperature C: unavailable");
  }

  if (reading.hasHumidity)
  {
    Serial.print("Humidity %: ");
    Serial.println(reading.humidityPercent);
  }
  else
  {
    Serial.println("Humidity %: unavailable");
  }

  displaySetLatestSensorReading(reading);
  displayShowCriticalIfNeeded();

  return reading;
}

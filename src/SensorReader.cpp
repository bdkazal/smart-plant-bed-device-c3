#include "SensorReader.h"

#include <Arduino.h>

static const int SOIL_MOISTURE_PIN = 1;

void beginSensorReader()
{
  pinMode(SOIL_MOISTURE_PIN, INPUT);

  Serial.println();
  Serial.println("Sensor reader initialized.");
  Serial.print("Soil moisture ADC GPIO: ");
  Serial.println(SOIL_MOISTURE_PIN);
  Serial.println("Soil mode: raw ADC only, no percent/calibration/automation yet");
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

SensorReading readSensors()
{
  SensorReading reading;

  int soilRaw = readSoilMoistureRaw();

  reading.soilMoistureRaw = soilRaw;
  reading.hasSoilMoisture = true;

  Serial.println();
  Serial.println("Sensor reading:");
  Serial.print("Soil moisture raw: ");
  Serial.println(soilRaw);
  Serial.println("Soil moisture %: disabled until C3 calibration");

  return reading;
}

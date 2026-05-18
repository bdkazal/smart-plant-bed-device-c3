#pragma once

#include <Arduino.h>

#include "SensorReader.h"

void beginDisplayManager();

void displayShowBootStatus(const String &line1, const String &line2 = "", const String &line3 = "");
void displaySetLatestSensorReading(const SensorReading &reading);
void displayShowCurrentStatus(unsigned long visibleMs = 0);
void displayShowWateringStatus(unsigned long visibleMs = 0);
void displayShowWateringDone(unsigned long visibleMs = 10000);
void displayShowCriticalIfNeeded();
void updateDisplayManager();

bool isDisplayAvailable();

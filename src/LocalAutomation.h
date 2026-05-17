#pragma once

#include "SensorReader.h"

void beginLocalAutomation();
void updateLocalAutomation(const SensorReading &reading);
void updateLocalScheduleFallback();

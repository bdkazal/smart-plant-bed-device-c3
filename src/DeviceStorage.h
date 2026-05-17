#pragma once

#include <Arduino.h>

void beginDeviceStorage();

String loadCachedConfigJson();
bool hasCachedConfigJson();
bool saveCachedConfigJsonIfChanged(const String &configJson);

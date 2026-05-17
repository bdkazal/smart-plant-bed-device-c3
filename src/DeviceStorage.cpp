#include "DeviceStorage.h"

#include <Preferences.h>

Preferences preferences;

static const char *NAMESPACE = "plantbed";
static const char *KEY_CONFIG_JSON = "cfg_json";

void beginDeviceStorage()
{
  preferences.begin(NAMESPACE, false);

  Serial.println();
  Serial.println("Device storage initialized.");
  Serial.println("Storage scope: cached Laravel config only");
}

String loadCachedConfigJson()
{
  if (!preferences.isKey(KEY_CONFIG_JSON))
  {
    return "";
  }

  return preferences.getString(KEY_CONFIG_JSON, "");
}

bool hasCachedConfigJson()
{
  return loadCachedConfigJson().length() > 0;
}

bool saveCachedConfigJsonIfChanged(const String &configJson)
{
  if (configJson.length() == 0)
  {
    Serial.println("Cannot cache config: JSON is empty.");
    return false;
  }

  String existingConfigJson = loadCachedConfigJson();

  if (existingConfigJson == configJson)
  {
    Serial.println("Config cache unchanged. Flash write skipped.");
    return false;
  }

  preferences.putString(KEY_CONFIG_JSON, configJson);

  Serial.println("Config cache saved to flash.");

  return true;
}

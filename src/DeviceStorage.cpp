#include "DeviceStorage.h"

#include <Preferences.h>

Preferences preferences;

static const char *NAMESPACE = "plantbed";
static const char *KEY_WIFI_SSID = "wifi_ssid";
static const char *KEY_WIFI_PASS = "wifi_pass";
static const char *KEY_CONFIG_JSON = "cfg_json";

void beginDeviceStorage()
{
  preferences.begin(NAMESPACE, false);

  Serial.println();
  Serial.println("Device storage initialized.");
  Serial.println("Storage scope: Wi-Fi credentials + cached Laravel config");
}

StoredDeviceConfig loadStoredDeviceConfig()
{
  StoredDeviceConfig config;

  if (preferences.isKey(KEY_WIFI_SSID))
  {
    config.wifiSsid = preferences.getString(KEY_WIFI_SSID, "");
  }

  if (preferences.isKey(KEY_WIFI_PASS))
  {
    config.wifiPassword = preferences.getString(KEY_WIFI_PASS, "");
  }

  config.hasWifiCredentials = config.wifiSsid.length() > 0;

  Serial.println();
  Serial.println("Loading stored Wi-Fi config...");

  if (config.hasWifiCredentials)
  {
    Serial.println("Stored Wi-Fi credentials found.");
    Serial.print("Stored SSID: ");
    Serial.println(config.wifiSsid);
  }
  else
  {
    Serial.println("No stored Wi-Fi credentials found.");
  }

  return config;
}

bool saveWifiCredentials(const String &ssid, const String &password)
{
  if (ssid.length() == 0)
  {
    Serial.println("Cannot save Wi-Fi credentials: SSID is empty.");
    return false;
  }

  preferences.putString(KEY_WIFI_SSID, ssid);
  preferences.putString(KEY_WIFI_PASS, password);

  Serial.println("Wi-Fi credentials saved to flash.");
  return true;
}

void clearStoredWifiCredentials()
{
  preferences.remove(KEY_WIFI_SSID);
  preferences.remove(KEY_WIFI_PASS);

  Serial.println("Stored Wi-Fi credentials cleared.");
  Serial.println("Cached Laravel config was not cleared.");
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

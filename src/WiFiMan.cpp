#include "WiFiMan.h"

#include <Arduino.h>
#include <WiFi.h>

#include "DeviceSecrets.h"
#include "DeviceStorage.h"
#include "StatusLed.h"

static const unsigned long WIFI_RECONNECT_INTERVAL_MS = 10000;
static const unsigned long WIFI_RECONNECT_STATUS_MS = 5000;
static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;
static const unsigned long WIFI_CONNECT_DOT_INTERVAL_MS = 500;
static const unsigned long HEARTBEAT_INTERVAL_MS = 15000;
static const unsigned long CONFIG_FETCH_INTERVAL_MS = 60000;
static const unsigned long COMMAND_POLL_INTERVAL_MS = 5000;
static const unsigned long OFFLINE_HEARTBEAT_INTERVAL_MS = 30000;
static const unsigned long OFFLINE_COMMAND_POLL_INTERVAL_MS = 30000;
static const unsigned long OFFLINE_CONFIG_FETCH_INTERVAL_MS = 120000;
static const unsigned long SERVER_REACHABLE_WINDOW_MS = 15000;

bool serverReachable = false;
unsigned long lastServerSuccessAt = 0;
unsigned long lastReconnectAttemptAt = 0;
unsigned long lastReconnectStatusAt = 0;
bool reconnectInProgress = false;

bool isWifiConnected()
{
  return WiFi.status() == WL_CONNECTED;
}

bool isServerRecentlyReachable()
{
  if (!isWifiConnected() || !serverReachable)
  {
    return false;
  }

  return millis() - lastServerSuccessAt <= SERVER_REACHABLE_WINDOW_MS;
}

void markServerResult(int statusCode)
{
  if (statusCode >= 200 && statusCode < 300)
  {
    serverReachable = true;
    lastServerSuccessAt = millis();
    return;
  }

  if (statusCode < 0)
  {
    serverReachable = false;
  }
}

void markServerUnavailable()
{
  serverReachable = false;
}

unsigned long heartbeatIntervalForCurrentReachability()
{
  return isServerRecentlyReachable() ? HEARTBEAT_INTERVAL_MS : OFFLINE_HEARTBEAT_INTERVAL_MS;
}

unsigned long commandPollIntervalForCurrentReachability()
{
  return isServerRecentlyReachable() ? COMMAND_POLL_INTERVAL_MS : OFFLINE_COMMAND_POLL_INTERVAL_MS;
}

unsigned long configFetchIntervalForCurrentReachability()
{
  return isServerRecentlyReachable() ? CONFIG_FETCH_INTERVAL_MS : OFFLINE_CONFIG_FETCH_INTERVAL_MS;
}

void printWifiConnectedInfo()
{
  Serial.println("Wi-Fi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("RSSI dBm: ");
  Serial.println(WiFi.RSSI());
}

void prepareStationRadio()
{
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
}

bool connectWithCredentials(const String &ssid, const String &password)
{
  if (ssid.length() == 0)
  {
    Serial.println("Cannot connect to Wi-Fi: SSID is empty.");
    return false;
  }

  Serial.println();
  Serial.println("Connecting Wi-Fi...");
  Serial.print("SSID: ");
  Serial.println(ssid);

  prepareStationRadio();
  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long startedAt = millis();

  while (!isWifiConnected() && millis() - startedAt < WIFI_CONNECT_TIMEOUT_MS)
  {
    delay(WIFI_CONNECT_DOT_INTERVAL_MS);
    Serial.print(".");
  }

  Serial.println();

  if (isWifiConnected())
  {
    setWifiStatusLedConnected();
    printWifiConnectedInfo();
    reconnectInProgress = false;
    return true;
  }

  updateWifiStatusLedDisconnected();
  markServerUnavailable();
  reconnectInProgress = false;
  Serial.println("Wi-Fi connection failed. Device will retry later.");
  return false;
}

bool connectWifiUsingConfig(const StoredDeviceConfig &storedConfig)
{
  if (storedConfig.hasWifiCredentials)
  {
    Serial.println("Trying stored Wi-Fi credentials...");

    bool connected = connectWithCredentials(
        storedConfig.wifiSsid,
        storedConfig.wifiPassword);

    if (connected)
    {
      return true;
    }

    Serial.println("Stored Wi-Fi failed. Falling back to development secrets.");
  }
  else
  {
    Serial.println("No stored Wi-Fi. Using development secrets.");
  }

  return connectWithCredentials(WIFI_SSID, WIFI_PASSWORD);
}

void connectWifi()
{
  StoredDeviceConfig storedConfig = loadStoredDeviceConfig();
  connectWifiUsingConfig(storedConfig);
}

void startNonBlockingReconnect()
{
  StoredDeviceConfig storedConfig = loadStoredDeviceConfig();

  String ssid = WIFI_SSID;
  String password = WIFI_PASSWORD;

  if (storedConfig.hasWifiCredentials)
  {
    ssid = storedConfig.wifiSsid;
    password = storedConfig.wifiPassword;
  }

  if (ssid.length() == 0)
  {
    Serial.println("Non-blocking Wi-Fi reconnect skipped: SSID is empty.");
    return;
  }

  Serial.println();
  Serial.println("Starting non-blocking Wi-Fi reconnect...");
  Serial.print("SSID: ");
  Serial.println(ssid);

  prepareStationRadio();
  WiFi.begin(ssid.c_str(), password.c_str());

  reconnectInProgress = true;
  lastReconnectStatusAt = millis();
}

void updateWiFiReconnect()
{
  unsigned long now = millis();

  if (isWifiConnected())
  {
    if (reconnectInProgress)
    {
      Serial.println();
      Serial.println("Non-blocking Wi-Fi reconnect completed.");
      printWifiConnectedInfo();
    }

    reconnectInProgress = false;
    return;
  }

  if (reconnectInProgress)
  {
    if (now - lastReconnectStatusAt >= WIFI_RECONNECT_STATUS_MS)
    {
      Serial.println("Wi-Fi reconnect still in progress. Local controls remain active.");
      lastReconnectStatusAt = now;
    }

    if (now - lastReconnectAttemptAt < WIFI_RECONNECT_INTERVAL_MS)
    {
      return;
    }
  }

  if (now - lastReconnectAttemptAt < WIFI_RECONNECT_INTERVAL_MS)
  {
    return;
  }

  lastReconnectAttemptAt = now;
  startNonBlockingReconnect();
}

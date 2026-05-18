#include "WiFiMan.h"

#include <Arduino.h>
#include <WiFi.h>

#include "DeviceSecrets.h"
#include "StatusLed.h"

static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;
static const unsigned long HEARTBEAT_INTERVAL_MS = 15000;
static const unsigned long CONFIG_FETCH_INTERVAL_MS = 60000;
static const unsigned long COMMAND_POLL_INTERVAL_MS = 5000;
static const unsigned long OFFLINE_HEARTBEAT_INTERVAL_MS = 30000;
static const unsigned long OFFLINE_COMMAND_POLL_INTERVAL_MS = 30000;
static const unsigned long OFFLINE_CONFIG_FETCH_INTERVAL_MS = 120000;
static const unsigned long SERVER_REACHABLE_WINDOW_MS = 15000;

bool serverReachable = false;
unsigned long lastServerSuccessAt = 0;

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

void connectWifi()
{
  Serial.println();
  Serial.print("Connecting Wi-Fi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startedAt = millis();

  while (!isWifiConnected() && millis() - startedAt < WIFI_CONNECT_TIMEOUT_MS)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (isWifiConnected())
  {
    setWifiStatusLedConnected();
    Serial.println("Wi-Fi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI dBm: ");
    Serial.println(WiFi.RSSI());
  }
  else
  {
    updateWifiStatusLedDisconnected();
    Serial.println("Wi-Fi connection failed. Device will retry later.");
    WiFi.disconnect(false);
    markServerUnavailable();
  }
}

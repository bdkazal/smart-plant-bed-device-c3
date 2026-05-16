#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "ApiClient.h"
#include "DeviceSecrets.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "smart-plant-bed-c3-m2-dev"
#endif

const unsigned long WIFI_RETRY_INTERVAL_MS = 10000;
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;
const unsigned long LOOP_IDLE_DELAY_MS = 20;
const unsigned long WIFI_STATUS_LOG_INTERVAL_MS = 10000;
const unsigned long HEARTBEAT_INTERVAL_MS = 15000;
const int HTTP_TIMEOUT_MS = 7000;

unsigned long lastWifiRetryAt = 0;
unsigned long lastWifiStatusLogAt = 0;
unsigned long lastHeartbeatAt = 0;

ApiClient apiClient;
bool serverReachableRecently = false;

bool isWifiConnected()
{
  return WiFi.status() == WL_CONNECTED;
}

void printBootInfo()
{
  Serial.println();
  Serial.println("Biztola Smart Plant Bed ESP32-C3 starting...");
  Serial.print("Firmware version: ");
  Serial.println(FIRMWARE_VERSION);
  Serial.print("Chip model: ");
  Serial.println(ESP.getChipModel());
  Serial.print("Chip revision: ");
  Serial.println(ESP.getChipRevision());
  Serial.print("CPU frequency MHz: ");
  Serial.println(ESP.getCpuFreqMHz());
  Serial.print("Flash size bytes: ");
  Serial.println(ESP.getFlashChipSize());
  Serial.print("SDK version: ");
  Serial.println(ESP.getSdkVersion());
}

void connectWifi()
{
  Serial.println();
  Serial.print("Connecting Wi-Fi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
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
    Serial.println("Wi-Fi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI dBm: ");
    Serial.println(WiFi.RSSI());
  }
  else
  {
    Serial.println("Wi-Fi connection failed. Device will retry later.");
    WiFi.disconnect(false);
  }
}

String buildHeartbeatPayload()
{
  JsonDocument doc;
  doc["device_uuid"] = DEVICE_UUID;

  String payload;
  serializeJson(doc, payload);
  return payload;
}

bool sendHeartbeat()
{
  if (!isWifiConnected())
  {
    Serial.println("Heartbeat skipped: Wi-Fi offline.");
    return false;
  }

  String url = apiClient.url("/api/device/heartbeat");
  String payload = buildHeartbeatPayload();

  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.begin(url);
  apiClient.addDeviceHeaders(http);

  Serial.println();
  Serial.print("POST ");
  Serial.println(url);
  Serial.print("Heartbeat payload: ");
  Serial.println(payload);

  int statusCode = http.POST(payload);
  String response = http.getString();
  http.end();

  Serial.print("Heartbeat HTTP status: ");
  Serial.println(statusCode);

  if (response.length() > 0)
  {
    Serial.print("Heartbeat response: ");
    Serial.println(response);
  }

  if (statusCode >= 200 && statusCode < 300)
  {
    serverReachableRecently = true;
    Serial.println("Heartbeat sent successfully.");
    return true;
  }

  serverReachableRecently = false;
  Serial.println("Heartbeat failed.");
  return false;
}

void logWifiStatusIfNeeded(unsigned long now)
{
  if (!isWifiConnected())
  {
    return;
  }

  if (now - lastWifiStatusLogAt < WIFI_STATUS_LOG_INTERVAL_MS)
  {
    return;
  }

  Serial.print("Wi-Fi OK. IP=");
  Serial.print(WiFi.localIP());
  Serial.print(" RSSI=");
  Serial.print(WiFi.RSSI());
  Serial.print(" dBm Laravel=");
  Serial.println(serverReachableRecently ? "reachable" : "not-confirmed");

  lastWifiStatusLogAt = now;
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  printBootInfo();
  apiClient.begin(API_BASE_URL, DEVICE_API_KEY);

  connectWifi();

  unsigned long now = millis();
  lastWifiRetryAt = now;
  lastWifiStatusLogAt = now;

  if (isWifiConnected())
  {
    sendHeartbeat();
    lastHeartbeatAt = millis();
  }
}

void loop()
{
  unsigned long now = millis();

  if (!isWifiConnected())
  {
    serverReachableRecently = false;

    if (now - lastWifiRetryAt >= WIFI_RETRY_INTERVAL_MS)
    {
      Serial.println("Wi-Fi offline. Retrying connection...");
      connectWifi();
      lastWifiRetryAt = now;

      if (isWifiConnected())
      {
        sendHeartbeat();
        lastHeartbeatAt = millis();
      }
    }

    delay(LOOP_IDLE_DELAY_MS);
    return;
  }

  logWifiStatusIfNeeded(now);

  if (now - lastHeartbeatAt >= HEARTBEAT_INTERVAL_MS)
  {
    sendHeartbeat();
    lastHeartbeatAt = millis();
  }

  delay(LOOP_IDLE_DELAY_MS);
}

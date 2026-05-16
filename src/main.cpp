#include <Arduino.h>
#include <WiFi.h>

#include "DeviceSecrets.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "smart-plant-bed-c3-m1-dev"
#endif

const unsigned long WIFI_RETRY_INTERVAL_MS = 10000;
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;
const unsigned long LOOP_IDLE_DELAY_MS = 20;

unsigned long lastWifiRetryAt = 0;

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

void setup()
{
  Serial.begin(115200);
  delay(1000);

  printBootInfo();
  connectWifi();
  lastWifiRetryAt = millis();
}

void loop()
{
  unsigned long now = millis();

  if (!isWifiConnected() && now - lastWifiRetryAt >= WIFI_RETRY_INTERVAL_MS)
  {
    Serial.println("Wi-Fi offline. Retrying connection...");
    connectWifi();
    lastWifiRetryAt = now;
  }

  if (isWifiConnected())
  {
    static unsigned long lastStatusAt = 0;

    if (now - lastStatusAt >= 10000)
    {
      Serial.print("Wi-Fi OK. IP=");
      Serial.print(WiFi.localIP());
      Serial.print(" RSSI=");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
      lastStatusAt = now;
    }
  }

  delay(LOOP_IDLE_DELAY_MS);
}

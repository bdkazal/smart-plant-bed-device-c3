#include "StatusLed.h"

#include <Arduino.h>

static const int WIFI_STATUS_LED_PIN = 6;
static const bool WIFI_STATUS_LED_ACTIVE_LOW = false;
static const unsigned long WIFI_BLINK_INTERVAL_MS = 2000;

bool wifiLedBlinkState = false;
unsigned long lastWifiBlinkAt = 0;

int ledOnLevel(bool activeLow)
{
  return activeLow ? LOW : HIGH;
}

int ledOffLevel(bool activeLow)
{
  return activeLow ? HIGH : LOW;
}

void writeWifiLed(bool on)
{
  digitalWrite(
      WIFI_STATUS_LED_PIN,
      on ? ledOnLevel(WIFI_STATUS_LED_ACTIVE_LOW) : ledOffLevel(WIFI_STATUS_LED_ACTIVE_LOW));
}

void beginStatusLed()
{
  pinMode(WIFI_STATUS_LED_PIN, OUTPUT);

  wifiLedBlinkState = false;
  lastWifiBlinkAt = 0;
  writeWifiLed(false);

  Serial.println();
  Serial.println("Status LEDs initialized.");
  Serial.print("Wi-Fi status LED GPIO: ");
  Serial.println(WIFI_STATUS_LED_PIN);
  Serial.print("Wi-Fi status LED active mode: ");
  Serial.println(WIFI_STATUS_LED_ACTIVE_LOW ? "ACTIVE LOW" : "ACTIVE HIGH");
  Serial.println("Watering status LED: disabled - physically mirrors GPIO5 valve output");
}

void setWifiStatusLedConnected()
{
  wifiLedBlinkState = true;
  writeWifiLed(true);
}

void updateWifiStatusLedDisconnected()
{
  unsigned long now = millis();

  if (now - lastWifiBlinkAt < WIFI_BLINK_INTERVAL_MS)
  {
    return;
  }

  lastWifiBlinkAt = now;
  wifiLedBlinkState = !wifiLedBlinkState;

  writeWifiLed(wifiLedBlinkState);
}

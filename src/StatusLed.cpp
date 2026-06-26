#include "StatusLed.h"

#include <Arduino.h>

static const int WIFI_STATUS_LED_PIN = 27;
static const bool WIFI_STATUS_LED_ACTIVE_LOW = false;
static const int WATERING_STATUS_LED_PIN = 14;
static const bool WATERING_STATUS_LED_ACTIVE_LOW = false;
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

void writeWateringLed(bool on)
{
  digitalWrite(
      WATERING_STATUS_LED_PIN,
      on ? ledOnLevel(WATERING_STATUS_LED_ACTIVE_LOW) : ledOffLevel(WATERING_STATUS_LED_ACTIVE_LOW));
}

void beginStatusLed()
{
  pinMode(WIFI_STATUS_LED_PIN, OUTPUT);
  pinMode(WATERING_STATUS_LED_PIN, OUTPUT);

  wifiLedBlinkState = false;
  lastWifiBlinkAt = 0;
  writeWifiLed(false);
  writeWateringLed(false);

  Serial.println();
  Serial.println("Status LEDs initialized.");
  Serial.print("Wi-Fi status LED GPIO: ");
  Serial.println(WIFI_STATUS_LED_PIN);
  Serial.print("Wi-Fi status LED active mode: ");
  Serial.println(WIFI_STATUS_LED_ACTIVE_LOW ? "ACTIVE LOW" : "ACTIVE HIGH");
  Serial.print("Watering status LED GPIO: ");
  Serial.println(WATERING_STATUS_LED_PIN);
  Serial.print("Watering status LED active mode: ");
  Serial.println(WATERING_STATUS_LED_ACTIVE_LOW ? "ACTIVE LOW" : "ACTIVE HIGH");
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

void setWateringStatusLed(bool on)
{
  writeWateringLed(on);
}

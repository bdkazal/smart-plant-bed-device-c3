#include "StatusLed.h"

#include <Arduino.h>
#include <driver/gpio.h>

#include "PinConfig.h"

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

bool isPinEnabled(int pin)
{
  return pin >= 0;
}

void writePinIfEnabled(int pin, int value)
{
  if (!isPinEnabled(pin))
  {
    return;
  }

  gpio_set_level((gpio_num_t)pin, value == HIGH ? 1 : 0);
}

void writeWifiLed(bool on)
{
  writePinIfEnabled(
      WIFI_STATUS_LED_PIN,
      on ? ledOnLevel(WIFI_STATUS_LED_ACTIVE_LOW) : ledOffLevel(WIFI_STATUS_LED_ACTIVE_LOW));
}

void writeWateringLed(bool on)
{
  writePinIfEnabled(
      WATERING_STATUS_LED_PIN,
      on ? ledOnLevel(WATERING_STATUS_LED_ACTIVE_LOW) : ledOffLevel(WATERING_STATUS_LED_ACTIVE_LOW));
}

void beginStatusLed()
{
  if (isPinEnabled(WIFI_STATUS_LED_PIN))
  {
    pinMode(WIFI_STATUS_LED_PIN, OUTPUT);
  }

  if (isPinEnabled(WATERING_STATUS_LED_PIN))
  {
    pinMode(WATERING_STATUS_LED_PIN, OUTPUT);
  }

  wifiLedBlinkState = false;
  lastWifiBlinkAt = 0;

  writeWifiLed(false);
  writeWateringLed(false);

  Serial.println();
  Serial.println("Status LEDs initialized.");
  Serial.print("Wi-Fi status LED GPIO: ");
  Serial.println(isPinEnabled(WIFI_STATUS_LED_PIN) ? String(WIFI_STATUS_LED_PIN) : "disabled");
  Serial.print("Wi-Fi status LED active mode: ");
  Serial.println(WIFI_STATUS_LED_ACTIVE_LOW ? "ACTIVE LOW" : "ACTIVE HIGH");
  Serial.print("Watering status LED GPIO: ");
  Serial.println(isPinEnabled(WATERING_STATUS_LED_PIN) ? String(WATERING_STATUS_LED_PIN) : "disabled");
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

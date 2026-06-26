#include "ManualButton.h"

#include <Arduino.h>

#include "PinConfig.h"
#include "ValveController.h"

const unsigned long BUTTON_DEBOUNCE_MS = 50;

bool lastManualButtonReading = HIGH;
bool stableManualButtonState = HIGH;
unsigned long lastManualButtonChangeAt = 0;

extern int getLocalManualDurationSeconds();

void beginManualButton()
{
  pinMode(MANUAL_WATER_BUTTON_PIN, INPUT_PULLUP);

  lastManualButtonReading = digitalRead(MANUAL_WATER_BUTTON_PIN);
  stableManualButtonState = lastManualButtonReading;
  lastManualButtonChangeAt = millis();

  Serial.println();
  Serial.println("Manual watering button initialized.");
  Serial.print("Manual button GPIO: ");
  Serial.println(MANUAL_WATER_BUTTON_PIN);
  Serial.println("Button mode: INPUT_PULLUP, press connects GPIO to GND");
}

void handleManualButtonPress()
{
  Serial.println();
  Serial.println("Manual watering button pressed.");

  if (isWateringActive())
  {
    stopLocalWatering();
    return;
  }

  startLocalWatering(getLocalManualDurationSeconds());
}

void updateManualButton()
{
  bool currentReading = digitalRead(MANUAL_WATER_BUTTON_PIN);
  unsigned long now = millis();

  if (currentReading != lastManualButtonReading)
  {
    lastManualButtonChangeAt = now;
    lastManualButtonReading = currentReading;
  }

  if (now - lastManualButtonChangeAt < BUTTON_DEBOUNCE_MS)
  {
    return;
  }

  if (currentReading == stableManualButtonState)
  {
    return;
  }

  stableManualButtonState = currentReading;

  if (stableManualButtonState == LOW)
  {
    handleManualButtonPress();
  }
}

#include "WifiReset.h"

#include <Arduino.h>

#include "DeviceStorage.h"
#include "DisplayManager.h"
#include "PinConfig.h"

static const unsigned long WIFI_RESET_HOLD_MS = 3000;

void checkWifiResetOnBoot()
{
  pinMode(WIFI_RESET_BUTTON_PIN, INPUT_PULLUP);

  Serial.println();
  Serial.println("Checking Wi-Fi reset button...");
  Serial.print("Wi-Fi reset GPIO: ");
  Serial.println(WIFI_RESET_BUTTON_PIN);
  Serial.println("Hold Wi-Fi reset button during startup to clear saved Wi-Fi only.");

  if (digitalRead(WIFI_RESET_BUTTON_PIN) == HIGH)
  {
    Serial.println("Wi-Fi reset button not pressed.");
    return;
  }

  Serial.println("Wi-Fi reset button pressed. Keep holding...");
  displayShowBootStatus("Wi-Fi Reset", "Keep holding", "3 seconds");

  unsigned long startedAt = millis();

  while (millis() - startedAt < WIFI_RESET_HOLD_MS)
  {
    if (digitalRead(WIFI_RESET_BUTTON_PIN) == HIGH)
    {
      Serial.println("Wi-Fi reset cancelled. Button released too early.");
      displayShowBootStatus("Wi-Fi Reset", "Cancelled", "Released early");
      delay(1000);
      return;
    }

    Serial.print(".");
    delay(250);
  }

  Serial.println();
  Serial.println("Wi-Fi reset confirmed.");
  displayShowBootStatus("Wi-Fi Setup", "Starting AP", "PlantBed-Setup");

  clearStoredWifiCredentials();
  requestWifiSetupPortalOnNextBoot();

  Serial.println("Saved Wi-Fi cleared. Cached Laravel config kept. Restarting into setup portal...");
  delay(1000);

  ESP.restart();
}

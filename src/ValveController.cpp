#include "ValveController.h"

#include <Arduino.h>
#include <driver/gpio.h>

#include "DisplayManager.h"
#include "PinConfig.h"
#include "StatusLed.h"

const int OUTPUT_ON_LEVEL = VALVE_ACTIVE_LOW ? LOW : HIGH;
const int OUTPUT_OFF_LEVEL = VALVE_ACTIVE_LOW ? HIGH : LOW;

bool valveOpen = false;
bool wateringActive = false;
int activeCommandId = 0;
unsigned long wateringStartedAt = 0;
unsigned long wateringDurationMs = 0;

extern bool ackCommand(int commandId, const char *status, const char *message);
extern bool syncDeviceState(int lastCompletedCommandId);
extern void syncDeviceStateIfServerReachable(int lastCompletedCommandId);

void writeOutputLevel(int level)
{
  gpio_set_level((gpio_num_t)VALVE_CONTROL_PIN, level == HIGH ? 1 : 0);
}

bool isValveOpen()
{
  return valveOpen;
}

bool isWateringActive()
{
  return wateringActive;
}

int getActiveCommandId()
{
  return activeCommandId;
}

int getWateringDurationSeconds()
{
  return wateringDurationMs / 1000UL;
}

void setOutputOff(const char *reason)
{
  writeOutputLevel(OUTPUT_OFF_LEVEL);
  setWateringStatusLed(false);
  valveOpen = false;

  Serial.print("Output OFF");
  if (reason != nullptr && strlen(reason) > 0)
  {
    Serial.print(" - ");
    Serial.print(reason);
  }
  Serial.println();
}

void setOutputOn(const char *reason)
{
  writeOutputLevel(OUTPUT_ON_LEVEL);
  setWateringStatusLed(true);
  valveOpen = true;

  Serial.print("Output ON");
  if (reason != nullptr && strlen(reason) > 0)
  {
    Serial.print(" - ");
    Serial.print(reason);
  }
  Serial.println();
}

void clearWateringRuntime()
{
  wateringActive = false;
  activeCommandId = 0;
  wateringStartedAt = 0;
  wateringDurationMs = 0;
}

void beginValveOutput()
{
  pinMode(VALVE_CONTROL_PIN, OUTPUT);
  setOutputOff("safe boot default");
  clearWateringRuntime();

  Serial.println();
  Serial.println("Runtime output initialized.");
  Serial.print("Output GPIO: ");
  Serial.println(VALVE_CONTROL_PIN);
  Serial.print("Output active mode: ");
  Serial.println(VALVE_ACTIVE_LOW ? "ACTIVE LOW" : "ACTIVE HIGH");
}

void startWateringCommand(int commandId, int durationSeconds)
{
  if (wateringActive)
  {
    ackCommand(commandId, "failed", "Device is already watering.");
    return;
  }

  if (durationSeconds <= 0)
  {
    ackCommand(commandId, "failed", "Invalid duration_seconds.");
    return;
  }

  activeCommandId = commandId;
  wateringStartedAt = millis();
  wateringDurationMs = (unsigned long)durationSeconds * 1000UL;
  wateringActive = true;

  setOutputOn("dashboard command");
  displayShowWateringStatus(0);
  syncDeviceState(0);
  ackCommand(commandId, "acknowledged", nullptr);
}

void completeActiveWatering(const char *reason)
{
  int completedCommandId = activeCommandId;

  setOutputOff(reason);
  clearWateringRuntime();
  displayShowWateringDone();

  if (completedCommandId > 0)
  {
    ackCommand(completedCommandId, "executed", nullptr);
    syncDeviceState(completedCommandId);
    return;
  }

  syncDeviceStateIfServerReachable(0);
}

void stopWateringCommand(int commandId)
{
  int interruptedCommandId = activeCommandId;

  setOutputOff("dashboard stop command");
  clearWateringRuntime();
  displayShowWateringDone();

  if (interruptedCommandId > 0 && interruptedCommandId != commandId)
  {
    ackCommand(interruptedCommandId, "executed", nullptr);
  }

  ackCommand(commandId, "acknowledged", nullptr);
  ackCommand(commandId, "executed", nullptr);
  syncDeviceState(commandId);
}

void startLocalWateringWithReason(int durationSeconds, const char *reason)
{
  if (wateringActive || durationSeconds <= 0)
  {
    return;
  }

  activeCommandId = 0;
  wateringStartedAt = millis();
  wateringDurationMs = (unsigned long)durationSeconds * 1000UL;
  wateringActive = true;

  setOutputOn(reason);
  displayShowWateringStatus(0);
  syncDeviceStateIfServerReachable(0);
}

void startLocalWatering(int durationSeconds)
{
  startLocalWateringWithReason(durationSeconds, "manual button");
}

void startLocalAutoWatering(int durationSeconds)
{
  startLocalWateringWithReason(durationSeconds, "local auto fallback");
}

void startLocalScheduleWatering(int durationSeconds)
{
  startLocalWateringWithReason(durationSeconds, "local schedule fallback");
}

void stopLocalWatering()
{
  if (!wateringActive)
  {
    return;
  }

  int stoppedCommandId = activeCommandId;

  setOutputOff("manual button stop");
  clearWateringRuntime();
  displayShowWateringDone();

  if (stoppedCommandId > 0)
  {
    ackCommand(stoppedCommandId, "executed", nullptr);
    syncDeviceState(stoppedCommandId);
    return;
  }

  syncDeviceStateIfServerReachable(0);
}

void updateWateringState()
{
  updateDisplayManager();

  if (!wateringActive)
  {
    return;
  }

  unsigned long now = millis();

  if (now - wateringStartedAt < wateringDurationMs)
  {
    return;
  }

  completeActiveWatering("duration completed");
}

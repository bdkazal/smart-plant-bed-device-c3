#include "ValveController.h"

#include <Arduino.h>

const int VALVE_PIN = 5;
const int VALVE_ON_LEVEL = HIGH;
const int VALVE_OFF_LEVEL = LOW;

bool valveOpen = false;
bool wateringActive = false;
int activeCommandId = 0;
unsigned long wateringStartedAt = 0;
unsigned long wateringDurationMs = 0;

extern bool ackCommand(int commandId, const char *status, const char *message);
extern bool syncDeviceState(int lastCompletedCommandId);
extern void syncDeviceStateIfServerReachable(int lastCompletedCommandId);

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

void setValveOff(const char *reason)
{
  digitalWrite(VALVE_PIN, VALVE_OFF_LEVEL);
  valveOpen = false;

  Serial.print("Valve OFF");
  if (reason != nullptr && strlen(reason) > 0)
  {
    Serial.print(" - ");
    Serial.print(reason);
  }
  Serial.println();
}

void setValveOn(const char *reason)
{
  digitalWrite(VALVE_PIN, VALVE_ON_LEVEL);
  valveOpen = true;

  Serial.print("Valve ON");
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
  pinMode(VALVE_PIN, OUTPUT);
  setValveOff("safe boot default");
  clearWateringRuntime();
}

void startWateringCommand(int commandId, int durationSeconds)
{
  if (wateringActive)
  {
    Serial.println("Valve ON rejected: already watering.");
    ackCommand(commandId, "failed", "Device is already watering.");
    return;
  }

  if (durationSeconds <= 0)
  {
    Serial.println("Valve ON rejected: invalid duration.");
    ackCommand(commandId, "failed", "Invalid duration_seconds.");
    return;
  }

  activeCommandId = commandId;
  wateringStartedAt = millis();
  wateringDurationMs = (unsigned long)durationSeconds * 1000UL;
  wateringActive = true;

  setValveOn("dashboard command");
  syncDeviceState(0);

  bool acknowledged = ackCommand(commandId, "acknowledged", nullptr);

  if (!acknowledged)
  {
    Serial.println("Warning: failed to send acknowledged ack. Local watering still started.");
  }

  Serial.print("Watering will auto-stop after seconds: ");
  Serial.println(durationSeconds);
}

void completeActiveWatering(const char *reason)
{
  int completedCommandId = activeCommandId;

  setValveOff(reason);
  clearWateringRuntime();

  if (completedCommandId > 0)
  {
    if (ackCommand(completedCommandId, "executed", nullptr))
    {
      Serial.print("Valve ON command completed and executed: #");
      Serial.println(completedCommandId);
    }

    syncDeviceState(completedCommandId);
    return;
  }

  syncDeviceStateIfServerReachable(0);
}

void stopWateringCommand(int commandId)
{
  int interruptedCommandId = activeCommandId;

  setValveOff("dashboard stop command");
  clearWateringRuntime();

  if (interruptedCommandId > 0 && interruptedCommandId != commandId)
  {
    Serial.print("Closing interrupted valve_on command: #");
    Serial.println(interruptedCommandId);
    ackCommand(interruptedCommandId, "executed", nullptr);
  }

  bool acknowledged = ackCommand(commandId, "acknowledged", nullptr);

  if (!acknowledged)
  {
    Serial.println("Warning: failed to send acknowledged ack for valve_off.");
  }

  if (ackCommand(commandId, "executed", nullptr))
  {
    Serial.println("Valve OFF command executed.");
  }
  else
  {
    Serial.println("Warning: failed to send executed ack for valve_off.");
  }

  syncDeviceState(commandId);
}

void startLocalWateringWithReason(int durationSeconds, const char *reason)
{
  if (wateringActive)
  {
    Serial.println("Local watering request ignored: already watering.");
    return;
  }

  if (durationSeconds <= 0)
  {
    Serial.println("Local watering request ignored: invalid duration.");
    return;
  }

  Serial.println();
  Serial.println("Starting local watering.");

  activeCommandId = 0;
  wateringStartedAt = millis();
  wateringDurationMs = (unsigned long)durationSeconds * 1000UL;
  wateringActive = true;

  setValveOn(reason);
  syncDeviceStateIfServerReachable(0);

  Serial.print("Local watering duration seconds: ");
  Serial.println(durationSeconds);
}

void startLocalWatering(int durationSeconds)
{
  startLocalWateringWithReason(durationSeconds, "manual button");
}

void startLocalAutoWatering(int durationSeconds)
{
  startLocalWateringWithReason(durationSeconds, "local auto fallback");
}

void stopLocalWatering()
{
  if (!wateringActive)
  {
    Serial.println("Local stop ignored: device is not watering.");
    return;
  }

  Serial.println();
  Serial.println("Stopping watering from physical button.");

  int stoppedCommandId = activeCommandId;

  setValveOff("manual button stop");
  clearWateringRuntime();

  if (stoppedCommandId > 0)
  {
    Serial.print("Physical button stopped Laravel command: ");
    Serial.println(stoppedCommandId);

    bool executed = ackCommand(stoppedCommandId, "executed", nullptr);

    if (!executed)
    {
      Serial.println("Warning: failed to mark stopped Laravel command as executed.");
    }

    syncDeviceState(stoppedCommandId);
    return;
  }

  syncDeviceStateIfServerReachable(0);
}

void updateWateringState()
{
  if (!wateringActive)
  {
    return;
  }

  unsigned long now = millis();

  if (now - wateringStartedAt < wateringDurationMs)
  {
    return;
  }

  Serial.println();
  Serial.println("Watering duration completed.");
  completeActiveWatering("duration completed");
}

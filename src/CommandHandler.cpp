#include "CommandHandler.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include "ApiRuntime.h"
#include "ValveController.h"

int commandDurationSeconds(JsonObject command)
{
  int duration = command["payload"]["duration_seconds"] | configMaxWateringDurationSeconds;

  if (duration <= 0)
  {
    duration = configMaxWateringDurationSeconds;
  }

  if (duration > configMaxWateringDurationSeconds)
  {
    duration = configMaxWateringDurationSeconds;
  }

  if (duration <= 0)
  {
    duration = 30;
  }

  return duration;
}

void handleCommand(JsonObject command)
{
  int commandId = command["id"] | 0;
  const char *commandType = command["command_type"] | "";

  if (commandId <= 0 || strlen(commandType) == 0)
  {
    Serial.println("Invalid command shape. Ignoring.");
    return;
  }

  Serial.println();
  Serial.print("Command found: #");
  Serial.print(commandId);
  Serial.print(" type=");
  Serial.println(commandType);

  JsonVariant payload = command["payload"];
  if (!payload.isNull())
  {
    String payloadJson;
    serializeJson(payload, payloadJson);
    Serial.print("Command payload: ");
    Serial.println(payloadJson);
  }

  String type = commandType;

  if (type == "valve_on")
  {
    int durationSeconds = commandDurationSeconds(command);
    Serial.print("Valve ON command duration_seconds: ");
    Serial.println(durationSeconds);
    startWateringCommand(commandId, durationSeconds);
    return;
  }

  if (type == "valve_off")
  {
    stopWateringCommand(commandId);
    return;
  }

  Serial.println("Unsupported command type for Milestone 12.");
  ackCommand(commandId, "failed", "Unsupported command type for ESP32-C3 Milestone 12.");
}

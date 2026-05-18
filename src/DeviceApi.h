#pragma once

#include "SensorReader.h"

void beginDeviceApi();
void printBootInfo();

bool fetchConfig();
bool sendHeartbeat();
bool sendSensorReading(const SensorReading &reading);
bool syncDeviceState(int lastCompletedCommandId);
void syncDeviceStateIfServerReachable(int lastCompletedCommandId);
bool ackCommand(int commandId, const char *status, const char *message);
bool pollCommands();

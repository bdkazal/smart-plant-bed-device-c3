#pragma once

#include <Arduino.h>

#include "SensorReader.h"

extern String configDeviceName;
extern String configTimezone;
extern String configWateringMode;
extern int configTimezoneOffsetMinutes;
extern int configMaxWateringDurationSeconds;
extern int configCooldownMinutes;
extern int configLocalManualDurationSeconds;
extern int configScheduleCount;
extern bool hasSoilMoistureThreshold;
extern int configSoilMoistureThreshold;
extern bool hasLoadedCachedConfig;

void beginApiRuntime();

bool isWifiConnected();
bool isServerRecentlyReachable();
void markServerUnavailable();

unsigned long heartbeatIntervalForCurrentReachability();
unsigned long commandPollIntervalForCurrentReachability();
unsigned long configFetchIntervalForCurrentReachability();

void printBootInfo();
void connectWifi();

bool sendHeartbeat();
bool sendSensorReading(const SensorReading &reading);
bool syncDeviceState(int lastCompletedCommandId);
void syncDeviceStateIfServerReachable(int lastCompletedCommandId);
bool fetchConfig();
bool ackCommand(int commandId, const char *status, const char *message);
bool pollCommands();

int getLocalManualDurationSeconds();

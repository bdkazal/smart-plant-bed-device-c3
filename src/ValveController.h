#pragma once

#include <Arduino.h>

void beginValveOutput();

void startWateringCommand(int commandId, int durationSeconds);
void stopWateringCommand(int commandId);

void startLocalWatering(int durationSeconds);
void stopLocalWatering();

void updateWateringState();

bool isValveOpen();
bool isWateringActive();
int getActiveCommandId();
int getWateringDurationSeconds();

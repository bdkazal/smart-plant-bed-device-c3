#include <Arduino.h>
#include <WiFi.h>

#include "ApiRuntime.h"
#include "DeviceStorage.h"
#include "DisplayManager.h"
#include "LocalAutomation.h"
#include "ManualButton.h"
#include "SensorReader.h"
#include "StatusLed.h"
#include "TimeSync.h"
#include "ValveController.h"

const unsigned long WIFI_RETRY_INTERVAL_MS = 10000;
const unsigned long LOOP_IDLE_DELAY_MS = 20;
const unsigned long WIFI_STATUS_LOG_INTERVAL_MS = 10000;
const unsigned long READING_INTERVAL_MS = 30000;
const unsigned long SCHEDULE_CHECK_INTERVAL_MS = 5000;

unsigned long lastWifiRetryAt = 0;
unsigned long lastWifiStatusLogAt = 0;
unsigned long lastHeartbeatAt = 0;
unsigned long lastConfigFetchAt = 0;
unsigned long lastCommandPollAt = 0;
unsigned long lastReadingAt = 0;
unsigned long lastScheduleCheckAt = 0;

void updateLocalControls()
{
  updateManualButton();
  updateWateringState();
  updateDisplayManager();
}

void handleSensorReadingCycle()
{
  SensorReading reading = readSensors();

  updateLocalControls();

  if (isServerRecentlyReachable())
  {
    sendSensorReading(reading);
  }
  else
  {
    Serial.println("Laravel not reachable. Sensor reading kept local for fallback automation.");
  }

  updateLocalControls();

  updateLocalAutomation(reading);
  displayShowCriticalIfNeeded();
}

void logWifiStatusIfNeeded(unsigned long now)
{
  if (!isWifiConnected())
  {
    return;
  }

  if (now - lastWifiStatusLogAt < WIFI_STATUS_LOG_INTERVAL_MS)
  {
    return;
  }

  Serial.print("Wi-Fi OK. IP=");
  Serial.print(WiFi.localIP());
  Serial.print(" RSSI=");
  Serial.print(WiFi.RSSI());
  Serial.print(" dBm Laravel=");
  Serial.print(isServerRecentlyReachable() ? "reachable" : "not-confirmed");
  Serial.print(" Time=");
  Serial.print(getTimeSourceText());
  Serial.print(" ");

  String localTime = getCurrentTimeString();
  Serial.print(localTime.length() ? localTime : "--:--:--");
  Serial.print(" Valve=");
  Serial.print(isValveOpen() ? "on" : "off");
  Serial.print(" Watering=");
  Serial.println(isWateringActive() ? "active" : "idle");

  lastWifiStatusLogAt = now;
}

void runStartupApiTasks()
{
  sendHeartbeat();
  fetchConfig();
  syncDeviceState(0);
  pollCommands();
  handleSensorReadingCycle();
  updateLocalScheduleFallback();

  unsigned long now = millis();
  lastHeartbeatAt = now;
  lastConfigFetchAt = now;
  lastCommandPollAt = now;
  lastReadingAt = now;
  lastScheduleCheckAt = now;
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  beginTimeSync();
  beginDeviceStorage();
  loadCachedConfigOnBoot();
  beginValveOutput();
  beginStatusLed();
  beginManualButton();
  beginSensorReader();
  beginLocalAutomation();
  beginApiRuntime();

  printBootInfo();
  connectWifi();

  unsigned long now = millis();
  lastWifiRetryAt = now;
  lastWifiStatusLogAt = now;
  lastReadingAt = now;
  lastScheduleCheckAt = now;

  if (isWifiConnected())
  {
    setWifiStatusLedConnected();
    runStartupApiTasks();
  }
  else
  {
    updateWifiStatusLedDisconnected();
    handleSensorReadingCycle();
    updateLocalScheduleFallback();
    lastReadingAt = millis();
    lastScheduleCheckAt = millis();
  }
}

void loop()
{
  unsigned long now = millis();

  updateLocalControls();

  if (!isWifiConnected())
  {
    markServerUnavailable();
    updateWifiStatusLedDisconnected();

    if (now - lastWifiRetryAt >= WIFI_RETRY_INTERVAL_MS)
    {
      Serial.println("Wi-Fi offline. Retrying connection...");
      connectWifi();
      lastWifiRetryAt = millis();

      if (isWifiConnected())
      {
        setWifiStatusLedConnected();
        runStartupApiTasks();
      }
    }

    now = millis();

    if (now - lastReadingAt >= READING_INTERVAL_MS)
    {
      handleSensorReadingCycle();
      lastReadingAt = millis();
    }

    now = millis();

    if (now - lastScheduleCheckAt >= SCHEDULE_CHECK_INTERVAL_MS)
    {
      updateLocalScheduleFallback();
      lastScheduleCheckAt = millis();
    }

    updateLocalControls();
    delay(LOOP_IDLE_DELAY_MS);
    return;
  }

  setWifiStatusLedConnected();
  logWifiStatusIfNeeded(now);

  if (now - lastHeartbeatAt >= heartbeatIntervalForCurrentReachability())
  {
    sendHeartbeat();
    lastHeartbeatAt = millis();
    updateLocalControls();
  }

  now = millis();

  if (now - lastCommandPollAt >= commandPollIntervalForCurrentReachability())
  {
    pollCommands();
    lastCommandPollAt = millis();
    updateLocalControls();
  }

  now = millis();

  if (now - lastReadingAt >= READING_INTERVAL_MS)
  {
    handleSensorReadingCycle();
    lastReadingAt = millis();
    updateLocalControls();
  }

  now = millis();

  if (now - lastScheduleCheckAt >= SCHEDULE_CHECK_INTERVAL_MS)
  {
    updateLocalScheduleFallback();
    lastScheduleCheckAt = millis();
    updateLocalControls();
  }

  now = millis();

  if (now - lastConfigFetchAt >= configFetchIntervalForCurrentReachability())
  {
    fetchConfig();
    lastConfigFetchAt = millis();
    updateLocalControls();
  }

  updateLocalControls();
  delay(LOOP_IDLE_DELAY_MS);
}

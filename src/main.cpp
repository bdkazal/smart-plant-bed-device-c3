#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "ApiClient.h"
#include "DeviceSecrets.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "smart-plant-bed-c3-m4-dev"
#endif

const int VALVE_PIN = 5;
const int VALVE_ON_LEVEL = HIGH;
const int VALVE_OFF_LEVEL = LOW;

const char DEVICE_TYPE[] = "plant_bed_controller";

const unsigned long WIFI_RETRY_INTERVAL_MS = 10000;
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;
const unsigned long LOOP_IDLE_DELAY_MS = 20;
const unsigned long WIFI_STATUS_LOG_INTERVAL_MS = 10000;
const unsigned long HEARTBEAT_INTERVAL_MS = 15000;
const unsigned long CONFIG_FETCH_INTERVAL_MS = 60000;
const unsigned long COMMAND_POLL_INTERVAL_MS = 5000;
const int HTTP_TIMEOUT_MS = 7000;

unsigned long lastWifiRetryAt = 0;
unsigned long lastWifiStatusLogAt = 0;
unsigned long lastHeartbeatAt = 0;
unsigned long lastConfigFetchAt = 0;
unsigned long lastCommandPollAt = 0;

ApiClient apiClient;
bool serverReachableRecently = false;
bool valveIsOn = false;
bool wateringActive = false;
int activeCommandId = 0;
unsigned long wateringStartedAt = 0;
unsigned long wateringDurationMs = 0;

String serverTimeUtc = "";
String serverTimeLocal = "";
String configDeviceName = "";
String configTimezone = "Asia/Dhaka";
String configWateringMode = "schedule";
int configTimezoneOffsetMinutes = 360;
int configMaxWateringDurationSeconds = 30;
int configCooldownMinutes = 60;
int configLocalManualDurationSeconds = 30;
int configScheduleCount = 0;
bool hasSoilMoistureThreshold = false;
int configSoilMoistureThreshold = 0;

bool isWifiConnected()
{
  return WiFi.status() == WL_CONNECTED;
}

void setValveOff(const char *reason)
{
  digitalWrite(VALVE_PIN, VALVE_OFF_LEVEL);
  valveIsOn = false;

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
  valveIsOn = true;

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

void printBootInfo()
{
  Serial.println();
  Serial.println("Biztola Smart Plant Bed ESP32-C3 starting...");
  Serial.print("Firmware version: ");
  Serial.println(FIRMWARE_VERSION);
  Serial.print("Chip model: ");
  Serial.println(ESP.getChipModel());
  Serial.print("Chip revision: ");
  Serial.println(ESP.getChipRevision());
  Serial.print("CPU frequency MHz: ");
  Serial.println(ESP.getCpuFreqMHz());
  Serial.print("Flash size bytes: ");
  Serial.println(ESP.getFlashChipSize());
  Serial.print("SDK version: ");
  Serial.println(ESP.getSdkVersion());
  Serial.print("Valve GPIO: ");
  Serial.println(VALVE_PIN);
}

void connectWifi()
{
  Serial.println();
  Serial.print("Connecting Wi-Fi: ");
  Serial.println(WIFI_SSID);

  // Match the stable Plant Bed / Smart Fountain C3 Wi-Fi pattern:
  // station mode, reduced TX power, then begin. Do not force Wi-Fi sleep off.
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startedAt = millis();

  while (!isWifiConnected() && millis() - startedAt < WIFI_CONNECT_TIMEOUT_MS)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (isWifiConnected())
  {
    Serial.println("Wi-Fi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI dBm: ");
    Serial.println(WiFi.RSSI());
  }
  else
  {
    Serial.println("Wi-Fi connection failed. Device will retry later.");
    WiFi.disconnect(false);
  }
}

bool httpGetJson(const String &url, String &response, int &statusCode)
{
  response = "";
  statusCode = -1;

  if (!isWifiConnected())
  {
    Serial.println("GET skipped: Wi-Fi offline.");
    return false;
  }

  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.begin(url);
  apiClient.addDeviceHeaders(http);

  Serial.println();
  Serial.print("GET ");
  Serial.println(url);

  statusCode = http.GET();
  response = http.getString();
  http.end();

  Serial.print("GET HTTP status: ");
  Serial.println(statusCode);

  if (statusCode >= 200 && statusCode < 300)
  {
    serverReachableRecently = true;
    return true;
  }

  if (response.length() > 0)
  {
    Serial.print("GET response: ");
    Serial.println(response);
  }

  serverReachableRecently = false;
  return false;
}

bool httpPostJson(const String &url, const String &payload, String &response, int &statusCode)
{
  response = "";
  statusCode = -1;

  if (!isWifiConnected())
  {
    Serial.println("POST skipped: Wi-Fi offline.");
    return false;
  }

  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.begin(url);
  apiClient.addDeviceHeaders(http);

  Serial.println();
  Serial.print("POST ");
  Serial.println(url);
  Serial.print("POST payload: ");
  Serial.println(payload);

  statusCode = http.POST(payload);
  response = http.getString();
  http.end();

  Serial.print("POST HTTP status: ");
  Serial.println(statusCode);

  if (response.length() > 0)
  {
    Serial.print("POST response: ");
    Serial.println(response);
  }

  if (statusCode >= 200 && statusCode < 300)
  {
    serverReachableRecently = true;
    return true;
  }

  serverReachableRecently = false;
  return false;
}

String buildHeartbeatPayload()
{
  JsonDocument doc;
  doc["device_uuid"] = DEVICE_UUID;

  String payload;
  serializeJson(doc, payload);
  return payload;
}

bool sendHeartbeat()
{
  String response;
  int statusCode;
  bool ok = httpPostJson(apiClient.url("/api/device/heartbeat"), buildHeartbeatPayload(), response, statusCode);

  if (ok)
  {
    Serial.println("Heartbeat sent successfully.");
    return true;
  }

  Serial.println("Heartbeat failed.");
  return false;
}

String buildDeviceStatePayload(int lastCompletedCommandId = 0)
{
  JsonDocument doc;
  doc["device_uuid"] = DEVICE_UUID;
  doc["device_type"] = DEVICE_TYPE;
  doc["firmware_version"] = FIRMWARE_VERSION;
  doc["operation_state"] = wateringActive ? "watering" : "idle";
  doc["valve_state"] = valveIsOn ? "open" : "closed";
  doc["watering_state"] = wateringActive ? "watering" : "idle";

  if (lastCompletedCommandId > 0)
  {
    doc["last_completed_command_id"] = lastCompletedCommandId;
  }

  String payload;
  serializeJson(doc, payload);
  return payload;
}

bool syncDeviceState(int lastCompletedCommandId = 0)
{
  String response;
  int statusCode;
  String payload = buildDeviceStatePayload(lastCompletedCommandId);
  bool ok = httpPostJson(apiClient.url("/api/device/state"), payload, response, statusCode);

  if (ok)
  {
    Serial.println("Device state synced successfully.");
    return true;
  }

  Serial.println("Device state sync failed.");
  return false;
}

void printConfigSummary()
{
  Serial.println("Config summary:");
  Serial.print("  device_name: ");
  Serial.println(configDeviceName.length() ? configDeviceName : "missing");
  Serial.print("  timezone: ");
  Serial.println(configTimezone);
  Serial.print("  timezone_offset_minutes: ");
  Serial.println(configTimezoneOffsetMinutes);
  Serial.print("  watering_mode: ");
  Serial.println(configWateringMode);
  Serial.print("  soil_moisture_threshold: ");
  if (hasSoilMoistureThreshold)
  {
    Serial.println(configSoilMoistureThreshold);
  }
  else
  {
    Serial.println("null");
  }
  Serial.print("  max_watering_duration_seconds: ");
  Serial.println(configMaxWateringDurationSeconds);
  Serial.print("  cooldown_minutes: ");
  Serial.println(configCooldownMinutes);
  Serial.print("  local_manual_duration_seconds: ");
  Serial.println(configLocalManualDurationSeconds);
  Serial.print("  schedules_count: ");
  Serial.println(configScheduleCount);
  Serial.print("  server_time_utc: ");
  Serial.println(serverTimeUtc.length() ? serverTimeUtc : "missing");
  Serial.print("  server_time_local: ");
  Serial.println(serverTimeLocal.length() ? serverTimeLocal : "missing");
  Serial.print("  valve_state: ");
  Serial.println(valveIsOn ? "on" : "off");
  Serial.print("  watering_active: ");
  Serial.println(wateringActive ? "yes" : "no");
}

bool parseConfigResponse(const String &response)
{
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, response);

  if (error)
  {
    Serial.print("Config JSON parse failed: ");
    Serial.println(error.c_str());
    return false;
  }

  JsonObject config = doc["config"].as<JsonObject>();

  if (config.isNull())
  {
    Serial.println("Config JSON missing config object.");
    return false;
  }

  serverTimeUtc = doc["server_time_utc"] | "";
  serverTimeLocal = doc["server_time_local"] | "";

  configDeviceName = config["device_name"] | "";
  configTimezone = config["timezone"] | "Asia/Dhaka";
  configTimezoneOffsetMinutes = config["timezone_offset_minutes"] | 360;
  configWateringMode = config["watering_mode"] | "schedule";
  configMaxWateringDurationSeconds = config["max_watering_duration_seconds"] | 30;
  configCooldownMinutes = config["cooldown_minutes"] | 60;
  configLocalManualDurationSeconds = config["local_manual_duration_seconds"] | 30;

  hasSoilMoistureThreshold = !config["soil_moisture_threshold"].isNull();
  configSoilMoistureThreshold = config["soil_moisture_threshold"] | 0;

  JsonArray schedules = config["schedules"].as<JsonArray>();
  configScheduleCount = schedules.isNull() ? 0 : schedules.size();

  printConfigSummary();
  return true;
}

bool fetchConfig()
{
  String response;
  int statusCode;
  String url = apiClient.url("/api/device/config?device_uuid=" + String(DEVICE_UUID));

  if (!httpGetJson(url, response, statusCode))
  {
    Serial.println("Config fetch failed.");
    return false;
  }

  if (!parseConfigResponse(response))
  {
    serverReachableRecently = false;
    return false;
  }

  Serial.println("Config fetched successfully.");
  return true;
}

String buildAckPayload(const char *status, const char *message = nullptr)
{
  JsonDocument doc;
  doc["device_uuid"] = DEVICE_UUID;
  doc["status"] = status;

  if (message != nullptr)
  {
    doc["message"] = message;
  }

  String payload;
  serializeJson(doc, payload);
  return payload;
}

bool ackCommand(int commandId, const char *status, const char *message = nullptr)
{
  String response;
  int statusCode;
  String url = apiClient.url("/api/device/commands/" + String(commandId) + "/ack");
  String payload = buildAckPayload(status, message);

  bool ok = httpPostJson(url, payload, response, statusCode);

  if (ok)
  {
    Serial.print("Command #");
    Serial.print(commandId);
    Serial.print(" marked as ");
    Serial.println(status);
    return true;
  }

  Serial.print("Command #");
  Serial.print(commandId);
  Serial.println(" ACK failed.");
  return false;
}

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

void handleValveOnCommand(int commandId, JsonObject command)
{
  int durationSeconds = commandDurationSeconds(command);

  Serial.print("Valve ON command duration_seconds: ");
  Serial.println(durationSeconds);

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

  if (!ackCommand(commandId, "acknowledged"))
  {
    Serial.println("Valve ON aborted: could not acknowledge command.");
    return;
  }

  activeCommandId = commandId;
  wateringStartedAt = millis();
  wateringDurationMs = (unsigned long)durationSeconds * 1000UL;
  wateringActive = true;

  setValveOn("dashboard command");
  syncDeviceState(0);

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
    if (ackCommand(completedCommandId, "executed"))
    {
      Serial.print("Valve ON command completed and executed: #");
      Serial.println(completedCommandId);
    }
  }

  syncDeviceState(completedCommandId);
}

void handleValveOffCommand(int commandId)
{
  int interruptedCommandId = activeCommandId;

  if (!ackCommand(commandId, "acknowledged"))
  {
    Serial.println("Valve OFF aborted: could not acknowledge command.");
    return;
  }

  setValveOff("dashboard stop command");
  clearWateringRuntime();

  if (interruptedCommandId > 0 && interruptedCommandId != commandId)
  {
    Serial.print("Closing interrupted valve_on command: #");
    Serial.println(interruptedCommandId);
    ackCommand(interruptedCommandId, "executed");
  }

  if (ackCommand(commandId, "executed"))
  {
    Serial.println("Valve OFF command executed.");
  }

  syncDeviceState(commandId);
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
    handleValveOnCommand(commandId, command);
    return;
  }

  if (type == "valve_off")
  {
    handleValveOffCommand(commandId);
    return;
  }

  Serial.println("Unsupported command type for Milestone 4.");
  ackCommand(commandId, "failed", "Unsupported command type for ESP32-C3 Milestone 4.");
}

bool pollCommands()
{
  String response;
  int statusCode;
  String url = apiClient.url("/api/device/commands?device_uuid=" + String(DEVICE_UUID));

  if (!httpGetJson(url, response, statusCode))
  {
    Serial.println("Command poll failed.");
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, response);

  if (error)
  {
    Serial.print("Command JSON parse failed: ");
    Serial.println(error.c_str());
    return false;
  }

  JsonObject command = doc["command"].as<JsonObject>();

  if (command.isNull())
  {
    Serial.println("No pending command.");
    return true;
  }

  handleCommand(command);
  return true;
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
  Serial.print(serverReachableRecently ? "reachable" : "not-confirmed");
  Serial.print(" Valve=");
  Serial.print(valveIsOn ? "on" : "off");
  Serial.print(" Watering=");
  Serial.println(wateringActive ? "active" : "idle");

  lastWifiStatusLogAt = now;
}

void runStartupApiTasks()
{
  sendHeartbeat();
  fetchConfig();
  syncDeviceState(0);
  pollCommands();

  unsigned long now = millis();
  lastHeartbeatAt = now;
  lastConfigFetchAt = now;
  lastCommandPollAt = now;
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  beginValveOutput();
  printBootInfo();
  apiClient.begin(API_BASE_URL, DEVICE_API_KEY);

  connectWifi();

  unsigned long now = millis();
  lastWifiRetryAt = now;
  lastWifiStatusLogAt = now;

  if (isWifiConnected())
  {
    runStartupApiTasks();
  }
}

void loop()
{
  unsigned long now = millis();

  updateWateringState();

  if (!isWifiConnected())
  {
    serverReachableRecently = false;

    if (now - lastWifiRetryAt >= WIFI_RETRY_INTERVAL_MS)
    {
      Serial.println("Wi-Fi offline. Retrying connection...");
      connectWifi();
      lastWifiRetryAt = now;

      if (isWifiConnected())
      {
        runStartupApiTasks();
      }
    }

    delay(LOOP_IDLE_DELAY_MS);
    return;
  }

  logWifiStatusIfNeeded(now);

  if (now - lastHeartbeatAt >= HEARTBEAT_INTERVAL_MS)
  {
    sendHeartbeat();
    lastHeartbeatAt = millis();
  }

  now = millis();

  if (now - lastCommandPollAt >= COMMAND_POLL_INTERVAL_MS)
  {
    pollCommands();
    lastCommandPollAt = millis();
  }

  now = millis();

  if (now - lastConfigFetchAt >= CONFIG_FETCH_INTERVAL_MS)
  {
    fetchConfig();
    lastConfigFetchAt = millis();
  }

  delay(LOOP_IDLE_DELAY_MS);
}

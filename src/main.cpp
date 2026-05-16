#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "ApiClient.h"
#include "DeviceSecrets.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "smart-plant-bed-c3-m3-dev"
#endif

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
}

void connectWifi()
{
  Serial.println();
  Serial.print("Connecting Wi-Fi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
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

  if (type == "valve_on" || type == "valve_off")
  {
    Serial.println("Milestone 3 safety: valve command received but GPIO valve control is not enabled yet.");
    ackCommand(commandId, "failed", "ESP32-C3 Milestone 3 received command, but valve GPIO control is not enabled yet.");
    return;
  }

  Serial.println("Unsupported command type for Milestone 3.");
  ackCommand(commandId, "failed", "Unsupported command type for ESP32-C3 Milestone 3.");
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
  Serial.println(serverReachableRecently ? "reachable" : "not-confirmed");

  lastWifiStatusLogAt = now;
}

void runStartupApiTasks()
{
  sendHeartbeat();
  fetchConfig();
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

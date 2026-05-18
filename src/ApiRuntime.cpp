#include "ApiRuntime.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "ApiClient.h"
#include "DeviceSecrets.h"
#include "DeviceStorage.h"
#include "ScheduleConfig.h"
#include "StatusLed.h"
#include "TimeSync.h"
#include "ValveController.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "smart-plant-bed-c3-refactor-dev"
#endif

static const char DEVICE_TYPE[] = "plant_bed_controller";

static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;
static const unsigned long HEARTBEAT_INTERVAL_MS = 15000;
static const unsigned long CONFIG_FETCH_INTERVAL_MS = 60000;
static const unsigned long COMMAND_POLL_INTERVAL_MS = 5000;
static const unsigned long OFFLINE_HEARTBEAT_INTERVAL_MS = 30000;
static const unsigned long OFFLINE_COMMAND_POLL_INTERVAL_MS = 30000;
static const unsigned long OFFLINE_CONFIG_FETCH_INTERVAL_MS = 120000;
static const unsigned long SERVER_REACHABLE_WINDOW_MS = 15000;
static const int HTTP_CONNECT_TIMEOUT_MS = 1000;
static const int HTTP_RESPONSE_TIMEOUT_MS = 1500;

ApiClient apiClient;
bool serverReachable = false;
unsigned long lastServerSuccessAt = 0;

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
bool hasLoadedCachedConfig = false;

void beginApiRuntime()
{
  apiClient.begin(API_BASE_URL, DEVICE_API_KEY);
}

bool isWifiConnected()
{
  return WiFi.status() == WL_CONNECTED;
}

bool isServerRecentlyReachable()
{
  if (!isWifiConnected() || !serverReachable)
  {
    return false;
  }

  return millis() - lastServerSuccessAt <= SERVER_REACHABLE_WINDOW_MS;
}

void markServerResult(int statusCode)
{
  if (statusCode >= 200 && statusCode < 300)
  {
    serverReachable = true;
    lastServerSuccessAt = millis();
    return;
  }

  if (statusCode < 0)
  {
    serverReachable = false;
  }
}

void markServerUnavailable()
{
  serverReachable = false;
}

unsigned long heartbeatIntervalForCurrentReachability()
{
  return isServerRecentlyReachable() ? HEARTBEAT_INTERVAL_MS : OFFLINE_HEARTBEAT_INTERVAL_MS;
}

unsigned long commandPollIntervalForCurrentReachability()
{
  return isServerRecentlyReachable() ? COMMAND_POLL_INTERVAL_MS : OFFLINE_COMMAND_POLL_INTERVAL_MS;
}

unsigned long configFetchIntervalForCurrentReachability()
{
  return isServerRecentlyReachable() ? CONFIG_FETCH_INTERVAL_MS : OFFLINE_CONFIG_FETCH_INTERVAL_MS;
}

void prepareHttpClient(HTTPClient &http)
{
  http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
  http.setTimeout(HTTP_RESPONSE_TIMEOUT_MS);
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
  Serial.println(5);
}

void connectWifi()
{
  Serial.println();
  Serial.print("Connecting Wi-Fi: ");
  Serial.println(WIFI_SSID);

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
    setWifiStatusLedConnected();
    Serial.println("Wi-Fi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI dBm: ");
    Serial.println(WiFi.RSSI());
  }
  else
  {
    updateWifiStatusLedDisconnected();
    Serial.println("Wi-Fi connection failed. Device will retry later.");
    WiFi.disconnect(false);
    markServerUnavailable();
  }
}

bool httpGetJson(const String &url, String &response, int &statusCode)
{
  response = "";
  statusCode = -1;

  if (!isWifiConnected())
  {
    markServerUnavailable();
    Serial.println("GET skipped: Wi-Fi offline.");
    return false;
  }

  HTTPClient http;
  prepareHttpClient(http);
  http.begin(url);
  apiClient.addDeviceHeaders(http);

  Serial.println();
  Serial.print("GET ");
  Serial.println(url);

  statusCode = http.GET();
  response = http.getString();
  http.end();

  markServerResult(statusCode);

  Serial.print("GET HTTP status: ");
  Serial.println(statusCode);

  if (statusCode >= 200 && statusCode < 300)
  {
    return true;
  }

  if (response.length() > 0)
  {
    Serial.print("GET response: ");
    Serial.println(response);
  }

  return false;
}

bool httpPostJson(const String &url, const String &payload, String &response, int &statusCode)
{
  response = "";
  statusCode = -1;

  if (!isWifiConnected())
  {
    markServerUnavailable();
    Serial.println("POST skipped: Wi-Fi offline.");
    return false;
  }

  HTTPClient http;
  prepareHttpClient(http);
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

  markServerResult(statusCode);

  Serial.print("POST HTTP status: ");
  Serial.println(statusCode);

  if (response.length() > 0)
  {
    Serial.print("POST response: ");
    Serial.println(response);
  }

  return statusCode >= 200 && statusCode < 300;
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

String buildSensorReadingPayload(const SensorReading &reading)
{
  JsonDocument doc;
  doc["device_uuid"] = DEVICE_UUID;

  if (reading.hasTemperature)
  {
    doc["temperature"] = reading.temperatureC;
  }
  else
  {
    doc["temperature"] = nullptr;
  }

  if (reading.hasHumidity)
  {
    doc["humidity"] = (int)reading.humidityPercent;
  }
  else
  {
    doc["humidity"] = nullptr;
  }

  if (reading.hasSoilMoisture)
  {
    doc["soil_moisture"] = reading.soilMoisturePercent;
  }
  else
  {
    doc["soil_moisture"] = nullptr;
  }

  String payload;
  serializeJson(doc, payload);
  return payload;
}

bool sendSensorReading(const SensorReading &reading)
{
  String response;
  int statusCode;
  String payload = buildSensorReadingPayload(reading);
  bool ok = httpPostJson(apiClient.url("/api/device/readings"), payload, response, statusCode);

  if (ok)
  {
    Serial.println("Sensor reading uploaded successfully.");
    return true;
  }

  Serial.println("Sensor reading upload failed.");
  return false;
}

String buildDeviceStatePayload(int lastCompletedCommandId)
{
  JsonDocument doc;
  doc["device_uuid"] = DEVICE_UUID;
  doc["device_type"] = DEVICE_TYPE;
  doc["firmware_version"] = FIRMWARE_VERSION;
  doc["operation_state"] = isWateringActive() ? "watering" : "idle";
  doc["valve_state"] = isValveOpen() ? "open" : "closed";
  doc["watering_state"] = isWateringActive() ? "watering" : "idle";

  if (lastCompletedCommandId > 0)
  {
    doc["last_completed_command_id"] = lastCompletedCommandId;
  }

  String payload;
  serializeJson(doc, payload);
  return payload;
}

bool syncDeviceState(int lastCompletedCommandId)
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

void syncDeviceStateIfServerReachable(int lastCompletedCommandId)
{
  if (!isServerRecentlyReachable())
  {
    Serial.println("Device state sync skipped: Laravel is not recently reachable.");
    return;
  }

  syncDeviceState(lastCompletedCommandId);
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
  Serial.print("  time_ready: ");
  Serial.println(isTimeReady() ? "yes" : "no");
  Serial.print("  time_source: ");
  Serial.println(getTimeSourceText());
  Serial.print("  local_time: ");
  String localTime = getCurrentTimeString();
  Serial.println(localTime.length() ? localTime : "missing");
  Serial.print("  valve_state: ");
  Serial.println(isValveOpen() ? "on" : "off");
  Serial.print("  watering_active: ");
  Serial.println(isWateringActive() ? "yes" : "no");
}

bool applyConfigObject(JsonObject config)
{
  if (config.isNull())
  {
    Serial.println("Config JSON missing config object.");
    return false;
  }

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
  parseScheduleConfigs(schedules);
  configScheduleCount = getScheduleConfigCount();

  return true;
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

  serverTimeUtc = doc["server_time_utc"] | "";
  serverTimeLocal = doc["server_time_local"] | "";

  if (!applyConfigObject(config))
  {
    return false;
  }

  syncTimeFromLaravelUtcTimestamp(serverTimeUtc);
  syncTimeFromLaravelTimestamp(serverTimeLocal);

  printConfigSummary();
  return true;
}

bool parseCachedConfigObjectJson(const String &configJson)
{
  if (configJson.length() == 0)
  {
    Serial.println("No cached config JSON to parse.");
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, configJson);

  if (error)
  {
    Serial.print("Failed to parse cached config JSON: ");
    Serial.println(error.c_str());
    return false;
  }

  if (!applyConfigObject(doc.as<JsonObject>()))
  {
    return false;
  }

  syncTimeFromNtp(configTimezone, configTimezoneOffsetMinutes);

  serverTimeUtc = "";
  serverTimeLocal = "";

  Serial.println("Cached config loaded from flash.");
  printConfigSummary();
  return true;
}

void loadCachedConfigOnBoot()
{
  String cachedConfigJson = loadCachedConfigJson();

  if (cachedConfigJson.length() == 0)
  {
    Serial.println("No cached Laravel config found in flash.");
    hasLoadedCachedConfig = false;
    return;
  }

  hasLoadedCachedConfig = parseCachedConfigObjectJson(cachedConfigJson);
}

String extractConfigJsonForCache(const String &response)
{
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, response);

  if (error)
  {
    Serial.print("Failed to parse config response for cache extraction: ");
    Serial.println(error.c_str());
    return "";
  }

  JsonObject config = doc["config"].as<JsonObject>();

  if (config.isNull())
  {
    Serial.println("Cannot extract cache config: config object missing.");
    return "";
  }

  String configJson;
  serializeJson(config, configJson);
  return configJson;
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
    return false;
  }

  String configJson = extractConfigJsonForCache(response);
  saveCachedConfigJsonIfChanged(configJson);

  Serial.println("Config fetched successfully.");
  return true;
}

String buildAckPayload(const char *status, const char *message)
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

bool ackCommand(int commandId, const char *status, const char *message)
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

int getLocalManualDurationSeconds()
{
  if (configLocalManualDurationSeconds > 0)
  {
    return configLocalManualDurationSeconds;
  }

  return 30;
}

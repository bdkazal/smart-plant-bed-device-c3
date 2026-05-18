#include "DeviceApi.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "ApiClient.h"
#include "AppConfig.h"
#include "CommandHandler.h"
#include "DeviceSecrets.h"
#include "DeviceStorage.h"
#include "ValveController.h"
#include "WiFiMan.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "smart-plant-bed-c3-refactor-dev"
#endif

static const char DEVICE_TYPE[] = "plant_bed_controller";
static const int HTTP_CONNECT_TIMEOUT_MS = 1000;
static const int HTTP_RESPONSE_TIMEOUT_MS = 1500;

ApiClient apiClient;

void beginDeviceApi()
{
  apiClient.begin(API_BASE_URL, DEVICE_API_KEY);
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

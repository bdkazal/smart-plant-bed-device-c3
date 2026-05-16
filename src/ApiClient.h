#pragma once

#include <Arduino.h>
#include <HTTPClient.h>

class ApiClient
{
public:
  void begin(const char *baseUrl, const char *deviceApiKey);
  String url(const String &path) const;
  void addDeviceHeaders(HTTPClient &http) const;

private:
  String normalizedBaseUrl;
  String apiKey;
};

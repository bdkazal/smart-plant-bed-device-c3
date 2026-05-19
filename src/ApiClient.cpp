#include "ApiClient.h"

void ApiClient::begin(const char *baseUrl, const char *deviceApiKey)
{
  normalizedBaseUrl = String(baseUrl);
  apiKey = String(deviceApiKey);

  if (normalizedBaseUrl.endsWith("/"))
  {
    normalizedBaseUrl.remove(normalizedBaseUrl.length() - 1);
  }
}

String ApiClient::url(const String &path) const
{
  return normalizedBaseUrl + path;
}

void ApiClient::addDeviceHeaders(HTTPClient &http) const
{
  http.addHeader("Accept", "application/json");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Connection", "close");
  http.addHeader("X-DEVICE-KEY", apiKey);
}

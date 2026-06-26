#include "RtcClock.h"

#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>
#include <sys/time.h>
#include <time.h>

#include "PinConfig.h"

static const long RTC_UPDATE_DRIFT_THRESHOLD_SECONDS = 5;

RTC_DS3231 rtc;

static bool rtcAvailable = false;
static bool rtcTimeValid = false;
static String rtcStatusText = "RTC not initialized";

bool isReasonableRtcTime(const DateTime &dateTime)
{
  int year = dateTime.year();
  return year >= 2025 && year <= 2099;
}

void printDateTime(const DateTime &dateTime)
{
  Serial.print(dateTime.year());
  Serial.print("-");
  Serial.print(dateTime.month());
  Serial.print("-");
  Serial.print(dateTime.day());
  Serial.print(" ");
  Serial.print(dateTime.hour());
  Serial.print(":");
  Serial.print(dateTime.minute());
  Serial.print(":");
  Serial.println(dateTime.second());
}

void beginRtcClock()
{
  Serial.println();
  Serial.println("Initializing DS3231 RTC...");
  Serial.print("DS3231 SDA GPIO: ");
  Serial.println(OLED_I2C_SDA_PIN);
  Serial.print("DS3231 SCL GPIO: ");
  Serial.println(OLED_I2C_SCL_PIN);

  Wire.begin(OLED_I2C_SDA_PIN, OLED_I2C_SCL_PIN);

  rtcAvailable = rtc.begin();

  if (!rtcAvailable)
  {
    rtcTimeValid = false;
    rtcStatusText = "RTC not found";
    return;
  }

  if (rtc.lostPower())
  {
    rtcTimeValid = false;
    rtcStatusText = "RTC lost power";
    return;
  }

  DateTime now = rtc.now();
  rtcTimeValid = isReasonableRtcTime(now);

  if (!rtcTimeValid)
  {
    rtcStatusText = "RTC invalid";
    return;
  }

  rtcStatusText = "RTC ready";
  Serial.print("DS3231 RTC ready UTC: ");
  printDateTime(now);
}

bool isRtcAvailable()
{
  return rtcAvailable;
}

bool isRtcTimeValid()
{
  return rtcAvailable && rtcTimeValid;
}

bool loadSystemTimeFromRtc()
{
  if (!isRtcTimeValid())
  {
    return false;
  }

  DateTime rtcNow = rtc.now();

  if (!isReasonableRtcTime(rtcNow))
  {
    rtcTimeValid = false;
    rtcStatusText = "RTC invalid";
    return false;
  }

  struct timeval tv;
  tv.tv_sec = rtcNow.unixtime();
  tv.tv_usec = 0;

  if (settimeofday(&tv, nullptr) != 0)
  {
    return false;
  }

  rtcStatusText = "RTC UTC time loaded";
  return true;
}

bool saveSystemTimeToRtc()
{
  if (!rtcAvailable)
  {
    return false;
  }

  time_t nowEpoch;
  time(&nowEpoch);

  if (nowEpoch <= 0)
  {
    return false;
  }

  struct tm utcTime;

  if (!gmtime_r(&nowEpoch, &utcTime))
  {
    return false;
  }

  DateTime systemUtcNow(
      utcTime.tm_year + 1900,
      utcTime.tm_mon + 1,
      utcTime.tm_mday,
      utcTime.tm_hour,
      utcTime.tm_min,
      utcTime.tm_sec);

  if (!isReasonableRtcTime(systemUtcNow))
  {
    return false;
  }

  DateTime rtcNow = rtc.now();

  if (isReasonableRtcTime(rtcNow))
  {
    long driftSeconds = labs((long)systemUtcNow.unixtime() - (long)rtcNow.unixtime());

    if (driftSeconds <= RTC_UPDATE_DRIFT_THRESHOLD_SECONDS)
    {
      rtcTimeValid = true;
      rtcStatusText = "RTC already in sync";
      return false;
    }
  }

  rtc.adjust(systemUtcNow);
  rtcTimeValid = true;
  rtcStatusText = "RTC synced";
  return true;
}

String getRtcStatusText()
{
  return rtcStatusText;
}

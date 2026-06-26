#include "RtcClock.h"

#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>
#include <sys/time.h>
#include <time.h>

static const int RTC_I2C_SDA_PIN = 21;
static const int RTC_I2C_SCL_PIN = 22;
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
  Serial.println(RTC_I2C_SDA_PIN);
  Serial.print("DS3231 SCL GPIO: ");
  Serial.println(RTC_I2C_SCL_PIN);

  Wire.begin(RTC_I2C_SDA_PIN, RTC_I2C_SCL_PIN);

  rtcAvailable = rtc.begin();

  if (!rtcAvailable)
  {
    rtcTimeValid = false;
    rtcStatusText = "RTC not found";
    Serial.println("DS3231 RTC not found on I2C bus.");
    return;
  }

  if (rtc.lostPower())
  {
    rtcTimeValid = false;
    rtcStatusText = "RTC lost power";
    Serial.println("DS3231 RTC found but lost power. RTC time is not trusted yet.");
    return;
  }

  DateTime now = rtc.now();
  rtcTimeValid = isReasonableRtcTime(now);

  if (!rtcTimeValid)
  {
    rtcStatusText = "RTC invalid";
    Serial.print("DS3231 RTC UTC time invalid: ");
    printDateTime(now);
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
    Serial.println("RTC time load skipped: DS3231 is unavailable or invalid.");
    return false;
  }

  DateTime rtcNow = rtc.now();

  if (!isReasonableRtcTime(rtcNow))
  {
    rtcTimeValid = false;
    rtcStatusText = "RTC invalid";
    Serial.println("RTC time load failed: DS3231 time became invalid.");
    return false;
  }

  struct timeval tv;
  tv.tv_sec = rtcNow.unixtime();
  tv.tv_usec = 0;

  if (settimeofday(&tv, nullptr) != 0)
  {
    Serial.println("RTC UTC time load failed: settimeofday failed.");
    return false;
  }

  rtcStatusText = "RTC UTC time loaded";

  Serial.print("System time loaded from DS3231 UTC: ");
  printDateTime(rtcNow);

  return true;
}

bool saveSystemTimeToRtc()
{
  if (!rtcAvailable)
  {
    Serial.println("RTC update skipped: DS3231 is not available.");
    return false;
  }

  time_t nowEpoch;
  time(&nowEpoch);

  if (nowEpoch <= 0)
  {
    Serial.println("RTC update skipped: system time is not ready.");
    return false;
  }

  struct tm utcTime;

  if (!gmtime_r(&nowEpoch, &utcTime))
  {
    Serial.println("RTC update skipped: UTC conversion failed.");
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
    Serial.println("RTC update skipped: UTC system time is not reasonable.");
    return false;
  }

  DateTime rtcNow = rtc.now();

  if (isReasonableRtcTime(rtcNow))
  {
    long driftSeconds = labs((long)systemUtcNow.unixtime() - (long)rtcNow.unixtime());

    Serial.print("DS3231 UTC drift seconds: ");
    Serial.println(driftSeconds);

    if (driftSeconds <= RTC_UPDATE_DRIFT_THRESHOLD_SECONDS)
    {
      rtcTimeValid = true;
      rtcStatusText = "RTC already in sync";
      Serial.println("RTC update skipped: DS3231 already within 5 seconds of system UTC.");
      return false;
    }
  }

  rtc.adjust(systemUtcNow);
  rtcTimeValid = true;
  rtcStatusText = "RTC synced from UTC system time";

  Serial.print("DS3231 updated from UTC system time: ");
  printDateTime(systemUtcNow);

  return true;
}

String getRtcStatusText()
{
  return rtcStatusText;
}

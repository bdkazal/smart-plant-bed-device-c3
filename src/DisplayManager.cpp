#include "DisplayManager.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <math.h>

#include "BootLogo.h"
#include "ScheduleConfig.h"
#include "TimeSync.h"
#include "ValveController.h"

static const int OLED_I2C_SDA_PIN = 8;
static const int OLED_I2C_SCL_PIN = 9;
static const int OLED_I2C_ADDRESS = 0x3C;
static const int OLED_SCREEN_WIDTH = 128;
static const int OLED_SCREEN_HEIGHT = 64;
static const int OLED_RESET_PIN = -1;
static const int DISPLAY_TEXT_COLUMNS = 20;

static const int DISPLAY_WAKE_BUTTON_PIN = 4;
static const int SOIL_CRITICAL_PERCENT = 15;

static const unsigned long OLED_BOOT_SHOW_MS = 12000;
static const unsigned long OLED_STATUS_SHOW_MS = 10000;
static const unsigned long OLED_WAKE_BUTTON_SHOW_MS = 15000;
static const unsigned long OLED_WATERING_SHOW_MS = 10000;
static const unsigned long DISPLAY_BUTTON_DEBOUNCE_MS = 50;

Adafruit_SSD1306 oled(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, &Wire, OLED_RESET_PIN);

extern bool isWifiConnected();
extern bool isServerRecentlyReachable();
extern String configWateringMode;
extern String configTimezone;
extern bool hasSoilMoistureThreshold;
extern int configSoilMoistureThreshold;
extern int configScheduleCount;

bool displayAvailable = false;
bool displayAwake = false;
bool criticalDisplayActive = false;
bool criticalPageDrawn = false;
unsigned long displaySleepAt = 0;
int currentStatusPage = 0;

bool lastDisplayButtonReading = HIGH;
bool stableDisplayButtonState = HIGH;
unsigned long lastDisplayButtonChangeAt = 0;

SensorReading latestDisplayReading;
bool hasLatestDisplayReading = false;

bool isDisplayAvailable()
{
  return displayAvailable;
}

String limitText(String text, int maxLength)
{
  if (text.length() <= maxLength)
  {
    return text;
  }

  return text.substring(0, maxLength);
}

String centerText(String text)
{
  text = limitText(text, DISPLAY_TEXT_COLUMNS);
  int totalPadding = DISPLAY_TEXT_COLUMNS - text.length();
  int leftPadding = totalPadding / 2;

  String result = "";
  for (int i = 0; i < leftPadding; i++)
  {
    result += " ";
  }
  result += text;

  return result;
}

String leftRightText(String left, String right)
{
  left = limitText(left, DISPLAY_TEXT_COLUMNS);
  right = limitText(right, DISPLAY_TEXT_COLUMNS);

  int spaces = DISPLAY_TEXT_COLUMNS - left.length() - right.length();
  if (spaces < 1)
  {
    spaces = 1;
  }

  String result = left;
  for (int i = 0; i < spaces; i++)
  {
    result += " ";
  }
  result += right;

  return limitText(result, DISPLAY_TEXT_COLUMNS);
}

void clearAndPrepareText()
{
  oled.clearDisplay();
  oled.drawRect(0, 0, OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);
}

void printDisplayRow(int row, const String &text)
{
  oled.setCursor(4, row * 16 + 4);
  oled.print(limitText(text, DISPLAY_TEXT_COLUMNS));
}

void wakeDisplay(unsigned long visibleMs)
{
  if (!displayAvailable)
  {
    return;
  }

  oled.ssd1306_command(SSD1306_DISPLAYON);
  displayAwake = true;

  if (visibleMs > 0)
  {
    displaySleepAt = millis() + visibleMs;
  }
  else
  {
    displaySleepAt = 0;
  }
}

void sleepDisplay()
{
  if (!displayAvailable || !displayAwake)
  {
    return;
  }

  oled.clearDisplay();
  oled.display();
  oled.ssd1306_command(SSD1306_DISPLAYOFF);
  displayAwake = false;
  displaySleepAt = 0;
}

void resetCriticalDisplay()
{
  criticalDisplayActive = false;
  criticalPageDrawn = false;
}

String modeText()
{
  if (configWateringMode == "schedule")
  {
    return "SCHEDULE";
  }

  if (configWateringMode == "auto")
  {
    return "AUTO";
  }

  if (configWateringMode.length() == 0)
  {
    return "--";
  }

  return configWateringMode;
}

String scheduleStateText()
{
  if (configWateringMode != "schedule")
  {
    return "OFF";
  }

  return isWateringActive() ? "ACTIVE" : "IDLE";
}

String wateringStateText()
{
  return isWateringActive() ? "Watering" : "IDLE";
}

String soilValueText()
{
  if (!hasLatestDisplayReading || !latestDisplayReading.hasSoilMoisture)
  {
    return "SOIL --";
  }

  return "SOIL " + String(latestDisplayReading.soilMoisturePercent) + "%";
}

String soilStatusValueText()
{
  if (!hasLatestDisplayReading || !latestDisplayReading.hasSoilMoisture)
  {
    return "N/A";
  }

  int soilPercent = latestDisplayReading.soilMoisturePercent;
  int threshold = hasSoilMoistureThreshold && configSoilMoistureThreshold > 0
                      ? configSoilMoistureThreshold
                      : 35;

  if (soilPercent <= 15)
  {
    return "V.DRY";
  }

  if (soilPercent <= 30)
  {
    return "DRY";
  }

  if (soilPercent <= threshold)
  {
    return "LOW";
  }

  if (soilPercent <= 75)
  {
    return "OK";
  }

  if (soilPercent <= 90)
  {
    return "WET";
  }

  return "V.WET";
}

String temperatureText()
{
  if (hasLatestDisplayReading && latestDisplayReading.hasTemperature)
  {
    return "Temp " + String((int)round(latestDisplayReading.temperatureC)) + "C";
  }

  return "Temp --C";
}

String humidityText()
{
  if (hasLatestDisplayReading && latestDisplayReading.hasHumidity)
  {
    return "Hum " + String((int)round(latestDisplayReading.humidityPercent)) + "%";
  }

  return "Hum --%";
}

String statusTitleText()
{
  if (!isWifiConnected())
  {
    return "WIFI OFFLINE";
  }

  if (!isServerRecentlyReachable())
  {
    return "Offline Mode";
  }

  return "|| Plant Buddy ||";
}

String currentTimeShortText()
{
  String localTime = getCurrentTimeString();
  if (localTime.length() >= 5)
  {
    return localTime.substring(0, 5);
  }

  return "--:--";
}

String scheduleTimeShortText(const WateringScheduleConfig &schedule)
{
  if (schedule.timeOfDay.length() >= 5)
  {
    return schedule.timeOfDay.substring(0, 5);
  }

  return "--:--";
}

int minutesFromTimeText(const String &timeText)
{
  if (timeText.length() < 5)
  {
    return -1;
  }

  int hour = timeText.substring(0, 2).toInt();
  int minute = timeText.substring(3, 5).toInt();

  if (hour < 0 || hour > 23 || minute < 0 || minute > 59)
  {
    return -1;
  }

  return hour * 60 + minute;
}

String nextScheduleTimeText()
{
  int scheduleCount = getScheduleConfigCount();

  if (scheduleCount <= 0)
  {
    return "--:--";
  }

  int currentDay = getCurrentDayOfWeekIso();
  int currentMinute = minutesFromTimeText(getCurrentTimeString());

  if (currentDay == 0 || currentMinute < 0)
  {
    return "--:--";
  }

  int bestDistance = 8 * 24 * 60;
  String bestTime = "--:--";

  for (int i = 0; i < scheduleCount; i++)
  {
    WateringScheduleConfig schedule = getScheduleConfigAt(i);

    if (!schedule.isEnabled || schedule.timeOfDay.length() < 5 || schedule.dayOfWeek < 1 || schedule.dayOfWeek > 7)
    {
      continue;
    }

    int scheduleMinute = minutesFromTimeText(schedule.timeOfDay);

    if (scheduleMinute < 0)
    {
      continue;
    }

    int dayDistance = schedule.dayOfWeek - currentDay;
    if (dayDistance < 0)
    {
      dayDistance += 7;
    }

    int totalDistance = dayDistance * 24 * 60 + scheduleMinute - currentMinute;

    if (totalDistance < 0)
    {
      totalDistance += 7 * 24 * 60;
    }

    if (totalDistance < bestDistance)
    {
      bestDistance = totalDistance;
      bestTime = scheduleTimeShortText(schedule);
    }
  }

  return bestTime;
}

void drawBootLogoBitmap()
{
  oled.clearDisplay();

  int x = (OLED_SCREEN_WIDTH - BOOT_LOGO_WIDTH) / 2;
  int y = (OLED_SCREEN_HEIGHT - BOOT_LOGO_HEIGHT) / 2;

  oled.drawBitmap(
      x,
      y,
      bootLogoBitmap,
      BOOT_LOGO_WIDTH,
      BOOT_LOGO_HEIGHT,
      SSD1306_WHITE);

  oled.display();
}

void beginDisplayManager()
{
  Wire.begin(OLED_I2C_SDA_PIN, OLED_I2C_SCL_PIN);

  pinMode(DISPLAY_WAKE_BUTTON_PIN, INPUT_PULLUP);
  lastDisplayButtonReading = digitalRead(DISPLAY_WAKE_BUTTON_PIN);
  stableDisplayButtonState = lastDisplayButtonReading;
  lastDisplayButtonChangeAt = millis();

  displayAvailable = oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS);

  Serial.println();
  Serial.println("OLED display manager initialized.");
  Serial.print("OLED available: ");
  Serial.println(displayAvailable ? "yes" : "no");
  Serial.print("OLED I2C address: 0x");
  Serial.println(OLED_I2C_ADDRESS, HEX);
  Serial.print("OLED SDA GPIO: ");
  Serial.println(OLED_I2C_SDA_PIN);
  Serial.print("OLED SCL GPIO: ");
  Serial.println(OLED_I2C_SCL_PIN);
  Serial.print("OLED wake/next button GPIO: ");
  Serial.println(DISPLAY_WAKE_BUTTON_PIN);
  Serial.println("OLED button mode: INPUT_PULLUP, press connects GPIO to GND");

  if (!displayAvailable)
  {
    return;
  }

  drawBootLogoBitmap();
  wakeDisplay(OLED_BOOT_SHOW_MS);
}

void displayShowBootLogo(unsigned long visibleMs)
{
  if (!displayAvailable)
  {
    return;
  }

  resetCriticalDisplay();
  wakeDisplay(visibleMs);
  drawBootLogoBitmap();
}

void displayShowBootStatus(const String &line1, const String &line2, const String &line3)
{
  if (!displayAvailable)
  {
    return;
  }

  resetCriticalDisplay();
  wakeDisplay(OLED_BOOT_SHOW_MS);

  clearAndPrepareText();
  printDisplayRow(0, centerText("BOOTING"));
  printDisplayRow(1, centerText(line1));
  printDisplayRow(2, centerText(line2));
  printDisplayRow(3, centerText(line3));
  oled.display();
}

void displaySetLatestSensorReading(const SensorReading &reading)
{
  latestDisplayReading = reading;
  hasLatestDisplayReading = true;
}

void displayShowCurrentStatus(unsigned long visibleMs)
{
  if (!displayAvailable)
  {
    return;
  }

  currentStatusPage = 0;
  resetCriticalDisplay();
  wakeDisplay(visibleMs);

  clearAndPrepareText();
  printDisplayRow(0, centerText(statusTitleText()));
  printDisplayRow(1, leftRightText(modeText(), wateringStateText()));
  printDisplayRow(2, leftRightText(soilValueText(), soilStatusValueText()));
  printDisplayRow(3, leftRightText(temperatureText(), humidityText()));
  oled.display();
}

void displayShowScheduleStatus(unsigned long visibleMs)
{
  if (!displayAvailable)
  {
    return;
  }

  currentStatusPage = 1;
  resetCriticalDisplay();
  wakeDisplay(visibleMs);

  clearAndPrepareText();
  printDisplayRow(0, centerText("Schedule Status"));
  printDisplayRow(1, leftRightText("Schedule", scheduleStateText()));
  printDisplayRow(2, leftRightText("Next", nextScheduleTimeText()));
  printDisplayRow(3, leftRightText("Time", currentTimeShortText()));
  oled.display();
}

void displayShowNextStatusPage(unsigned long visibleMs)
{
  if (currentStatusPage == 0)
  {
    displayShowScheduleStatus(visibleMs);
    return;
  }

  displayShowCurrentStatus(visibleMs);
}

void displayShowWateringStatus(unsigned long visibleMs)
{
  if (!displayAvailable)
  {
    return;
  }

  resetCriticalDisplay();
  wakeDisplay(visibleMs);

  clearAndPrepareText();
  printDisplayRow(0, centerText("WATERING"));
  printDisplayRow(1, leftRightText(modeText(), wateringStateText()));
  printDisplayRow(2, leftRightText(soilValueText(), soilStatusValueText()));
  printDisplayRow(3, leftRightText("TIME", String(getWateringDurationSeconds()) + " sec"));
  oled.display();
}

void displayShowWateringDone(unsigned long visibleMs)
{
  if (!displayAvailable)
  {
    return;
  }

  resetCriticalDisplay();
  wakeDisplay(visibleMs);

  clearAndPrepareText();
  printDisplayRow(0, centerText("Watering DONE"));
  printDisplayRow(1, leftRightText("VALVE", "CLOSED"));
  printDisplayRow(2, leftRightText(soilValueText(), soilStatusValueText()));
  printDisplayRow(3, centerText("Returning idle"));
  oled.display();
}

void displayShowCriticalIfNeeded()
{
  if (!displayAvailable || !hasLatestDisplayReading)
  {
    return;
  }

  bool criticalDry = latestDisplayReading.hasSoilMoisture && latestDisplayReading.soilMoisturePercent <= SOIL_CRITICAL_PERCENT;

  if (!criticalDry)
  {
    if (criticalDisplayActive)
    {
      resetCriticalDisplay();
      displayShowCurrentStatus(OLED_STATUS_SHOW_MS);
    }
    return;
  }

  criticalDisplayActive = true;
  wakeDisplay(0);

  if (criticalPageDrawn)
  {
    return;
  }

  clearAndPrepareText();
  printDisplayRow(0, centerText("* VERY DRY *"));
  printDisplayRow(1, leftRightText(soilValueText(), soilStatusValueText()));
  printDisplayRow(2, leftRightText("LIMIT", String(SOIL_CRITICAL_PERCENT) + "%"));
  printDisplayRow(3, leftRightText(modeText(), wateringStateText()));
  oled.display();

  criticalPageDrawn = true;
}

void handleDisplayButton()
{
  bool currentReading = digitalRead(DISPLAY_WAKE_BUTTON_PIN);
  unsigned long now = millis();

  if (currentReading != lastDisplayButtonReading)
  {
    lastDisplayButtonChangeAt = now;
    lastDisplayButtonReading = currentReading;
  }

  if (now - lastDisplayButtonChangeAt < DISPLAY_BUTTON_DEBOUNCE_MS)
  {
    return;
  }

  if (currentReading == stableDisplayButtonState)
  {
    return;
  }

  stableDisplayButtonState = currentReading;

  if (stableDisplayButtonState == LOW)
  {
    Serial.println("OLED wake/next button pressed.");

    if (criticalDisplayActive)
    {
      resetCriticalDisplay();
      displayShowNextStatusPage(OLED_WAKE_BUTTON_SHOW_MS);
      return;
    }

    displayShowNextStatusPage(OLED_WAKE_BUTTON_SHOW_MS);
  }
}

void updateDisplayManager()
{
  if (!displayAvailable)
  {
    return;
  }

  handleDisplayButton();

  if (displayAwake && displaySleepAt > 0 && millis() >= displaySleepAt)
  {
    sleepDisplay();
  }
}

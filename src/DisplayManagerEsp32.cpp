#include "DisplayManager.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#include "BootLogo.h"
#include "TimeSync.h"
#include "ValveController.h"

static const int OLED_I2C_SDA_PIN = 21;
static const int OLED_I2C_SCL_PIN = 22;
static const int OLED_I2C_ADDRESS = 0x3C;
static const int OLED_SCREEN_WIDTH = 128;
static const int OLED_SCREEN_HEIGHT = 64;
static const int OLED_RESET_PIN = -1;
static const int DISPLAY_WAKE_BUTTON_PIN = 33;
static const int SOIL_CRITICAL_PERCENT = 15;
static const unsigned long OLED_BOOT_LOGO_SHOW_MS = 2500;
static const unsigned long OLED_BOOT_SHOW_MS = 12000;
static const unsigned long OLED_STATUS_SHOW_MS = 10000;
static const unsigned long OLED_WAKE_BUTTON_SHOW_MS = 30000;
static const unsigned long DISPLAY_BUTTON_DEBOUNCE_MS = 50;

Adafruit_SSD1306 oled(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, &Wire, OLED_RESET_PIN);

extern bool isWifiConnected();
extern bool isServerRecentlyReachable();
extern String configWateringMode;

bool displayInitialized = false;
bool displayAvailable = false;
bool displayAwake = false;
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
  return text.length() <= maxLength ? text : text.substring(0, maxLength);
}

void drawRows(const String &row0, const String &row1, const String &row2, const String &row3)
{
  oled.clearDisplay();
  oled.drawRect(0, 0, OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(4, 4);
  oled.print(limitText(row0, 20));
  oled.setCursor(4, 20);
  oled.print(limitText(row1, 20));
  oled.setCursor(4, 36);
  oled.print(limitText(row2, 20));
  oled.setCursor(4, 52);
  oled.print(limitText(row3, 20));
  oled.display();
}

void wakeDisplay(unsigned long visibleMs)
{
  if (!displayAvailable)
  {
    return;
  }

  oled.ssd1306_command(SSD1306_DISPLAYON);
  displayAwake = true;
  displaySleepAt = visibleMs > 0 ? millis() + visibleMs : 0;
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

void drawBootLogoBitmap()
{
  oled.clearDisplay();
  int x = (OLED_SCREEN_WIDTH - BOOT_LOGO_WIDTH) / 2;
  int y = (OLED_SCREEN_HEIGHT - BOOT_LOGO_HEIGHT) / 2;
  oled.drawBitmap(x, y, bootLogoBitmap, BOOT_LOGO_WIDTH, BOOT_LOGO_HEIGHT, SSD1306_WHITE);
  oled.display();
}

String soilValueText()
{
  if (!hasLatestDisplayReading || !latestDisplayReading.hasSoilMoisture)
  {
    return "SOIL --";
  }

  return "SOIL " + String(latestDisplayReading.soilMoisturePercent) + "%";
}

String networkText()
{
  if (!isWifiConnected())
  {
    return "WiFi offline";
  }

  return isServerRecentlyReachable() ? "Laravel online" : "Laravel offline";
}

void beginDisplayManager()
{
  if (displayInitialized)
  {
    return;
  }

  displayInitialized = true;
  Wire.begin(OLED_I2C_SDA_PIN, OLED_I2C_SCL_PIN);
  pinMode(DISPLAY_WAKE_BUTTON_PIN, INPUT_PULLUP);

  lastDisplayButtonReading = digitalRead(DISPLAY_WAKE_BUTTON_PIN);
  stableDisplayButtonState = lastDisplayButtonReading;
  lastDisplayButtonChangeAt = millis();

  displayAvailable = oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS);

  Serial.println();
  Serial.println("OLED display manager initialized for ESP32 DevKit.");
  Serial.print("OLED SDA GPIO: ");
  Serial.println(OLED_I2C_SDA_PIN);
  Serial.print("OLED SCL GPIO: ");
  Serial.println(OLED_I2C_SCL_PIN);
  Serial.print("OLED button GPIO: ");
  Serial.println(DISPLAY_WAKE_BUTTON_PIN);

  if (!displayAvailable)
  {
    return;
  }

  drawBootLogoBitmap();
  wakeDisplay(OLED_BOOT_LOGO_SHOW_MS);
  delay(OLED_BOOT_LOGO_SHOW_MS);
  displayShowBootStatus("ESP32 DevKit", "Loading config", "Please wait");
}

void displayShowBootLogo(unsigned long visibleMs)
{
  if (!displayAvailable)
  {
    return;
  }

  wakeDisplay(visibleMs);
  drawBootLogoBitmap();
}

void displayShowBootStatus(const String &line1, const String &line2, const String &line3)
{
  if (!displayAvailable)
  {
    return;
  }

  wakeDisplay(OLED_BOOT_SHOW_MS);
  drawRows("BOOTING", line1, line2, line3);
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
  wakeDisplay(visibleMs);
  drawRows("Plant Buddy", networkText(), soilValueText(), isWateringActive() ? "Watering" : "Idle");
}

void displayShowScheduleStatus(unsigned long visibleMs)
{
  if (!displayAvailable)
  {
    return;
  }

  currentStatusPage = 1;
  wakeDisplay(visibleMs);
  drawRows("Schedule", "Mode " + configWateringMode, "Time " + getCurrentTimeString(), networkText());
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

  wakeDisplay(visibleMs);
  drawRows("WATERING", soilValueText(), "Duration " + String(getWateringDurationSeconds()) + "s", networkText());
}

void displayShowWateringDone(unsigned long visibleMs)
{
  if (!displayAvailable)
  {
    return;
  }

  wakeDisplay(visibleMs);
  drawRows("Watering DONE", "GPIO26 OFF", soilValueText(), "Returning idle");
}

void displayShowCriticalIfNeeded()
{
  if (!displayAvailable || !hasLatestDisplayReading || !latestDisplayReading.hasSoilMoisture)
  {
    return;
  }

  if (latestDisplayReading.soilMoisturePercent > SOIL_CRITICAL_PERCENT)
  {
    return;
  }

  wakeDisplay(0);
  drawRows("VERY DRY", soilValueText(), "Limit " + String(SOIL_CRITICAL_PERCENT) + "%", networkText());
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

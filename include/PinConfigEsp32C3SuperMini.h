#pragma once

#include <Arduino.h>

static const char BOARD_PROFILE_NAME[] = "ESP32-C3 Super Mini";

static const int VALVE_CONTROL_PIN = 5;
static const bool VALVE_ACTIVE_LOW = false;

static const int WIFI_STATUS_LED_PIN = 6;
static const bool WIFI_STATUS_LED_ACTIVE_LOW = false;

// -1 means disabled. On C3 the watering LED physically mirrors the valve GPIO.
static const int WATERING_STATUS_LED_PIN = -1;
static const bool WATERING_STATUS_LED_ACTIVE_LOW = false;

static const int WIFI_RESET_BUTTON_PIN = 7;

static const int SOIL_MOISTURE_PIN = 1;
static const int SOIL_DISCONNECTED_RAW_MAX = 800;
static const int SOIL_WET_RAW = 1750;
static const int SOIL_DRY_RAW = 2365;
static const int SOIL_CRITICAL_PERCENT = 15;

static const int MANUAL_WATER_BUTTON_PIN = 3;
static const int DISPLAY_WAKE_BUTTON_PIN = 4;

static const int DHT_SENSOR_PIN = 2;
static const int DHT_TYPE = DHT11;

static const int OLED_I2C_SDA_PIN = 8;
static const int OLED_I2C_SCL_PIN = 9;
static const int OLED_I2C_ADDRESS = 0x3C;
static const int OLED_SCREEN_WIDTH = 128;
static const int OLED_SCREEN_HEIGHT = 64;
static const int OLED_RESET_PIN = -1;

static const unsigned long OLED_BOOT_LOGO_SHOW_MS = 2500;
static const unsigned long OLED_BOOT_SHOW_MS = 12000;
static const unsigned long OLED_STATUS_SHOW_MS = 10000;
static const unsigned long OLED_WAKE_BUTTON_SHOW_MS = 15000;

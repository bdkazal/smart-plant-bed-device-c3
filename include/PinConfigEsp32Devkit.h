#pragma once

#include <Arduino.h>

static const char BOARD_PROFILE_NAME[] = "ESP32 DevKit";

static const int VALVE_CONTROL_PIN = 26;
static const bool VALVE_ACTIVE_LOW = false;

static const int WIFI_STATUS_LED_PIN = 27;
static const bool WIFI_STATUS_LED_ACTIVE_LOW = false;

static const int WATERING_STATUS_LED_PIN = 14;
static const bool WATERING_STATUS_LED_ACTIVE_LOW = false;

static const int WIFI_RESET_BUTTON_PIN = 0;

static const int SOIL_MOISTURE_PIN = 34;
static const int SOIL_DISCONNECTED_RAW_MAX = 800;
static const int SOIL_WET_RAW = 1000;
static const int SOIL_DRY_RAW = 1550;
static const int SOIL_CRITICAL_PERCENT = 15;

static const int MANUAL_WATER_BUTTON_PIN = 25;
static const int DISPLAY_WAKE_BUTTON_PIN = 33;

static const int DHT_SENSOR_PIN = 32;

static const int OLED_I2C_SDA_PIN = 21;
static const int OLED_I2C_SCL_PIN = 22;
static const int OLED_I2C_ADDRESS = 0x3C;
static const int OLED_SCREEN_WIDTH = 128;
static const int OLED_SCREEN_HEIGHT = 64;
static const int OLED_RESET_PIN = -1;

static const unsigned long OLED_BOOT_LOGO_SHOW_MS = 2500;
static const unsigned long OLED_BOOT_SHOW_MS = 12000;
static const unsigned long OLED_STATUS_SHOW_MS = 10000;
static const unsigned long OLED_WAKE_BUTTON_SHOW_MS = 30000;

#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDR     0x3C

class DisplayManager {
private:
    Adafruit_SSD1306 display;
    bool initialized;

public:
    // Constructor binds the display to the existing hardware I2C Wire
    DisplayManager();

    // Boots the display and shows the startup screen
    bool init();

    // Renders the telemetry frame cleanly
    void update(float wattage, float temp, float hum, float pres);
};

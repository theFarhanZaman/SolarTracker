#include "DisplayManager.h"

// Initialize the Adafruit object in the constructor initialization list
DisplayManager::DisplayManager() : display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET), initialized(false) {}

bool DisplayManager::init() {
    // SSD1306_SWITCHCAPVCC generates display voltage from the 3.3v internally
    if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        return false;
    }
    
    initialized = true;
    
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println(F("SYSTEM BOOTING..."));
    display.display();
    
    return true;
}

void DisplayManager::update(float wattage, float temp, float hum, float pres) {
    if (!initialized) return; // Fail-safe: don't try to draw if I2C binding failed

    display.clearDisplay();
    
    // Line 0: Fixed System Header
    display.setCursor(0, 0);
    display.println(F("=== EDGE NODE SYS ==="));
    
    // Line 2: Calculated Wattage Generation
    display.setCursor(0, 16);
    display.print(F("POWER:  ")); 
    display.print(wattage, 3); 
    display.println(F(" W"));
    
    // Line 4: Temperature & Relative Humidity split horizontally
    display.setCursor(0, 32);
    display.print(F("TEMP: ")); 
    display.print(temp, 1); 
    display.print(F("C"));
    
    display.setCursor(70, 32);
    display.print(F("RH: ")); 
    display.print(hum, 0); 
    display.println(F("%"));
    
    // Line 6: Atmospheric Pressure
    display.setCursor(0, 48);
    display.print(F("PRESS:  ")); 
    display.print(pres, 1); 
    display.println(F(" hPa"));
    
    // Push local RAM buffer to physical SSD1306 controller
    display.display();
}

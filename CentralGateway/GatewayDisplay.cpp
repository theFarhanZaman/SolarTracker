#include "GatewayDisplay.h"

GatewayDisplayManager::GatewayDisplayManager() 
    : display(GATEWAY_SCREEN_WIDTH, GATEWAY_SCREEN_HEIGHT, &Wire, GATEWAY_OLED_RESET), initialized(false) {}

bool GatewayDisplayManager::init() {
    if(!display.begin(SSD1306_SWITCHCAPVCC, GATEWAY_OLED_ADDR)) {
        return false;
    }
    
    initialized = true;
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    
    display.setCursor(0, 20);
    display.println(F("GATEWAY HUB BOOTING"));
    display.display();
    
    return true;
}

void GatewayDisplayManager::update(bool isConnected, uint8_t nodeCount, uint32_t packetsSent) {
    if (!initialized) return;

    display.clearDisplay();
    
    // Header
    display.setCursor(0, 0);
    display.println(F("=== SWARM GATEWAY ==="));
    
    // Comm Protocol
    display.setCursor(0, 16);
    display.println(F("PROTOCOL: ESP-NOW"));
    
    // Network Status
    display.setCursor(0, 32);
    display.print(F("STATUS: "));
    if (isConnected) {
        // Highlight active connections safely without CSS/colors
        display.println(F("[ CONNECTED ]"));
    } else {
        display.println(F("[ WAITING ]"));
    }
    
    // Node Count & Packets
    display.setCursor(0, 48);
    display.print(F("NODES: "));
    display.print(nodeCount);
    
    display.setCursor(64, 48);
    display.print(F("TX: "));
    display.print(packetsSent);
    
    display.display();
}

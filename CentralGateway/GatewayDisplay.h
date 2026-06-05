#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define GATEWAY_SCREEN_WIDTH 128
#define GATEWAY_SCREEN_HEIGHT 64
#define GATEWAY_OLED_RESET    -1
#define GATEWAY_OLED_ADDR     0x3C

class GatewayDisplayManager {
private:
    Adafruit_SSD1306 display;
    bool initialized;

public:
    GatewayDisplayManager();

    bool init();

    /**
     * @brief Refreshes the gateway command matrix UI
     * @param isConnected True if at least one node is actively sending telemetry
     * @param nodeCount Current number of active edge nodes in the swarm
     * @param packetsSent Total number of ML Override command packets broadcasted
     */
    void update(bool isConnected, uint8_t nodeCount, uint32_t packetsSent);
};

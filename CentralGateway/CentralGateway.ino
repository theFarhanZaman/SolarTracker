#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Wire.h>
#include "NetworkProtocol.h"
#include "GatewayDisplay.h" // <-- NEW: Include Display Module

// --- Gateway Display & Tracking State ---
#define I2C_SDA_PIN 21 // Standard ESP32 I2C SDA
#define I2C_SCL_PIN 22 // Standard ESP32 I2C SCL

GatewayDisplayManager gatewayUI;
volatile uint32_t lastSeenMatrix[256] = {0}; // Tracks the last millis() epoch each sourceID was seen

uint32_t gatewaySeqCount = 0;
uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Fixed Signatures for ESP32 Core v3.x compatibility [cite: 45]
void OnDataRecv(const esp_now_recv_info *recvInfo, const uint8_t *incomingData, int len);
void OnDataSent(const wifi_tx_info_t *txInfo, esp_now_send_status_t status); [cite: 46]

void setup() {
    Serial.begin(921600);
    
    // Initialize I2C and OLED
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    if (!gatewayUI.init()) {
        Serial.println("STATUS,WARN,OLED_INIT_FAILED");
    }

    // Defensive Field Timeout: Prevents permanent locking if booting headless in production [cite: 47]
    uint32_t startTime = millis();
    while (!Serial) {
        if (millis() - startTime > 4000) break; [cite: 48]
        delay(10);
    }

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("STATUS,ERROR,ESP_NOW_INIT_FAILED");
        while (1) { delay(1000); } [cite: 50]
    }

    esp_now_register_recv_cb(OnDataRecv);
    esp_now_register_send_cb(OnDataSent);

    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, broadcastAddress, 6); [cite: 51]
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("STATUS,ERROR,PEER_ADD_FAILED");
    } else {
        Serial.println("STATUS,READY,GATEWAY_ACTIVE");
    } [cite: 52]
}

void loop() {
    // --- 1. NON-BLOCKING DISPLAY REFRESH (1Hz) ---
    static uint32_t lastDisplayUpdate = 0;
    uint32_t currentMillis = millis();
    
    if (currentMillis - lastDisplayUpdate > 1000) {
        uint8_t activeNodes = 0;
        
        // Sweep the matrix: Any node heard from in the last 15 seconds is "Online"
        for (int i = 0; i < 256; i++) {
            if (lastSeenMatrix[i] > 0 && (currentMillis - lastSeenMatrix[i] < 15000)) {
                activeNodes++;
            }
        }
        
        bool isConnected = (activeNodes > 0);
        gatewayUI.update(isConnected, activeNodes, gatewaySeqCount);
        lastDisplayUpdate = currentMillis;
    }

    // --- 2. COMMAND INGESTION FROM REACT DASHBOARD ---
    if (Serial.available() > 0) {
        String csvLine = Serial.readStringUntil('\n'); [cite: 53]
        csvLine.trim(); [cite: 54]

        if (csvLine.startsWith("CMD")) {
            int index1 = csvLine.indexOf(',');
            int index2 = csvLine.indexOf(',', index1 + 1); [cite: 55]
            int index3 = csvLine.indexOf(',', index2 + 1);
            int index4 = csvLine.indexOf(',', index3 + 1); [cite: 56]

            if (index1 != -1 && index2 != -1 && index3 != -1 && index4 != -1) {
                uint8_t targetID = (uint8_t)csvLine.substring(index1 + 1, index2).toInt();
                uint8_t mode     = (uint8_t)csvLine.substring(index2 + 1, index3).toInt(); [cite: 57]
                int16_t biasPan  = (int16_t)csvLine.substring(index3 + 1, index4).toInt();
                int16_t biasTilt = (int16_t)csvLine.substring(index4 + 1).toInt(); [cite: 58]

                NetworkPacket outboundPacket;
                outboundPacket.header.sourceID = 0x00; // 0x00 denotes Central Gateway Base Station Hub [cite: 59]
                outboundPacket.header.packetType = PACKET_ML_OVERRIDE;
                outboundPacket.header.sequenceNumber = gatewaySeqCount++; [cite: 60]

                // Pack Routed Addressing Footprint Variables
                outboundPacket.payload.overrideCmd.targetID       = targetID;
                outboundPacket.payload.overrideCmd.structuralMode = mode; [cite: 61]
                outboundPacket.payload.overrideCmd.appliedBiasPan = biasPan;
                outboundPacket.payload.overrideCmd.appliedBiasTilt = biasTilt;

                esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*)&outboundPacket, sizeof(NetworkPacket));
                if (result == ESP_OK) {
                    Serial.printf("STATUS,TX_OK,%02X\n", targetID); [cite: 62]
                } else {
                    Serial.printf("STATUS,TX_FAIL,%02X\n", targetID); [cite: 63]
                } [cite: 64]
            }
        }
    }
}

void OnDataRecv(const esp_now_recv_info *recvInfo, const uint8_t *incomingData, int len) {
    if (len == sizeof(NetworkPacket)) {
        NetworkPacket packet;
        memcpy(&packet, incomingData, sizeof(NetworkPacket)); [cite: 65]
        
        // Log the heartbeat for the active connection matrix
        lastSeenMatrix[packet.header.sourceID] = millis();

        if (packet.header.packetType == PACKET_TELEMETRY) {
            Serial.printf("DATA,%02X,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%d\n",
                          packet.header.sourceID,
                          packet.payload.telemetry.busVoltage,
                          packet.payload.telemetry.currentmA, [cite: 65]
                          packet.payload.telemetry.temperature,
                          packet.payload.telemetry.humidity,
                          packet.payload.telemetry.pressure, [cite: 66]
                          packet.payload.telemetry.panAngle,
                          packet.payload.telemetry.tiltAngle); [cite: 67]
        } [cite: 68]
    }
}

// Fixed function definition signature matching esp_now_send_cb_t under IDF 5.1/Core v3
void OnDataSent(const wifi_tx_info_t *txInfo, esp_now_send_status_t status) {
    // If you ever need to inspect metadata in the future:
    // txInfo->id contains the target MAC address properties
}

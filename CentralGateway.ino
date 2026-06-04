#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "NetworkProtocol.h"

uint32_t gatewaySeqCount = 0;
uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void OnDataRecv(const esp_now_recv_info *recvInfo, const uint8_t *incomingData, int len);
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);

void setup() {
    Serial.begin(921600);
    
    // Defensive Field Timeout: Prevents permanent locking if booting headless in production
    uint32_t startTime = millis();
    while (!Serial) { 
        if (millis() - startTime > 4000) break; 
        delay(10); 
    }

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("STATUS,ERROR,ESP_NOW_INIT_FAILED");
        while (1) { delay(1000); }
    }

    esp_now_register_recv_cb(OnDataRecv);
    esp_now_register_send_cb(OnDataSent);

    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("STATUS,ERROR,PEER_ADD_FAILED");
    } else {
        Serial.println("STATUS,READY,GATEWAY_ACTIVE");
    }
}

void loop() {
    if (Serial.available() > 0) {
        String csvLine = Serial.readStringUntil('\n');
        csvLine.trim();

        if (csvLine.startsWith("CMD")) {
            int index1 = csvLine.indexOf(',');
            int index2 = csvLine.indexOf(',', index1 + 1);
            int index3 = csvLine.indexOf(',', index2 + 1);
            int index4 = csvLine.indexOf(',', index3 + 1);

            if (index1 != -1 && index2 != -1 && index3 != -1 && index4 != -1) {
                uint8_t targetID = (uint8_t)csvLine.substring(index1 + 1, index2).toInt();
                uint8_t mode     = (uint8_t)csvLine.substring(index2 + 1, index3).toInt();
                int16_t biasPan  = (int16_t)csvLine.substring(index3 + 1, index4).toInt();
                int16_t biasTilt = (int16_t)csvLine.substring(index4 + 1).toInt();

                NetworkPacket outboundPacket;
                outboundPacket.header.sourceID = 0x00; // 0x00 denotes Central Gateway Base Station Hub
                outboundPacket.header.packetType = PACKET_ML_OVERRIDE;
                outboundPacket.header.sequenceNumber = gatewaySeqCount++;
                
                // Pack Routed Addressing Footprint Variables
                outboundPacket.payload.overrideCmd.targetID       = targetID; // Handled safely now
                outboundPacket.payload.overrideCmd.structuralMode = mode;
                outboundPacket.payload.overrideCmd.appliedBiasPan = biasPan;
                outboundPacket.payload.overrideCmd.appliedBiasTilt = biasTilt;

                esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*)&outboundPacket, sizeof(NetworkPacket));
                if (result == ESP_OK) {
                    Serial.printf("STATUS,TX_OK,%02X\n", targetID);
                } else {
                    Serial.printf("STATUS,TX_FAIL,%02X\n", targetID);
                }
            }
        }
    }
}

void OnDataRecv(const esp_now_recv_info *recvInfo, const uint8_t *incomingData, int len) {
    if (len == sizeof(NetworkPacket)) {
        NetworkPacket packet;
        memcpy(&packet, incomingData, sizeof(NetworkPacket));

        if (packet.header.packetType == PACKET_TELEMETRY) {
            Serial.printf("DATA,%02X,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%d\n",
                          packet.header.sourceID,
                          packet.payload.telemetry.busVoltage,
                          packet.payload.telemetry.currentmA,
                          packet.payload.telemetry.temperature,
                          packet.payload.telemetry.humidity,
                          packet.payload.telemetry.pressure,
                          packet.payload.telemetry.panAngle,
                          packet.payload.telemetry.tiltAngle);
        }
    }
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {}

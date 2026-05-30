#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "NetworkProtocol.h"

uint32_t gatewaySeqCount = 0;
uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Forward declaration of callbacks
void OnDataRecv(const esp_now_recv_info *recvInfo, const uint8_t *incomingData, int len);
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);

void setup() {
    // Open high-speed Serial pipeline to match Python throughput capabilities
    Serial.begin(921600);
    while (!Serial) { delay(10); }

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("STATUS,ERROR,ESP_NOW_INIT_FAILED");
        while (1) { delay(1000); }
    }

    // Register networking handlers
    esp_now_register_recv_cb(OnDataRecv);
    esp_now_register_send_cb(OnDataSent);

    // Register universal broadcast token inside tracking registers
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
    // Read incoming commands from Python via Serial Link
    if (Serial.available() > 0) {
        String csvLine = Serial.readStringUntil('\n');
        csvLine.trim();

        if (csvLine.startsWith("CMD")) {
            // Parser Index Matrix: CMD, TargetNodeID, Mode, BiasPan, BiasTilt
            int index1 = csvLine.indexOf(',');
            int index2 = csvLine.indexOf(',', index1 + 1);
            int index3 = csvLine.indexOf(',', index2 + 1);
            int index4 = csvLine.indexOf(',', index3 + 1);

            if (index1 != -1 && index2 != -1 && index3 != -1 && index4 != -1) {
                uint8_t targetID = (uint8_t)csvLine.substring(index1 + 1, index2).toInt();
                uint8_t mode     = (uint8_t)csvLine.substring(index2 + 1, index3).toInt();
                int16_t biasPan  = (int16_t)csvLine.substring(index3 + 1, index4).toInt();
                int16_t biasTilt = (int16_t)csvLine.substring(index4 + 1).toInt();

                // Pack parameters cleanly into the outbound structural frame
                NetworkPacket outboundPacket;
                outboundPacket.sourceNodeID = 0x00; // 0x00 denotes Master Cluster Gateway
                outboundPacket.packetType = PACKET_ML_OVERRIDE;
                outboundPacket.sequenceNumber = gatewaySeqCount++;
                outboundPacket.data.overrideCmd.structuralMode = mode;
                outboundPacket.data.overrideCmd.appliedBiasPan = biasPan;
                outboundPacket.data.overrideCmd.appliedBiasTilt = biasTilt;

                // Broadcast modification directives to the edge node mesh
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

        if (packet.packetType == PACKET_TELEMETRY) {
            // Push structured CSV metrics to Python over USB CDC Serial
            Serial.printf("DATA,%02X,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%d\n",
                          packet.sourceNodeID,
                          packet.data.telemetry.busVoltage,
                          packet.data.telemetry.currentmA,
                          packet.data.telemetry.temperature,
                          packet.data.telemetry.humidity,
                          packet.data.telemetry.pressure,
                          packet.data.telemetry.panAngle,
                          packet.data.telemetry.tiltAngle);
        }
    }
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    // Optional diagnostics link hook hook
}

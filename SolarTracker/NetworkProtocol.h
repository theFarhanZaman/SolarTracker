#include <esp_now.h>
#include <WiFi.h>
#include "networkprotocol.h" // Include your exact protocol file here

// Broadcast address to target all listening nodes on the network channel
uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint32_t gatewaySequenceOut = 0;

// Callback triggered whenever an Edge Node transmits a packet over the airwaves
void OnDataRecv(const esp_now_recv_info_t *recvInfo, const uint8_t *incomingData, int len) {
    if (len == sizeof(NetworkPacket)) {
        NetworkPacket packet;
        memcpy(&packet, incomingData, sizeof(NetworkPacket));

        // ROUTE PHASE: Filter based on your PacketType enum indicators
        if (packet.header.packetType == PACKET_TELEMETRY) {
            
            // Unpack the TelemetryPayload variant from the active union memory space
            TelemetryPayload data = packet.payload.telemetry;

            // Stream a flat, single-line JSON structure out of the native USB interface
            Serial.print("{\"node_id\":");        Serial.print(packet.header.sourceID);
            Serial.print(",\"seq\":");            Serial.print(packet.header.sequenceNumber);
            Serial.print(",\"voltage\":");        Serial.print(data.busVoltage, 2);
            Serial.print(",\"current\":");        Serial.print(data.currentmA / 1000.0, 3); // Convert mA to Amperes for dashboard graph lines
            Serial.print(",\"temp\":");           Serial.print(data.temperature, 1);
            Serial.print(",\"humidity\":");       Serial.print(data.humidity, 1);
            Serial.print(",\"pressure\":");       Serial.print(data.pressure, 1);
            Serial.print(",\"pan_angle\":");      Serial.print(data.panAngle);
            Serial.print(",\"tilt_angle\":");     Serial.print(data.tiltAngle);
            Serial.println("}"); // Terminating newline character triggers FastAPI's readline() handle
        }
        else if (packet.header.packetType == PACKET_ACTION_SYNC) {
            // Placeholder: Handle peer-to-peer mesh synchronization logs if required by the gateway
        }
    }
}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("{\"error\":\"Gateway ESP-NOW hardware initialization failure\"}");
        return;
    }

    // Register broadcast peer configuration parameters
    esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, broadcastMac, 6);
    peerInfo.channel = 1; // Explicit channel lock  
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("{\"error\":\"Gateway failed to map broadcast mesh peer footprint\"}");
        return;
    }

    esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
}

void loop() {
    // DOWNSTREAM TRANSMISSION pipeline: Intercept control vectors coming from FastAPI via USB
    if (Serial.available() > 0) {
        String inputBuffer = Serial.readStringUntil('\n');
        inputBuffer.trim();

        // Looks for optimization directives: $OVERRIDE,mode,bias_pan,bias_tilt
        if (inputBuffer.startsWith("$OVERRIDE,")) {
            int firstComma  = inputBuffer.indexOf(',');
            int secondComma = inputBuffer.indexOf(',', firstComma + 1);
            int thirdComma  = inputBuffer.indexOf(',', secondComma + 1);

            uint8_t mode  = (uint8_t)inputBuffer.substring(firstComma + 1, secondComma).toInt();
            int16_t bPan  = (int16_t)inputBuffer.substring(secondComma + 1, thirdComma).toInt();
            int16_t bTilt = (int16_t)inputBuffer.substring(thirdComma + 1).toInt();

            // Structure your exact unified packet properties
            NetworkPacket outboundPacket;
            outboundPacket.header.sourceID = 0x00; // 0x00 denotes Master Gateway Hub
            outboundPacket.header.packetType = PACKET_ML_OVERRIDE;
            outboundPacket.header.sequenceNumber = gatewaySequenceOut++;

            // Pack the MLOverridePayload variant inside the union footprint
            outboundPacket.payload.overrideCmd.structuralMode = mode;
            outboundPacket.payload.overrideCmd.appliedBiasPan = bPan;
            outboundPacket.payload.overrideCmd.appliedBiasTilt = bTilt;

            // Broadcast the packet across the local airwaves
            esp_now_send(broadcastMac, (uint8_t *)&outboundPacket, sizeof(NetworkPacket));
        }
    }
}

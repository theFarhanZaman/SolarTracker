#include "TrackerNetManager.h"

// Initialize the static singleton pointer
TrackerNetManager* TrackerNetManager::instance = nullptr;

TrackerNetManager::TrackerNetManager()
{
    instance = this;

    memset(
        lastSequenceSeen,
        0,
        sizeof(lastSequenceSeen));
}

bool TrackerNetManager::begin() {
    // 1. Force strict Wi-Fi Station Mode (Disable AP mode to prevent RF noise)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // 2. Hardware Channel Locking
    // ESP-NOW requires nodes to be on the exact same RF channel. 
    // This ESP-IDF call bypasses the Arduino abstraction to guarantee the channel lock.
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(WSN_COMM_CHANNEL, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    // 3. Initialize the ESP-NOW protocol stack
    if (esp_now_init() != ESP_OK) {
        Serial.println("[NET CRITICAL] ESP-NOW Core Stack Initialization Failed.");
        return false;
    }

    // 4. Register the Receive Callback Hook
    esp_now_register_recv_cb(OnDataRecv);

    // 5. Generate a unique local Node ID from the hardware MAC address
    // This prevents manual hardcoding when flashing 50+ different edge nodes.
    uint8_t baseMac[6];
    esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
    localNodeID = baseMac[5]; // Use the last byte of the MAC as a quick unique identifier

    // 6. THE FIX: Explicitly register the universal broadcast address as a trusted peer
    if (!registerPeer(broadcastMac)) {
        Serial.println("[NET CRITICAL] Hardware layer rejected broadcast peer registration.");
        return false;
    }

    return true;
}

bool TrackerNetManager::registerPeer(const uint8_t *mac_addr) {
    // Check if the peer already exists in the hardware routing table
    if (esp_now_is_peer_exist(mac_addr)) {
        return true; 
    }

    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo)); // Zero out memory to prevent struct garbage
    memcpy(peerInfo.peer_addr, mac_addr, 6);
    
    peerInfo.channel = WSN_COMM_CHANNEL; // Bind peer to the strict system channel
    peerInfo.encrypt = false;            // ESP-NOW broadcasts cannot use payload encryption

    // Push the struct to the ESP-IDF driver
    esp_err_t addStatus = esp_now_add_peer(&peerInfo);
    return (addStatus == ESP_OK);
}

bool TrackerNetManager::broadcastSync(const SyncPayload &syncData) {
    NetworkPacket packet;
    
    // Assemble the universal network header
    packet.header.sourceID = localNodeID;
    packet.header.packetType = PACKET_ACTION_SYNC;
    packet.header.sequenceNumber = seqCounter++;

    // Inject the specific sync payload
    packet.payload.syncData = syncData;

    // Fire the packet into the RF layer
    esp_err_t result = esp_now_send(broadcastMac, (uint8_t *)&packet, sizeof(NetworkPacket));
    
    if (result != ESP_OK) {
        Serial.printf("[NET WARNING] RF Hardware rejected TX buffer (Err: %d)\n", result);
        return false;
    }
    
    return true;
}

void TrackerNetManager::OnDataRecv(const esp_now_recv_info *recvInfo, const uint8_t *incomingData, int len) {
    // Safety check: Ensure the instance exists and the packet size matches our strict architecture
    if (instance == nullptr || len != sizeof(NetworkPacket)) {
        return; 
    }

    NetworkPacket packet;
    memcpy(&packet, incomingData, sizeof(NetworkPacket));

    // Dynamic Peer Learning: If we hear from a new MAC, add it to our routing table automatically
    instance->registerPeer(recvInfo->src_addr);

    // Route the packet based on its signature
    if (packet.header.packetType == PACKET_ACTION_SYNC) {
        instance->latestSyncCommand = packet.payload.syncData;
        instance->hasPendingSync = true;
    }
}

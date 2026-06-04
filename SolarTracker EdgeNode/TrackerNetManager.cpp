#include "TrackerNetManager.h"

TrackerNetManager* TrackerNetManager::instance = nullptr;

bool TrackerNetManager::begin() {
    WiFi.mode(WIFI_STA);
    
    // Auto-generate a unique 1-byte ID from the last byte of the chip's base MAC address
    uint8_t baseMac[6];
    WiFi.macAddress(baseMac);
    localNodeID = baseMac[5]; 

    if (esp_now_init() != ESP_OK) return false;

    // Register receipt intercept callback
    esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

    // Register the universal broadcast link as the baseline permanent peer
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, broadcastMac, 6);
    peerInfo.channel = 0;  // Stay on current Wi-Fi channel
    peerInfo.encrypt = false;
    
    return (esp_now_add_peer(&peerInfo) == ESP_OK);
}

bool TrackerNetManager::registerPeer(const uint8_t *mac_addr) {
    if (esp_now_is_peer_exist(mac_addr)) return true; // Already registered

    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, mac_addr, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    
    return (esp_now_add_peer(&peerInfo) == ESP_OK);
}

bool TrackerNetManager::broadcastSync(const SyncPayload &syncData) {
    NetworkPacket packet;
    packet.header.sourceID = localNodeID;
    packet.header.packetType = PACKET_ACTION_SYNC;
    packet.header.sequenceNumber = seqCounter++;
    packet.payload.syncData = syncData;

    // NULL target tells ESP-NOW to send via the broadcast pipeline to all active listeners
    esp_err_t result = esp_now_send(broadcastMac, (uint8_t *)&packet, sizeof(packet));
    return (result == ESP_OK);
}

void TrackerNetManager::OnDataRecv(const esp_now_recv_info *recvInfo, const uint8_t *incomingData, int len) {
    if (len < sizeof(NetworkHeader) || instance == nullptr) return;

    // Dynamically learn the sender's identity if they are new to the local array
    instance->registerPeer(recvInfo->src_addr);

    NetworkPacket *receivedPacket = (NetworkPacket *)incomingData;

    // Prevent a node from parsing its own broadcast reflections
    if (receivedPacket->header.sourceID == instance->localNodeID) return;

    switch (receivedPacket->header.packetType) {
        case PACKET_ACTION_SYNC:
            instance->latestSyncCommand = receivedPacket->payload.syncData;
            instance->hasPendingSync = true;
            break;
            
        default:
            break;
    }
}

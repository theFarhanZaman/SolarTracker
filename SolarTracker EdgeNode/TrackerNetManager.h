#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "NetworkProtocol.h"

class TrackerNetManager {
private:
    uint8_t localNodeID;
    uint32_t seqCounter;
    uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    static void OnDataRecv(const esp_now_recv_info *recvInfo, const uint8_t *incomingData, int len);
    bool registerPeer(const uint8_t *mac_addr);

public:
    static TrackerNetManager* instance; // Singleton pointer for callback routing
    
    // Shared state references for synchronization loops
    bool hasPendingSync;
    SyncPayload latestSyncCommand;

    TrackerNetManager() : localNodeID(0), seqCounter(0), hasPendingSync(false) { instance = this; }
    
    bool begin();
    bool broadcastSync(const SyncPayload &syncData);
    uint8_t getLocalNodeID() const { return localNodeID; }
};

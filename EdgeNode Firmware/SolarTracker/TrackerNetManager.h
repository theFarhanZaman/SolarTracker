#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include "NetworkProtocol.h"

// Define the universal system channel for the mesh (Must match Gateway)
#define WSN_COMM_CHANNEL 1

class TrackerNetManager {
private:
    uint8_t localNodeID;
    uint32_t seqCounter;
    uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    // Static callback required by the ESP-IDF underlying C API
    static void OnDataRecv(const esp_now_recv_info *recvInfo, const uint8_t *incomingData, int len);
    
    // Internal helper to dynamically learn and register new RF peers
    bool registerPeer(const uint8_t *mac_addr);

public:
    static TrackerNetManager* instance; // Singleton pointer for callback routing
    
    // Shared state references for synchronization loops
    bool hasPendingSync;
    SyncPayload latestSyncCommand;

    TrackerNetManager();
    
    /**
     * @brief Initializes the Wi-Fi radio, locks the RF channel, and registers ESP-NOW peers.
     * @return true if the hardware layer binds successfully, false otherwise.
     */
    bool begin();

    /**
     * @brief Packages and dispatches a synchronization command to the swarm.
     * @param syncData The structural command payload to broadcast.
     * @return true if the packet was accepted by the RF hardware layer.
     */
    bool broadcastSync(const SyncPayload &syncData);

    /**
     * @brief Retrieves the dynamically generated hardware ID for this node.
     */
    uint8_t getLocalNodeID() const { return localNodeID; }
};

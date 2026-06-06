#pragma once

/*
===============================================================================
 SolarTracker - Tracker Network Manager
-------------------------------------------------------------------------------
 Purpose:
    ESP-NOW transport layer for SolarTracker Edge Nodes.

 Features:
    - ESP-NOW initialization
    - Dynamic peer discovery
    - Broadcast support
    - Telemetry transmission
    - Sync command reception
    - ML override reception
    - Duplicate packet rejection
    - Protocol validation
    - Sequence tracking
    - Future swarm expansion support

 Target:
    ESP32-S3
    Arduino Core v3.x

===============================================================================
*/

#include <Arduino.h>

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "NetworkProtocol.h"

#ifndef WSN_COMM_CHANNEL
#define WSN_COMM_CHANNEL 1
#endif

class TrackerNetManager
{
public:

    //---------------------------------------------------------------------
    // Singleton
    //---------------------------------------------------------------------

    static TrackerNetManager* instance;

    //---------------------------------------------------------------------
    // Constructor
    //---------------------------------------------------------------------

    TrackerNetManager();

    //---------------------------------------------------------------------
    // Initialization
    //---------------------------------------------------------------------

    bool begin();

    //---------------------------------------------------------------------
    // Telemetry TX
    //---------------------------------------------------------------------

    bool sendTelemetry(
        const WSN::TelemetryPayload& telemetry);

    //---------------------------------------------------------------------
    // Sync TX
    //---------------------------------------------------------------------

    bool broadcastSync(
        const WSN::SyncPayload& syncData);

    //---------------------------------------------------------------------
    // Generic Packet TX
    //---------------------------------------------------------------------

    bool sendPacket(
        const uint8_t* destinationMAC,
        const WSN::NetworkPacket& packet);

    //---------------------------------------------------------------------
    // Status
    //---------------------------------------------------------------------

    bool isInitialized() const
    {
        return initialized;
    }

    uint8_t getLocalNodeID() const
    {
        return localNodeID;
    }

    uint32_t getPacketsSent() const
    {
        return packetsSent;
    }

    uint32_t getPacketsReceived() const
    {
        return packetsReceived;
    }

    uint32_t getPacketsDropped() const
    {
        return packetsDropped;
    }

    uint32_t getLastSequenceNumber() const
    {
        return sequenceCounter;
    }

    //---------------------------------------------------------------------
    // Sync State
    //---------------------------------------------------------------------

    bool hasPendingSync = false;

    WSN::SyncPayload latestSyncCommand {};

    //---------------------------------------------------------------------
    // ML Override State
    //---------------------------------------------------------------------

    bool hasPendingMLOverride = false;

    WSN::MLOverridePayload latestMLOverride {};

private:

    //---------------------------------------------------------------------
    // Runtime State
    //---------------------------------------------------------------------

    bool initialized = false;

    uint8_t localNodeID = 0;

    uint32_t sequenceCounter = 0;

    //---------------------------------------------------------------------
    // Statistics
    //---------------------------------------------------------------------

    uint32_t packetsSent     = 0;
    uint32_t packetsReceived = 0;
    uint32_t packetsDropped  = 0;

    //---------------------------------------------------------------------
    // Duplicate Packet Protection
    //
    // Index = source node ID
    // Value = highest sequence received
    //---------------------------------------------------------------------

    uint32_t lastSequenceSeen[256] = {};

    //---------------------------------------------------------------------
    // Broadcast MAC
    //---------------------------------------------------------------------

    static constexpr uint8_t BROADCAST_MAC[6] =
    {
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF
    };

    //---------------------------------------------------------------------
    // Internal Helpers
    //---------------------------------------------------------------------

    bool initializeWiFi();

    bool initializeEspNow();

    bool registerBroadcastPeer();

    bool registerPeer(
        const uint8_t* macAddress);

    bool isPeerKnown(
        const uint8_t* macAddress) const;

    bool validatePacket(
        const WSN::NetworkPacket& packet) const;

    bool isDuplicatePacket(
        const WSN::NetworkPacket& packet);

    uint8_t generateNodeID() const;

    //---------------------------------------------------------------------
    // ESP-NOW Event Callbacks
    //---------------------------------------------------------------------

    static void OnDataRecv(
        const esp_now_recv_info_t* recvInfo,
        const uint8_t* data,
        int len);

    static void OnDataSent(
        const uint8_t* macAddr,
        esp_now_send_status_t status);

    //---------------------------------------------------------------------
    // Packet Routing
    //---------------------------------------------------------------------

    void processIncomingPacket(
        const uint8_t* sourceMAC,
        const WSN::NetworkPacket& packet);

    void handleTelemetryPacket(
        const WSN::NetworkPacket& packet);

    void handleSyncPacket(
        const WSN::NetworkPacket& packet);

    void handleMLOverridePacket(
        const WSN::NetworkPacket& packet);

    void handleHeartbeatPacket(
        const WSN::NetworkPacket& packet);

    //---------------------------------------------------------------------
    // Logging
    //---------------------------------------------------------------------

    void logPacketReceived(
        const WSN::NetworkPacket& packet);

    void logPacketDropped(
        const char* reason);
};

#pragma once
#include <Arduino.h>

// Unique signatures for structural routing
enum PacketType : uint8_t {
    PACKET_PING_DISCOVERY,
    PACKET_TELEMETRY_SHARE,
    PACKET_ACTION_SYNC
};

// Uniform header appended to ALL network transmissions
struct __attribute__((packed)) NetworkHeader {
    uint8_t  sourceID;        // Last byte of the sender's MAC address
    uint8_t  packetType;      // Maps to PacketType enum
    uint32_t sequenceNumber;  // Prevents processing duplicate packets
};

// Payload definitions - Add new structures here as your project grows!
struct __attribute__((packed)) SyncPayload {
    uint32_t targetEpoch;     // Synchronized timestamp
    int16_t  masterPanAngle;  // Targeted orientation sharing
    int16_t  masterTiltAngle;
};

struct __attribute__((packed)) NetworkPacket {
    NetworkHeader header;
    union {
        SyncPayload syncData;
        // You can layer environmental payloads here later without changing the header logic
    } payload;
};

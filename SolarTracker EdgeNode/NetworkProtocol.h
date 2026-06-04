#pragma once
#include <Arduino.h>

// =========================================================================
// UNIFIED NETWORK PACKET DEFINITIONS
// =========================================================================

enum PacketType : uint8_t {
    PACKET_PING_DISCOVERY,
    PACKET_TELEMETRY,       
    PACKET_ML_OVERRIDE,     
    PACKET_ACTION_SYNC      
};

struct __attribute__((packed)) NetworkHeader {
    uint8_t  sourceID;        // Sender node ID (0x00 = Gateway Hub)
    uint8_t  packetType;      // Associated PacketType enumeration
    uint32_t sequenceNumber;  // Monotonically increasing tracking ID
};

struct __attribute__((packed)) SyncPayload {
    uint32_t targetEpoch;     // Timestamp signature
    int16_t  masterPanAngle;  // Global alignment target for horizontal axis
    int16_t  masterTiltAngle; // Global alignment target for vertical axis
};

struct __attribute__((packed)) TelemetryPayload {
    float busVoltage;         // INA226 Solar Panel Voltage
    float currentmA;          // INA226 Solar Panel Current (mA)
    float temperature;        // BME280 Ambient Temperature
    float humidity;           // BME280 Relative Humidity
    float pressure;           // BME280 Atmospheric Pressure (hPa)
    int16_t panAngle;         // Current physical position of pan servo
    int16_t tiltAngle;        // Current physical position of tilt servo
};

struct __attribute__((packed)) MLOverridePayload {
    uint8_t  targetID;        // Specific destination address (0xFF = Universal Broadcast Swarm)
    uint8_t  structuralMode;  // 0 = Autonomous LDR, 1 = Safe Flat Lock, 2 = Remote Expert
    int16_t  appliedBiasPan;  // Tuning horizontal bias requested by central server
    int16_t  appliedBiasTilt; // Tuning vertical bias requested by central server
};

struct __attribute__((packed)) NetworkPacket {
    NetworkHeader header;
    union {
        SyncPayload syncData;          
        TelemetryPayload telemetry;    
        MLOverridePayload overrideCmd; 
    } payload;
};

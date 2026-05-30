#pragma once
#include <Arduino.h>

enum PacketType : uint8_t {
    PACKET_PING_DISCOVERY,
    PACKET_TELEMETRY,       // For Python ML telemetry streaming
    PACKET_ML_OVERRIDE,     // For Python ML central control commands
    PACKET_ACTION_SYNC      // Restored for your original peer-to-peer node sync
};

struct __attribute__((packed)) NetworkHeader {
    uint8_t  sourceID;        
    uint8_t  packetType;      
    uint32_t sequenceNumber;  
};#pragma once
#include <Arduino.h>

enum PacketType : uint8_t {
    PACKET_TELEMETRY,
    PACKET_ML_OVERRIDE
};

struct __attribute__((packed)) TelemetryPayload {
    float busVoltage;
    float currentmA;
    float temperature;
    float humidity;
    float pressure;
    int16_t panAngle;
    int16_t tiltAngle;
};

struct __attribute__((packed)) MLOverridePayload {
    uint8_t structuralMode; // 0 = Normal Auto, 1 = Park Flat (Storm), 2 = Applied Bias
    int16_t appliedBiasPan; 
    int16_t appliedBiasTilt;
};

struct __attribute__((packed)) NetworkPacket {
    uint8_t sourceNodeID;
    uint8_t packetType;
    uint32_t sequenceNumber;
    union {
        TelemetryPayload telemetry;
        MLOverridePayload overrideCmd;
    } data;
};


struct __attribute__((packed)) SyncPayload {
    uint32_t targetEpoch;     
    int16_t  masterPanAngle;  
    int16_t  masterTiltAngle;
};

struct __attribute__((packed)) TelemetryPayload {
    float busVoltage;
    float currentmA;
    float temperature;
    float humidity;
    float pressure;
    int16_t panAngle;
    int16_t tiltAngle;
};

struct __attribute__((packed)) MLOverridePayload {
    uint8_t structuralMode; // 0 = Normal Auto, 1 = Park Flat (Storm), 2 = Applied Bias
    int16_t appliedBiasPan; 
    int16_t appliedBiasTilt;
};

struct __attribute__((packed)) NetworkPacket {
    NetworkHeader header;
    union {
        SyncPayload syncData;          // Restored for original peer code sync
        TelemetryPayload telemetry;    // For Python telemetry stream
        MLOverridePayload overrideCmd; // For Python central controls
    } payload;
};

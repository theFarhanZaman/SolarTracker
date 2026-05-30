#pragma once
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

#pragma once
#include <Arduino.h>

enum PacketType : uint8_t {
    PACKET_PING_DISCOVERY,
    PACKET_TELEMETRY,       
    PACKET_ML_OVERRIDE,     
    PACKET_ACTION_SYNC      
};

struct __attribute__((packed)) NetworkHeader {
    uint8_t  sourceID;        
    uint8_t  packetType;      
    uint32_t sequenceNumber;  
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
    uint8_t structuralMode; 
    int16_t appliedBiasPan; 
    int16_t appliedBiasTilt;
};

struct __attribute__((packed)) NetworkPacket {
    NetworkHeader header;
    union {
        SyncPayload syncData;          
        TelemetryPayload telemetry;    
        MLOverridePayload overrideCmd; 
    } payload;
};

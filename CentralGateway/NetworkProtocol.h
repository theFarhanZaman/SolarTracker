#pragma once
#include <Arduino.h>

// 1. Packet Architecture Type Anchors
enum PacketType : uint8_t {
    PACKET_PING_DISCOVERY,
    PACKET_TELEMETRY,
    PACKET_ML_OVERRIDE,
    PACKET_ACTION_SYNC
};

// 2. Uniform Wireless Packet Header Block
struct NetworkHeader {
    uint8_t  sourceID;         // Local identifier tracking origin node
    uint8_t  packetType;       // Bound signature mapped to PacketType enum
    uint32_t sequenceNumber;   // Monotonically increasing tracking frame token
};

// 3. Concrete Struct Payload Layout Models
struct TelemetryPayload {
    float   busVoltage;        // Monitored high-side solar pane bus potential (V)
    float   currentmA;         // Harvested dynamic load output current profile (mA)
    float   temperature;       // Thermal footprint reading (°C)
    float   humidity;          // Atmospheric moisture saturation footprint (%)
    float   pressure;          // Barometric sensor payload calculation (hPa)
    int16_t panAngle;          // Active physical horizontal coordinates
    int16_t tiltAngle;         // Active physical vertical alignment angle
};

struct MLOverridePayload {
    uint8_t  targetID;         // Explicit target execution selector node index
    uint8_t  structuralMode;   // State engine router (0=Auto, 1=Lock, 2=Expert Bias)
    int16_t  appliedBiasPan;   // Micro-optimization horizontal step angle deviation
    int16_t  appliedBiasTilt;  // Micro-optimization vertical step angle deviation
};

struct SyncPayload {
    uint32_t targetEpoch;      // Shared operational baseline coordination timeline
    int16_t  masterPanAngle;   // Swarm leader tracking anchor coordinate reference
    int16_t  masterTiltAngle;  // Swarm leader tracking anchor coordinate reference
};

// 4. Unified Shared Memory Network Packet Architecture
struct NetworkPacket {
    NetworkHeader header;
    union {
        TelemetryPayload  telemetry;
        MLOverridePayload overrideCmd;
        SyncPayload       syncData;
    } payload;
};

#pragma once

#include <Arduino.h>

namespace WSN
{
    static constexpr uint8_t PROTOCOL_VERSION = 1;
    static constexpr uint8_t GATEWAY_ID       = 0x00;
    static constexpr uint8_t BROADCAST_ID     = 0xFF;

    enum PacketType : uint8_t
    {
        PACKET_TELEMETRY   = 0x01,
        PACKET_ACTION_SYNC = 0x02,
        PACKET_ML_OVERRIDE = 0x03,
        PACKET_HEARTBEAT   = 0x04
    };

    struct __attribute__((packed)) NetworkHeader
    {
        uint8_t  protocolVersion;
        uint8_t  sourceID;
        uint8_t  packetType;
        uint32_t sequenceNumber;
    };

    struct __attribute__((packed)) TelemetryPayload
    {
        float busVoltage;
        float currentmA;
        float temperature;
        float humidity;
        float pressure;

        int16_t panAngle;
        int16_t tiltAngle;
    };

    struct __attribute__((packed)) SyncPayload
    {
        uint32_t targetEpoch;

        int16_t masterPanAngle;
        int16_t masterTiltAngle;
    };

    struct __attribute__((packed)) MLOverridePayload
    {
        uint8_t targetID;

        uint8_t structuralMode;

        int16_t appliedBiasPan;
        int16_t appliedBiasTilt;
    };

    struct __attribute__((packed)) HeartbeatPayload
    {
        uint32_t uptimeSeconds;
    };

    union PacketPayload
    {
        TelemetryPayload telemetry;
        SyncPayload syncData;
        MLOverridePayload overrideCmd;
        HeartbeatPayload heartbeat;
    };

    struct __attribute__((packed)) NetworkPacket
    {
        NetworkHeader header;
        PacketPayload payload;
    };

    static_assert(sizeof(NetworkHeader) == 7);
}

#include "TrackerNetManager.h"

TrackerNetManager* TrackerNetManager::instance = nullptr;

constexpr uint8_t TrackerNetManager::BROADCAST_MAC[6];

TrackerNetManager::TrackerNetManager()
{
    instance = this;

    memset(
        lastSequenceSeen,
        0,
        sizeof(lastSequenceSeen));
}

bool TrackerNetManager::begin()
{
    WiFi.mode(WIFI_STA);

    WiFi.disconnect(true, true);

    delay(100);

    esp_wifi_set_promiscuous(true);

    esp_wifi_set_channel(
        WSN_COMM_CHANNEL,
        WIFI_SECOND_CHAN_NONE);

    esp_wifi_set_promiscuous(false);

    if (esp_now_init() != ESP_OK)
    {
        Serial.println(
            "[NET] ESP-NOW init failed");

        return false;
    }

    esp_now_register_recv_cb(
        TrackerNetManager::OnDataRecv);

    esp_now_register_send_cb(TrackerNetManager::OnDataSent);

    uint64_t chipID =
        ESP.getEfuseMac();

    localNodeID =
        static_cast<uint8_t>(
            chipID & 0xFF);

    esp_now_peer_info_t peer = {};

    memcpy(
        peer.peer_addr,
        BROADCAST_MAC,
        6);

    peer.channel =
        WSN_COMM_CHANNEL;

    peer.encrypt = false;

    if (!esp_now_is_peer_exist(
            BROADCAST_MAC))
    {
        esp_now_add_peer(&peer);
    }

    initialized = true;

    return true;
}

bool TrackerNetManager::sendTelemetry(
    const WSN::TelemetryPayload& telemetry)
{
    WSN::NetworkPacket packet = {};

    packet.header.protocolVersion =
        WSN::PROTOCOL_VERSION;

    packet.header.sourceID =
        localNodeID;

    packet.header.packetType =
        WSN::PACKET_TELEMETRY;

    packet.header.sequenceNumber =
        ++sequenceCounter;

    packet.payload.telemetry =
        telemetry;

    return sendPacket(
        BROADCAST_MAC,
        packet);
}

bool TrackerNetManager::broadcastSync(
    const WSN::SyncPayload& syncData)
{
    WSN::NetworkPacket packet = {};

    packet.header.protocolVersion =
        WSN::PROTOCOL_VERSION;

    packet.header.sourceID =
        localNodeID;

    packet.header.packetType =
        WSN::PACKET_ACTION_SYNC;

    packet.header.sequenceNumber =
        ++sequenceCounter;

    packet.payload.syncData =
        syncData;

    return sendPacket(
        BROADCAST_MAC,
        packet);
}

bool TrackerNetManager::sendPacket(
    const uint8_t* destinationMAC,
    const WSN::NetworkPacket& packet)
{
    if (!initialized)
    {
        return false;
    }

    esp_err_t result =
        esp_now_send(
            destinationMAC,
            reinterpret_cast<const uint8_t*>(&packet),
            sizeof(packet));

    if (result == ESP_OK)
    {
        packetsSent++;
        return true;
    }

    packetsDropped++;

    return false;
}

bool TrackerNetManager::registerPeer(
    const uint8_t* macAddress)
{
    if (esp_now_is_peer_exist(
            macAddress))
    {
        return true;
    }

    esp_now_peer_info_t peer = {};

    memcpy(
        peer.peer_addr,
        macAddress,
        6);

    peer.channel =
        WSN_COMM_CHANNEL;

    peer.encrypt = false;

    return (
        esp_now_add_peer(&peer)
        == ESP_OK);
}

bool TrackerNetManager::isPeerKnown(
    const uint8_t* macAddress) const
{
    return esp_now_is_peer_exist(
        macAddress);
}

bool TrackerNetManager::validatePacket(
    const WSN::NetworkPacket& packet) const
{
    if (
        packet.header.protocolVersion
        != WSN::PROTOCOL_VERSION)
    {
        return false;
    }

    return true;
}

bool TrackerNetManager::isDuplicatePacket(
    const WSN::NetworkPacket& packet)
{
    uint8_t source =
        packet.header.sourceID;

    uint32_t sequence =
        packet.header.sequenceNumber;

    if (
        sequence <=
        lastSequenceSeen[source])
    {
        return true;
    }

    lastSequenceSeen[source] =
        sequence;

    return false;
}

uint8_t TrackerNetManager::generateNodeID() const
{
    uint64_t chipID =
        ESP.getEfuseMac();

    return static_cast<uint8_t>(
        chipID & 0xFF);
}

void TrackerNetManager::OnDataRecv(
    const esp_now_recv_info_t* recvInfo,
    const uint8_t* data,
    int len)
{
    if (instance == nullptr)
    {
        return;
    }

    if (
        len != sizeof(
            WSN::NetworkPacket))
    {
        return;
    }

    WSN::NetworkPacket packet;

    memcpy(
        &packet,
        data,
        sizeof(packet));

    if (
        !instance->validatePacket(
            packet))
    {
        return;
    }

    if (
        instance->isDuplicatePacket(
            packet))
    {
        return;
    }

    instance->packetsReceived++;

    instance->registerPeer(
        recvInfo->src_addr);

    instance->processIncomingPacket(
        recvInfo->src_addr,
        packet);
}

#include "esp_now.h"
#include "esp_wifi.h"

void TrackerNetManager::OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status)
{
    uint8_t *mac = info->des_addr;  // destination MAC

    Serial.print("ESP-NOW Send Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}
{
}

void TrackerNetManager::processIncomingPacket(
    const uint8_t*,
    const WSN::NetworkPacket& packet)
{
    switch (
        packet.header.packetType)
    {
        case WSN::PACKET_ACTION_SYNC:

            latestSyncCommand =
                packet.payload.syncData;

            hasPendingSync = true;

            break;

        case WSN::PACKET_ML_OVERRIDE:

            latestMLOverride =
                packet.payload.overrideCmd;

            hasPendingMLOverride =
                true;

            break;

        default:
            break;
    }
}

void TrackerNetManager::handleTelemetryPacket(
    const WSN::NetworkPacket&)
{
}

void TrackerNetManager::handleSyncPacket(
    const WSN::NetworkPacket&)
{
}

void TrackerNetManager::handleMLOverridePacket(
    const WSN::NetworkPacket&)
{
}

void TrackerNetManager::handleHeartbeatPacket(
    const WSN::NetworkPacket&)
{
}

void TrackerNetManager::logPacketReceived(
    const WSN::NetworkPacket&)
{
}

void TrackerNetManager::logPacketDropped(
    const char*)
{
}

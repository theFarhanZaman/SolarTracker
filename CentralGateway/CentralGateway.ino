#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>

#include "NetworkProtocol.h"
#include "GatewayDisplay.h"

#define I2C_SDA_PIN 9
#define I2C_SCL_PIN 8

#define WSN_COMM_CHANNEL 1
#define NODE_TIMEOUT_MS 15000

GatewayDisplayManager gatewayUI;

static uint32_t lastSeenMatrix[256] = {0};
static uint32_t lastSequenceSeen[256] = {0};

static uint32_t gatewaySeqCount = 0;

static uint32_t packetsReceived = 0;
static uint32_t packetsSent = 0;
static uint32_t packetsDropped = 0;

static uint8_t activeNodeCount = 0;

static uint8_t broadcastAddress[6] =
{
    0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF
};

void OnDataRecv(
    const esp_now_recv_info *recvInfo,
    const uint8_t *incomingData,
    int len);

void OnDataSent(
    const wifi_tx_info_t *txInfo,
    esp_now_send_status_t status);

static bool registerPeer(
    const uint8_t* mac)
{
    if (esp_now_is_peer_exist(mac))
    {
        return true;
    }

    esp_now_peer_info_t peer = {};

    memcpy(peer.peer_addr, mac, 6);

    peer.channel = WSN_COMM_CHANNEL;
    peer.encrypt = false;

    return (
        esp_now_add_peer(&peer)
        == ESP_OK
    );
}

static bool initializeEspNow()
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
        return false;
    }

    esp_now_register_recv_cb(
        OnDataRecv);

    esp_now_register_send_cb(
        OnDataSent);

    esp_now_peer_info_t peer = {};

    memcpy(
        peer.peer_addr,
        broadcastAddress,
        6);

    peer.channel =
        WSN_COMM_CHANNEL;

    peer.encrypt = false;

    if (!esp_now_is_peer_exist(
            broadcastAddress))
    {
        esp_now_add_peer(&peer);
    }

    return true;
}

static uint8_t computeActiveNodes()
{
    uint32_t now = millis();

    uint8_t count = 0;

    for (int i = 0; i < 256; i++)
    {
        if (
            lastSeenMatrix[i] != 0 &&
            (now - lastSeenMatrix[i])
                < NODE_TIMEOUT_MS
        )
        {
            count++;
        }
    }

    return count;
}

void setup()
{
    Serial.begin(921600);

    Wire.begin(
        I2C_SDA_PIN,
        I2C_SCL_PIN);

    if (!gatewayUI.init())
    {
        Serial.println(
            "STATUS,WARN,OLED_INIT_FAILED");
    }

    uint32_t startTime = millis();

    while (!Serial)
    {
        if (
            millis() - startTime
            > 30000
        )
        {
            break;
        }

        delay(10);
    }

    if (!initializeEspNow())
    {
        Serial.println(
            "STATUS,ERROR,ESP_NOW_INIT_FAILED");

        while (true)
        {
            delay(1000);
        }
    }

    Serial.println(
        "STATUS,READY,GATEWAY_ACTIVE");
}

void loop()
{
    static uint32_t lastDisplayUpdate = 0;

    uint32_t now = millis();

    if (
        now - lastDisplayUpdate
        >= 1000
    )
    {
        activeNodeCount =
            computeActiveNodes();

        gatewayUI.update(
            activeNodeCount > 0,
            activeNodeCount,
            packetsSent);

        lastDisplayUpdate = now;
    }

    if (Serial.available())
    {
        String line =
            Serial.readStringUntil('\n');

        line.trim();

        if (!line.startsWith("CMD"))
        {
            return;
        }

        int i1 = line.indexOf(',');
        int i2 = line.indexOf(',', i1 + 1);
        int i3 = line.indexOf(',', i2 + 1);
        int i4 = line.indexOf(',', i3 + 1);

        if (
            i1 < 0 ||
            i2 < 0 ||
            i3 < 0 ||
            i4 < 0
        )
        {
            return;
        }

        uint8_t targetID =
            (uint8_t)
            line.substring(
                i1 + 1,
                i2).toInt();

        uint8_t mode =
            (uint8_t)
            line.substring(
                i2 + 1,
                i3).toInt();

        int16_t biasPan =
            (int16_t)
            line.substring(
                i3 + 1,
                i4).toInt();

        int16_t biasTilt =
            (int16_t)
            line.substring(
                i4 + 1).toInt();

        NetworkPacket packet = {};

        packet.header.sourceID = 0x00;

        packet.header.packetType =
            PACKET_ML_OVERRIDE;

        packet.header.sequenceNumber =
            ++gatewaySeqCount;

        packet.payload.overrideCmd.targetID =
            targetID;

        packet.payload.overrideCmd.structuralMode =
            mode;

        packet.payload.overrideCmd.appliedBiasPan =
            biasPan;

        packet.payload.overrideCmd.appliedBiasTilt =
            biasTilt;

        esp_err_t result =
            esp_now_send(
                broadcastAddress,
                reinterpret_cast<uint8_t*>(
                    &packet),
                sizeof(packet));

        if (result == ESP_OK)
        {
            packetsSent++;

            Serial.printf(
                "STATUS,TX_OK,%02X\n",
                targetID);
        }
        else
        {
            Serial.printf(
                "STATUS,TX_FAIL,%02X\n",
                targetID);
        }
    }
}

void OnDataRecv(
    const esp_now_recv_info *recvInfo,
    const uint8_t *incomingData,
    int len)
{
    if (
        len != sizeof(NetworkPacket)
    )
    {
        packetsDropped++;
        return;
    }

    registerPeer(
        recvInfo->src_addr);

    NetworkPacket packet;

    memcpy(
        &packet,
        incomingData,
        sizeof(packet));

    uint8_t source =
        packet.header.sourceID;

    if (
        source == 0 ||
        source == 255
    )
    {
        packetsDropped++;
        return;
    }

    if (
        packet.header.sequenceNumber
        <= lastSequenceSeen[source]
    )
    {
        return;
    }

    lastSequenceSeen[source] =
        packet.header.sequenceNumber;

    lastSeenMatrix[source] =
        millis();

    packetsReceived++;

    switch (
        packet.header.packetType
    )
    {
        case PACKET_TELEMETRY:
        {
            Serial.printf(
                "DATA,%02X,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%d\n",
                source,
                packet.payload.telemetry.busVoltage,
                packet.payload.telemetry.currentmA,
                packet.payload.telemetry.temperature,
                packet.payload.telemetry.humidity,
                packet.payload.telemetry.pressure,
                packet.payload.telemetry.panAngle,
                packet.payload.telemetry.tiltAngle);

            break;
        }

        default:
            break;
    }
}

void OnDataSent(
    const wifi_tx_info_t *txInfo,
    esp_now_send_status_t status)
{
    (void)txInfo;

    if (
        status
        != ESP_NOW_SEND_SUCCESS
    )
    {
        Serial.println(
            "STATUS,WARN,SEND_FAILED");
    }
}

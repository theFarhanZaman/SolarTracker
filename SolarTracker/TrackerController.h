#pragma once
#include <Arduino.h>
#include "ManagedServo.h"
#include "SolarSensor.h"
#include "SensorManager.h"
#include "TrackerNetManager.h"
#include "NetworkProtocol.h"

// Define the designated Master Node ID for the swarm to prevent broadcast storms
#define MASTER_NODE_ID 0x01 

class TrackerController
{
private:
    ManagedServo &pan;
    ManagedServo &tilt;
    SensorManager &sensors;
    uint32_t axisCooldownMs;
    uint32_t cooldownStart;
    int threshold;
    int stepDegrees;

public:
    int panMin, panMax, tiltMin, tiltMax;

    TrackerController(ManagedServo &p, ManagedServo &t, SensorManager &sm, int thresh = 60, int step = 3, uint32_t cooldown = 1500)
        : pan(p), tilt(t), sensors(sm), axisCooldownMs(cooldown), cooldownStart(0), threshold(thresh), stepDegrees(step),
          panMin(0), panMax(180), tiltMin(20), tiltMax(160) {}

    bool cooldownExpired() const { return millis() - cooldownStart >= axisCooldownMs; }

    /**
     * Executes tracking operations based on safety states, network sync telemetry, or analog LDR inputs.
     * @param s Consumed light differential readings across the LDR matrix
     * @param net Instantiated reference to the custom ESP-NOW interface
     */
    void track(const SolarReading &s, TrackerNetManager &net)
    {
        // 1. Hardware Over-Discharge Protection Rule
        if (sensors.systemCritical)
        {
            pan.detach();
            tilt.detach();
            return;
        }

        // 2. Cooperative Network Synchronization Overrides (Worker Node Behavior)
        if (net.hasPendingSync)
        {
            net.hasPendingSync = false; 
            
            pan.moveTo(net.latestSyncCommand.masterPanAngle);
            tilt.moveTo(net.latestSyncCommand.masterTiltAngle);
            cooldownStart = millis(); 
            return;
        }

        // 3. Local Autonomous Tracking Execution Loop
        if (pan.busy() || tilt.busy() || !cooldownExpired()) return;

        bool moved = false;

        if (abs(s.horizontalError) > threshold)
        {
            int targetAngle = pan.angle + (s.horizontalError > 0 ? stepDegrees : -stepDegrees);
            pan.moveTo(constrain(targetAngle, panMin, panMax));
            moved = true;
        }
        else if (abs(s.verticalError) > threshold)
        {
            int targetAngle = tilt.angle + (s.verticalError > 0 ? stepDegrees : -stepDegrees);
            tilt.moveTo(constrain(targetAngle, tiltMin, tiltMax));
            moved = true;
        }

        // 4. Peer Broadcast Latch (Master Node Behavior Only)
        // Prevents 2.4GHz ESP-NOW packet collision storms by ensuring only the designated swarm leader broadcasts sync commands.
        if (moved && net.getLocalNodeID() == MASTER_NODE_ID)
        {
            cooldownStart = millis();

            SyncPayload syncData;
            syncData.targetEpoch = millis();
            syncData.masterPanAngle = pan.angle;
            syncData.masterTiltAngle = tilt.angle;
            
            net.broadcastSync(syncData);
        }
    }
};

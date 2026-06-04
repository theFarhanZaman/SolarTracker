#pragma once
#include <Arduino.h>
#include <esp_now.h>

// Modular Framework Extensions (Ensure these exist in your include path)
#include "NetworkProtocol.h"
#include "SolarSensor.h"
#include "ManagedServo.h"
#include "TrackerController.h"
#include "TrackerNetManager.h"
#include "SensorManager.h"

namespace MLReceiver {

    // =========================================================================
    // SHARED OPERATIONAL STATE REGISTERS (Thread-Safe / Inline Linkage)
    // =========================================================================
    inline uint8_t  mlControlledMode = 0;
    inline int16_t  mlBiasPan        = 0;   // Dynamic horizontal adjustment angle offset
    inline int16_t  mlBiasTilt       = 0;   // Dynamic vertical adjustment angle offset

    // =========================================================================
    // ESP-NOW NETWORK PACKET INTERCEPT HOOK
    // =========================================================================
    inline void OnDataRecv(const esp_now_recv_info *recvInfo, const uint8_t *incomingData, int len) {
        if (len == sizeof(NetworkPacket)) {
            NetworkPacket packet;
            memcpy(&packet, incomingData, sizeof(NetworkPacket));

            if (packet.header.packetType == PACKET_ML_OVERRIDE) {
                mlControlledMode = packet.payload.overrideCmd.structuralMode;
                mlBiasPan        = packet.payload.overrideCmd.appliedBiasPan;
                mlBiasTilt       = packet.payload.overrideCmd.appliedBiasTilt;
            }
        }
    }

    // =========================================================================
    // CONTROL ARBITRATION PIPELINE ENGINE
    // =========================================================================
    inline void processTracking(SolarReading& rawReading, ManagedServo& panServo,
                                ManagedServo& tiltServo, TrackerController& tracker,
                                TrackerNetManager& network, SensorManager& sensors) {

        // ---------------------------------------------------------------------
        // PRIORITY 1: CRITICAL SYSTEM FAULTS & MECHANICAL SAFE-LOCK OVERRIDES
        // ---------------------------------------------------------------------
        if (sensors.systemCritical || mlControlledMode == 1) {

            // Only trigger moveTo if not already at the safe point and not moving.
            // This prevents resetting the internal transit timer on every single frame loop.
            if (panServo.angle != 90 && !panServo.busy()) {
                panServo.moveTo(90);  // Store flat and parallel to the ground layout
            }
            if (tiltServo.angle != 0 && !tiltServo.busy()) {
                tiltServo.moveTo(0);
            }

            // Allow the background transit timers to tick naturally.
            // ManagedServo will automatically call detach() internally after 800ms!
            panServo.update();
            tiltServo.update();
            return;
        }

        // ---------------------------------------------------------------------
        // STEP 2: RUN LOCAL BASELINE ALIGNMENT GRAPH
        // ---------------------------------------------------------------------
        tracker.track(rawReading, network);

        // ---------------------------------------------------------------------
        // PRIORITY 3: REMOTE EXPERT MODE (ADDITIVE AI PREDICTIVE CORRECTIONS)
        // ---------------------------------------------------------------------
        if (mlControlledMode == 2) {
            // Compute the new optimization target angle by overlaying micro-biases
            int16_t targetPan  = constrain(panServo.angle + mlBiasPan,  0, 180);
            int16_t targetTilt = constrain(tiltServo.angle + mlBiasTilt, 0, 90);

            // Commit structural positions safely without flooding updates
            if (targetPan != panServo.angle && !panServo.busy()) {
                panServo.moveTo(targetPan);
            }
            if (targetTilt != tiltServo.angle && !tiltServo.busy()) {
                tiltServo.moveTo(targetTilt);
            }
        }

        // Ensure servo tracking state clocks keep updated during standard loops
        panServo.update();
        tiltServo.update();
    }
}

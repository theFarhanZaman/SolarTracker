#pragma once
#include <Arduino.h>
#include <esp_now.h>

// Modular Framework Extensions
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
    inline uint8_t  mlControlledMode = 0;   // 0 = Local LDR, 1 = Safe Flat, 2 = AI Expert
    inline int16_t  mlBiasPan        = 0;   // Dynamic target horizontal offset
    inline int16_t  mlBiasTilt       = 0;   // Dynamic target vertical offset

    // Tracking registers to prevent runaway additive servo loops
    inline int16_t  lastAppliedBiasPan  = 0;
    inline int16_t  lastAppliedBiasTilt = 0;

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
            // Command positions. moveTo() automatically handles attach operations internally.
            panServo.moveTo(90);  // Park perfectly flat parallel to the ground
            tiltServo.moveTo(0);   
            
            // Reset bias memory registers so they re-engage cleanly when exiting safe-lock
            lastAppliedBiasPan = 0;
            lastAppliedBiasTilt = 0;
            return; 
            // Note: We DO NOT call detach() here. The main loop's async panServo.update() 
            // will automatically cut the PWM holding current after 800ms transit time.
        }

        // ---------------------------------------------------------------------
        // STEP 2: RUN LOCAL BASELINE ALIGNMENT GRAPH
        // ---------------------------------------------------------------------
        // Execute the ground-truth photodiode matrix calculations first.
        tracker.track(rawReading, network);

        // ---------------------------------------------------------------------
        // PRIORITY 3: REMOTE EXPERT MODE (DELTA AI PREDICTIVE CORRECTIONS)
        // ---------------------------------------------------------------------
        if (mlControlledMode == 2) {
            // Calculate the change (delta) between the new bias and what was already injected
            int16_t deltaPan  = mlBiasPan - lastAppliedBiasPan;
            int16_t deltaTilt = mlBiasTilt - lastAppliedBiasTilt;

            // Only actuate the servos if the central server updated its bias layout
            if (deltaPan != 0 || deltaTilt != 0) {
                int16_t targetPan  = constrain(panServo.angle + deltaPan,  tracker.panMin,  tracker.panMax);
                int16_t targetTilt = constrain(tiltServo.angle + deltaTilt, tracker.tiltMin, tracker.tiltMax);
                
                panServo.moveTo(targetPan);
                tiltServo.moveTo(targetTilt);

                // Lock current offsets into memory
                lastAppliedBiasPan  = mlBiasPan;
                lastAppliedBiasTilt = mlBiasTilt;
            }
        } else {
            // If the server switches out of Mode 2 back to Mode 0, clear bias tracking alignment
            lastAppliedBiasPan  = 0;
            lastAppliedBiasTilt = 0;
        }
    }
}

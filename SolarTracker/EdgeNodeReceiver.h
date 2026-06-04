#pragma once
#include <Arduino.h>
#include <esp_now.h>

// Modular Framework Cross-Dependencies
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
    inline int16_t  mlBiasPan        = 0;   // Dynamic micro-bias targets
    inline int16_t  mlBiasTilt       = 0;   

    // Local persistent mirrors to calculate deltas and prevent runaway accumulation loops
    inline int16_t  lastAppliedBiasPan  = 0;
    inline int16_t  lastAppliedBiasTilt = 0;

    // =========================================================================
    // ESP-NOW NETWORK PACKET INTERCEPT HOOK
    // =========================================================================
    /**
     * @brief Asynchronous entry callback executed on packet capture over the air.
     * Identifies, unpacks, and authenticates targeted command configurations.
     */
    inline void OnDataRecv(const esp_now_recv_info *recvInfo, const uint8_t *incomingData, int len) {
        if (len == sizeof(NetworkPacket)) {
            NetworkPacket packet;
            memcpy(&packet, incomingData, sizeof(NetworkPacket));

            if (packet.header.packetType == PACKET_ML_OVERRIDE) {
                // Address Routing Guard: Filter for universal swarm broadcast (0xFF) or this local node ID
                if (packet.payload.overrideCmd.targetID == 0xFF || 
                    (TrackerNetManager::instance != nullptr && 
                     packet.payload.overrideCmd.targetID == TrackerNetManager::instance->getLocalNodeID())) {
                    
                    mlControlledMode = packet.payload.overrideCmd.structuralMode;
                    mlBiasPan        = packet.payload.overrideCmd.appliedBiasPan;
                    mlBiasTilt       = packet.payload.overrideCmd.appliedBiasTilt;
                }
            }
        }
    }

    // =========================================================================
    // CONTROL ARBITRATION PIPELINE ENGINE
    // =========================================================================
    /**
     * @brief Evaluates local analog vectors alongside state configurations.
     * Evaluates safety protocols before assigning movement profiles to active pins.
     */
    inline void processTracking(SolarReading& rawReading, ManagedServo& panServo, 
                                ManagedServo& tiltServo, TrackerController& tracker, 
                                TrackerNetManager& network, SensorManager& sensors) {
        
        // ---------------------------------------------------------------------
        // PRIORITY 1: CRITICAL HARDWARE FAULTS & MECHANICAL SAFE-LOCK OVERRIDES
        // ---------------------------------------------------------------------
        if (sensors.systemCritical || mlControlledMode == 1) {
            // Target horizontal park and safe flat tilt configurations
            panServo.moveTo(90);  
            tiltServo.moveTo(0);   
            
            // Wipe bias memories to ensure clean scaling profiles upon recovery exit
            lastAppliedBiasPan = 0;
            lastAppliedBiasTilt = 0;
            return; 
            // NOTE: Do not call detach() manually here. The core application loop 
            // naturally runs panServo.update() asynchronously, turning off holding current 
            // after the 800ms physical travel path completes.
        }

        // ---------------------------------------------------------------------
        // STEP 2: RUN AUTONOMOUS PHOTO-SENSOR ALIGNMENT
        // ---------------------------------------------------------------------
        // Executes ground-truth tracking matrix calculations.
        tracker.track(rawReading, network);

        // ---------------------------------------------------------------------
        // PRIORITY 3: REMOTE EXPERT MODE (DELTA AI PREDICTIVE CORRECTIONS)
        // ---------------------------------------------------------------------
        if (mlControlledMode == 2) {
            // Compute structural deltas relative to previously locked parameters
            int16_t deltaPan  = mlBiasPan - lastAppliedBiasPan;
            int16_t deltaTilt = mlBiasTilt - lastAppliedBiasTilt;

            // Only actuate actuators if the server sends updated bias frames
            if (deltaPan != 0 || deltaTilt != 0) {
                int16_t targetPan  = constrain(panServo.angle + deltaPan,  tracker.panMin,  tracker.panMax);
                int16_t targetTilt = constrain(tiltServo.angle + deltaTilt, tracker.tiltMin, tracker.tiltMax);
                
                panServo.moveTo(targetPan);
                tiltServo.moveTo(targetTilt);

                // Lock updated shifts into local state memory
                lastAppliedBiasPan  = mlBiasPan;
                lastAppliedBiasTilt = mlBiasTilt;
            }
        } else {
            // Clear tracking states if the system drops back down to local mode
            lastAppliedBiasPan  = 0;
            lastAppliedBiasTilt = 0;
        }
    }
}

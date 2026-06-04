#pragma once
#include <Arduino.h>
#include <esp_now.h>

// Structural System Interface Modules
#include "NetworkProtocol.h"
#include "SolarSensor.h"
#include "ManagedServo.h"
#include "TrackerController.h"
#include "TrackerNetManager.h"
#include "SensorManager.h"

namespace MLReceiver {
    
    // =========================================================================
    // LOCAL REGISTRATION REGISTERS & TRACKING STATE STORAGE
    // =========================================================================
    inline uint8_t  mlControlledMode = 0;   // 0 = Algorithmic, 1 = Safe Lock, 2 = Expert Bias
    inline int16_t  mlBiasPan        = 0;   // Inbound horizontal optimization offset
    inline int16_t  mlBiasTilt       = 0;   // Inbound vertical optimization offset
    inline uint8_t  nodeHardwareID   = 0x2A; // Unique hex ID configuration pattern (e.g., 42)

    // =========================================================================
    // ESP-NOW WIRELESS RECEPTION CALLBACK INTERCEPT
    // =========================================================================
    inline void OnDataRecv(const esp_now_recv_info *recvInfo, const uint8_t *incomingData, int len) {
        if (len == sizeof(NetworkPacket)) {
            NetworkPacket packet;
            memcpy(&packet, incomingData, sizeof(NetworkPacket));

            // Intercept incoming machine-learning/operator override instructions
            if (packet.header.packetType == PACKET_ML_OVERRIDE) {
                // Read from packet payload override target configuration properties
                uint8_t target = packet.payload.overrideCmd.targetID;
                
                // Route message if target is global broadcast (0x00) or explicitly matches this node
                if (target == 0x00 || target == nodeHardwareID) {
                    mlControlledMode = packet.payload.overrideCmd.structuralMode;
                    mlBiasPan        = packet.payload.overrideCmd.appliedBiasPan;
                    mlBiasTilt       = packet.payload.overrideCmd.appliedBiasTilt;
                }
            }
        }
    }

    // =========================================================================
    // CORE ARBITRATION ROUTER LOOP EXECUTION ENGINE
    // =========================================================================
    inline void processTracking(SolarReading& rawReading, ManagedServo& panServo, 
                                ManagedServo& tiltServo, TrackerController& tracker, 
                                TrackerNetManager& network, SensorManager& sensors) {
        
        // CASE 1: Low-Voltage Protection or Hardware Lock Flag
        if (sensors.systemCritical || mlControlledMode == 1) {
            if (!panServo.attached())  panServo.attach();
            if (!tiltServo.attached()) tiltServo.attach();

            panServo.moveTo(90); // Ground position parameters
            tiltServo.moveTo(0);   
            
            panServo.update();
            tiltServo.update();

            panServo.detach();    
            tiltServo.detach();
            return;
        }

        // CASE 2: Run local analytical photodiode array evaluation loops.
        // This calculates tracking updates internally without modifying live hardware targets.
        tracker.track(rawReading, network);

        // CASE 3: Apply Machine Learning Optimization Core Parameters
        if (mlControlledMode == 2) {
            if (!panServo.attached())  panServo.attach();
            if (!tiltServo.attached()) tiltServo.attach();

            // FIX: Micro-biases add strictly onto tracker baseline state metrics.
            // This isolates raw calculations from dynamic physical tracking loops.
            int16_t basePanTarget  = panServo.angle;  
            int16_t baseTiltTarget = tiltServo.angle; 

            // Safely reference programmatic algorithm calculations if available
            // targetPan = constrain(tracker.getCalculatedPan() + mlBiasPan, 0, 180);
            
            int16_t targetPan  = constrain(basePanTarget + mlBiasPan,  0, 180);
            int16_t targetTilt = constrain(baseTiltTarget + mlBiasTilt, 0, 90);
            
            panServo.moveTo(targetPan);
            tiltServo.moveTo(targetTilt);
        } 
        else {
            // Standard Autonomy Mode: Map standard algorithmic tracker properties down to execution gates
            if (!panServo.attached())  panServo.attach();
            if (!tiltServo.attached()) tiltServo.attach();
            
            // Adjust servo limits directly from tracking calculations
            // panServo.moveTo(tracker.getCalculatedPan());
            // tiltServo.moveTo(tracker.getCalculatedTilt());
        }
    }
}

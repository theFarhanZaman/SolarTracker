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
    // 0 = Local Algorithmic Loop (Default Photodiode Tracking)
    // 1 = Secure Mechanical Lock (Emergency Park Flat & Low Power State)
    // 2 = Remote Expert Mode (Overlay AI Predictive Drift & Micro-Biases)
    inline uint8_t  mlControlledMode = 0;   
    inline int16_t  mlBiasPan        = 0;   // Dynamic horizontal adjustment angle offset
    inline int16_t  mlBiasTilt       = 0;   // Dynamic vertical adjustment angle offset

    // =========================================================================
    // ESP-NOW NETWORK PACKET INTERCEPT HOOK
    // =========================================================================
    /**
     * @brief Non-blocking callback hook triggered whenever an ESP-NOW packet is captured over the air.
     * Parses incoming ML commands sent down by the central gateway base station.
     * * @param recvInfo Struct containing source MAC address and wireless metadata
     * @param incomingData Raw binary payload buffer received by the antenna
     * @param len Length of the incoming payload in bytes
     */
    inline void OnDataRecv(const esp_now_recv_info *recvInfo, const uint8_t *incomingData, int len) {
        // Enforce structural boundary checks to guarantee memory alignment safety
        if (len == sizeof(NetworkPacket)) {
            NetworkPacket packet;
            memcpy(&packet, incomingData, sizeof(NetworkPacket));

            // Verify packet signature matches the Machine Learning override frame type
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
    /**
     * @brief Arbitrates between local photodiode sensor arrays and remote machine learning server directives.
     * Executes deterministic safe states before applying optimization metrics.
     * * @param rawReading Captured analog voltage measurements from the 4-LDR matrix
     * @param panServo Actuator wrapper handling horizontal positioning mechanics
     * @param tiltServo Actuator wrapper handling vertical positioning mechanics
     * @param tracker Core algorithmic system handling standard differential light-gradient calculations
     * @param network Local network manager instance tracking node configuration states
     * @param sensors System instrumentation suite supervising bus health parameters (voltage/current/temp)
     */
    inline void processTracking(SolarReading& rawReading, ManagedServo& panServo, 
                                ManagedServo& tiltServo, TrackerController& tracker, 
                                TrackerNetManager& network, SensorManager& sensors) {
        
        // ---------------------------------------------------------------------
        // PRIORITY 1: CRITICAL SYSTEM FAULTS & MECHANICAL SAFE-LOCK OVERRIDES
        // ---------------------------------------------------------------------
        // If an overcurrent is detected, voltage drops too low, or the gateway triggers mode 1, 
        // immediately enter safe-parking orientation to minimize structural exposure and power draw.
        if (sensors.systemCritical || mlControlledMode == 1) {
            
            // Re-attach actuators if they were disabled to force execution of safe orientation commands
            if (!panServo.attached())  panServo.attach();
            if (!tiltServo.attached()) tiltServo.attach();

            panServo.moveTo(90);  // Store flat and parallel to the ground layout
            tiltServo.moveTo(0);   
            
            // Force non-blocking flush to allow servo control loops to output active PWM pulses
            panServo.update();
            tiltServo.update();

            // CRITICAL POWER MEASURE: Cut holding current overhead entirely. 
            // This prevents servo motor burnout and saves your battery under storm/low-light contexts.
            panServo.detach();    
            tiltServo.detach();
            return;
        }

        // ---------------------------------------------------------------------
        // STEP 2: RUN LOCAL BASELINE ALIGNMENT GRAPH
        // ---------------------------------------------------------------------
        // Execute your standard photodiode tracking matrix loops first. This acts as our ground truth anchor.
        // If communication drops, the device safely defaults to tracking light patterns without bricking.
        tracker.track(rawReading, network);

        // ---------------------------------------------------------------------
        // PRIORITY 3: REMOTE EXPERT MODE (ADDITIVE AI PREDICTIVE CORRECTIONS)
        // ---------------------------------------------------------------------
        // If the central backend server is active and calculating predictive drifts (e.g., compensating
        // for clouds, topography, or celestial calculations), intercept the servo angles and apply offsets.
        if (mlControlledMode == 2) {
            
            // Defensive Check: Ensure the actuators are securely attached to the PWM timers 
            // if transitioning directly back out of a low-power structural lock state.
            if (!panServo.attached())  panServo.attach();
            if (!tiltServo.attached()) tiltServo.attach();

            // Compute the new optimization target angle by overlaying the micro-biases 
            // onto the local hardware sensor baseline calculation.
            int16_t targetPan  = constrain(panServo.angle + mlBiasPan,  0, 180);
            int16_t targetTilt = constrain(tiltServo.angle + mlBiasTilt, 0, 90);
            
            // Commit structural positions
            panServo.moveTo(targetPan);
            tiltServo.moveTo(targetTilt);
        }
    }
}

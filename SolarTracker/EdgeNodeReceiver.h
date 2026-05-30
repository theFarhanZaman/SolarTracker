#pragma once
#include <Arduino.h>
#include <esp_now.h>
#include "NetworkProtocol.h"
#include "SolarSensor.h"
#include "ManagedServo.h"
#include "TrackerController.h"
#include "TrackerNetManager.h"
#include "SensorManager.h"

namespace MLReceiver {
    // Shared Operational State Registers
    inline uint8_t  mlControlledMode = 0;   // 0 = Local Algorithmic Loop, 1 = Secure Mechanical Lock, 2 = Remote Expert Mode
    inline int16_t  mlBiasPan = 0;
    inline int16_t  mlBiasTilt = 0;

    /**
     * Non-blocking callback hook triggered whenever an ESP-NOW packet is captured.
     * Parses incoming ML commands sent by the central gateway.
     */
    inline void OnDataRecv(const esp_now_recv_info *recvInfo, const uint8_t *incomingData, int len) {
        if (len == sizeof(NetworkPacket)) {
            NetworkPacket packet;
            memcpy(&packet, incomingData, sizeof(NetworkPacket));

            if (packet.packetType == PACKET_ML_OVERRIDE) {
                mlControlledMode = packet.data.overrideCmd.structuralMode;
                mlBiasPan        = packet.data.overrideCmd.appliedBiasPan;
                mlBiasTilt       = packet.data.overrideCmd.appliedBiasTilt;
            }
        }
    }

    /**
     * Arbitrates between local photodiode sensor configurations and 
     * remote machine learning server directives.
     */
    inline void processTracking(SolarReading& rawReading, ManagedServo& panServo, 
                                ManagedServo& tiltServo, TrackerController& tracker, 
                                TrackerNetManager& network, SensorManager& sensors) {
        
        // Priority 1: System Hardware Faults or Emergency ML "Park Flat" command
        if (sensors.systemCritical || mlControlledMode == 1) {
            panServo.writePosition(90);  // Store parallel to ground structure
            tiltServo.writePosition(0);   
            panServo.detach();           // Cut current overhead entirely
            tiltServo.detach();
            return;
        }

        // Priority 2: Remote Expert Mode (Applying AI Predictive Drift & Micro-Biases)
        if (mlControlledMode == 2) {
            int16_t targetPan  = constrain(rawReading.calculatedPan  + mlBiasPan,  0, 180);
            int16_t targetTilt = constrain(rawReading.calculatedTilt + mlBiasTilt, 0, 90);
            
            panServo.writePosition(targetPan);
            tiltServo.writePosition(targetTilt);
        } 
        // Priority 3: Fallback Default (Standard Local Light Gradient Control)
        else {
            tracker.track(rawReading, network);
        }
    }
}

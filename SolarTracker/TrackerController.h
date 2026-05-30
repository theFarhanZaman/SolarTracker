#pragma once
#include <Arduino.h>
#include "ManagedServo.h"
#include "SolarSensor.h"

class TrackerController
{
private:
    ManagedServo &pan;
    ManagedServo &tilt;
    uint32_t axisCooldownMs;
    uint32_t cooldownStart;
    int threshold;
    int stepDegrees;

public:
    int panMin, panMax, tiltMin, tiltMax;

    TrackerController(ManagedServo &p, ManagedServo &t, int thresh = 60, int step = 3, uint32_t cooldown = 1500)
        : pan(p), tilt(t), axisCooldownMs(cooldown), cooldownStart(0), threshold(thresh), stepDegrees(step),
          panMin(0), panMax(180), tiltMin(20), tiltMax(160) {}

    bool cooldownExpired() const { return millis() - cooldownStart >= axisCooldownMs; }

    void track(const SolarReading &s)
    {
        if (pan.busy() || tilt.busy() || !cooldownExpired()) return;

        bool moved = false;

        if (abs(s.horizontalError) > threshold)
        {
            pan.moveTo(pan.angle + (s.horizontalError > 0 ? stepDegrees : -stepDegrees));
            moved = true;
        }
        else if (abs(s.verticalError) > threshold)
        {
            tilt.moveTo(tilt.angle + (s.verticalError > 0 ? stepDegrees : -stepDegrees));
            moved = true;
        }

        if (moved) cooldownStart = millis();

        pan.angle = constrain(pan.angle, panMin, panMax);
        tilt.angle = constrain(tilt.angle, tiltMin, tiltMax);
    }
};

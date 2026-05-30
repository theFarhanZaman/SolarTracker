#pragma once
#include <Arduino.h>
#include <ESP32Servo.h>

class ManagedServo
{
private:
    Servo servo;
    uint8_t pin;
    uint16_t minUs;
    uint16_t maxUs;
    uint32_t transitTimeMs;
    uint32_t moveStart;
    bool attachedState;
    bool moving;

public:
    int angle;

    ManagedServo(uint8_t p, uint16_t min_us = 500, uint16_t max_us = 2500, uint32_t transit = 800)
        : pin(p), minUs(min_us), maxUs(max_us), transitTimeMs(transit),
          moveStart(0), attachedState(false), moving(false), angle(90) {}

    void begin() { detach(); }

    void moveTo(int newAngle)
    {
        newAngle = constrain(newAngle, 0, 180);
        if (!attachedState)
        {
            servo.attach(pin, minUs, maxUs);
            attachedState = true;
        }
        servo.write(newAngle);
        angle = newAngle;
        moving = true;
        moveStart = millis();
    }

    void update()
    {
        if (moving && (millis() - moveStart >= transitTimeMs))
        {
            detach();
            moving = false;
        }
    }

    bool busy() const { return moving; }

    void detach()
    {
        if (attachedState)
        {
            servo.detach();
            attachedState = false;
        }
    }
};

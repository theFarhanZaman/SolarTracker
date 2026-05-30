#pragma once
#include <Arduino.h>

class SoftTimer
{
private:
    uint32_t previous;
    uint32_t interval;

public:
    SoftTimer(uint32_t i = 1000) : interval(i), previous(0) {}

    void setInterval(uint32_t i) { interval = i; }
    void reset() { previous = millis(); }

    bool expired()
    {
        uint32_t now = millis();
        if (now - previous >= interval)
        {
            previous = now;
            return true;
        }
        return false;
    }
};

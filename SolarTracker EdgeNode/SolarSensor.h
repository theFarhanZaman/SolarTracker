#pragma once
#include <Arduino.h>

struct SolarReading
{
    uint16_t tl;
    uint16_t tr;
    uint16_t bl;
    uint16_t br;
    int horizontalError;
    int verticalError;
};

class SolarSensor
{
private:
    uint8_t pinTL, pinTR, pinBL, pinBR;
    uint8_t resolution;

public:
    SolarSensor(uint8_t tl, uint8_t tr, uint8_t bl, uint8_t br, uint8_t res = 12)
        : pinTL(tl), pinTR(tr), pinBL(bl), pinBR(br), resolution(res) {}

    void begin()
    {
        analogReadResolution(resolution);
    }

    SolarReading read()
    {
        SolarReading s;
        s.tl = analogRead(pinTL);
        s.tr = analogRead(pinTR);
        s.bl = analogRead(pinBL);
        s.br = analogRead(pinBR);

        int top = (s.tl + s.tr) / 2;
        int bottom = (s.bl + s.br) / 2;
        int left = (s.tl + s.bl) / 2;
        int right = (s.tr + s.br) / 2;

        s.verticalError = top - bottom;
        s.horizontalError = left - right;
        return s;
    }
};

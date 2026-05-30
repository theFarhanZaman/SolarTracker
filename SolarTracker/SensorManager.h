#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_BME280.h>
#include <INA226.h>

class SensorManager
{
private:
    uint8_t sdaPin, sclPin;
    uint8_t bmeAddr;
    uint8_t inaAddr;
    float shuntOhms;
    float maxCurrent;

public:
    Adafruit_MPU6050 mpu;
    Adafruit_BME280 bme;
    INA226 ina;

    float temperature, humidity, pressure;
    float busVoltage, currentmA, powerW;

    SensorManager(uint8_t sda, uint8_t scl, uint8_t bme_a = 0x76, uint8_t ina_a = 0x40, float shunt = 0.1f, float max_i = 1.0f);

    bool init();
    void update();
};

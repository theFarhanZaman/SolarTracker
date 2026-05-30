#include "SensorManager.h"

SensorManager::SensorManager(uint8_t sda, uint8_t scl, uint8_t bme_a, uint8_t ina_a, float shunt, float max_i)
    : sdaPin(sda), sclPin(scl), bmeAddr(bme_a), inaAddr(ina_a), shuntOhms(shunt), maxCurrent(max_i),
      ina(ina_a, &Wire), temperature(0), humidity(0), pressure(0), busVoltage(0), currentmA(0), powerW(0) {}

bool SensorManager::init()
{
    bool ok = true;
    Wire.begin(sdaPin, sclPin);

    if (!mpu.begin()) ok = false;
    if (!bme.begin(bmeAddr)) ok = false;
    if (!ina.begin())
    {
        ok = false;
    }
    else
    {
        ina.setMaxCurrentShunt(maxCurrent, shuntOhms);
    }
    return ok;
}

void SensorManager::update()
{
    temperature = bme.readTemperature();
    humidity = bme.readHumidity();
    pressure = bme.readPressure() / 100.0f;

    busVoltage = ina.getBusVoltage();
    currentmA = ina.getCurrent_mA();
    powerW = (busVoltage * currentmA) / 1000.0f;
}

#include "SensorManager.h"

SensorManager::SensorManager(uint8_t sda, uint8_t scl, uint8_t bat_p, uint8_t bme_a, uint8_t ina_a, float shunt, float max_i)
    : sdaPin(sda), sclPin(scl), batPin(bat_p), bmeAddr(bme_a), inaAddr(ina_a), shuntOhms(shunt), maxCurrent(max_i),
      ina(ina_a, &Wire), temperature(0), humidity(0), pressure(0), busVoltage(0), currentmA(0), powerW(0),
      batteryVoltage(0.0f), systemCritical(false) {}

bool SensorManager::init()
{
    bool ok = true;
    Wire.begin(sdaPin, sclPin);
    
    pinMode(batPin, INPUT); // Initialize battery monitoring pin

    if (!mpu.begin()) ok = false;
    if (!bme.begin(bmeAddr)) ok = false;
    if (!ina.begin())
    {
        ok = false;
    }
    else
    {
        // INA226 is wired on the high side of the solar panel to track harvested power
        ina.setMaxCurrentShunt(maxCurrent, shuntOhms);
    }
    return ok;
}

void SensorManager::update()
{
    // 1. Read Environmental Sensors
    temperature = bme.readTemperature();
    humidity = bme.readHumidity();
    pressure = bme.readPressure() / 100.0f;

    // 2. Read Solar Panel Telemetry via INA226
    busVoltage = ina.getBusVoltage();
    currentmA = ina.getCurrent_mA();
    powerW = (busVoltage * currentmA) / 1000.0f;

    // 3. Read LiPo Battery Level via Voltage Divider
    int rawAdc = analogRead(batPin);
    // 12-bit ADC (4095) with a 3.3V reference, scaled by 2 due to equal 10k-10k divider resistors
    batteryVoltage = ((float)rawAdc / 4095.0f) * 3.3f * 2.0f;

    // 4. Over-Discharge Protection Assessment
    if (batteryVoltage < 3.2f && batteryVoltage > 1.0f) // >1.0V filters out disconnected battery noise
    {
        systemCritical = true;
    }
    else
    {
        systemCritical = false;
    }
}

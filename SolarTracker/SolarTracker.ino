#include <Wire.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>

#include "SoftTimer.h"
#include "SensorManager.h"
#include "SolarSensor.h"
#include "ManagedServo.h"
#include "TrackerController.h"

/***********************************************************************
    UPDATED CORRECT PIN CONFIGURATIONS (ESP32-S3 SUPERMINI)
************************************************************************/
// Kinetic Subsystem (PWM)
#define PAN_SERVO_PIN           5   // D3 Pin on Left Rail
#define TILT_SERVO_PIN          21  // D6 Pin on Bottom Left Rail

// Native Hardware I2C Interface
#define I2C_SDA_PIN             6   // Labeled SDA on Left Rail
#define I2C_SCL_PIN             7   // Labeled SCL on Left Rail

// Solar Sensor Array (Analog Inputs)
#define LDR_TOP_LEFT_PIN        1   // ADC1-1 on Right Rail
#define LDR_TOP_RIGHT_PIN       2   // A0 on Right Rail
#define LDR_BOTTOM_LEFT_PIN     3   // A1 on Right Rail
#define LDR_BOTTOM_RIGHT_PIN    4   // A2 on Right Rail

// Battery Protection Telemetry
#define BATTERY_MONITOR_PIN     0   // ADC1-0 on Bottom Right Rail

// Legacy Serial RTC Pin Relocations (Shifted from I2C pins)
#define RTC_RST_PIN             9   // D9 on Left Rail
#define RTC_IO_PIN              10  // D10 on Left Rail
#define RTC_CLK_PIN             20  // D7 on Left Rail

// Infrastructure & Framework Instances
ThreeWire rtcWire(RTC_IO_PIN, RTC_CLK_PIN, RTC_RST_PIN);
RtcDS1302<ThreeWire> rtc(rtcWire);

SensorManager sensors(I2C_SDA_PIN, I2C_SCL_PIN, BATTERY_MONITOR_PIN);
SolarSensor solar(LDR_TOP_LEFT_PIN, LDR_TOP_RIGHT_PIN, LDR_BOTTOM_LEFT_PIN, LDR_BOTTOM_RIGHT_PIN);
ManagedServo panServo(PAN_SERVO_PIN);
ManagedServo tiltServo(TILT_SERVO_PIN);
TrackerController tracker(panServo, tiltServo, sensors); // Passed sensors reference here

// Timers
SoftTimer sensorTimer(1000);
SoftTimer trackingTimer(45000);
SoftTimer dashboardTimer(3000);

SolarReading latestSolar;

void setup() 
{
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[BOOT] Starting Protected Asynchronous Dual-Axis Tracker...");

    solar.begin();
    panServo.begin();
    tiltServo.begin();

    if (!sensors.init()) {
        Serial.println("[WARNING] Instrumentation failure detected on I2C bus.");
    }

    rtc.Begin();
    if (!rtc.IsDateTimeValid()) {
        rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__));
    }
    Serial.println("[BOOT] Initial configurations completed successfully.");
}

void loop() 
{
    // Continuous real-time asynchronous background updates
    panServo.update();
    tiltServo.update();

    if (sensorTimer.expired()) {
        sensors.update();
        latestSolar = solar.read();
    }

    if (trackingTimer.expired()) {
        tracker.track(latestSolar);
    }

    if (dashboardTimer.expired()) {
        RtcDateTime now = rtc.GetDateTime();
        
        // Comprehensive Output Telemetry
        Serial.printf("\n==================== SYSTEM TELEMETRY ====================\n");
        Serial.printf("TIMESTAMP   : %04u-%02u-%02u %02u:%02u:%02u\n", now.Year(), now.Month(), now.Day(), now.Hour(), now.Minute(), now.Second());
        Serial.printf("SOLAR YIELD : %.3f V | %.2f mA | %.3f W\n", sensors.busVoltage, sensors.currentmA, sensors.powerW);
        Serial.printf("BATTERY CELL: %.2f V [%s]\n", sensors.batteryVoltage, sensors.systemCritical ? "CRITICAL HALT ACTIVATED" : "HEALTHY");
        Serial.printf("ATMOSPHERE  : Temp: %.1f°C | Pressure: %.1f hPa | Hum: %.1f%%\n", sensors.temperature, sensors.pressure, sensors.humidity);
        Serial.printf("MECHANICS   : Pan Orientation: %d° | Tilt Orientation: %d°\n", panServo.angle, tiltServo.angle);
        Serial.printf("==========================================================\n");
    }
}

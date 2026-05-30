#include <Wire.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>

#include "SoftTimer.h"
#include "SensorManager.h"
#include "SolarSensor.h"
#include "ManagedServo.h"
#include "TrackerController.h"

// Configuration Pin Allocations
#define PAN_SERVO_PIN           10
#define TILT_SERVO_PIN          11
#define I2C_SDA_PIN             13
#define I2C_SCL_PIN             12
#define LDR_TOP_LEFT_PIN        1
#define LDR_TOP_RIGHT_PIN       2
#define LDR_BOTTOM_LEFT_PIN     3
#define LDR_BOTTOM_RIGHT_PIN    4
#define RTC_RST_PIN             5
#define RTC_IO_PIN              6
#define RTC_CLK_PIN             7

#define BATTERY_MONITOR_PIN     8 // Resistor divider center node connected to GPIO 8

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

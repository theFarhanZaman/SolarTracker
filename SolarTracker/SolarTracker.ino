#include <Wire.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>

// Import Custom Modular Components
#include "SoftTimer.h"
#include "SensorManager.h"
#include "SolarSensor.h"
#include "ManagedServo.h"
#include "TrackerController.h"

// Configuration Parameters
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

// Hardware and Global Object Instantiation
ThreeWire rtcWire(RTC_IO_PIN, RTC_CLK_PIN, RTC_RST_PIN);
RtcDS1302<ThreeWire> rtc(rtcWire);

SensorManager sensors(I2C_SDA_PIN, I2C_SCL_PIN);
SolarSensor solar(LDR_TOP_LEFT_PIN, LDR_TOP_RIGHT_PIN, LDR_BOTTOM_LEFT_PIN, LDR_BOTTOM_RIGHT_PIN);
ManagedServo panServo(PAN_SERVO_PIN);
ManagedServo tiltServo(TILT_SERVO_PIN);
TrackerController tracker(panServo, tiltServo);

// Execution Timers
SoftTimer sensorTimer(1000);
SoftTimer trackingTimer(45000);
SoftTimer dashboardTimer(3000);

SolarReading latestSolar;

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[BOOT] Starting Modular Dual-Axis System...");

    solar.begin();
    panServo.begin();
    tiltServo.begin();

    if (!sensors.init()) {
        Serial.println("[WARNING] Some sensors failed to respond.");
    }

    rtc.Begin();
    if (!rtc.IsDateTimeValid()) {
        rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__));
    }
    Serial.println("[BOOT] System configuration complete.");
}

void loop()
{
    // Real-time asynchronous state updates
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

        // Output clean diagnostic dashboard
        Serial.printf("\n--- TELEMETRY %02u:%02u:%02u ---\n", now.Hour(), now.Minute(), now.Second());
        Serial.printf("Solar Panel: %.2fV | Current: %.1fmA\n", sensors.busVoltage, sensors.currentmA);
        Serial.printf("Env: %.1f C | %.1f hPa\n", sensors.temperature, sensors.pressure);
        Serial.printf("Servo Orientations: Pan %d° | Tilt %d°\n", panServo.angle, tiltServo.angle);
    }
}

#include <Wire.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>

// Modular Framework Extensions
#include "SoftTimer.h"
#include "SensorManager.h"
#include "SolarSensor.h"
#include "ManagedServo.h"
#include "TrackerController.h"
#include "NetworkProtocol.h"
#include "NetworkManager.h"

/***********************************************************************
    VERIFIED TARGET PIN ALLOCATIONS FOR ESP32-S3 SUPERMINI
************************************************************************/
// Kinetic Actuators (PWM Signal Control lines)
#define PAN_SERVO_PIN           5   // Left Rail, Pin 1 (GPIO 5)
#define TILT_SERVO_PIN          21  // Left Rail, Pin 8 (GPIO 21)

// Native Multi-Sensor I2C Instrumentation Interface Bus
#define I2C_SDA_PIN             6   // Left Rail, Pin 2 (GPIO 6)
#define I2C_SCL_PIN             7   // Left Rail, Pin 3 (GPIO 7)

// Photo-Reactive Sensor Array Matrix Channels
#define LDR_TOP_LEFT_PIN        1   // Right Rail, Pin 7 (GPIO 1)
#define LDR_TOP_RIGHT_PIN       2   // Right Rail, Pin 6 (GPIO 2)
#define LDR_BOTTOM_LEFT_PIN     3   // Right Rail, Pin 5 (GPIO 3)
#define LDR_BOTTOM_RIGHT_PIN    4   // Right Rail, Pin 4 (GPIO 4)

// Power Subsystem Monitoring Channel (Resistor Divider Tap Node)
#define BATTERY_MONITOR_PIN     0   // Right Rail, Pin 8 (GPIO 0)

// Shifted Logic Pins for DS1302 Real-Time Clock
#define RTC_RST_PIN             9   // Left Rail, Pin 5 (GPIO 9)
#define RTC_IO_PIN              10  // Left Rail, Pin 6 (GPIO 10)
#define RTC_CLK_PIN             20  // Left Rail, Pin 7 (GPIO 20)

/***********************************************************************
    GLOBAL CLASS INSTANTIATIONS & REFERENCES
************************************************************************/
ThreeWire rtcWire(RTC_IO_PIN, RTC_CLK_PIN, RTC_RST_PIN);
RtcDS1302<ThreeWire> rtc(rtcWire);

SensorManager sensors(I2C_SDA_PIN, I2C_SCL_PIN, BATTERY_MONITOR_PIN);
SolarSensor solar(LDR_TOP_LEFT_PIN, LDR_TOP_RIGHT_PIN, LDR_BOTTOM_LEFT_PIN, LDR_BOTTOM_RIGHT_PIN);
ManagedServo panServo(PAN_SERVO_PIN);
ManagedServo tiltServo(TILT_SERVO_PIN);
TrackerController tracker(panServo, tiltServo, sensors);
NetworkManager network; // Handles the scalable ESP-NOW peer topology

// Non-blocking Polling Interval Timers
SoftTimer sensorTimer(1000);       // Poll environmental telemetry every 1s
SoftTimer trackingTimer(30000);    // Evaluate solar positioning trends every 30s
SoftTimer dashboardTimer(5000);    // Stream system statistics to console every 5s

SolarReading latestSolar;

void setup() 
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n========================================================");
    Serial.println("[BOOT] Initializing Synchronized Distributed WSN Node... ");
    Serial.println("========================================================");

    // 1. Spin up Peer Network Protocols
    if (network.begin()) {
        Serial.printf("[NET] Node Active. Dynamic Address Matrix Key: ID [0x%02X]\n", network.getLocalNodeID());
    } else {
        Serial.println("[CRITICAL] ESP-NOW Communications stack failure.");
    }

    // 2. Initialize Internal Sensory Array Controls
    solar.begin();
    panServo.begin();
    tiltServo.begin();

    // 3. Initialize Shared I2C Instrumentation Bus
    if (!sensors.init()) {
        Serial.println("[WARNING] Peripheral communication anomaly detected on I2C bus.");
    }

    // 4. Clock Reference Validation
    rtc.Begin();
    if (!rtc.IsDateTimeValid()) {
        Serial.println("[RTC] Hardware clock invalid. Compiling structural defaults...");
        rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__));
    }
    
    Serial.println("[BOOT] Hardware configuration sequences successfully finished.\n");
}

void loop() 
{
    // Continuous background operations (Servos process detachment time checks asynchronously)
    panServo.update();
    tiltServo.update();

    // Non-blocking instrumentation data acquisition loop
    if (sensorTimer.expired()) {
        sensors.update();
        latestSolar = solar.read();
    }

    // Active positional orientation tracking logic step
    if (trackingTimer.expired()) {
        tracker.track(latestSolar, network);
    }

    // Diagnostics Dashboard console print loop
    if (dashboardTimer.expired()) {
        RtcDateTime now = rtc.GetDateTime();
        
        Serial.printf("\n[NODE TELEMETRY - ID 0x%02X] Timestamp: %02u:%02u:%02u\n", 
                      network.getLocalNodeID(), now.Hour(), now.Minute(), now.Second());
        Serial.printf("ENERGY LOG  : Harvest: %.2fV | Draw: %.1fmA | Battery Storage: %.2fV [%s]\n", 
                      sensors.busVoltage, sensors.currentmA, sensors.batteryVoltage, 
                      sensors.systemCritical ? "CRITICAL SLEEP MODE" : "HEALTHY STATUS");
        Serial.printf("ATMOSPHERE  : Temp: %.1f°C | Humidity: %.1f%% | Air Pressure: %.1f hPa\n", 
                      sensors.temperature, sensors.humidity, sensors.pressure);
        Serial.printf("ALIGNMENT   : Pan Servo: %d° | Tilt Servo: %d°\n", 
                      panServo.angle, tiltServo.angle);
        Serial.println("--------------------------------------------------------------------------------");
    }
}

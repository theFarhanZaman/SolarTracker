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
#include "TrackerNetManager.h"

/***********************************************************************
    VERIFIED TARGET PIN ALLOCATIONS FOR ESP32-S3 SUPERMINI
************************************************************************/
#define PAN_SERVO_PIN           5   
#define TILT_SERVO_PIN          21  
#define I2C_SDA_PIN             6   
#define I2C_SCL_PIN             7   
#define LDR_TOP_LEFT_PIN        1   
#define LDR_TOP_RIGHT_PIN       2   
#define LDR_BOTTOM_LEFT_PIN     3   
#define LDR_BOTTOM_RIGHT_PIN    4   
#define BATTERY_MONITOR_PIN     0   
#define RTC_RST_PIN             9   
#define RTC_IO_PIN              10  
#define RTC_CLK_PIN             20  

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
TrackerNetManager network; 

SoftTimer sensorTimer(1000);       
SoftTimer trackingTimer(30000);    
SoftTimer dashboardTimer(5000);    

SolarReading latestSolar;

void setup() 
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n========================================================");
    Serial.println("[BOOT] Initializing Synchronized Distributed WSN Node... ");
    Serial.println("========================================================");

    // Spin up Peer Network Protocols
    if (network.begin()) {
        Serial.printf("[NET] Node Active. Dynamic Address Matrix Key: ID [0x%02X]\n", network.getLocalNodeID());
    } else {
        Serial.println("[CRITICAL] ESP-NOW Communications stack failure.");
    }

    solar.begin();
    panServo.begin();
    tiltServo.begin();

    if (!sensors.init()) {
        Serial.println("[WARNING] Peripheral communication anomaly detected on I2C bus.");
    }

    rtc.Begin();
    if (!rtc.IsDateTimeValid()) {
        Serial.println("[RTC] Hardware clock invalid. Compiling structural defaults...");
        rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__));
    }
    
    Serial.println("[BOOT] Hardware configuration sequences successfully finished.\n");
}

void loop() 
{
    panServo.update();
    tiltServo.update();

    if (sensorTimer.expired()) {
        sensors.update();
        latestSolar = solar.read();
    }

    if (trackingTimer.expired()) {
        tracker.track(latestSolar, network);
    }

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

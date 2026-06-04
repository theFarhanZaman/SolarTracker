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
#include "EdgeNodeReceiver.h"  // <--- Imported ML Modularity Frame

/***********************************************************************
    VERIFIED HIGH-ACCURACY PIN ALLOCATIONS (STRICTLY <= GPIO 13)
    Mapped directly to the ESP32-S3 Supermini Edge Headers
************************************************************************/
// --- LEFT RAIL: Sensor Inputs & System I2C Bus ---
#define LDR_TOP_LEFT_PIN        1   // Pin Label: GP1  | Role: Analog Sensor Array
#define LDR_TOP_RIGHT_PIN       2   // Pin Label: GP2  | Role: Analog Sensor Array
#define LDR_BOTTOM_LEFT_PIN     3   // Pin Label: GP3  | Role: Analog Sensor Array
#define LDR_BOTTOM_RIGHT_PIN    4   // Pin Label: GP4  | Role: Analog Sensor Array
#define BATTERY_MONITOR_PIN     5   // Pin Label: GP5  | Role: Analog LiPo Battery Monitor
#define I2C_SDA_PIN             6   // Pin Label: GP6  | Role: Hardware I2C SDA Bus Line
#define I2C_SCL_PIN             7   // Pin Label: GP7  | Role: Hardware I2C SCL Bus Line

// --- RIGHT RAIL: Kinetic Servos & Serial Timekeeping ---
#define PAN_SERVO_PIN           8   // Pin Label: GP8  | Role: PWM Pan Servo Actuation
#define TILT_SERVO_PIN          9   // Pin Label: GP9  | Role: PWM Tilt Servo Actuation
#define RTC_RST_PIN             11  // Pin Label: GP11 | Role: DS1302 RTC Chip Select (RST)
#define RTC_IO_PIN              12  // Pin Label: GP12 | Role: DS1302 RTC Serial Data (I/O)
#define RTC_CLK_PIN             13  // Pin Label: GP13 | Role: DS1302 RTC Serial Clock (CLK)

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
    // Initialize serial pipeline via Native USB CDC
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n========================================================");
    Serial.println("[BOOT] Initializing Synchronized Distributed WSN Node... ");
    Serial.println("========================================================");

    // Spin up Peer Network Protocols
    if (network.begin()) {
        Serial.printf("[NET] Node Active. Dynamic Address Matrix Key: ID [0x%02X]\n", network.getLocalNodeID());
        // Register the dynamic ML processing function inside the active ESP-NOW intercept stack
        esp_now_register_recv_cb(MLReceiver::OnDataRecv);
    } else {
        Serial.println("[CRITICAL] ESP-NOW Communications stack failure.");
    }

    // Hardware Peripheral Initialization
    solar.begin();
    panServo.begin();
    tiltServo.begin();

    // Verify I2C Bus Stability (INA226 / BME280 / MPU6050)
    if (!sensors.init()) {
        Serial.println("[WARNING] Peripheral communication anomaly detected on I2C bus.");
    }

    // Secure Timekeeping Synchronization
    rtc.Begin();
    if (!rtc.IsDateTimeValid()) {
        Serial.println("[RTC] Hardware clock invalid. Compiling structural defaults...");
        rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__));
    }
    
    Serial.println("[BOOT] Hardware configuration sequences successfully finished.\n");
}

void loop() 
{
    // Execute non-blocking servo position calculations
    panServo.update();
    tiltServo.update();

    // Async task loop: Collect data from sensors
    if (sensorTimer.expired()) {
        sensors.update();
        latestSolar = solar.read();
    }

    // Async task loop: Evaluate tracking decision hierarchy using ML arbitration mapping
    if (trackingTimer.expired()) {
        MLReceiver::processTracking(latestSolar, panServo, tiltServo, tracker, network, sensors);
    }

    // Async task loop: Print operational dashboard out of USB CDC interface
   // Async task loop: Print operational dashboard and broadcast machine-parsable JSON
    if (dashboardTimer.expired()) {
        RtcDateTime now = rtc.GetDateTime();
        
        // ==========================================
        // 1. LOCAL HUMAN-READABLE SERIAL DIAGNOSTICS
        // ==========================================
        Serial.printf("\n[NODE TELEMETRY - ID 0x%02X] Timestamp: %02u:%02u:%02u\n", 
                      network.getLocalNodeID(), now.Hour(), now.Minute(), now.Second());
        Serial.printf("ML STATE    : Mode=%d | Active Bias Vector: Pan=%d°, Tilt=%d°\n",
                      MLReceiver::mlControlledMode, MLReceiver::mlBiasPan, MLReceiver::mlBiasTilt);
        Serial.printf("ENERGY LOG  : Harvest: %.2fV | Draw: %.1fmA | Battery Storage: %.2fV [%s]\n", 
                      sensors.busVoltage, sensors.currentmA, sensors.batteryVoltage, 
                      sensors.systemCritical ? "CRITICAL SLEEP MODE" : "HEALTHY STATUS");
        Serial.printf("ATMOSPHERE  : Temp: %.1f°C | Humidity: %.1f%% | Air Pressure: %.1f hPa\n", 
                      sensors.temperature, sensors.humidity, sensors.pressure);
        Serial.printf("ALIGNMENT   : Pan Servo: %d° | Tilt Servo: %d°\n", 
                      panServo.angle, tiltServo.angle);
        Serial.println("--------------------------------------------------------------------------------");

        // ==========================================
        // 2. BACKEND GATEWAY SERIALIZATION (CRITICAL FOR FASTAPI)
        // ==========================================
        // Prefix token "$TELEMETRY;" lets FastAPI filter raw text from machine data strings
        Serial.print("$TELEMETRY;{");
        Serial.printf("\"node_id\":%d,",      network.getLocalNodeID());
        Serial.printf("\"hour\":%02u,",       now.Hour());
        Serial.printf("\"min\":%02u,",        now.Minute());
        Serial.printf("\"sec\":%02u,",        now.Second());
        Serial.printf("\"ml_mode\":%d,",      MLReceiver::mlControlledMode);
        Serial.printf("\"bias_pan\":%d,",     MLReceiver::mlBiasPan);
        Serial.printf("\"bias_tilt\":%d,",    MLReceiver::mlBiasTilt);
        Serial.printf("\"voltage\":%.2f,",    sensors.busVoltage);
        Serial.printf("\"current\":%.3f,",    sensors.currentmA / 1000.0); // Convert mA to Amperes for UI scaling
        Serial.printf("\"bat_volt\":%.2f,",   sensors.batteryVoltage);
        Serial.printf("\"critical\":%s,",     sensors.systemCritical ? "true" : "false");
        Serial.printf("\"temp\":%.1f,",       sensors.temperature);
        Serial.printf("\"humidity\":%.1f,",   sensors.humidity);
        Serial.printf("\"pressure\":%.1f,",   sensors.pressure);
        Serial.printf("\"pan_angle\":%d,",    panServo.angle);
        Serial.printf("\"tilt_angle\":%d",     tiltServo.angle);
        Serial.println("}"); // Closes the JSON syntax and appends a clear newline (\n)
    }
}

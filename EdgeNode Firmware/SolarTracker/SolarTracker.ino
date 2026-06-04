#include <Wire.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <esp_wifi.h> // Required for hardware-level Wi-Fi channel forcing

// Modular Framework Extensions
#include "SoftTimer.h"
#include "SensorManager.h"
#include "SolarSensor.h"
#include "ManagedServo.h"
#include "TrackerController.h"
#include "NetworkProtocol.h"
#include "TrackerNetManager.h"
#include "EdgeNodeReceiver.h"  // Dynamic ML processing & control arbitration matrix

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

// Non-blocking task execution schedule mappings
SoftTimer sensorTimer(1000);       // Sample ambient sensors at 1Hz
SoftTimer trackingTimer(30000);    // Recalculate kinematics every 30 seconds 
SoftTimer dashboardTimer(5000);    // Broadcast telemetry package every 5 seconds

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
        
        // Explicitly lock the underlying Wi-Fi radio to Channel 1 to match the Gateway
        esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
        Serial.println("[NET] Radio RF interface locked securely to Channel 1.");

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
    // Persistent state counter across loop executions (Scoping Bug Fix)
    static uint32_t localNodeSequenceNum = 0;

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

    // Async task loop: Process telemetry transmission & local diagnostic logging
    if (dashboardTimer.expired()) {
        RtcDateTime now = rtc.GetDateTime();
        
        // =========================================================================
        // 1. LOCAL FIELD-DEBUGGING LOGS (Via USB-CDC)
        // =========================================================================
        Serial.printf("\n[LOCAL NODE - ID 0x%02X] Tx Seq: %u | Time: %02u:%02u:%02u\n", 
                      network.getLocalNodeID(), localNodeSequenceNum, now.Hour(), now.Minute(), now.Second());
        Serial.printf("KINEMATICS  : Pan Angle = %d° | Tilt Angle = %d°\n", panServo.angle, tiltServo.angle);
        Serial.printf("ENVIRONMENT : V_Bus = %.2fV | I_Draw = %.1fmA | Temp = %.1f°C\n", 
                      sensors.busVoltage, sensors.currentmA, sensors.temperature);
        Serial.println("----------------------------------------------------------------");

        // =========================================================================
        // 2. DISPATCH PHASE: Build and send the structured NetworkPacket over ESP-NOW
        // =========================================================================
        NetworkPacket telemetryPacket;
        
        // Populate the common packet network header
        telemetryPacket.header.sourceID       = network.getLocalNodeID();
        telemetryPacket.header.packetType     = PACKET_TELEMETRY;
        telemetryPacket.header.sequenceNumber = localNodeSequenceNum++;

        // Populate the telemetry data space inside the active union configuration
        telemetryPacket.payload.telemetry.busVoltage  = sensors.busVoltage;
        telemetryPacket.payload.telemetry.currentmA   = sensors.currentmA;
        telemetryPacket.payload.telemetry.temperature = sensors.temperature;
        telemetryPacket.payload.telemetry.humidity    = sensors.humidity;
        telemetryPacket.payload.telemetry.pressure    = sensors.pressure;
        telemetryPacket.payload.telemetry.panAngle    = (int16_t)panServo.angle;
        telemetryPacket.payload.telemetry.tiltAngle   = (int16_t)tiltServo.angle;

        // Broadcast the binary block over the air to the Central Gateway Base Station Hub
        uint8_t targetGatewayMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; 
        esp_err_t sendResult = esp_now_send(targetGatewayMac, (uint8_t *)&telemetryPacket, sizeof(NetworkPacket));
        
        if (sendResult != ESP_OK) {
            Serial.println("[NET ERROR] RF hardware layer rejected packet buffer transmission.");
        }
    }
}

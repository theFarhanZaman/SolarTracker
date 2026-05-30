# ☀️ Advanced Synchronized Dual-Axis Solar Tracker & Distributed WSN Node

A production-grade, highly scalable C++ firmware architecture for distributed Wireless Sensor Networks (WSN) deployed on edge-computing hardware.

[![Platform: ESP32-S3](https://img.shields.io/badge/Platform-ESP32--S3_Supermini-blue.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Framework: Arduino](https://img.shields.io/badge/Framework-Arduino_C++-00979D.svg)](https://www.arduino.cc/)
[![Network: ESP-NOW](https://img.shields.io/badge/Network-ESP--NOW_WSN-success.svg)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html)
[![Architecture: OOP](https://img.shields.io/badge/Architecture-Modular_OOP-orange.svg)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

---

## 📖 System Abstract
This repository implements an enterprise-grade **Dual-Axis Solar Tracker and Atmospheric Monitoring Node** designed around the **ESP32-S3 Supermini** form factor. Moving beyond traditional, isolated tracking configurations, this system enables a decentralized **Wireless Sensor Network (WSN)** capable of cooperative estimation across multiple nodes. 

Through a peer-to-peer, low-overhead **ESP-NOW** mesh backbone, nodes execute real-time orientation synchronization and distributed telemetry aggregation. This structure mitigates localized shading, physical obstructions, and individual sensory anomalies by allowing the cluster to align with the most efficient harvesting node dynamically.

### 🌟 Key Enhancements & Features
* **Decoupled Architecture:** Follows strict Object-Oriented Design and the **Open-Closed Principle**. Hardware drivers, telemetry routing, timekeeping, and networking layers exist as strictly isolated subsystems.
* **Deterministic Event Scheduling:** 100% asynchronous tracking, sampling, and reporting engine built on lightweight `SoftTimer` delta-time tracking—eliminating processor blocking and `delay()` calls.
* **Dynamic Peer Auto-Discovery:** No hardcoded MAC address tables. Nodes utilize hardware-based registers to assign dynamic IDs and apply packet sniffing to learn and register peer nodes on the fly.
* **Precision Energy Analytics:** High-side power tracking via a dedicated I2C INA226 monitor tracking exact micro-level Solar Input Voltage, Current draw, and net Power Generation.
* **Multi-Stage Critical Safety Latches:** Integrated hardware-software safety boundaries mapping LiPo voltage states through an analog attenuation matrix to automatically isolate mechanical components during low-power events.
* **Zero-Drift Kinematics:** Active-duty cycle servo actuation that immediately detaches PWM control lines post-maneuver to eradicate micro-vibration, minimize mechanical gear wear, and completely kill holding current overhead.

---

## 📐 System Topology & Data Flow

The edge node utilizes a highly efficient multi-layered execution plan. Hardware interactions are isolated behind abstraction boundaries, allowing the telemetry routing engine to independently evaluate environmental states before allowing the system controller to settle on motor trajectories.

```
                    +---------------------------------------+
                    |        Environmental Sensors          |
                    |   (LDR Matrix, INA226, BME280, RTC)   |
                    +-------------------+-------------------+
                                        |
                                        v (Asynchronous Polling)
                    +-------------------+-------------------+
                    |           SensorManager               |
                    |    (Data Buffering & Formatting)      |
                    +-------------------+-------------------+
                                        |
                                        v (State Reference)
+-------------------+       +-----------+-----------+       +-------------------+
|  TrackerNetManager|       |   TrackerController   |       |   ManagedServo    |
| (ESP-NOW Routing) |<=====>|  (Decision Engine)    |======>|  (Kinetic Output) |
+-------------------+       +-----------------------+       +-------------------+
```

---

## ⚡ Hardware Realization & Verified Pinouts

> **⚠️ CRITICAL HARDWARE SPECIFICATION:** This deployment explicitly tracks the **ESP32-S3 Supermini Pinout Layout** detailed below. The Left Rail routes the primary high-speed hardware communication protocols (I2C/SPI) along with native PWM tracking outputs, while the Right Rail houses power management infrastructure (`5V`, `GND`, `3V3`) alongside dedicated successive-approximation analog-to-digital converter channels (`ADC1`).

### Verified Master Pin-Allocation Ledger

| Subsystem Target Component | Physical Pin Role / Context | Native Hardware GPIO | Silk-Screen Pin Label | Physical Board Location | Expected Signal Vector |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Pan Servo Motor** | PWM Actuational Control | **GPIO 5** | `D3` (A3) | Left Rail, Pin 1 | 50Hz Pulse Train |
| **Tilt Servo Motor** | PWM Actuational Control | **GPIO 21** | `D6` (TX) | Left Rail, Pin 8 | 50Hz Pulse Train |
| **I2C Shared Bus Link** | SDA (Serial Data Line) | **GPIO 6** | `D4` (SDA) | Left Rail, Pin 2 | Synchronous Open-Drain |
| **I2C Shared Bus Link** | SCL (Serial Clock Line) | **GPIO 7** | `D5` (SCL) | Left Rail, Pin 3 | Synchronous Clock Pulse |
| **DS1302 Real-Time Clock**| RST / Chip Select | **GPIO 9** | `D9` (MISO) | Left Rail, Pin 5 | Logic High Latch |
| **DS1302 Real-Time Clock**| I/O (Serial Data Line) | **GPIO 10** | `D10` (MOSI)| Left Rail, Pin 6 | Bi-Directional Stream |
| **DS1302 Real-Time Clock**| SCLK (Serial Clock) | **GPIO 20** | `D7` (RX) | Left Rail, Pin 7 | Timing Clock Pulse |
| **Photo-Sensor Matrix** | Top-Left LDR Divider | **GPIO 1** | `ADC1-1` | Right Rail, Pin 7 | 12-Bit Analog Input |
| **Photo-Sensor Matrix** | Top-Right LDR Divider | **GPIO 2** | `D0` (A0) | Right Rail, Pin 6 | 12-Bit Analog Input |
| **Photo-Sensor Matrix** | Bottom-Left LDR Divider | **GPIO 3** | `D1` (A1) | Right Rail, Pin 5 | 12-Bit Analog Input |
| **Photo-Sensor Matrix** | Bottom-Right LDR Divider| **GPIO 4** | `D2` (A2) | Right Rail, Pin 4 | 12-Bit Analog Input |
| **LiPo Power Monitoring** | 10kΩ/10kΩ Midpoint | **GPIO 0** | `ADC1-0` | Right Rail, Pin 8 | 12-Bit Attenuated Input |

---

## 🔋 Power Management & Safety Architecture

To guarantee long-term operational survival in remote field environments, the power architecture relies on a decoupled, dual-rail distribution matrix. High-current inductive loads (servos) are isolated from high-precision instrumentation circuits to prevent voltage sags from generating calculation drift.

### 1. High-Side Isolation Tracking
The solar panel energy pathway is routed through an **INA226 Bi-Directional Current/Power Monitor** before interfacing with the charging regulators. Ground references are coupled at a single star-point to mitigate ground bounce during mechanical motor acceleration.
* **VBUS Sensor Pin:** Samples true open-circuit/load voltage directly off the panel.
* **Shunt Resistor Bridge:** Configured via a **0.1Ω** metal-foil resistor to monitor charge input with micro-ampere precision.

### 2. Dual-Rail Step-Up Subsystem
* The system uses a high-density **MT3608 DC-DC Boost Converter** connected directly to the lithium storage cells.
* **Pre-Flight Tuning Step:** The MT3608 trim-potentiometer **must** be adjusted to yield an exact output of **5.0V** under a simulated **1.5A** resistive load prior to connecting the MCU board or servo logic inputs. This 5V output drives the ESP32-S3 input rail and delivers independent operating power to the servos.

### 3. Deep Discharge Software Latch
The deployment of typical lithium cells with compact SE9018 charging topologies leaves the storage battery exposed to catastrophic over-discharge since these basic chargers lack integrated protection ICs. 
* A dedicated **10kΩ / 10kΩ (±0.1% tolerance)** resistor divider network steps down the battery's raw voltage to safe analog inputs on **GPIO 0**.
* **The Rule Engine:** If the sampled runtime potential drops below **3.2V** (V_crit), the system invokes an un-interruptible lock state. The `TrackerController` detaches all servo channels to bring holding current down to zero, suspends WSN transmissions, and sleeps until the incoming solar power pushes the battery bank back past a safe threshold (**3.5V** hysteresis).

---

## 🌐 Dynamic Auto-Discovery ESP-NOW Protocol

The networking layer is designed to scale horizontally without manually rewriting firmware or maintaining rigid tracking lists.

### The Dynamic Learning Loop
1. **Universal Interface Initialization:** At boot, every node registers the universal broadcast address `FF:FF:FF:FF:FF:FF` as a permanent communication peer.
2. **Dynamic Peer Extraction:** Rather than maintaining an explicit list of neighbors, every node captures incoming packets using raw callback context parameters:
   ```cpp
   void TrackerNetManager::OnDataRecv(const esp_now_recv_info *recvInfo, const uint8_t *incomingData, int len)
   ```
3. **Automated Peer Insertion:** The runtime checks the sender’s source MAC address (`recvInfo->src_addr`). If that physical address is not found within the local ESP-NOW tracking tables, the node calls `esp_now_add_peer()` to dynamically add it on the fly.
4. **Cooperative Override Engine:** When a node calculates a clear light gradient update, it broadcasts its positional vectors via a unified `SyncPayload`. If a nearby node experiences localized clouding or sensor failures, it drops its local analog loop and passes control directly to the network payload tracking data to maintain alignment with the cluster.

---

## 📂 Codebase Reference & Architecture Directory

The software architecture completely isolates components to make tracking errors easy to trace and test.

### 1. Unified Network Protocol Layer (`NetworkProtocol.h`)
Defines the binary structural boundaries across the wireless space. Packed attributes ensure identical memory alignment across compilers.
```cpp
#pragma once
#include <Arduino.h>

enum PacketType : uint8_t {
    PACKET_PING_DISCOVERY,
    PACKET_TELEMETRY_SHARE,
    PACKET_ACTION_SYNC
};

struct __attribute__((packed)) NetworkHeader {
    uint8_t  sourceID;        
    uint8_t  packetType;      
    uint32_t sequenceNumber;  
};

struct __attribute__((packed)) SyncPayload {
    uint32_t targetEpoch;     
    int16_t  masterPanAngle;  
    int16_t  masterTiltAngle;
};

struct __attribute__((packed)) NetworkPacket {
    NetworkHeader header;
    union {
        SyncPayload syncData;
    } payload;
};
```

### 2. Custom WSN Network Coordinator (`TrackerNetManager.h`)
Manages the non-blocking asynchronous callback hooks and provides local reference registers.
```cpp
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "NetworkProtocol.h"

class TrackerNetManager {
private:
    uint8_t localNodeID;
    uint32_t seqCounter;
    uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    static void OnDataRecv(const esp_now_recv_info *recvInfo, const uint8_t *incomingData, int len);
    bool registerPeer(const uint8_t *mac_addr);

public:
    static TrackerNetManager* instance; 
    bool hasPendingSync;
    SyncPayload latestSyncCommand;

    TrackerNetManager() : localNodeID(0), seqCounter(0), hasPendingSync(false) { instance = this; }
    
    bool begin();
    bool broadcastSync(const SyncPayload &syncData);
    uint8_t getLocalNodeID() const { return localNodeID; }
};
```

---

## 💻 Compilation & Deployment Manual

### Firmware Dependencies
Ensure these libraries are available within your compiler's environment pathing toolchain before kicking off a build:

| Library Name | Official Maintainer | Intended Abstraction Target |
| :--- | :--- | :--- |
| **ESP32Servo** | Kevin Harrington | Hardware Timer PWM Control for ESP32 Architecture |
| **Adafruit BME280** | Adafruit | Digital Barometric and Humidity Interface |
| **Adafruit MPU6050** | Adafruit | 6-DOF Micro-Electro-Mechanical Accel/Gyro Sensor |
| **INA226** | Korneliusz Jarzebski | Precision Bus Voltage and High-Side Current Monitor |
| **Rtc_by_Makuna** | Makuna | Asynchronous Serial Real-Time Clock Core |

### Deployment Protocol
1. Clone the repository directory structure locally to your terminal workbench.
2. Initialize the project file root using your preferred development IDE (e.g., VSCode with PlatformIO or the Arduino IDE extension).
3. Select **ESP32S3 Dev Module** as the primary hardware compilation target.
4. Verify the compiler flags are updated to include:
   * **USB CDC On Boot:** `Enabled` (Crucial for streaming standard runtime diagnostics out of the native Supermini Type-C interface).
5. Compile the binary and execute an upload sequence while holding down the physical boot pin if the device fails to enter the automatic flashing sequence.
6. Launch your terminal monitoring software at a baud rate of **115200** to inspect the tracking loops and dynamic ID assignment.

---

## 📈 Future System Roadmap
* **Astronomical Ephemeris Integration:** Implementing high-precision solar positioning algorithms (SPA) based on solar time offsets to run predictive positioning on cloudy days.
* **Decentralized Master Election (Raft Protocol Mini):** Implementing a dynamic voting mechanism across nodes so that if a primary tracking node drops offline, the remaining nodes elect a new cluster head.
* **Aerodynamic Drag Protection Latch:** Utilizing real-time MPU6050 accelerometer frequencies to flag mechanical vibration spikes and park the panel matrix horizontally during dangerous winds.

---

## 📝 License Summary
This system is licensed under the open-source **MIT License**. Review the `LICENSE` file for legal definitions regarding distribution and commercial production usage permissions.
```

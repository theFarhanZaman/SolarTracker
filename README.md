# ☀️ SolarTracker Sky-Link: Distributed Solar WSN & Command Gateway

A production-grade, highly scalable C++ firmware and full-stack React/FastAPI architecture for distributed Wireless Sensor Networks (WSN) and autonomous solar tracking. Designed for resilient, decentralized operations in disaster-response scenarios and remote edge-computing deployments.

[![Platform: ESP32-S3](https://img.shields.io/badge/Platform-ESP32--S3_Supermini-blue.svg)](https://www.espressif.com/)
[![Framework: Arduino & ESP-IDF](https://img.shields.io/badge/Framework-Arduino_C++-00979D.svg)](https://www.arduino.cc/)
[![Frontend: React & Vite](https://img.shields.io/badge/Frontend-React_TS-61DAFB.svg)](https://react.dev/)
[![Backend: FastAPI](https://img.shields.io/badge/Backend-FastAPI-009688.svg)](https://fastapi.tiangolo.com/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

---

## 📖 System Abstract
The **Sky-Link Ecosystem** is an enterprise-grade hardware and software platform designed to manage decentralized tracking and atmospheric monitoring nodes. Moving beyond isolated microcontrollers, this architecture establishes a resilient **ESP-NOW Mesh Backbone** bridged to a high-speed Python/FastAPI gateway. 

Data is streamed in real-time to a dark-mode React Command Deck equipped with a synthetic fallback simulation engine. The system leverages machine learning (K-Means clustering) to analyze microclimate patterns, allowing the swarm to adapt to overcast weather, approaching storms, or localized hardware faults dynamically.

### 🌟 Core Architectural Milestones
* **Decoupled Cooperative Multitasking:** Edge nodes run 100% asynchronous tracking, sampling, and reporting engines built on a lightweight `SoftTimer` protocol, eliminating all blocking `delay()` calls.
* **Dual-Mode Dashboard Execution:** The React command interface seamlessly transitions between live physical telemetry and a high-fidelity synthetic simulation engine if the hardware gateway drops, ensuring tactical charts never freeze.
* **Hardware-Abstracted OLED Subsystems:** Both Edge Nodes and the Central Gateway feature independent, non-blocking I2C `DisplayManager` classes for localized diagnostic readouts (Power, Temp, Active Nodes, Packet TX) without impacting loop execution.
* **Master/Worker Arbitration Pipeline:** Dynamic ESP-NOW peer discovery with a strict hierarchy that prevents broadcast storms. Swarm leaders dictate global tracking epochs, while the ML Hub pushes predictive bias overrides.
* **Deep-Discharge Hardware Safety:** Integrated analog attenuation matrices constantly monitor LiPo potentials, invoking an un-interruptible mechanical safe-lock if voltages drop below 3.2V.

---

## 📐 System Topology & Data Flow

The platform separates execution into three distinct isolated domains: **Edge Acquisition**, **Gateway Aggregation**, and **Command Intelligence**.

```text
[ EDGE NODE SWARM (ESP32-S3) ]
  |-- Sensors: INA226 (Power), BME280 (Atmos), LDR Matrix
  |-- Actuators: PWM Pan/Tilt Servos
  |-- Output: Local SSD1306 Display
  | 
  | (ESP-NOW 2.4GHz Mesh Protocol)
  v
[ CENTRAL GATEWAY (ESP32-S3) ]
  |-- Role: High-Speed RF Bridge & Heartbeat Matrix Tracker
  |-- Output: Local SSD1306 Diagnostic Display
  |
  | (921600 Baud USB-CDC)
  v
[ INTELLIGENCE HUB & BACKEND (FastAPI / Python) ]
  |-- Role: Serial Ingestion & K-Means Inference Engine
  |
  | (Full-Duplex WebSockets)
  v
[ COMMAND DECK (React / TypeScript / Recharts) ]
  |-- Role: Tactical Dashboard, Analytics Deck, Override Controls
```

---

## ⚡ Hardware Realization & Verified Pinouts

> **⚠️ CRITICAL HARDWARE SPECIFICATION:** This deployment is optimized strictly for the **ESP32-S3 Supermini Edge Headers** where all assignments remain bound below **GPIO 13**. To minimize electromagnetic cross-coupling and maintain clear tracing, the infrastructure segregates analog input sensors and the I2C bus down the Left Rail, while routing mechanical PWM control and high-speed clock timing lines down the Right Rail.
> ![ESP32-S3 Supermini Verified Pinout Diagram](Assets/esp32_s3_supermini_pinout.jpg)


### Core Pin Allocation Ledger

| Target Component | Physical Pin Role / Context | ESP32-S3 GPIO | Signal Vector |
| :--- | :--- | :--- | :--- |
| **Top-Left LDR** | Analog Matrix Coordinate | **GPIO 1** | 12-Bit Analog Input |
| **Top-Right LDR** | Analog Matrix Coordinate | **GPIO 2** | 12-Bit Analog Input |
| **Bottom-Left LDR** | Analog Matrix Coordinate | **GPIO 3** | 12-Bit Analog Input |
| **Bottom-Right LDR** | Analog Matrix Coordinate | **GPIO 4** | 12-Bit Analog Input |
| **LiPo Monitor** | 10k/10k Midpoint Voltage | **GPIO 5** | 12-Bit Attenuated Input |
| **I2C SDA** | Shared Sensor Bus Data | **GPIO 6** | Open-Drain (BME/INA/OLED) |
| **I2C SCL** | Shared Sensor Bus Clock | **GPIO 7** | Synchronous Clock Pulse |
| **Pan Servo** | PWM Actuational Control | **GPIO 8** | 50Hz Pulse Train |
| **Tilt Servo** | PWM Actuational Control | **GPIO 9** | 50Hz Pulse Train |
| **DS1302 RTC** | Chip Select (RST) | **GPIO 11** | Logic High Latch |
| **DS1302 RTC** | Serial Data (I/O) | **GPIO 12** | Bi-Directional Stream |
| **DS1302 RTC** | Serial Clock (CLK) | **GPIO 13** | Timing Clock Pulse |

---
## 📋 Master Pin Connection & Netlist Table

| From Component Pin | To Component Pin | Wire Specification | Net Type / Signal Role |
| :--- | :--- | :--- | :--- |
| **Solar Panel (+)** | INA219 Terminal `IN+` | 30 AWG Single Core | Raw Harvest VCC Ingest |
| **INA219 Terminal `IN-`** | SE9018 Module `IN+` | 30 AWG Single Core | Monitored Charge Path |
| **Solar Panel (-)** | SE9018 Module `IN-` | 30 AWG Single Core | Panel Ground Return |
| **SE9018 `BAT+`** | 1S LiPo (+) & Boost `VIN+` | 30 AWG Single Core | Raw Battery Voltage Potentials |
| **SE9018 `GND-`** | 1S LiPo (-) & Boost `VIN-` | 3x Twisted 30 AWG | **Master Ground Node** |
| **Boost Converter `VOUT+`**| ESP32 `5V` & Servo `VCC` | 2x Twisted 30 AWG | **Regulated 5V Rail** |
| **Boost Converter `VOUT-`**| Master Ground Trunk | 3x Twisted 30 AWG | Main Ground System Sink |
| **ESP32-S3 `3V3`** | Sensors, RTC, LDR Matrix | 30 AWG Single Core | **Clean 3.3V Logic Bus** |
| **ESP32-S3 `GND`** | Master Ground Trunk | 30 AWG Single Core | MCU Ground Reference |
| **ESP32-S3 `GPIO 1`** | Top-Left LDR Divider Node | 30 AWG Single Core | Analog Input (ADC1_CH0) |
| **ESP32-S3 `GPIO 2`** | Top-Right LDR Divider Node | 30 AWG Single Core | Analog Input (ADC1_CH1) |
| **ESP32-S3 `GPIO 3`** | Bottom-Left LDR Divider Node | 30 AWG Single Core | Analog Input (ADC1_CH2) |
| **ESP32-S3 `GPIO 4`** | Bottom-Right LDR Divider Node| 30 AWG Single Core | Analog Input (ADC1_CH3) |
| **ESP32-S3 `GPIO 5`** | 10k/10k LiPo Attenuation Node| 30 AWG Single Core | Analog Input (Battery Health) |
| **ESP32-S3 `GPIO 6`** | INA219 / BMP280 / MPU6050 / Display `SDA` | 30 AWG Single Core | I2C Synchronous Data Line |
| **ESP32-S3 `GPIO 7`** | INA219 / BMP280 / MPU6050 / Display `SCL` | 30 AWG Single Core | I2C Synchronous Clock Line |
| **ESP32-S3 `GPIO 8`** | Pan Servo Signal Wire (Orange) | 30 AWG Single Core | 50Hz PWM Actuation Vector |
| **ESP32-S3 `GPIO 9`** | Tilt Servo Signal Wire (Orange)| 30 AWG Single Core | 50Hz PWM Actuation Vector |
| **ESP32-S3 `GPIO 11`**| DS1302 Module `RST` | 30 AWG Single Core | Chip Select Latch Line |
| **ESP32-S3 `GPIO 12`**| DS1302 Module `DAT` | 30 AWG Single Core | 3-Wire Serial Data Stream |
| **ESP32-S3 `GPIO 13`**| DS1302 Module `CLK` | 30 AWG Single Core | Serial Clock Timing Train |

## 🔋 Power Management & Safety Architecture

To guarantee long-term operational survival in remote field environments, the power architecture relies on a decoupled, dual-rail distribution matrix. High-current inductive loads (servos) are isolated from high-precision instrumentation circuits to prevent voltage sags that could cause calculation drift.

### 1. High-Side Isolation Tracking
The solar panel energy pathway is routed through an **INA226 Bi-Directional Current/Power Monitor** before interfacing with the charging regulators. Ground references are coupled at a single star-point to mitigate ground bounce during mechanical motor acceleration.
* **VBUS Sensor Pin:** Samples true open-circuit/load voltage directly off the panel.
* **Shunt Resistor Bridge:** Configured via a **0.1Ω** metal-foil resistor to monitor charge input with micro-ampere precision.

### 2. Dual-Rail Step-Up Subsystem
* The system uses a high-density **MT3608 DC-DC Boost Converter** connected directly to the lithium storage cells.
* **Pre-Flight Tuning Step:** The MT3608 trim-potentiometer **must** be adjusted to yield an exact output of **5.0V** under a simulated **1.5A** resistive load prior to connecting the MCU board or servo logic inputs. This 5V output drives the ESP32-S3 input rail and delivers independent operating power to the servos.

### 3. Deep Discharge Software Latch
* A dedicated **10kΩ / 10kΩ (±0.1% tolerance)** resistor divider network steps down the battery's raw voltage to safe analog inputs on **GPIO 5**.
* **The Rule Engine:** If the sampled runtime potential drops below **3.2V** (V_crit), the system invokes an un-interruptible lock state. The `TrackerController` detaches all servo channels to bring holding current down to zero, suspends WSN transmissions, and sleeps until the incoming solar power pushes the battery bank back past a safe threshold (**3.5V** hysteresis).

---

## 🌐 Dynamic Auto-Discovery ESP-NOW Protocol

The networking layer is designed to scale horizontally without manually rewriting firmware or maintaining rigid tracking lists.

### The Dynamic Learning Loop
1. **Universal Interface Initialization:** At boot, every node registers the universal broadcast address `FF:FF:FF:FF:FF:FF` as a permanent communication peer.
2. **Dynamic Peer Extraction:** Every node captures incoming packets using raw callback context parameters.
3. **Automated Peer Insertion:** The runtime checks the sender’s source MAC address (`recvInfo->src_addr`). If that physical address is not found within the local ESP-NOW tracking tables, the node calls `esp_now_add_peer()` to dynamically add it on the fly.
4. **Cooperative Override Engine:** When a node calculates a clear light gradient update, it broadcasts its positional vectors via a unified `SyncPayload`. If a nearby node experiences localized clouding, it drops its local analog loop and passes control directly to the network payload to maintain alignment with the cluster.

---

## 🧠 Automated Machine Learning Expansion Framework

Integrating Python into your architecture opens up access to the entire modern data science and machine learning ecosystem (`scikit-learn`, `pandas`, `numpy`). 

### Phase 1: Unsupervised State Discovery (K-Means)
Analyze historical multi-node telemetry logs to find hidden structural boundaries in the data.

```python
import numpy as np
import pandas as pd
from sklearn.cluster import KMeans
from sklearn.preprocessing import StandardScaler
import joblib

# Ingest historical multi-node telemetry logs
data = pd.read_csv("historical_wsn_telemetry.csv", names=[
    "Prefix", "NodeID", "Voltage", "Current", "Temp", "Humidity", "Pressure", "Pan", "Tilt"
])

features = ["Temp", "Humidity", "Pressure", "Voltage"]
X = data[features]

scaler = StandardScaler()
X_scaled = scaler.fit_transform(X)

# K=4 maps to clear, overcast, storm fronts, and localized array anomalies
kmeans = KMeans(n_clusters=4, random_state=42, n_init=10)
data["SystemState"] = kmeans.fit_predict(X_scaled)

joblib.dump(scaler, "feature_scaler.pkl")
joblib.dump(kmeans, "kmeans_core_model.pkl")
```

### Phase 2: Live Inference Loop & Feedback Engine
A real-time Python script executing live classification and immediately pushing hardware overrides back down the wire.

```python
import serial
import joblib
import numpy as np

ser = serial.Serial('/dev/ttyACM0', 921600, timeout=0.1)
scaler = joblib.load("feature_scaler.pkl")
kmeans = joblib.load("kmeans_core_model.pkl")

while True:
    if ser.in_waiting > 0:
        raw_line = ser.readline().decode('utf-8', errors='ignore').strip()
        
        if raw_line.startswith("DATA"):
            parts = raw_line.split(',')
            node_id = parts[1]
            v_in, c_out, temp, hum, press = map(float, parts[2:7])
            
            raw_vector = np.array([[temp, hum, press, v_in]])
            scaled_vector = scaler.transform(raw_vector)
            assigned_cluster = kmeans.predict(scaled_vector)[0]
            
            # Cluster 2 Example: Weather metrics match a severe incoming storm profile
            if assigned_cluster == 2: 
                command = f"CMD,{node_id},PARK_FLAT,0,0\n"
                ser.write(command.encode('utf-8'))
```

---

## 🛠️ Software Stack & Architectures

### 1. Edge Node Firmware (`SolarTracker.ino`)
* **SensorManager:** Encapsulates the I2C bus, unifying data from the BME280 and INA226.
* **DisplayManager:** Drives a local SSD1306 OLED screen, updating key metrics non-blockingly at a 1Hz cadence.
* **EdgeNodeReceiver:** The command arbitration pipeline. Decides whether to follow local analog light algorithms, sync with the swarm master, or execute a direct machine learning structural override.
* **ManagedServo:** Wraps PWM functionality with an automatic `detach()` timer, eliminating micro-jitter.

### 2. Central Gateway Firmware (`CentralGateway.ino`)
* Acts as the translation layer between wireless RF and the local host machine.
* Maintains a **Heartbeat Timeout Matrix**, tracking the 15-second lifespan of every incoming node packet.
* Features a `GatewayDisplayManager` to output active connection states, ESP-NOW protocol health, and upstream packet transmission totals directly to an attached OLED.

### 3. Sky-Link Command Center (React/Vite)
* Built on React, TypeScript, and Tailwind CSS.
* **Live Fallback Engine:** If the WebSocket drops, an internal data synthesis engine takes over, calculating sinusoidal thermal drift and realistic voltage jitter to keep the Recharts analytics matrices moving smoothly.
* **Hardware Control Tab:** Allows operators to manually force Actuator Optimization Cores via an intuitive sliding interface.

### 4. Intelligence Hub (FastAPI/Python)
* Scrapes offline CSV historical telemetry logs to train a `scikit-learn` model.
* Serves data frames over a real-time WebSocket protocol layer to the frontend.

---

## 🚀 Deployment & Quick Start Manual

### Phase A: Firmware Flashing
1. Open the project root in PlatformIO or the Arduino IDE.
2. Select **ESP32S3 Dev Module** as the target. Enable **USB CDC On Boot** in your compiler flags.
3. Install necessary C++ dependencies: `ESP32Servo`, `Adafruit BME280`, `Adafruit SSD1306`, `INA226`, and `Rtc_by_Makuna`.
4. Flash `CentralGateway.ino` to the hub node, and `SolarTracker.ino` to your respective edge tracking units.

### Phase B: Launching the FastAPI Backend
Ensure your host machine (Linux/Ubuntu) has user permissions to read serial ports (`sudo usermod -aG dialout $USER`).
```bash
# 1. Navigate to the backend directory
cd "Gateway Application"

# 2. Establish isolated environment
python3 -m venv venv
source venv/bin/activate
pip install fastapi uvicorn pyserial scikit-learn numpy pandas joblib

# 3. Ignite the Uvicorn ASGI server
python3 -m uvicorn main:app --host 0.0.0.0 --port 8000 --reload
```

### Phase C: Booting the React Command Deck
In a new terminal window:
```bash
# 1. Navigate to the UI directory
cd "Gateway Application/gateway-ui"

# 2. Build the dependency tree
npm install

# 3. Launch the Vite Hot-Reloading environment
npm run dev
```
Navigate to `http://localhost:5173`. If hardware is physically connected, the status badge will indicate **LIVE**. If disconnected, the system will transparently invoke the **SIMULATION** engine.

---

## 🔍 Troubleshooting Matrix

| Symptoms Encountered | Core Engineering Root Cause | Corrective Operations |
| :--- | :--- | :--- |
| `npm error code ENOENT` | The execution command was fired while standing outside the path scope containing `package.json`. | Run `cd "Gateway Application/gateway-ui"` before executing npm routines. |
| Dashboard permanently says **`Connecting`** | The React UI is functional but cannot verify a connection bridge to port `8000`. | Check your backend terminal window. Ensure `uvicorn` is initialized on port 8000. |
| Python script crashes with `SerialException` | The user account lacks standard system group permissions to tap into physical system hardware registers. | Run `sudo usermod -aG dialout $USER`, then log completely out of your operating system session and back in. |
| I2C Devices Not Found | OLED initialized before `Wire.begin()`. | Ensure `sensors.init()` executes prior to `oledDisplay.init()` in the setup sequence. |

---

## 📈 Future System Roadmap
* **Decentralized Master Election (Raft Protocol Mini):** Upgrading the ESP-NOW network to utilize a dynamic voting mechanism. If the primary master node goes dark, the remaining edge nodes will autonomously elect a new sync leader.
* **Aerodynamic Drag Protection Latch:** Utilizing onboard MPU6050 accelerometer frequencies to flag physical vibration spikes, parking the solar matrix horizontally during dangerous wind conditions.
* **Astronomical Ephemeris Integration:** Implementing high-precision solar positioning algorithms (SPA) based on solar time offsets to run predictive positioning on cloudy days.

---

## 📝 License Summary
This system is licensed under the open-source **MIT License**. Review the `LICENSE` file for legal definitions regarding distribution and commercial production usage permissions.

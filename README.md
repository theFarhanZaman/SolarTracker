# Hardware Configuration & Wiring Guide

This document outlines the complete hardware architecture, pin mapping, and power distribution for the Advanced Modular Dual-Axis Solar Tracker and Environmental Monitor. The system is designed around the **ESP32-S3 Supermini** development board, emphasizing low-power optimization, autonomous battery protection, and high-side solar harvesting telemetry.

---

## 1. Master Pinout Mapping Table

The following matrix maps every physical subsystem component directly to the corresponding GPIO on the specific layout of the ESP32-S3 Supermini.

| Subsystem Component | Component Pin / Wire | ESP32-S3 Supermini GPIO | Physical Board Location | Signal Type / Protocol |
| :--- | :--- | :--- | :--- | :--- |
| **Pan Servo** | PWM (Orange/Yellow) | **GPIO 5** | Left Rail, Pin 1 (D3) | Hardware PWM Output |
| **Tilt Servo** | PWM (Orange/Yellow) | **GPIO 21** | Left Rail, Pin 8 (D6) | Hardware PWM Output |
| **INA226 (Solar)** | SDA | **GPIO 6** | Left Rail, Pin 2 (SDA) | I2C Data Bus (Shared) |
| **INA226 (Solar)** | SCL | **GPIO 7** | Left Rail, Pin 3 (SCL) | I2C Clock Bus (Shared) |
| **MPU6050 (IMU)** | SDA | **GPIO 6** | Left Rail, Pin 2 (SDA) | I2C Data Bus (Shared) |
| **MPU6050 (IMU)** | SCL | **GPIO 7** | Left Rail, Pin 3 (SCL) | I2C Clock Bus (Shared) |
| **BME280 (Env)** | SDA | **GPIO 6** | Left Rail, Pin 2 (SDA) | I2C Data Bus (Shared) |
| **BME280 (Env)** | SCL | **GPIO 7** | Left Rail, Pin 3 (SCL) | I2C Clock Bus (Shared) |
| **DS1302 RTC** | RST | **GPIO 9** | Left Rail, Pin 5 (D9) | Digital Output (Reset) |
| **DS1302 RTC** | DAT (I/O) | **GPIO 10** | Left Rail, Pin 6 (D10) | Bi-directional Serial Data |
| **DS1302 RTC** | CLK | **GPIO 20** | Left Rail, Pin 7 (D7) | Digital Output (Clock) |
| **Top Left LDR** | Divider Center | **GPIO 1** | Right Rail, Pin 7 (ADC1-1) | Analog Input |
| **Top Right LDR** | Divider Center | **GPIO 2** | Right Rail, Pin 6 (A0) | Analog Input |
| **Bottom Left LDR**| Divider Center | **GPIO 3** | Right Rail, Pin 5 (A1) | Analog Input |
| **Bottom Right LDR**| Divider Center | **GPIO 4** | Right Rail, Pin 4 (A2) | Analog Input |
| **LiPo Battery** | 10k/10k Midpoint | **GPIO 0** | Right Rail, Pin 8 (ADC1-0) | Analog Input (Telemetry) |

---

## 2. Power Architecture & Integration

To safely isolate high-current mechanical surges from the high-accuracy instrumentation layer, the system utilizes a split-bus power topology.

### A. Solar Charging & High-Side Telemetry Loop
To ensure the dashboard reads true energy yield before losses, the solar panel is routed directly through the INA226 current shunt resistor:
1. **Solar Panel [+]** $\rightarrow$ Connects to **IN+** and **VBUS** pins on the INA226 Power Monitor module.
2. **INA226 IN-** $\rightarrow$ Connects to **IN+** on the SE9018 MPPT Lithium Battery Charger.
3. **Solar Panel [-]** $\rightarrow$ Connects to the **Main System Ground Rail**.
4. **SE9018 IN-** $\rightarrow$ Connects to the **Main System Ground Rail**.

### B. LiPo Storage & Regulated 5V Servo Rail
The SE9018 autonomously manages the LiPo charging profile. The battery power is then boosted to a clean, stable 5V for kinetic deployment:
1. **SE9018 BAT+** $\rightarrow$ Connects to **LiPo Battery [+]** $\rightarrow$ Connects to **MT3608 Boost Converter VIN+**.
2. **SE9018 BAT-** $\rightarrow$ Connects to **LiPo Battery [-]** $\rightarrow$ Connects to **MT3608 Boost Converter VIN-**.
3. **MT3608 VOUT+** *(Must be manually preset to exactly 5.0V using a multimeter before connection)* $\rightarrow$ Connects to the **Main 5V Power Rail**:
   * Powers the ESP32-S3 Supermini **5V Pin** (Right Rail, Pin 1).
   * Powers the **5V Power Pin** (Red wire) on both the Pan and Tilt Servos.
4. **MT3608 VOUT-** $\rightarrow$ Connects to the **Main System Ground Rail**:
   * Ties to ESP32-S3 **GND Pin** (Right Rail, Pin 2), Servo grounds (Brown/Black wire), and all module grounds.

### C. Over-Discharge Telemetry Safeguard (Resistor Divider)
Because the standard SE9018 module lacks low-voltage cut-off circuitry, a hardware voltage divider drops the battery voltage down to safe 3.3V logic levels for firmware monitoring:
* **LiPo Battery [+]** $\rightarrow$ `10kΩ Resistor` $\rightarrow$ **Junction Node** $\rightarrow$ `10kΩ Resistor` $\rightarrow$ **Main System Ground Rail**.
* **Junction Node** $\rightarrow$ Connects directly to **GPIO 0 (ADC1-0)** on the ESP32-S3.

---

## 3. Sensor Bus Connections

### I2C Shared Instrumentation Bus
The **INA226**, **MPU6050**, and **BME280** are connected in parallel on the native I2C bus:
* **SDA Line:** Connect all sensor `SDA` pins together $\rightarrow$ **GPIO 6** (Left Rail, Pin 2).
* **SCL Line:** Connect all sensor `SCL` pins together $\rightarrow$ **GPIO 7** (Left Rail, Pin 3).
* **Logic Power:** Connect all sensor `VCC` / `VIN` pins to the ESP32-S3 **3V3 Pin** (Right Rail, Pin 3) for clean 3.3V logic signaling.

### LDR Light Sensor Arrays
Each of the four light-dependent resistors must be wired in a voltage divider circuit to output accurate lux differentials:
* Connect one leg of the LDR to the ESP32's **3V3 Pin**.
* Connect the other leg to its corresponding analog input pin (**GPIO 1, 2, 3, or 4**).
* Connect a **10kΩ pull-down resistor** from that exact GPIO junction point straight down to the **Main System Ground Rail**.

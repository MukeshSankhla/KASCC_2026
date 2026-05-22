# 🚀 KASCC 2026 — ESP8266 IoT Projects Collection

A collection of **7 hands-on IoT projects** built with the ESP8266 (NodeMCU) microcontroller. Each project demonstrates real-world sensor interfacing, embedded web dashboards, and Access Point (AP) mode networking — all running **offline without internet**.

---

## 📂 Projects Overview

| # | Project Title | Relevant SDG(s) | Code & Docs |
|---|---------------|-----------------|-------------|
| 1 | **Smart Distance Monitoring System** using Ultrasonic Sensor and NodeMCU ESP8266 | 🏗️ **SDG 9**: Industry, Innovation & Infrastructure<br>🏙️ **SDG 11**: Sustainable Cities & Communities | [Smart_Distance_Monitoring_System](./Smart_Distance_Monitoring_System/) |
| 2 | **IoT-Based Ambient Light Intensity Analyzer** using NodeMCU | ⚡ **SDG 7**: Affordable & Clean Energy<br>🌍 **SDG 13**: Climate Action | [Ambient_Light_Intensity_Analyzer](./Ambient_Light_Intensity_Analyzer/) |
| 3 | **Smart Soil Moisture Monitoring System for Precision Irrigation** | 🌾 **SDG 2**: Zero Hunger<br>💧 **SDG 6**: Clean Water & Sanitation | [Smart_Soil_Moisture_Monitoring_System](./Smart_Soil_Moisture_Monitoring_System/) |
| 4 | **ClimateSense**: IoT Temperature & Humidity Monitoring using DHT11 | 🏙️ **SDG 11**: Sustainable Cities & Communities<br>🌍 **SDG 13**: Climate Action | [ClimateSense](./ClimateSense/) |
| 5 | **IoT Obstacle Detection and Alert System** using IR Sensor | 🏥 **SDG 3**: Good Health & Well-Being<br>🏗️ **SDG 9**: Industry, Innovation & Infrastructure | [IoT_Obstacle_Detection_System](./IoT_Obstacle_Detection_System/) |
| 6 | **HeartCare**: IoT Heart Beat Monitoring and Visualization System | 🏥 **SDG 3**: Good Health & Well-Being | [HeartCare](./HeartCare/) |
| 7 | **Smart Motion Radar** with Servo Rotation using Ultrasonic Sensor and NodeMCU | 🏗️ **SDG 9**: Industry, Innovation & Infrastructure<br>🏙️ **SDG 11**: Sustainable Cities & Communities | [Smart_Motion_Radar](./Smart_Motion_Radar/) |

---

## 🛠️ Development Environment Setup

Follow these steps to set up everything you need to program the ESP8266 NodeMCU board.

### Step 1: Install Arduino IDE

1. Go to the official Arduino website: **https://www.arduino.cc/en/software**
2. Download **Arduino IDE 2.x** for your operating system (Windows / macOS / Linux)
3. Run the installer and follow the on-screen instructions
4. Launch Arduino IDE after installation

> **Note:** Arduino IDE 2.x is recommended for the latest features and better performance. Arduino IDE 1.8.x also works if preferred.

---

### Step 2: Install CP210x USB-to-UART Driver

The NodeMCU uses a **CP2102 / CP2104** USB-to-Serial chip. Your computer needs its driver to communicate with the board.

1. Go to the Silicon Labs driver page: **https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers**
2. Download the **CP210x Universal Windows Driver** (or macOS/Linux version)
3. Extract the ZIP file
4. Run the installer (`CP210xVCPInstaller_x64.exe` for 64-bit Windows)
5. Restart your computer after installation

**Verify Installation:**
- Plug in your NodeMCU via USB cable
- Open **Device Manager** (Windows) → Look under **Ports (COM & LPT)**
- You should see **Silicon Labs CP210x USB to UART Bridge (COMx)**
- Note the **COM port number** (e.g., COM3) — you'll need this later

> **Troubleshooting:** If the device shows as "Unknown Device" with a yellow warning, try a different USB cable (some are charge-only and don't support data).

---

### Step 3: Install ESP8266 Board Manager

The Arduino IDE doesn't support ESP8266 by default. You need to add it via the Board Manager.

1. Go to **File → Preferences** (or `Ctrl + ,`)
2. In the **"Additional Board Manager URLs"** field, paste this URL:

```
https://arduino.esp8266.com/stable/package_esp8266com_index.json
```

> If you already have other URLs there, separate them with a comma.

3. Click **OK**
4. Go to **Tools → Board → Boards Manager...**
5. Search for **"esp8266"**
6. Find **"esp8266 by ESP8266 Community"** and click **Install**
7. Wait for the installation to complete (this may take a few minutes)

**Configure Board Settings:**

After installation, go to **Tools** and set:

| Setting | Value |
|---------|-------|
| Board | `NodeMCU 1.0 (ESP-12E Module)` |
| Upload Speed | `115200` |
| CPU Frequency | `80 MHz` |
| Flash Size | `4MB (FS:2MB OTA:~1019KB)` |
| Port | Your COM port (e.g., `COM3`) |

---

### Step 4: Install Required Arduino Libraries

Some projects require external libraries. Install them via the **Library Manager**:

1. Go to **Sketch → Include Library → Manage Libraries...** (or `Ctrl + Shift + I`)
2. Search and install each library listed below:

#### Required Libraries

| Library | Used By | How to Install |
|---------|---------|----------------|
| **DHT11** | ClimateSense | Search `DHT11` in Library Manager → Install by **Dhruba Saha** |
| **Servo** | Smart Motion Radar | Usually pre-installed. If not, search `Servo` → Install by **Arduino** |
| **ESP8266WiFi** | All projects | Included with ESP8266 Board Manager (no separate install needed) |
| **ESP8266WebServer** | All web projects | Included with ESP8266 Board Manager (no separate install needed) |

> **Important:** The `ESP8266WiFi` and `ESP8266WebServer` libraries are automatically installed when you install the ESP8266 Board Manager in Step 3. Do NOT try to install them separately.

---

### Step 5: Upload Your First Sketch

1. Open any `.ino` file from this repository (e.g., `HeartCare/HeartCare.ino`)
2. Connect your NodeMCU via USB
3. Select the correct **Board** and **Port** from the **Tools** menu
4. Click the **Upload** button (→ arrow) or press `Ctrl + U`
5. Wait for the upload to complete — you should see `Done uploading.` in the console
6. Open **Serial Monitor** (`Ctrl + Shift + M`) and set baud rate to **9600** to see debug output

---

## 📡 How to Use the Web Dashboards

Each project acts as an offline, standalone **Wi-Fi Access Point** (AP) that hosts its own dashboard.

1. **Upload** the sketch to the NodeMCU board.
2. On your phone, tablet, or laptop, open your **Wi-Fi Settings**.
3. Scan for networks and connect to the specific project's network (SSID):
   
   | # | Project Name | Wi-Fi Access Point SSID | Password | IP Address |
   |---|--------------|-------------------------|----------|------------|
   | 1 | Smart Distance Monitoring System | `MeasureTool_AP` | `12345678` | `192.168.4.1` |
   | 2 | Ambient Light Intensity Analyzer | `LuxMeter_AP` | `12345678` | `192.168.4.1` |
   | 3 | Smart Soil Moisture Monitoring System | `AgriMonitor_AP` | `12345678` | `192.168.4.1` |
   | 4 | ClimateSense | `ClimateSense_AP` | `12345678` | `192.168.4.1` |
   | 5 | IoT Obstacle Detection & Alert System | `ObstacleAlert_AP` | `12345678` | `192.168.4.1` |
   | 6 | HeartCare | `HeartCare_AP` | `12345678` | `192.168.4.1` |
   | 7 | Smart Motion Radar | `SmartRadar_AP` | `12345678` | `192.168.4.1` |

4. When prompted, enter the password: **`12345678`**
5. Open your web browser and navigate to: **`http://192.168.4.1`**
6. The interactive web dashboard will load with live telemetry and visualizations.

> **No Internet Connection Required!** The ESP8266 acts as a self-contained local server. Everything runs directly on the hardware.

---

## 📋 Common Pin Connections

| NodeMCU Pin | GPIO | Common Use in Projects |
|-------------|------|----------------------|
| A0 | ADC0 | Analog sensors (Pulse, Soil, TEMT6000) |
| D2 | GPIO4 | LED indicator / Servo signal |
| D4 | GPIO2 | DHT11 data pin |
| D5 | GPIO14 | HC-SR04 Trigger |
| D6 | GPIO12 | HC-SR04 Echo (use voltage divider!) |
| D7 | GPIO13 | Buzzer |

> ⚠️ **Voltage Divider Required:** The HC-SR04 Echo pin outputs 5V, but ESP8266 GPIO pins are **3.3V tolerant only**. Use a simple resistor voltage divider (1kΩ + 2kΩ) to step down the voltage.

---

## 📜 License

These projects are created for educational purposes as part of KASCC 2026. Feel free to use, modify, and learn from them.

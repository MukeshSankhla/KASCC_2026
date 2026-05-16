# 🚀 KASCC 2026 — ESP8266 IoT Projects Collection

A collection of **7 hands-on IoT projects** built with the ESP8266 (NodeMCU) microcontroller. Each project demonstrates real-world sensor interfacing, embedded web dashboards, and Access Point (AP) mode networking — all running **offline without internet**.

---

## 📂 Projects Overview

| # | Project | Sensor | Dashboard | Description |
|---|---------|--------|-----------|-------------|
| 1 | [Oximeter](./Oximeter/) | Pulse Sensor | ✅ Beating Heart UI | Heart rate monitor with BPM-synced animated heart & ECG waveform |
| 2 | [Weather Station](./ESP8266_DHT11_Dashboard/) | DHT11 | ✅ Radial Gauges | Temperature & humidity monitor with animated circular SVG gauges |
| 3 | [Farm Monitor](./Farm_Monitor/) | Soil Moisture | ✅ Animated Plant | Soil moisture monitor with reactive plant character |
| 4 | [Lux Meter](./Lux_Meter/) | TEMT6000 | ✅ Animated Sun | Light intensity meter with rotating sun visualization |
| 5 | [Water Tank Monitor](./Water_Tank_Monitor/) | HC-SR04 | ✅ Tank + Bubbles | Water level monitor with animated tank, bubbles & fish |
| 6 | [Radar Scanner](./Radar/) | HC-SR04 + Servo | ✅ Radar Scope | 180° radar scanner with phosphor glow trail |
| 7 | [Smart Blind Stick](./Obstacle_Detection_System/) | HC-SR04 | ✅ Walking Person | Assistive obstacle detector with animated person & cane |
| 8 | [Measurement Tool](./Measurement_Tool/) | HC-SR04 | ✅ Digital Ruler | Contactless distance meter with ruler bar & beam animation |

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

1. Open **Arduino IDE**
2. Go to **File → Preferences** (or `Ctrl + ,`)
3. In the **"Additional Board Manager URLs"** field, paste this URL:

```
https://arduino.esp8266.com/stable/package_esp8266com_index.json
```

> If you already have other URLs there, separate them with a comma.

4. Click **OK**
5. Go to **Tools → Board → Boards Manager...**
6. Search for **"esp8266"**
7. Find **"esp8266 by ESP8266 Community"** and click **Install**
8. Wait for the installation to complete (this may take a few minutes)

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
| **DHT11** | Weather Station | Search `DHT11` in Library Manager → Install by **Dhruba Saha** |
| **Servo** | Radar Scanner | Usually pre-installed. If not, search `Servo` → Install by **Arduino** |
| **ESP8266WiFi** | All projects | Included with ESP8266 Board Manager (no separate install needed) |
| **ESP8266WebServer** | All web projects | Included with ESP8266 Board Manager (no separate install needed) |

> **Important:** The `ESP8266WiFi` and `ESP8266WebServer` libraries are automatically installed when you install the ESP8266 Board Manager in Step 3. Do NOT try to install them separately.

---

### Step 5: Upload Your First Sketch

1. Open any `.ino` file from this repository (e.g., `Oximeter/Oximeter.ino`)
2. Connect your NodeMCU via USB
3. Select the correct **Board** and **Port** from the **Tools** menu
4. Click the **Upload** button (→ arrow) or press `Ctrl + U`
5. Wait for the upload to complete — you should see `Done uploading.` in the console
6. Open **Serial Monitor** (`Ctrl + Shift + M`) and set baud rate to **9600** to see debug output

---

## 📡 How to Use the Web Dashboards

Most projects in this collection create a **WiFi Access Point** (AP) that you connect to with your phone or laptop:

1. **Upload** the sketch to NodeMCU
2. On your phone/laptop, go to **WiFi Settings**
3. Connect to the WiFi network shown in the project (e.g., `HeartMonitor_AP`)
4. Password for all projects: **`12345678`**
5. Open a browser and navigate to: **`http://192.168.4.1`**
6. The live dashboard will load with real-time sensor data and animations

> **No internet required!** The ESP8266 acts as a standalone access point and web server. Everything runs locally.

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

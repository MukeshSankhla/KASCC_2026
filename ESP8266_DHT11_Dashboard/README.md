# 🌡️ ESP8266 Weather Station — Temperature & Humidity Monitor

## 1. Problem Statement

Weather monitoring is essential for agriculture, home automation, and environmental awareness. Commercial weather stations are often expensive and rely on cloud services or proprietary apps. There is a need for a **low-cost, self-contained weather monitoring system** that provides real-time temperature and humidity data through an accessible interface — without requiring internet connectivity or subscription services.

## 2. Our Solution

We built a **standalone weather station** using the ESP8266 NodeMCU and a DHT11 temperature/humidity sensor. The system:

- Reads temperature (°C) and relative humidity (%) every 5 seconds
- Computes the **Heat Index** (feels-like temperature) using the Rothfusz regression equation
- Creates its own WiFi Access Point (no router needed)
- Serves an **animated web dashboard** with beautiful circular SVG gauges
- Displays a **dynamic weather emoji** that changes based on conditions (☀️ hot, ❄️ cold, 🌧️ humid)
- Stores up to 100 historical readings in memory

## 3. Implementation

### System Architecture

```
DHT11 Sensor → GPIO2 (Digital Read) → ESP8266 (Data Processing)
                                            ↓
                                      WiFi Access Point
                                            ↓
                                  Phone/Laptop Browser
                                            ↓
                                Animated Web Dashboard
                            (SVG Gauges + Weather Icon + History)
```

### How It Works

1. **Sensor Reading:** The DHT11 communicates via a single-wire digital protocol. It provides integer values for temperature (0–50°C) and humidity (20–90%).
2. **Heat Index Calculation:** Using the Rothfusz regression formula (the same used by the US National Weather Service), we compute the "feels-like" temperature factoring in humidity.
3. **Data Logging:** Each reading is stored in a circular buffer of 100 entries with timestamps.
4. **Web Dashboard:** The ESP8266 creates a WiFi AP and serves the dashboard. Two JSON endpoints (`/live` and `/history`) provide data to the browser.
5. **LED Feedback:** A physical LED briefly flashes on each successful sensor read.

### 3.1 Components Used

| Component | Purpose | Details |
|-----------|---------|---------|
| **NodeMCU ESP8266** | Microcontroller & WiFi AP | Runs the web server and handles sensor communication. 80MHz CPU, 4MB flash, built-in WiFi. |
| **DHT11 Sensor** | Temperature & humidity sensing | Digital sensor measuring 0–50°C (±2°C accuracy) and 20–90% RH (±5% accuracy). Uses single-wire protocol. Low cost and widely available. |
| **LED + Resistor** | Read indicator | Connected to GPIO4 (D2). Flashes briefly on each successful sensor read to confirm operation. |
| **USB Cable** | Power & programming | Powers the NodeMCU and enables code upload. |

### Wiring Diagram

| DHT11 Pin | NodeMCU Pin |
|-----------|-------------|
| DATA | D4 (GPIO2) |
| VCC | 3.3V |
| GND | GND |
| LED Anode | D2 (GPIO4) via resistor |

### Dashboard Features

- **Circular Temperature Gauge** — SVG arc-fill gauge (blue→orange→red gradient) with animated needle
- **Circular Humidity Gauge** — SVG arc-fill gauge (yellow→cyan→blue gradient) with animated needle
- **Floating Weather Icon** — Dynamic emoji (☀️/❄️/🌧️/⛅) with gentle bobbing animation
- **Heat Index Card** — Computed "feels-like" temperature
- **Historical Data Table** — Last 50 readings with timestamps
- **Sensor Status Indicator** — Green dot (OK) or red dot (Error)

## 4. Conclusion and Learnings

### Conclusion

We successfully created an affordable, self-contained weather monitoring station. The animated circular gauges provide an intuitive at-a-glance view of conditions, making the dashboard both functional and visually appealing. The system works completely offline, making it suitable for remote locations.

### Key Learnings

- **Digital Sensor Protocols:** Understanding the single-wire DHT11 communication protocol and its timing requirements.
- **Heat Index Computation:** Implementing the Rothfusz regression equation for computing perceived temperature — the same formula used by meteorological services.
- **SVG Gauge Design:** Creating animated circular gauges using `stroke-dasharray` and `stroke-dashoffset` CSS properties for smooth arc-fill effects.
- **Data Visualization:** Mapping raw sensor values to visual gauge positions using mathematical transformations.
- **Error Handling:** Gracefully handling sensor read failures and displaying appropriate error states in the UI.

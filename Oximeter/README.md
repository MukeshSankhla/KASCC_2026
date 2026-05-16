# 💓 ESP8266 Pulse Oximeter — Heart Rate Monitor

## 1. Problem Statement

Monitoring heart rate is a fundamental aspect of healthcare and fitness. Commercial pulse oximeters can be expensive and are often not customizable. In remote or resource-constrained settings, access to even basic heart rate monitoring tools may be limited. There is a need for an **affordable, portable, and offline heart rate monitoring system** that can display live readings through an intuitive interface — without requiring an internet connection or a dedicated mobile app.

## 2. Our Solution

We built a **standalone heart rate monitor** using the ESP8266 NodeMCU and an analog Pulse Sensor. The system:

- Reads the user's pulse through a fingertip sensor
- Detects heartbeats using a peak-detection algorithm
- Calculates real-time **BPM (Beats Per Minute)**
- Creates its own WiFi Access Point (no internet/router needed)
- Serves a **beautiful animated web dashboard** accessible from any phone or laptop browser
- Features a **live beating heart SVG** that syncs its animation speed to the actual detected BPM
- Stores the last 100 readings in memory for historical review

## 3. Implementation

### System Architecture

```
Pulse Sensor → A0 (Analog Read) → ESP8266 (Peak Detection Algorithm)
                                        ↓
                                  WiFi Access Point
                                        ↓
                              Phone/Laptop Browser
                                        ↓
                            Animated Web Dashboard
                        (Heart SVG + ECG + BPM + History)
```

### How It Works

1. **Signal Acquisition:** The pulse sensor outputs an analog voltage that fluctuates with each heartbeat. The ESP8266 reads this at A0 every 10ms.
2. **Peak Detection:** When the signal crosses a configurable threshold, a "beat" is registered. The time between consecutive beats (Inter-Beat Interval) is used to calculate BPM: `BPM = 60000 / IBI_ms`.
3. **Noise Filtering:** Beats with IBI outside the 333ms–1200ms range (corresponding to 50–180 BPM) are rejected as noise to ensure only true human heart rate values are displayed.
4. **Web Dashboard:** The ESP8266 runs as a WiFi Access Point and HTTP server. The dashboard fetches live data via JSON API every 2 seconds.
5. **LED Feedback:** A physical LED on GPIO4 flashes with each detected heartbeat.

### 3.1 Components Used

| Component | Purpose | Details |
|-----------|---------|---------|
| **NodeMCU ESP8266** | Microcontroller & WiFi AP | Runs the web server and processes sensor data. 80MHz CPU, 4MB flash, built-in WiFi. |
| **Pulse Sensor (Analog)** | Heart rate detection | Optical sensor that detects blood volume changes in the fingertip using photoplethysmography (PPG). Outputs analog voltage proportional to pulse. |
| **LED + 220Ω Resistor** | Visual heartbeat indicator | Connected to GPIO4 (D2). Flashes on each detected beat for physical feedback. |
| **USB Cable** | Power & programming | Powers the NodeMCU and allows uploading code from Arduino IDE. |

### Wiring Diagram

| Pulse Sensor Pin | NodeMCU Pin |
|-----------------|-------------|
| Signal (S) | A0 |
| VCC (+) | 3.3V |
| GND (−) | GND |
| LED Anode | D2 (GPIO4) via 220Ω resistor |

### Dashboard Features

- **Animated SVG Heart** — Beats in real-time, synced to detected BPM
- **Expanding Pulse Rings** — Visual ripple effect around the heart
- **ECG Waveform Trace** — Animated cardiac rhythm line with glow
- **Live BPM Display** — Shown inside the heart with large font
- **Signal Status Card** — Shows "Reading Pulse" or "No Finger / Weak"
- **Beat History Table** — Last 50 recorded beats with timestamps

## 4. Conclusion and Learnings

### Conclusion

We successfully built a fully functional, offline heart rate monitor that costs a fraction of commercial devices. The web-based interface eliminates the need for any app installation — users simply connect to the WiFi and open a browser. The animated dashboard makes the experience engaging and intuitive.

### Key Learnings

- **Analog Signal Processing:** Learned how to implement peak detection and threshold-based algorithms on a microcontroller for real-time biomedical signal analysis.
- **Non-Blocking Programming:** Using `millis()` instead of `delay()` to sample the sensor without blocking the web server.
- **Embedded Web Development:** Serving a full HTML/CSS/JS dashboard from limited microcontroller flash memory using PROGMEM.
- **SVG Animation:** Creating responsive, data-driven SVG animations (beating heart, ECG trace) that run smoothly on mobile browsers.
- **AP Mode Networking:** Setting up the ESP8266 as a standalone Access Point, eliminating dependency on external WiFi infrastructure.

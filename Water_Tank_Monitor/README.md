# 💧 ESP8266 Water Tank Monitor — Ultrasonic Level Monitoring System

## 1. Problem Statement

Water scarcity is a growing global concern, yet water tank management in most homes and buildings relies on manual checking — climbing to the rooftop to look inside the tank. This leads to **overflow wastage** (when the pump is forgotten running) and **dry-run damage** to water pumps (when the tank runs empty unnoticed). There is a need for a **smart, affordable water level monitoring system** that provides real-time fill levels with alerts — accessible from a phone without any app installation.

## 2. Our Solution

We built a **smart water tank monitor** using the ESP8266 NodeMCU, an HC-SR04 ultrasonic distance sensor, and a piezo buzzer. The system:

- Measures the distance from the tank top to the water surface using ultrasonic ranging
- Calculates **fill level as a percentage** (0–100%) based on configurable tank dimensions
- Classifies the tank status: Critical Low, Low, Normal, High, Overflow Warning
- Triggers a **piezo buzzer alarm** for critical low and overflow conditions
- Creates its own WiFi Access Point
- Serves an **animated web dashboard** with a realistic water tank visualization — complete with **animated waves, rising bubbles, surface ripples, and a swimming fish**

## 3. Implementation

### System Architecture

```
HC-SR04 Ultrasonic → GPIO14/12 (Trig/Echo) → ESP8266 (Distance → % Calculation)
                                                    ↓
                                              WiFi Access Point
                                                    ↓
                                          Phone/Laptop Browser
                                                    ↓
                                        Animated Web Dashboard
                                    (Tank + Waves + Bubbles + Alerts)
                                                    ↓
                                        Buzzer Alarm (GPIO13)
```

### How It Works

1. **Ultrasonic Ranging:** The HC-SR04 sends a 40kHz pulse burst and measures the echo return time. Distance is calculated as: `Distance = (Duration × 0.0343) / 2`.
2. **Level Calculation:** Using configurable tank dimensions (empty distance = 150cm, full distance = 20cm), the fill percentage is computed: `Fill% = (EmptyDist - CurrentDist) / (EmptyDist - FullDist) × 100`.
3. **Status Classification:**
   - `≤ 10%` → CRITICAL LOW (long buzzer beep)
   - `< 30%` → Low
   - `< 80%` → Normal
   - `< 95%` → High
   - `≥ 95%` → OVERFLOW WARNING (3 fast beeps)
4. **Animated Tank:** The dashboard shows a realistic tank with CSS wave animation, bubbles rising through the water, ripples at the surface, and a fish that swims when water is sufficient.

### 3.1 Components Used

| Component | Purpose | Details |
|-----------|---------|---------|
| **NodeMCU ESP8266** | Microcontroller & WiFi AP | Handles ultrasonic timing, percentage calculation, alarm logic, and web server. |
| **HC-SR04 Ultrasonic Sensor** | Distance measurement | Measures distance using ultrasonic echo. Range: 2cm–400cm, accuracy: ~3mm. Requires 5V supply but 3.3V logic on Echo pin (use voltage divider). |
| **Piezo Buzzer** | Audible alarm | Connected to GPIO13 (D7). Produces warning tones for critical low and overflow conditions. Active buzzer — just apply voltage to sound. |
| **Resistors (1kΩ + 2kΩ)** | Voltage divider | Steps down the HC-SR04's 5V Echo output to ~3.3V for safe ESP8266 GPIO input. |
| **USB Cable** | Power & programming | Powers the system and enables code upload. |

### Wiring Diagram

| HC-SR04 Pin | NodeMCU Pin | Notes |
|------------|-------------|-------|
| VCC | Vin (5V) | Needs 5V supply |
| GND | GND | |
| TRIG | D5 (GPIO14) | 3.3V output is enough to trigger |
| ECHO | D6 (GPIO12) | **Through voltage divider!** (1kΩ + 2kΩ) |
| Buzzer + | D7 (GPIO13) | Active buzzer |
| Buzzer − | GND | |

> ⚠️ **Critical:** The HC-SR04 Echo pin outputs 5V pulses. ESP8266 GPIOs are **NOT 5V tolerant**. Always use a voltage divider (1kΩ series + 2kΩ to ground) to protect the ESP8266.

### Dashboard Features

- **Animated Water Tank** with realistic wave animation at the surface
- **Rising Bubbles** — 4 staggered bubble animations inside the water
- **Surface Ripples** — Expanding ring animations at the waterline
- **Swimming Fish** — 🐟 emoji swims back and forth (visible when level >20%)
- **Dynamic Water Color:**
  - Red gradient for Critical Low
  - Cyan-to-blue for Normal operation
  - Blue with red glow for Overflow Warning
- **Flashing Alert Text** — Status text pulses for critical conditions
- **Large Percentage Display** — Bold fill level shown below the tank
- **Historical Level Logs** — Table of last 50 readings

## 4. Conclusion and Learnings

### Conclusion

We built a practical, production-ready water tank monitoring system that solves a real everyday problem. The buzzer alarms prevent overflow and dry-run scenarios, while the animated web dashboard provides a beautiful and intuitive way to check water levels remotely from any device. The system is completely self-contained and works without internet.

### Key Learnings

- **Ultrasonic Distance Measurement:** Understanding the physics of ultrasonic time-of-flight measurement and its accuracy limitations (temperature dependence, beam angle, surface reflection).
- **Voltage Level Shifting:** Learning why voltage dividers are essential when interfacing 5V sensors with 3.3V microcontrollers — and the consequences of not using them.
- **Alarm System Design:** Implementing different alarm patterns (fast beeps vs. long tone) to convey urgency levels through audio feedback.
- **CSS Wave Animation:** Creating realistic water surface waves using CSS pseudo-elements with rotating border-radius — a clever visual trick that requires no JavaScript.
- **Inverse Distance Mapping:** Converting "distance from sensor to water" into "fill percentage" — an inverse relationship that requires careful mathematical mapping.

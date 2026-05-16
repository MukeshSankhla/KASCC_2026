# 🚨 ESP8266 Obstacle Detection System — Proximity Alert with Buzzer

## 1. Problem Statement

Obstacle detection is a fundamental requirement in robotics, autonomous vehicles, and assistive technologies for the visually impaired. Many applications need a simple, low-cost proximity alert system that warns when an object is too close — using audible feedback whose urgency scales with proximity. There is a need for a **basic, reliable obstacle detection circuit** that can be embedded into robots, vehicles, or wearable assistive devices.

## 2. Our Solution

We built a **simple obstacle detection system** using the ESP8266 and an HC-SR04 ultrasonic distance sensor paired with a piezo buzzer. The system:

- Continuously measures the distance to the nearest object
- Provides **proximity-based audible alerts** through a buzzer:
  - **Close range (≤40cm):** Fast beeping (100ms on/off) — urgent warning
  - **Medium range (20–100cm):** Slow beeping (300ms on/off) — caution alert
  - **Far range (>100cm):** No beeping — all clear
- Outputs distance readings to the Serial Monitor for debugging
- Uses no WiFi or web dashboard — a focused, minimal circuit design

> **Note:** This project does not include a web dashboard. It demonstrates basic sensor-actuator interfacing for real-time proximity feedback.

## 3. Implementation

### System Architecture

```
HC-SR04 Ultrasonic → GPIO14/12 (Trig/Echo) → ESP8266 (Distance Calculation)
                                                    ↓
                                            Distance Logic
                                                    ↓
                                         ┌──────────┴──────────┐
                                         ↓                      ↓
                                   Buzzer (GPIO13)        Serial Monitor
                                 (Audible Alert)          (Debug Output)
```

### How It Works

1. **Ultrasonic Pulse:** The Trigger pin sends a 10μs HIGH pulse, causing the HC-SR04 to emit eight 40kHz ultrasonic bursts.
2. **Echo Timing:** The Echo pin goes HIGH for the duration proportional to the round-trip time of the sound wave.
3. **Distance Calculation:** `Distance (cm) = Duration × 0.034 / 2` (speed of sound = 343 m/s at 20°C).
4. **Alert Logic:**
   - `≤ 40cm` → Buzzer beeps rapidly (100ms interval) — object is dangerously close
   - `20–100cm` → Buzzer beeps slowly (300ms interval) — object approaching
   - `> 100cm` → Buzzer silent — path is clear

### 3.1 Components Used

| Component | Purpose | Details |
|-----------|---------|---------|
| **NodeMCU ESP8266** | Microcontroller | Handles timing for ultrasonic measurement and controls buzzer output. Could be replaced with any Arduino-compatible board. |
| **HC-SR04 Ultrasonic Sensor** | Distance measurement | Non-contact distance sensor using ultrasonic echo. Range: 2–400cm. Accuracy: ~3mm. Beam angle: ~15°. Requires 5V power supply. |
| **Piezo Buzzer** | Audible feedback | Active buzzer that sounds when voltage is applied to GPIO13. Used to create proximity-based beeping patterns. |
| **USB Cable** | Power & programming | Powers the system and provides serial communication for distance readout. |

### Wiring Diagram

| Component Pin | NodeMCU Pin | Notes |
|--------------|-------------|-------|
| HC-SR04 VCC | Vin (5V) | Requires 5V supply |
| HC-SR04 GND | GND | |
| HC-SR04 TRIG | D5 (GPIO14) | |
| HC-SR04 ECHO | D6 (GPIO12) | Use voltage divider for safety! |
| Buzzer (+) | D7 (GPIO13) | |
| Buzzer (−) | GND | |

## 4. Conclusion and Learnings

### Conclusion

We built a simple yet effective obstacle detection system that demonstrates the core principles of ultrasonic ranging and proximity-based alert systems. Despite its simplicity, this project forms the foundation for more complex applications — from robotic obstacle avoidance to assistive devices for the visually impaired.

### Key Learnings

- **Ultrasonic Sensor Fundamentals:** Understanding the physics of sound-based distance measurement — pulse transmission, echo reception, and time-of-flight calculation.
- **Sensor-Actuator Loop:** Implementing a basic sense-decide-act loop — the fundamental paradigm of all robotics and embedded control systems.
- **Proximity-Based Feedback:** Designing escalating alert patterns (fast vs. slow beeping) to convey urgency — a common UX pattern in automotive parking sensors and industrial safety systems.
- **Timing Considerations:** Understanding that `delay()` calls block program execution — acceptable in this simple project but problematic when combined with web servers or other concurrent tasks.
- **GPIO Digital Output:** Using `digitalWrite()` to control actuators (buzzer) based on computed conditions — basic but essential embedded programming skill.

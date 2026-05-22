# 🦯 IoT Obstacle Detection and Alert System using IR Sensor

## 1. Problem Statement

Over **285 million people** worldwide are visually impaired, and navigating unfamiliar environments remains one of their greatest daily challenges. Traditional white canes can detect obstacles only upon physical contact, giving very little reaction time. Commercial electronic canes with ultrasonic sensors exist but are prohibitively expensive. There is a need for an **affordable, smart assistive walking stick** that detects obstacles ahead contactlessly and warns the user through intuitive audio feedback — with an optional monitoring dashboard for caregivers.

## 2. Our Solution

We built a **smart blind stick** using the ESP8266 NodeMCU, an HC-SR04 ultrasonic sensor, and a piezo buzzer. The system:

- Detects obstacles up to **400cm ahead** using ultrasonic ranging
- Classifies proximity into **3 zones**: Danger (≤40cm), Caution (41–100cm), Clear (>100cm)
- Provides **haptic-style audio feedback** through a buzzer:
  - **Danger zone:** Rapid beeping (80ms pulses) — immediate obstacle
  - **Caution zone:** Slow intermittent beeping — object approaching
  - **Clear zone:** Silent — safe to walk
- Creates a **WiFi Access Point** for caregiver monitoring
- Serves an **animated web dashboard** featuring:
  - A walking **person with a cane** and sonar wave animation
  - An **obstacle wall** that grows/shrinks based on proximity
  - Color-coded zone indicators (green/yellow/red)
  - Alert counter and detection history log

## 3. Implementation

### System Architecture

```
HC-SR04 Ultrasonic → GPIO14/12 (Trig/Echo) → ESP8266 (Distance + Zone Logic)
                                                    ↓
                                        ┌───────────┼───────────┐
                                        ↓           ↓           ↓
                                  WiFi AP       Buzzer       Serial
                                    ↓          (GPIO13)     Monitor
                              Web Dashboard
                       (Person Animation + Zones + History)
```

### How It Works

1. **Ultrasonic Scanning:** The HC-SR04 sends ultrasonic pulses every 200ms, measuring the distance to the nearest obstacle.
2. **Zone Classification:**
   - `≤ 40cm` → **DANGER** — Obstacle very close, rapid beeping warns user to stop
   - `41–100cm` → **CAUTION** — Object detected ahead, slow beeping advises caution
   - `> 100cm` → **CLEAR** — Path is free, buzzer stays silent
3. **Audio Feedback:** The buzzer pattern mimics commercial parking sensors — faster beeping = closer obstacle. This intuitive feedback requires no training.
4. **Caregiver Dashboard:** The ESP8266 creates a WiFi AP. A caregiver can connect via phone and monitor:
   - Real-time distance readings and zone status
   - Animated visualization showing a person walking with sonar waves
   - An obstacle wall that visually grows as objects get closer
   - Total alert count (how many times the danger zone was triggered)
   - Full detection history log

### 3.1 Components Used

| Component | Purpose | Details |
|-----------|---------|---------|
| **NodeMCU ESP8266** | Microcontroller & WiFi AP | Handles ultrasonic timing, zone logic, buzzer control, and serves the monitoring dashboard. 80MHz CPU, 4MB flash. |
| **HC-SR04 Ultrasonic Sensor** | Obstacle detection | Mounted at the top of the walking stick, pointing forward. Detects objects 2–400cm away using ultrasonic echo. Beam angle: ~15°. |
| **Piezo Buzzer** | Audio warning | Connected to GPIO13 (D7). Produces proximity-based beeping patterns that the visually impaired user can hear and interpret. Active buzzer type. |
| **Resistors (1kΩ + 2kΩ)** | Voltage divider | Steps down HC-SR04's 5V Echo output to ~3.3V for safe ESP8266 GPIO input. |
| **Walking Stick / PVC Pipe** | Physical mount | The sensor and ESP8266 are mounted on a walking stick or PVC pipe handle. Battery pack provides portable power. |
| **USB Power Bank** | Portable power | Standard 5V power bank powers the NodeMCU for hours of portable operation. |

### Wiring Diagram

| Component Pin | NodeMCU Pin | Notes |
|--------------|-------------|-------|
| HC-SR04 VCC  | Vin (5V)    | Requires 5V supply |
| HC-SR04 GND  | GND         | |
| HC-SR04 TRIG | D5 (GPIO14) | |
| HC-SR04 ECHO | D6 (GPIO12) | **Through voltage divider!** |
| Buzzer (+)   | D7 (GPIO13) | Active buzzer |
| Buzzer (−)   | GND         | |

### Circuit Diagram

![Circuit Diagram](../images/iot_obstacle_detection_system.png)

### Dashboard Features

- **Animated Walking Person** — CSS figure with:
  - Head with dark glasses (visually impaired representation)
  - Walking leg animation that speeds up near obstacles
  - White cane held in the right hand with glowing tip
- **Sonar Waves** — 3 expanding circular pulses emanating from the cane tip, color changes by zone
- **Obstacle Wall** — A vertical block on the right side that grows taller as objects get closer; glows red in danger zone
- **Zone Badge** — Large status indicator:
  - ✅ `PATH CLEAR` (green, calm)
  - ⚠️ `SLOW DOWN — Object Ahead` (yellow, flashing)
  - ⛔ `OBSTACLE DETECTED!` (red, rapid flash)
- **Distance Bar** — Horizontal proximity gauge with labeled zone boundaries
- **Alert Counter** — Total number of danger zone triggers in the session
- **Detection History Table** — Color-coded log of readings with timestamps

## 4. Conclusion and Learnings

### Conclusion

We successfully built a low-cost assistive device that can meaningfully improve mobility independence for visually impaired individuals. The ultrasonic detection provides reliable contactless obstacle sensing, and the graduated audio feedback is intuitive enough to use without training. The optional web dashboard adds a caregiver monitoring capability that commercial products rarely offer at this price point.

### Key Learnings

- **Assistive Technology Design:** Designing for accessibility requires thinking beyond visual interfaces — audio feedback patterns must be intuitive, distinct, and non-fatiguing for the user.
- **Graduated Alert Systems:** Implementing multi-level proximity warnings (danger/caution/clear) with different buzzer patterns — the same principle used in automotive parking sensors and industrial safety systems.
- **Real-Time Embedded Processing:** Balancing sensor polling speed (200ms for responsive detection) with web server responsiveness on a single-threaded microcontroller.
- **CSS Character Animation:** Building an animated human figure using pure CSS divs — head, body, arms, legs, and cane — with walk-cycle keyframe animations.
- **Social Impact of IoT:** Understanding how a simple sensor + microcontroller combination can create meaningful assistive technology that costs under ₹500, compared to commercial electronic canes costing ₹5000+.
- **Caregiver Monitoring:** Adding a WiFi dashboard creates a secondary use case — caregivers can monitor the person's environment remotely, adding a layer of safety.

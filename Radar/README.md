# 🎯 ESP8266 Radar Scanner — 180° Ultrasonic Radar System

## 1. Problem Statement

Radar and sonar systems are fascinating technologies used in defense, robotics, and autonomous navigation. However, understanding how radar works is often limited to theoretical textbook descriptions. There is a need for a **hands-on, visual radar demonstration system** that can detect objects in real-time and display them on a tactical radar scope — making radar concepts tangible and interactive for students and hobbyists.

## 2. Our Solution

We built a **180° ultrasonic radar scanner** using the ESP8266 NodeMCU, an HC-SR04 distance sensor mounted on an SG90 servo motor. The system:

- Sweeps a servo-mounted ultrasonic sensor across a 180° arc (15°–165°)
- Measures distance to objects at each angle position
- Creates its own WiFi Access Point
- Serves a **tactical-themed web dashboard** with a real-time canvas-based radar scope
- Features a **phosphor glow trail** behind the sweep line (like a real CRT radar)
- Detected objects appear as **pulsing blips** — green for distant, red for close proximity
- CRT scanline overlay for authentic retro radar aesthetic

## 3. Implementation

### System Architecture

```
SG90 Servo (Sweep 15°–165°) ← GPIO4 (PWM Signal)
        ↕
HC-SR04 Ultrasonic (mounted on servo) → GPIO14/12 (Trig/Echo)
        ↓
   ESP8266 (Angle + Distance Data)
        ↓
   WiFi Access Point
        ↓
   Phone/Laptop Browser
        ↓
   Canvas Radar Scope (Real-time Visualization)
```

### How It Works

1. **Non-Blocking Sweep:** The servo sweeps from 15° to 165° and back in 3° increments every 40ms. This is done non-blocking using `millis()` so the web server remains responsive.
2. **Distance Mapping:** At each angle, the HC-SR04 measures distance. Results are stored in a 37-element array (one bucket per 5°).
3. **Closest Object Tracking:** The system continuously scans the array to find the nearest detected object.
4. **Canvas Rendering:** The browser fetches radar data every 250ms and renders it on an HTML5 Canvas:
   - Concentric arc grid with distance labels (25cm, 50cm, 75cm, 100cm)
   - Sweep beam line with phosphor glow trail
   - Pulsing blip markers at detected object positions
5. **CRT Effect:** A CSS pseudo-element overlays subtle horizontal scanlines across the entire page for a retro CRT television effect.

### 3.1 Components Used

| Component | Purpose | Details |
|-----------|---------|---------|
| **NodeMCU ESP8266** | Microcontroller & WiFi AP | Runs the non-blocking sweep state machine and serves the web dashboard. |
| **HC-SR04 Ultrasonic Sensor** | Distance measurement | Detects objects up to 100cm away at each angle. Range: 2–400cm, but we cap at 100cm for this project. |
| **SG90 Micro Servo Motor** | Angular positioning | Rotates the ultrasonic sensor across a 180° arc. Operating voltage: 4.8–6V. Rotation range: 0–180°. Speed: ~0.1s/60° at 4.8V. |
| **Resistors (1kΩ + 2kΩ)** | Voltage divider for Echo pin | Protects ESP8266 from HC-SR04's 5V output. |
| **USB Cable** | Power & programming | Powers the system (servo may need external 5V for heavy loads). |

### Wiring Diagram

| Component Pin | NodeMCU Pin | Notes |
|--------------|-------------|-------|
| Servo Signal | D2 (GPIO4) | PWM control |
| Servo VCC | Vin (5V) | External 5V recommended for stability |
| Servo GND | GND | |
| HC-SR04 TRIG | D5 (GPIO14) | |
| HC-SR04 ECHO | D6 (GPIO12) | **Through voltage divider!** |
| HC-SR04 VCC | Vin (5V) | |
| HC-SR04 GND | GND | |

### Dashboard Features

- **Real-time Canvas Radar Scope** — Semicircular tactical display
- **Phosphor Glow Trail** — Sweep line leaves a 2-second fading green trail (like real CRT radar screens)
- **Pulsing Blip Markers** — Detected objects throb with shadow glow; red if close (<20cm), green if farther
- **CRT Scanline Overlay** — Horizontal line pattern for authentic retro CRT aesthetic
- **Distance Ring Labels** — 25cm, 50cm, 75cm, 100cm marked on concentric arcs
- **Angle Labels** — Degree markers at 30° intervals
- **Stats Panel** — Current scan angle, closest proximity alert, total objects detected
- **Scope Legend** — Color key for blip distance interpretation

## 4. Conclusion and Learnings

### Conclusion

We successfully built a working radar scanner that visualizes detected objects in real-time on a tactical-themed web interface. The project brings radar concepts to life — from servo-based mechanical scanning to canvas-based polar coordinate rendering. The phosphor trail and CRT effects make the interface feel authentic and impressive.

### Key Learnings

- **Non-Blocking State Machines:** Implementing servo sweep without `delay()` calls — critical for maintaining web server responsiveness on a single-threaded microcontroller.
- **Polar Coordinate Mathematics:** Converting angle + distance readings into X,Y canvas coordinates using trigonometric functions (`cos`, `sin`).
- **HTML5 Canvas Programming:** Drawing dynamic graphics (arcs, lines, circles) with the Canvas 2D API, including gradient strokes and shadow effects.
- **Phosphor Simulation:** Implementing a time-based trail effect by storing historical sweep positions and rendering them with decreasing opacity.
- **Servo Motor Control:** Understanding PWM-based servo positioning and the practical limitations of cheap servos (jitter at extremes, avoiding 0° and 180°).
- **Real-Time Data Streaming:** Achieving smooth radar animation by polling the ESP8266's JSON API at 250ms intervals — balancing update frequency against network load.

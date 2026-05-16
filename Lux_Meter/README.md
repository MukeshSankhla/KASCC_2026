# ☀️ ESP8266 Lux Meter — Light Intensity Monitor

## 1. Problem Statement

Light intensity measurement is critical in many fields — from photography and indoor farming to workplace safety compliance and energy-efficient building design. Professional lux meters can be expensive, and smartphone-based alternatives lack accuracy. There is a need for a **dedicated, affordable, and accurate light measurement device** that provides real-time illuminance readings through an accessible and visually engaging interface.

## 2. Our Solution

We built a **digital lux meter** using the ESP8266 NodeMCU and a TEMT6000 ambient light sensor. The system:

- Measures ambient light intensity in **Lux** using an analog light sensor
- Classifies the environment into 5 conditions: Dark, Dim, Normal, Bright, Intense
- Creates its own WiFi Access Point
- Serves an **animated web dashboard** with a dynamic **SVG sun** whose appearance changes with light levels
- The sun's rays grow, glow intensifies, and rotation speeds up as light increases
- Stores the last 100 readings for historical analysis

## 3. Implementation

### System Architecture

```
TEMT6000 Sensor → A0 (Analog Read) → ESP8266 (Voltage → Current → Lux Conversion)
                                            ↓
                                      WiFi Access Point
                                            ↓
                                  Phone/Laptop Browser
                                            ↓
                                Animated Web Dashboard
                            (Sun SVG + Light Cone + History)
```

### How It Works

1. **Analog Reading:** The TEMT6000 outputs a current proportional to light intensity. This current flows through a 10kΩ resistor, creating a measurable voltage.
2. **Lux Conversion:** The conversion chain is:
   - `Voltage = Raw × (3.3V / 1023)`
   - `Current = Voltage / 10000Ω`
   - `Lux = Microamps × 2.0` (from TEMT6000 datasheet: 2 Lux per μA)
3. **Classification:** Based on lux values: Dark (<10), Dim (<50), Normal (<300), Bright (<800), Intense (≥800).
4. **Animated Sun:** The SVG sun dynamically adjusts — grey and still in darkness, bright yellow with thick rays and fast rotation in intense light.

### 3.1 Components Used

| Component | Purpose | Details |
|-----------|---------|---------|
| **NodeMCU ESP8266** | Microcontroller & WiFi AP | Handles ADC reading, lux computation, and web server. 80MHz CPU, built-in WiFi. |
| **TEMT6000 Light Sensor** | Ambient light detection | Ambient light sensor based on a phototransistor. Spectral response similar to the human eye. Linear current output proportional to illuminance. Range: ~1–1000+ Lux. |
| **LED + 220Ω Resistor** | Read indicator | Connected to GPIO4 (D2). Flashes on each reading cycle. |
| **USB Cable** | Power & programming | Powers the system and enables code upload. |

### Wiring Diagram

| TEMT6000 Pin | NodeMCU Pin |
|-------------|-------------|
| OUT / SIG | A0 |
| VCC | 3.3V |
| GND | GND |
| LED Anode | D2 (GPIO4) via 220Ω resistor |

### Dashboard Features

- **Animated SVG Sun** — 8 radiating rays that continuously rotate, with 5 visual states:
  - 🌑 Dark: Grey sun, thin rays, very slow rotation (60s/rev)
  - 🌙 Dim: Amber sun, medium rays, slow rotation (40s/rev)
  - ☀️ Normal: Yellow sun, standard rays, normal rotation (30s/rev)
  - 🔆 Bright: Bright yellow, thick rays, fast rotation (20s/rev)
  - ⚡ Intense: White-hot sun, maximum glow halo, fastest spin (10s/rev)
- **Light Cone** — Trapezoid beam below sun with opacity tied to lux level
- **Glow Halo** — CSS box-shadow intensifies with brightness
- **Lux Display** — Large value shown inside the sun center
- **Historical Records Table** — Last 50 readings with timestamps

## 4. Conclusion and Learnings

### Conclusion

We built a functional, low-cost lux meter that provides accurate illuminance measurements with an engaging visual interface. The animated sun provides instant intuitive feedback about light conditions, making it useful for education, photography, indoor farming, and workspace assessment.

### Key Learnings

- **Photoelectric Sensor Physics:** Understanding how the TEMT6000 converts photons to proportional electrical current, and the mathematical chain from ADC reading to Lux.
- **Data-Driven CSS Transitions:** Dynamically changing SVG attributes (fill color, stroke-width, radius) and CSS properties (box-shadow, animation-duration) from JavaScript based on sensor data.
- **Human-Centric Visualization:** Using the universal metaphor of a sun — bright/dim/dark — to make technical lux readings immediately understandable.
- **ESP8266 ADC Limitations:** Understanding that the ESP8266 has only one analog input (A0) with 10-bit resolution (0–1023) and 0–1V internal range (NodeMCU maps 0–3.3V via voltage divider).
- **Sensor Calibration:** Learning that the TEMT6000's linear response makes it ideal for relative measurements, but absolute lux accuracy depends on proper resistor selection and calibration.

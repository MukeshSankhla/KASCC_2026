# 🌱 ESP8266 Farm Monitor — Soil Moisture Monitoring System

## 1. Problem Statement

Agriculture accounts for a major portion of global water consumption, yet most small-scale farmers rely on guesswork to decide when and how much to water their crops. Over-watering wastes resources and can cause root rot, while under-watering leads to crop stress and reduced yields. There is a need for an **affordable, easy-to-use soil moisture monitoring system** that provides clear, intuitive feedback about soil conditions — enabling data-driven irrigation decisions without expensive commercial systems.

## 2. Our Solution

We built a **smart soil moisture monitor** using the ESP8266 NodeMCU and a capacitive/resistive soil moisture sensor. The system:

- Reads soil moisture levels continuously via analog input
- Converts raw resistance values to a **0–100% moisture percentage**
- Classifies soil into 4 conditions: Bone Dry, Needs Water, Moist & Healthy, Waterlogged
- Creates its own WiFi Access Point for phone/laptop access
- Serves an **animated web dashboard** featuring a **reactive SVG plant character** that visually shows how the crop "feels"
- Stores the last 100 moisture readings for trend analysis

## 3. Implementation

### System Architecture

```
Soil Moisture Sensor → A0 (Analog Read) → ESP8266 (Calibration & Mapping)
                                                ↓
                                          WiFi Access Point
                                                ↓
                                      Phone/Laptop Browser
                                                ↓
                                    Animated Web Dashboard
                                (Plant Character + Gauges + History)
```

### How It Works

1. **Analog Reading:** The soil sensor outputs a voltage inversely proportional to moisture. Dry soil = high resistance = high analog value (~1023). Wet soil = low resistance = low value (~400).
2. **Calibration & Mapping:** Raw values are mapped to 0–100% using calibrated dry (1023) and wet (400) reference points with Arduino's `map()` and `constrain()` functions.
3. **Condition Classification:**
   - `< 15%` → Bone Dry
   - `15–40%` → Needs Water
   - `40–75%` → Moist & Healthy
   - `> 75%` → Waterlogged
4. **Animated Plant:** The dashboard shows an SVG plant that reacts to moisture levels — standing tall when healthy, drooping when thirsty, wilting when dry, and showing overflow when waterlogged.
5. **Data Logging:** Each reading is stored in a circular buffer with timestamps.

### 3.1 Components Used

| Component | Purpose | Details |
|-----------|---------|---------|
| **NodeMCU ESP8266** | Microcontroller & WiFi AP | Processes sensor data and serves the web dashboard. 80MHz CPU, 4MB flash, built-in WiFi. |
| **Soil Moisture Sensor** | Soil moisture detection | Measures soil moisture by detecting electrical resistance between two probes. Lower resistance means more moisture. Outputs analog voltage (0–3.3V). |
| **LED + 220Ω Resistor** | Read indicator | Connected to GPIO4 (D2). Flashes on each reading cycle. |
| **USB Cable** | Power & programming | Powers the system and enables code upload. |

### Wiring Diagram

| Soil Sensor Pin | NodeMCU Pin |
|----------------|-------------|
| A0 (Analog Out) | A0 |
| VCC | 3.3V |
| GND | GND |
| LED Anode | D2 (GPIO4) via 220Ω resistor |

### Dashboard Features

- **Animated SVG Plant Character** with 4 reactive states:
  - 🌿 **Healthy:** Upright plant with gentle swaying leaves and smiling face
  - 😰 **Thirsty:** Drooping leaves, falling sweat drops, worried expression
  - 💀 **Bone Dry:** Wilted brown plant, cracked soil, distressed face
  - 💦 **Waterlogged:** Vibrant green with water overflow puddle and ripples
- **Mood Text** — Dynamic message ("Healthy & happy!", "Feeling thirsty...")
- **Moisture Gauge** — Color-gradient bar (brown→green→blue)
- **Soil Condition Card** — Color-coded status text
- **Historical Logs Table** — Last 50 readings with timestamps

## 4. Conclusion and Learnings

### Conclusion

We successfully created a practical, low-cost soil moisture monitoring system that even non-technical users can understand at a glance. The animated plant character provides an immediately intuitive visual — no need to interpret numbers or graphs. This makes it ideal for farmers, gardeners, and agricultural education.

### Key Learnings

- **Analog Sensor Calibration:** Learning to calibrate raw ADC readings against known reference points (air-dry and water-submerged) and mapping them to meaningful percentages.
- **Data-Driven SVG Animation:** Creating an SVG illustration with CSS classes that dynamically change appearance based on live data — a technique applicable to any IoT dashboard.
- **Threshold-Based Classification:** Implementing multi-level condition classification with appropriate hysteresis to avoid rapid state flickering.
- **User-Centered Design:** Designing interfaces that convey information through familiar metaphors (a plant's health) rather than abstract numbers.
- **Circular Buffer Implementation:** Using a ring buffer data structure for efficient fixed-size history storage on memory-constrained microcontrollers.

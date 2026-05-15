/*
 * ═══════════════════════════════════════════════════════════════
 * ESP8266 Radar Scanner — Access Point + Web Dashboard
 * (Offline-Ready, Tactical Green Radar Theme, Non-Blocking Sweep)
 * * Wiring:
 * SG90 Servo   → VCC to Vin(5V), GND to GND, Signal to D2 (GPIO4)
 * HC-SR04      → VCC to Vin(5V), GND to GND
 * HC-SR04 TRIG → D5 (GPIO14)
 * HC-SR04 ECHO → D6 (GPIO12) -> Use Voltage Divider!
 * * Board: NodeMCU 1.0 (ESP-12E Module) or any ESP8266 board
 * Connect to WiFi: "Radar_AP" Password: "12345678"
 * Open browser → http://192.168.4.1
 * ═══════════════════════════════════════════════════════════════
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Servo.h>

// ── Configuration ────────────────────────────────────────────────
#define SERVO_PIN     4          // D2
#define TRIG_PIN      14         // D5
#define ECHO_PIN      12         // D6

const char* AP_SSID     = "Radar_AP";
const char* AP_PASSWORD = "12345678";
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

const int MAX_DISTANCE_CM = 100; // Radar range (ignore things further than this)

// ── Globals ──────────────────────────────────────────────────────
ESP8266WebServer server(80);
Servo myServo;

// Radar Sweep State Machine
int currentAngle = 15;
int sweepStep = 3;             // Move 3 degrees per tick
uint32_t lastMoveTime = 0;
const int MOVE_INTERVAL = 40;  // ms between servo steps (speed of radar)

// Store distances for a 180-degree arc.
// To save memory and JSON size, we map 0-180 into 37 buckets (every 5 degrees)
int radarMap[37]; 
int closestDist = 999;
int closestAngle = 0;

// ── Helpers ──────────────────────────────────────────────────────
float getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH, 20000); // 20ms timeout (~3.4m)
  if (duration == 0) return MAX_DISTANCE_CM; 
  return (duration * 0.0343) / 2.0; 
}

// ── HTML Dashboard (stored in PROGMEM) ────────────────────────────
const char HTML_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>ESP8266 Tactical Radar</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&display=swap');

  :root {
    --bg:       #020804;
    --panel:    #05140a;
    --border:   #0a2e16;
    --radar:    #00ff41; /* Classic Terminal Green */
    --radar-dim:#005515;
    --blip:     #ff3333; /* Hostile Red */
    --text:     #ccffcc;
  }

  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

  body {
    background: var(--bg); color: var(--text);
    font-family: 'Share Tech Mono', monospace;
    min-height: 100vh; padding: 20px;
    background-image: 
      linear-gradient(rgba(0, 255, 65, 0.03) 1px, transparent 1px),
      linear-gradient(90deg, rgba(0, 255, 65, 0.03) 1px, transparent 1px);
    background-size: 30px 30px;
  }

  header { text-align: center; margin-bottom: 20px; text-transform: uppercase; }
  header h1 { font-size: 2rem; color: var(--radar); text-shadow: 0 0 10px var(--radar); letter-spacing: 2px;}
  header p { color: var(--radar-dim); font-size: 0.8rem; letter-spacing: 4px; }

  /* ── Stats Grid ── */
  .stats {
    display: flex; justify-content: center; gap: 20px; margin-bottom: 30px; flex-wrap: wrap;
  }
  .stat-box {
    background: var(--panel); border: 1px solid var(--border);
    padding: 15px 25px; border-radius: 8px; text-align: center;
    min-width: 150px; box-shadow: inset 0 0 15px rgba(0,255,65,0.05);
  }
  .stat-label { font-size: 0.7rem; color: var(--radar-dim); margin-bottom: 5px; }
  .stat-val { font-size: 1.8rem; font-weight: bold; color: var(--radar); text-shadow: 0 0 8px rgba(0,255,65,0.4); }
  .stat-val.alert { color: var(--blip); text-shadow: 0 0 10px var(--blip); }

  /* ── Radar Canvas Area ── */
  .radar-container {
    display: flex; justify-content: center; position: relative;
    margin: 0 auto; width: 100%; max-width: 600px;
  }
  canvas {
    background: var(--panel);
    border: 2px solid var(--radar-dim);
    border-radius: 50% 50% 0 0;
    box-shadow: 0 0 30px rgba(0,255,65,0.1);
    max-width: 100%;
    height: auto;
  }
  
  .scanline {
    position: absolute; width: 100%; height: 2px; background: rgba(0,255,65,0.2);
    top: 0; left: 0; animation: scan 4s linear infinite; pointer-events: none;
  }
  @keyframes scan { 0% { top: 0; } 100% { top: 100%; } }

  footer { text-align: center; margin-top: 40px; color: var(--radar-dim); font-size: 0.7rem; }
</style>
</head>
<body>

<header>
  <h1>SYS.RADAR_SCAN</h1>
  <p>AP-MODE // 192.168.4.1 // SECURE</p>
</header>

<div class="stats">
  <div class="stat-box">
    <div class="stat-label">SCAN ANGLE</div>
    <div class="stat-val" id="angleVal">--&deg;</div>
  </div>
  <div class="stat-box">
    <div class="stat-label">PROXIMITY ALERT</div>
    <div class="stat-val" id="distVal">-- cm</div>
  </div>
</div>

<div class="radar-container">
  <canvas id="radarCanvas" width="600" height="300"></canvas>
  <div class="scanline"></div>
</div>

<footer>AEROSPACE DEFENSE DASHBOARD &bull; SYSTEM ONLINE</footer>

<script>
const canvas = document.getElementById('radarCanvas');
const ctx = canvas.getContext('2d');
const cx = canvas.width / 2;
const cy = canvas.height;
const radius = canvas.width / 2 - 20; 
const MAX_RANGE = 100; // cm

function drawRadarGrid() {
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  ctx.strokeStyle = 'rgba(0, 255, 65, 0.3)';
  ctx.lineWidth = 1;

  // Concentric arcs
  for (let r = 0.25; r <= 1; r += 0.25) {
    ctx.beginPath();
    ctx.arc(cx, cy, radius * r, Math.PI, 0);
    ctx.stroke();
  }

  // Angle lines
  for (let a = 0; a <= 180; a += 30) {
    let rad = a * Math.PI / 180;
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.lineTo(cx - radius * Math.cos(rad), cy - radius * Math.sin(rad));
    ctx.stroke();
  }
}

function drawBlips(dataArray, currentAngle) {
  // Draw the sweeping beam
  let beamRad = currentAngle * Math.PI / 180;
  ctx.beginPath();
  ctx.moveTo(cx, cy);
  ctx.lineTo(cx - radius * Math.cos(beamRad), cy - radius * Math.sin(beamRad));
  ctx.strokeStyle = 'rgba(0, 255, 65, 0.8)';
  ctx.lineWidth = 3;
  ctx.stroke();

  // Draw detected objects
  for(let i = 0; i < dataArray.length; i++) {
    let dist = dataArray[i];
    if (dist < MAX_RANGE) {
      let angle = i * 5; 
      let rad = angle * Math.PI / 180;
      let scaledDist = (dist / MAX_RANGE) * radius;
      
      let x = cx - scaledDist * Math.cos(rad);
      let y = cy - scaledDist * Math.sin(rad);

      ctx.beginPath();
      ctx.arc(x, y, 4, 0, Math.PI * 2);
      ctx.fillStyle = dist < 20 ? '#ff3333' : '#00ff41'; // Red if very close
      ctx.fill();
      ctx.shadowBlur = 10;
      ctx.shadowColor = ctx.fillStyle;
      ctx.fill();
      ctx.shadowBlur = 0; // reset
    }
  }
}

async function fetchRadarData() {
  try {
    const res = await fetch('/data');
    const data = await res.json();

    document.getElementById('angleVal').innerHTML = data.angle + '&deg;';
    
    const distEl = document.getElementById('distVal');
    if(data.closest < MAX_RANGE) {
      distEl.textContent = data.closest + ' cm';
      distEl.className = data.closest < 20 ? 'stat-val alert' : 'stat-val';
    } else {
      distEl.textContent = 'CLEAR';
      distEl.className = 'stat-val';
    }

    drawRadarGrid();
    drawBlips(data.map, data.angle);

  } catch(e) {
    console.error("Radar connection lost");
  }
}

// Draw initial empty grid
drawRadarGrid();

// Fetch rapid updates to keep the animation smooth
setInterval(fetchRadarData, 250);
</script>
</body>
</html>
)rawhtml";

// ── HTTP Route Handlers ───────────────────────────────────────────

void handleRoot() {
  server.send_P(200, "text/html", HTML_PAGE);
}

void handleData() {
  // Construct a lightweight JSON payload
  String json = "{";
  json += "\"angle\":" + String(currentAngle) + ",";
  json += "\"closest\":" + String(closestDist) + ",";
  json += "\"map\":[";
  for(int i = 0; i < 37; i++) {
    json += String(radarMap[i]);
    if(i < 36) json += ",";
  }
  json += "]}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

// ── Setup ─────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  myServo.attach(SERVO_PIN);
  myServo.write(currentAngle);
  
  // Init map with max distance (empty)
  for(int i=0; i<37; i++) radarMap[i] = MAX_DISTANCE_CM;

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, AP_SUBNET);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  
  Serial.println("\n[SYSTEM] RADAR ONLINE");
  Serial.println("[WIFI] SSID: " + String(AP_SSID));

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
}

// ── Loop (Non-Blocking) ───────────────────────────────────────────
void loop() {
  server.handleClient();

  uint32_t now = millis();
  
  // Non-blocking servo sweep
  if (now - lastMoveTime >= MOVE_INTERVAL) {
    lastMoveTime = now;

    // 1. Take reading
    int dist = (int)getDistance();
    if(dist > MAX_DISTANCE_CM) dist = MAX_DISTANCE_CM;

    // 2. Map angle to array index (0-180 mapped to 0-36)
    int index = currentAngle / 5;
    if (index >= 0 && index <= 36) {
      radarMap[index] = dist;
    }

    // Calculate closest object dynamically
    closestDist = MAX_DISTANCE_CM;
    for(int i=0; i<37; i++) {
      if(radarMap[i] < closestDist) {
        closestDist = radarMap[i];
      }
    }

    // 3. Move Servo
    currentAngle += sweepStep;
    
    // Bounds check (Avoid 0 and 180 to prevent cheap servos from jittering/stalling)
    if (currentAngle >= 165) {
      currentAngle = 165;
      sweepStep = -abs(sweepStep); // Reverse direction
    } 
    else if (currentAngle <= 15) {
      currentAngle = 15;
      sweepStep = abs(sweepStep);  // Forward direction
    }
    
    myServo.write(currentAngle);
  }
}
/*
 * ═══════════════════════════════════════════════════════════════
 * ESP8266 TEMT6000 Lux Meter — Access Point + Web Dashboard
 * (Offline-Ready, Rounded Gauges, LED Indicator)
 * * Wiring:
 * TEMT6000 OUT/SIG → A0 (Analog Pin)
 * TEMT6000 VCC     → 3.3V
 * TEMT6000 GND     → GND
 * Status LED       → D2 (GPIO4) -> 220Ω Resistor -> GND
 * * Board: NodeMCU 1.0 (ESP-12E Module) or any ESP8266 board
 * Connect to WiFi SSID: "LuxMeter_AP"  Password: "12345678"
 * Then open browser → http://192.168.4.1
 * ═══════════════════════════════════════════════════════════════
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ── Configuration ────────────────────────────────────────────────
#define SENSOR_PIN    A0         // Analog input for TEMT6000
#define LED_PIN       4          // GPIO4 = D2 on NodeMCU

const char* AP_SSID     = "LuxMeter_AP";
const char* AP_PASSWORD = "12345678";      // min 8 chars for WPA2
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

const uint16_t HISTORY_SIZE    = 100;   // max data points kept in RAM
const uint32_t SAMPLE_INTERVAL = 5000;  // ms between readings

// ── Globals ──────────────────────────────────────────────────────
ESP8266WebServer server(80);

struct Record {
  float    lux;
  int      raw;
  uint32_t ts;   // seconds since boot
};

Record   history[HISTORY_SIZE];
uint16_t histHead  = 0;   
uint16_t histCount = 0;   
uint32_t lastSample = 0;

float currentLux = 0.0;
int   currentRaw = 0;
String currentCondition = "Unknown";

// ── Helpers ──────────────────────────────────────────────────────
void addRecord(float l, int r) {
  uint16_t idx = (histHead + histCount) % HISTORY_SIZE;
  history[idx].lux = l;
  history[idx].raw = r;
  history[idx].ts  = millis() / 1000;

  if (histCount < HISTORY_SIZE) {
    histCount++;
  } else {
    histHead = (histHead + 1) % HISTORY_SIZE; 
  }
}

String buildHistoryJSON() {
  String json = "{\"count\":" + String(histCount) + ",\"records\":[";
  for (uint16_t i = 0; i < histCount; i++) {
    uint16_t idx = (histHead + i) % HISTORY_SIZE;
    if (i > 0) json += ",";
    json += "{\"l\":" + String(history[idx].lux, 1) +
            ",\"r\":" + String(history[idx].raw)    +
            ",\"s\":" + String(history[idx].ts)     + "}";
  }
  json += "]}";
  return json;
}

// ── HTML Dashboard (stored in PROGMEM to save RAM) ────────────────
const char HTML_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>ESP8266 Lux Meter</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Exo+2:wght@300;600;800&display=swap');

  :root {
    --bg:       #0a0a0a;
    --panel:    #121212;
    --border:   #2a2a2a;
    --accent1:  #ffcc00; /* Sun Yellow */
    --accent2:  #fdb813; /* Deep Yellow */
    --accent3:  #00ff9d; /* Success Green */
    --text:     #e0e0e0;
    --muted:    #666666;
    --glow1:    0 0 20px rgba(255,204,0,0.25);
  }

  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

  body {
    background: var(--bg);
    color: var(--text);
    font-family: 'Exo 2', sans-serif;
    min-height: 100vh;
    padding: 20px;
    background-image:
      radial-gradient(ellipse at 50% 0%, rgba(255,204,0,0.05) 0%, transparent 60%);
  }

  header { text-align: center; margin-bottom: 32px; }
  header h1 {
    font-size: clamp(1.5rem, 5vw, 2.6rem);
    font-weight: 800;
    letter-spacing: 0.06em;
    background: linear-gradient(135deg, var(--accent1), #fff);
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
    background-clip: text;
    text-transform: uppercase;
  }
  header p {
    color: var(--muted);
    font-family: 'Share Tech Mono', monospace;
    font-size: 0.78rem;
    margin-top: 6px;
    letter-spacing: 0.12em;
  }

  .status-bar {
    display: flex; align-items: center; justify-content: center;
    gap: 10px; margin-bottom: 28px;
    font-family: 'Share Tech Mono', monospace; font-size: 0.75rem; color: var(--muted);
  }
  .dot {
    width: 8px; height: 8px; border-radius: 50%;
    background: var(--accent3); box-shadow: 0 0 8px var(--accent3);
    animation: pulse 2s infinite;
  }
  @keyframes pulse { 0%,100% { opacity:1; } 50% { opacity:0.3; } }

  /* ── Cards grid ── */
  .cards {
    display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
    gap: 18px; margin-bottom: 28px;
  }
  .card {
    background: var(--panel); border: 1px solid var(--border);
    border-radius: 16px; padding: 28px 24px; position: relative;
    overflow: hidden; transition: border-color 0.3s;
  }
  .card::before { content: ''; position: absolute; top: 0; left: 0; right: 0; height: 2px; }
  .card.lux::before  { background: linear-gradient(90deg, var(--accent1), transparent); }
  .card.raw::before  { background: linear-gradient(90deg, #a855f7, transparent); }
  .card.cond::before { background: linear-gradient(90deg, var(--accent3), transparent); }
  .card.count::before{ background: linear-gradient(90deg, #4a6070, transparent); }
  .card:hover { border-color: var(--accent1); }

  .card-icon { font-size: 2rem; margin-bottom: 10px; display: block; }
  .card-label {
    font-size: 0.72rem; letter-spacing: 0.18em; text-transform: uppercase;
    color: var(--muted); margin-bottom: 6px;
  }
  .card-value { font-family: 'Share Tech Mono', monospace; font-size: clamp(2rem, 5vw, 2.8rem); font-weight: 700; line-height: 1; }
  
  .card.lux .card-value { color: var(--accent1); text-shadow: var(--glow1); }
  .card.raw .card-value { color: #a855f7; }
  .card.cond .card-value{ color: var(--accent3); font-size: clamp(1.5rem, 4vw, 2rem); }
  
  .card-unit { font-size: 0.85rem; color: var(--muted); margin-left: 4px; }

  /* Rounded gauge bar */
  .gauge-track {
    margin-top: 14px; height: 10px; background: var(--border);
    border-radius: 10px; overflow: hidden;
  }
  .gauge-fill { height: 100%; border-radius: 10px; transition: width 0.6s ease; }
  .card.lux .gauge-fill { background: linear-gradient(90deg, var(--accent2), #ffff99); }
  .card.raw .gauge-fill { background: linear-gradient(90deg, #a855f7, #d8b4fe); }

  /* ── History table ── */
  .table-box {
    background: var(--panel); border: 1px solid var(--border);
    border-radius: 16px; padding: 24px; overflow-x: auto;
  }
  .table-box h2 {
    font-size: 0.75rem; letter-spacing: 0.16em; text-transform: uppercase;
    color: var(--muted); margin-bottom: 16px;
  }
  table { width: 100%; border-collapse: collapse; font-family: 'Share Tech Mono', monospace; font-size: 0.82rem; }
  th { text-align: left; color: var(--muted); font-size: 0.7rem; letter-spacing: 0.12em; padding: 0 12px 10px; border-bottom: 1px solid var(--border); }
  td { padding: 9px 12px; border-bottom: 1px solid rgba(42,42,42,0.5); color: var(--text); }
  tr:last-child td { border-bottom: none; }
  tr:hover td { background: rgba(255,204,0,0.04); }
  .td-lux { color: var(--accent1); }
  .td-raw { color: #a855f7; }
  .td-idx { color: var(--muted); font-size: 0.7rem; }

  footer { text-align: center; margin-top: 32px; color: var(--muted); font-size: 0.72rem; font-family: 'Share Tech Mono', monospace; letter-spacing: 0.1em; }
</style>
</head>
<body>

<header>
  <h1>&#9728;&#xFE0F; Lux Meter</h1>
  <p>ESP8266 &bull; TEMT6000 &bull; ACCESS POINT MODE</p>
</header>

<div class="status-bar">
  <span class="dot" id="statusDot"></span>
  <span id="statusText">Sensor Active</span>
  &nbsp;|&nbsp;
  <span id="uptimeText">Uptime: --</span>
  &nbsp;|&nbsp;
  Auto-refresh: 5s
</div>

<div class="cards">
  <div class="card lux">
    <span class="card-icon">&#128161;</span>
    <div class="card-label">Illuminance</div>
    <div class="card-value" id="luxVal">--<span class="card-unit">lx</span></div>
    <div class="gauge-track"><div class="gauge-fill" id="luxGauge" style="width:0%"></div></div>
  </div>
  <div class="card raw">
    <span class="card-icon">&#128208;</span>
    <div class="card-label">Analog Raw (0-1023)</div>
    <div class="card-value" id="rawVal">--</div>
    <div class="gauge-track"><div class="gauge-fill" id="rawGauge" style="width:0%"></div></div>
  </div>
  <div class="card cond">
    <span class="card-icon">&#127748;</span>
    <div class="card-label">Environment</div>
    <div class="card-value" id="condVal">--</div>
  </div>
  <div class="card count">
    <span class="card-icon">&#x1F4CA;</span>
    <div class="card-label">Samples Stored</div>
    <div class="card-value" id="countVal">0</div>
  </div>
</div>

<div class="table-box">
  <h2>&#x1F4CB; Historical Records (newest first)</h2>
  <table>
    <thead>
      <tr>
        <th>#</th>
        <th>ILLUMINANCE (Lux)</th>
        <th>RAW ANALOG</th>
        <th>UPTIME (s)</th>
      </tr>
    </thead>
    <tbody id="histTable">
      <tr><td colspan="4" style="color:var(--muted);text-align:center">Loading...</td></tr>
    </tbody>
  </table>
</div>

<footer>ESP8266 Lux Meter &bull; 192.168.4.1 &bull; SSID: LuxMeter_AP</footer>

<script>
function fmtUptime(s) {
  const h = Math.floor(s / 3600);
  const m = Math.floor((s % 3600) / 60);
  const sec = s % 60;
  return (h ? h + 'h ' : '') + (m ? m + 'm ' : '') + sec + 's';
}

async function fetchData() {
  try {
    const [liveRes, histRes] = await Promise.all([ fetch('/live'), fetch('/history') ]);
    const live = await liveRes.json();
    const hist = await histRes.json();

    // ── Live cards ──
    document.getElementById('luxVal').innerHTML  = live.lux.toFixed(1) + '<span class="card-unit">lx</span>';
    document.getElementById('rawVal').textContent = live.raw;
    document.getElementById('condVal').textContent = live.cond;
    document.getElementById('countVal').textContent = hist.count;

    // Gauges: Assuming typical indoor max is ~1000 lux for the visual gauge scale
    document.getElementById('luxGauge').style.width = Math.min((live.lux / 1000) * 100, 100) + '%';
    document.getElementById('rawGauge').style.width = Math.min((live.raw / 1023) * 100, 100) + '%';

    document.getElementById('uptimeText').textContent = 'Uptime: ' + fmtUptime(live.uptime);

    // ── History table (newest first) ──
    const tbody = document.getElementById('histTable');
    if (hist.count === 0) {
      tbody.innerHTML = '<tr><td colspan="4" style="color:var(--muted);text-align:center">No data yet</td></tr>';
    } else {
      const rows = hist.records.slice().reverse().slice(0, 50);
      tbody.innerHTML = rows.map((r, i) =>
        `<tr>
          <td class="td-idx">${hist.count - i}</td>
          <td class="td-lux">${r.l.toFixed(1)}</td>
          <td class="td-raw">${r.r}</td>
          <td>${fmtUptime(r.s)}</td>
        </tr>`
      ).join('');
    }
  } catch(e) {
    document.getElementById('statusText').textContent = 'Connection error';
    document.getElementById('statusDot').style.background = '#ff4444';
    document.getElementById('statusDot').style.boxShadow  = '0 0 8px #ff4444';
  }
}

fetchData();
setInterval(fetchData, 5000);
</script>
</body>
</html>
)rawhtml";

// ── HTTP Route Handlers ───────────────────────────────────────────

void handleRoot() {
  server.send_P(200, "text/html", HTML_PAGE);
}

void handleLive() {
  String json = "{";
  json += "\"lux\":"  + String(currentLux, 2) + ",";
  json += "\"raw\":"  + String(currentRaw) + ",";
  json += "\"cond\":\"" + currentCondition + "\",";
  json += "\"uptime\":" + String(millis() / 1000);
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleHistory() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", buildHistoryJSON());
}

void handleNotFound() {
  server.send(404, "text/plain", "404 - Not Found");
}

// ── Setup ─────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  delay(200);
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  Serial.println("\n\n=== ESP8266 TEMT6000 Lux Meter ===");

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, AP_SUBNET);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  
  Serial.println("[WiFi] AP started");
  Serial.println("  SSID    : " + String(AP_SSID));
  Serial.println("  Password: " + String(AP_PASSWORD));
  Serial.println("  IP      : " + AP_IP.toString());

  server.on("/",        handleRoot);
  server.on("/live",    handleLive);
  server.on("/history", handleHistory);
  server.onNotFound(handleNotFound);
  server.begin();
  
  Serial.println("[HTTP] Server started on port 80");
  Serial.println("Open browser → http://192.168.4.1");
  Serial.println("================================\n");
}

// ── Loop ──────────────────────────────────────────────────────────
void loop() {
  server.handleClient();

  uint32_t now = millis();
  if (now - lastSample >= SAMPLE_INTERVAL) {
    lastSample = now;

    // 1. Read Analog Pin
    currentRaw = analogRead(SENSOR_PIN);
    
    // 2. Convert to Lux (Approximation for standard 10k resistor module)
    // ESP8266 ADC is 0-1V internally, but NodeMCU board has a voltage divider mapping 0-3.3V to 0-1023
    float volts = currentRaw * (3.3 / 1023.0);
    float amps = volts / 10000.0;           // Assuming 10k pull-down resistor on module
    float microamps = amps * 1000000.0;
    currentLux = microamps * 2.0;           // TEMT6000 spec: 50uA @ 100 Lux (2 Lux per uA)

    // 3. Determine text condition
    if (currentLux < 10)       currentCondition = "Dark";
    else if (currentLux < 50)  currentCondition = "Dim";
    else if (currentLux < 300) currentCondition = "Normal";
    else if (currentLux < 800) currentCondition = "Bright";
    else                       currentCondition = "Intense";

    // 4. Save and output
    addRecord(currentLux, currentRaw);
    Serial.printf("[TEMT6000] Raw: %d | Lux: %.1f | Status: %s\n", currentRaw, currentLux, currentCondition.c_str());
    
    // Blink LED upon successful read
    digitalWrite(LED_PIN, HIGH);
    delay(100); 
    digitalWrite(LED_PIN, LOW);
  }

  yield();
}
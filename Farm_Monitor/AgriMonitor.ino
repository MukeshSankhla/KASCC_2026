/*
 * ═══════════════════════════════════════════════════════════════
 * ESP8266 Soil Moisture Monitor — Access Point + Web Dashboard
 * (Offline-Ready, Agriculture Theme, Rounded Gauges)
 * * Wiring:
 * Soil Sensor A0  → A0 (Analog Pin)
 * Soil Sensor VCC → 3.3V
 * Soil Sensor GND → GND
 * Status LED      → D2 (GPIO4) -> 220Ω Resistor -> GND
 * * Board: NodeMCU 1.0 (ESP-12E Module) or any ESP8266 board
 * Connect to WiFi SSID: "AgriMonitor_AP"  Password: "12345678"
 * Then open browser → http://192.168.4.1
 * ═══════════════════════════════════════════════════════════════
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ── Configuration ────────────────────────────────────────────────
#define SENSOR_PIN    A0         // Analog input for Soil Sensor
#define LED_PIN       4          // GPIO4 = D2 on NodeMCU

const char* AP_SSID     = "AgriMonitor_AP";
const char* AP_PASSWORD = "12345678";
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

const uint16_t HISTORY_SIZE    = 100;
const uint32_t SAMPLE_INTERVAL = 5000;  // ms between readings

// Calibration Values (Adjust these based on your specific sensor testing)
const int DRY_SOIL_VAL  = 1023; // Value when completely dry in air
const int WET_SOIL_VAL  = 400;  // Value when submerged in a glass of water

// ── Globals ──────────────────────────────────────────────────────
ESP8266WebServer server(80);

struct Record {
  int      percent;
  int      raw;
  uint32_t ts;
};

Record   history[HISTORY_SIZE];
uint16_t histHead  = 0;   
uint16_t histCount = 0;   
uint32_t lastSample = 0;

int currentPercent = 0;
int currentRaw = 0;
String currentStatus = "Unknown";

// ── Helpers ──────────────────────────────────────────────────────
void addRecord(int p, int r) {
  uint16_t idx = (histHead + histCount) % HISTORY_SIZE;
  history[idx].percent = p;
  history[idx].raw     = r;
  history[idx].ts      = millis() / 1000;

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
    json += "{\"p\":" + String(history[idx].percent) +
            ",\"r\":" + String(history[idx].raw) +
            ",\"s\":" + String(history[idx].ts)  + "}";
  }
  json += "]}";
  return json;
}

// ── HTML Dashboard (stored in PROGMEM) ────────────────────────────
const char HTML_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>ESP8266 Soil Monitor</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Exo+2:wght@300;600;800&display=swap');

  :root {
    /* Agriculture / Greenhouse Theme */
    --bg:       #0a100d; /* Very dark earthy green */
    --panel:    #141c16; /* Lighter forest panel */
    --border:   #243627;
    --accent1:  #4ade80; /* Healthy Leaf Green */
    --accent2:  #38bdf8; /* Water Blue */
    --accent3:  #d97706; /* Dry Dirt Orange/Brown */
    --text:     #e2e8f0;
    --muted:    #64748b;
    --glow1:    0 0 20px rgba(74, 222, 128, 0.25);
  }

  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

  body {
    background: var(--bg);
    color: var(--text);
    font-family: 'Exo 2', sans-serif;
    min-height: 100vh;
    padding: 20px;
    background-image: radial-gradient(ellipse at 50% 0%, rgba(74, 222, 128, 0.05) 0%, transparent 60%);
  }

  header { text-align: center; margin-bottom: 32px; }
  header h1 {
    font-size: clamp(1.5rem, 5vw, 2.6rem);
    font-weight: 800;
    letter-spacing: 0.06em;
    background: linear-gradient(135deg, var(--accent1), var(--accent2));
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
    background: var(--accent1); box-shadow: 0 0 8px var(--accent1);
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
  .card.moist::before { background: linear-gradient(90deg, var(--accent1), transparent); }
  .card.raw::before   { background: linear-gradient(90deg, var(--accent3), transparent); }
  .card.cond::before  { background: linear-gradient(90deg, var(--accent2), transparent); }
  .card.count::before { background: linear-gradient(90deg, #8b5cf6, transparent); }
  .card:hover { border-color: var(--accent1); }

  .card-icon { font-size: 2rem; margin-bottom: 10px; display: block; }
  .card-label {
    font-size: 0.72rem; letter-spacing: 0.18em; text-transform: uppercase;
    color: var(--muted); margin-bottom: 6px;
  }
  .card-value { font-family: 'Share Tech Mono', monospace; font-size: clamp(2rem, 5vw, 2.8rem); font-weight: 700; line-height: 1; }
  
  .card.moist .card-value { color: var(--accent1); text-shadow: var(--glow1); }
  .card.raw .card-value   { color: var(--accent3); }
  .card.cond .card-value  { color: var(--accent2); font-size: clamp(1.4rem, 4vw, 2.2rem); }
  
  .card-unit { font-size: 0.85rem; color: var(--muted); margin-left: 4px; }

  /* Rounded gauge bar */
  .gauge-track {
    margin-top: 14px; height: 10px; background: var(--border);
    border-radius: 10px; overflow: hidden;
  }
  .gauge-fill { height: 100%; border-radius: 10px; transition: width 0.6s ease; }
  .card.moist .gauge-fill { background: linear-gradient(90deg, var(--accent3), var(--accent1), var(--accent2)); }
  .card.raw .gauge-fill   { background: linear-gradient(90deg, #b45309, var(--accent3)); }

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
  td { padding: 9px 12px; border-bottom: 1px solid rgba(36, 54, 39, 0.5); color: var(--text); }
  tr:last-child td { border-bottom: none; }
  tr:hover td { background: rgba(74, 222, 128, 0.04); }
  .td-moist { color: var(--accent1); font-weight: bold;}
  .td-raw   { color: var(--accent3); }
  .td-idx   { color: var(--muted); font-size: 0.7rem; }

  footer { text-align: center; margin-top: 32px; color: var(--muted); font-size: 0.72rem; font-family: 'Share Tech Mono', monospace; letter-spacing: 0.1em; }
</style>
</head>
<body>

<header>
  <h1>&#x1F33F; Agri Monitor</h1>
  <p>ESP8266 &bull; SOIL MOISTURE &bull; AP MODE</p>
</header>

<div class="status-bar">
  <span class="dot" id="statusDot"></span>
  <span id="statusText">Sensor Active</span>
  &nbsp;|&nbsp;
  <span id="uptimeText">Uptime: --</span>
  &nbsp;|&nbsp;
  Refresh: 5s
</div>

<div class="cards">
  <div class="card moist">
    <span class="card-icon">&#x1F331;</span>
    <div class="card-label">Soil Moisture</div>
    <div class="card-value" id="moistVal">--<span class="card-unit">%</span></div>
    <div class="gauge-track"><div class="gauge-fill" id="moistGauge" style="width:0%"></div></div>
  </div>
  <div class="card raw">
    <span class="card-icon">&#x1F30D;</span>
    <div class="card-label">Analog Raw (0-1023)</div>
    <div class="card-value" id="rawVal">--</div>
    <div class="gauge-track"><div class="gauge-fill" id="rawGauge" style="width:0%"></div></div>
  </div>
  <div class="card cond">
    <span class="card-icon">&#x1F4A7;</span>
    <div class="card-label">Soil Condition</div>
    <div class="card-value" id="condVal">--</div>
  </div>
  <div class="card count">
    <span class="card-icon">&#x1F4CA;</span>
    <div class="card-label">Samples Stored</div>
    <div class="card-value" id="countVal">0</div>
  </div>
</div>

<div class="table-box">
  <h2>&#x1F4CB; Moisture Logs (newest first)</h2>
  <table>
    <thead>
      <tr>
        <th>#</th>
        <th>MOISTURE (%)</th>
        <th>RAW RESISTANCE</th>
        <th>UPTIME (s)</th>
      </tr>
    </thead>
    <tbody id="histTable">
      <tr><td colspan="4" style="color:var(--muted);text-align:center">Loading...</td></tr>
    </tbody>
  </table>
</div>

<footer>ESP8266 Agri Monitor &bull; 192.168.4.1 &bull; SSID: AgriMonitor_AP</footer>

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

    document.getElementById('moistVal').innerHTML = live.percent + '<span class="card-unit">%</span>';
    document.getElementById('rawVal').textContent = live.raw;
    document.getElementById('condVal').textContent = live.cond;
    document.getElementById('countVal').textContent = hist.count;

    document.getElementById('moistGauge').style.width = Math.min(live.percent, 100) + '%';
    // For the raw gauge, map it backwards visually so less resistance (wetter) looks fuller
    document.getElementById('rawGauge').style.width = Math.max(0, 100 - (live.raw / 1023 * 100)) + '%';

    document.getElementById('uptimeText').textContent = 'Uptime: ' + fmtUptime(live.uptime);

    // Color code the condition text
    const condEl = document.getElementById('condVal');
    if(live.cond === "Bone Dry") condEl.style.color = "var(--accent3)";
    else if(live.cond === "Waterlogged") condEl.style.color = "var(--accent2)";
    else condEl.style.color = "var(--accent1)";

    const tbody = document.getElementById('histTable');
    if (hist.count === 0) {
      tbody.innerHTML = '<tr><td colspan="4" style="color:var(--muted);text-align:center">No records yet</td></tr>';
    } else {
      const rows = hist.records.slice().reverse().slice(0, 50);
      tbody.innerHTML = rows.map((r, i) =>
        `<tr>
          <td class="td-idx">${hist.count - i}</td>
          <td class="td-moist">${r.p}%</td>
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
  json += "\"percent\":" + String(currentPercent) + ",";
  json += "\"raw\":"     + String(currentRaw) + ",";
  json += "\"cond\":\""  + currentStatus + "\",";
  json += "\"uptime\":"  + String(millis() / 1000);
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
  
  Serial.println("\n\n=== ESP8266 Soil Moisture Monitor ===");

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
    
    // 2. Map raw value to a 0-100% moisture reading
    // map(value, fromLow, fromHigh, toLow, toHigh)
    currentPercent = map(currentRaw, DRY_SOIL_VAL, WET_SOIL_VAL, 0, 100);
    
    // Constrain ensures we don't get negative values or >100% if the sensor drifts
    currentPercent = constrain(currentPercent, 0, 100);

    // 3. Determine textual soil condition
    if (currentPercent < 15)      currentStatus = "Bone Dry";
    else if (currentPercent < 40) currentStatus = "Needs Water";
    else if (currentPercent < 75) currentStatus = "Moist & Healthy";
    else                          currentStatus = "Waterlogged";

    // 4. Save and output
    addRecord(currentPercent, currentRaw);
    Serial.printf("[SOIL] Raw: %d | Moisture: %d%% | Status: %s\n", currentRaw, currentPercent, currentStatus.c_str());
    
    // Blink LED upon successful read
    digitalWrite(LED_PIN, HIGH);
    delay(100); 
    digitalWrite(LED_PIN, LOW);
  }

  yield();
}
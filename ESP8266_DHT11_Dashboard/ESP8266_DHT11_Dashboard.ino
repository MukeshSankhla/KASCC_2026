/*
 * ═══════════════════════════════════════════════════════════════
 * ESP8266 DHT11 Weather Station — Access Point + Web Dashboard
 * (Chart-Free, Rounded Gauges, LED Indicator Version)
 * * Wiring:
 * DHT11 DATA  → D4 (GPIO2)
 * Status LED  → D2 (GPIO4) -> Resistor -> GND
 * DHT11 VCC   → 3.3V
 * DHT11 GND   → GND
 * ═══════════════════════════════════════════════════════════════
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DHT11.h>

// ── Configuration ────────────────────────────────────────────────
#define DHTPIN        5          // D1 on NodeMCU
#define LED_PIN       2          // D4 on NodeMCU

const char* AP_SSID     = "WeatherStation";
const char* AP_PASSWORD = "12345678";      // min 8 chars for WPA2
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

const uint16_t HISTORY_SIZE    = 100;   // max data points kept in RAM
const uint32_t SAMPLE_INTERVAL = 5000;  // ms between readings

// ── Globals ──────────────────────────────────────────────────────
DHT11          dht11(DHTPIN);           
ESP8266WebServer server(80);

struct Record {
  float    temp;
  float    hum;
  uint32_t ts;   // seconds since boot
};

Record   history[HISTORY_SIZE];
uint16_t histHead  = 0;   
uint16_t histCount = 0;   
uint32_t lastSample = 0;

float currentTemp = 0.0;
float currentHum  = 0.0;
bool  sensorOK    = false;

// ── Helpers ──────────────────────────────────────────────────────
void addRecord(float t, float h) {
  uint16_t idx = (histHead + histCount) % HISTORY_SIZE;
  history[idx].temp = t;
  history[idx].hum  = h;
  history[idx].ts   = millis() / 1000;

  if (histCount < HISTORY_SIZE) {
    histCount++;
  } else {
    histHead = (histHead + 1) % HISTORY_SIZE; 
  }
}

// Build a JSON array of the last N records
String buildHistoryJSON() {
  String json = "{\"count\":" + String(histCount) + ",\"records\":[";
  for (uint16_t i = 0; i < histCount; i++) {
    uint16_t idx = (histHead + i) % HISTORY_SIZE;
    if (i > 0) json += ",";
    json += "{\"t\":" + String(history[idx].temp, 1) +
            ",\"h\":" + String(history[idx].hum, 1)  +
            ",\"s\":" + String(history[idx].ts)      + "}";
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
<title>ESP8266 Weather Station</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Exo+2:wght@300;600;800&display=swap');

  :root {
    --bg:       #07090f;
    --panel:    #0d1117;
    --border:   #1e2d40;
    --accent1:  #00d4ff;
    --accent2:  #ff6b35;
    --accent3:  #00ff9d;
    --text:     #c9d8e8;
    --muted:    #4a6070;
    --glow1:    0 0 20px rgba(0,212,255,0.25);
    --glow2:    0 0 20px rgba(255,107,53,0.25);
  }

  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

  body {
    background: var(--bg);
    color: var(--text);
    font-family: 'Exo 2', sans-serif;
    min-height: 100vh;
    padding: 20px;
    background-image:
      radial-gradient(ellipse at 20% 10%, rgba(0,212,255,0.04) 0%, transparent 50%),
      radial-gradient(ellipse at 80% 90%, rgba(255,107,53,0.04) 0%, transparent 50%);
  }

  header {
    text-align: center;
    margin-bottom: 32px;
  }
  header h1 {
    font-size: clamp(1.5rem, 5vw, 2.6rem);
    font-weight: 800;
    letter-spacing: 0.06em;
    background: linear-gradient(135deg, var(--accent1), var(--accent3));
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
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 10px;
    margin-bottom: 28px;
    font-family: 'Share Tech Mono', monospace;
    font-size: 0.75rem;
    color: var(--muted);
  }
  .dot {
    width: 8px; height: 8px;
    border-radius: 50%;
    background: var(--accent3);
    box-shadow: 0 0 8px var(--accent3);
    animation: pulse 2s infinite;
  }
  @keyframes pulse {
    0%,100% { opacity:1; } 50% { opacity:0.3; }
  }

  /* ── Cards grid ── */
  .cards {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
    gap: 18px;
    margin-bottom: 28px;
  }

  .card {
    background: var(--panel);
    border: 1px solid var(--border);
    border-radius: 16px;
    padding: 28px 24px;
    position: relative;
    overflow: hidden;
    transition: border-color 0.3s;
  }
  .card::before {
    content: '';
    position: absolute;
    top: 0; left: 0; right: 0;
    height: 2px;
  }
  .card.temp::before  { background: linear-gradient(90deg, var(--accent2), transparent); }
  .card.hum::before   { background: linear-gradient(90deg, var(--accent1), transparent); }
  .card.heat::before  { background: linear-gradient(90deg, var(--accent3), transparent); }
  .card.count::before { background: linear-gradient(90deg, #a855f7, transparent); }

  .card:hover { border-color: var(--accent1); }

  .card-icon {
    font-size: 2rem;
    margin-bottom: 10px;
    display: block;
  }
  .card-label {
    font-size: 0.72rem;
    letter-spacing: 0.18em;
    text-transform: uppercase;
    color: var(--muted);
    margin-bottom: 6px;
  }
  .card-value {
    font-family: 'Share Tech Mono', monospace;
    font-size: clamp(2rem, 5vw, 2.8rem);
    font-weight: 700;
    line-height: 1;
  }
  .card.temp  .card-value { color: var(--accent2); text-shadow: var(--glow2); }
  .card.hum   .card-value { color: var(--accent1); text-shadow: var(--glow1); }
  .card.heat  .card-value { color: var(--accent3); }
  .card.count .card-value { color: #a855f7; }

  .card-unit {
    font-size: 0.85rem;
    color: var(--muted);
    margin-left: 4px;
  }

  /* Rounded gauge bar */
  .gauge-track {
    margin-top: 14px;
    height: 10px;
    background: var(--border);
    border-radius: 10px;
    overflow: hidden;
  }
  .gauge-fill {
    height: 100%;
    border-radius: 10px;
    transition: width 0.6s ease;
  }
  .card.temp  .gauge-fill { background: linear-gradient(90deg, var(--accent2), #ffcc00); }
  .card.hum   .gauge-fill { background: linear-gradient(90deg, var(--accent1), var(--accent3)); }

  /* ── History table ── */
  .table-box {
    background: var(--panel);
    border: 1px solid var(--border);
    border-radius: 16px;
    padding: 24px;
    overflow-x: auto;
  }
  .table-box h2 {
    font-size: 0.75rem;
    letter-spacing: 0.16em;
    text-transform: uppercase;
    color: var(--muted);
    margin-bottom: 16px;
  }
  table {
    width: 100%;
    border-collapse: collapse;
    font-family: 'Share Tech Mono', monospace;
    font-size: 0.82rem;
  }
  th {
    text-align: left;
    color: var(--muted);
    font-size: 0.7rem;
    letter-spacing: 0.12em;
    padding: 0 12px 10px;
    border-bottom: 1px solid var(--border);
  }
  td {
    padding: 9px 12px;
    border-bottom: 1px solid rgba(30,45,64,0.5);
    color: var(--text);
  }
  tr:last-child td { border-bottom: none; }
  tr:hover td { background: rgba(0,212,255,0.04); }
  .td-temp { color: var(--accent2); }
  .td-hum  { color: var(--accent1); }
  .td-idx  { color: var(--muted); font-size: 0.7rem; }

  footer{text-align:center;margin-top:20px;color:var(--muted);font-size:.72rem;font-family:'Share Tech Mono',monospace;letter-spacing:.1em}
  .college-info{background:var(--panel);border:1px solid var(--border);border-radius:16px;padding:16px 24px;margin-bottom:24px;text-align:center}
  .ci-name{font-size:.95rem;font-weight:700;letter-spacing:.06em;margin-bottom:4px}
  .ci-proj{font-size:.75rem;color:var(--accent2);margin-bottom:12px;font-family:'Share Tech Mono',monospace;letter-spacing:.08em}
  .ci-team{font-size:.72rem;color:var(--muted);line-height:2}
  .ci-team b{color:var(--text);font-weight:600}
  .ci-team .roll{color:var(--muted);font-size:.65rem}
</style>
</head>
<body>

<header>
  <h1>&#127782; Weather Station</h1>
  <p>ESP8266 &bull; DHT11 &bull; ACCESS POINT MODE</p>
</header>
<div class="college-info">
  <div class="ci-name">Karnatak Arts, Science and Commerce College, Bidar</div>
  <div class="ci-proj">Project: Weather Station using ESP8266 &amp; DHT11 Sensor</div>
  <div class="ci-team">
    <b>Rahul Sharma</b> <span class="roll">(Roll No. 2024CS101)</span> &bull;
    <b>Priya Patil</b> <span class="roll">(Roll No. 2024CS102)</span> &bull;
    <b>Amit Kumar</b> <span class="roll">(Roll No. 2024CS103)</span> &bull;
    <b>Sneha Reddy</b> <span class="roll">(Roll No. 2024CS104)</span>
  </div>
</div>

<div class="status-bar">
  <span class="dot" id="statusDot"></span>
  <span id="statusText">Connecting...</span>
  &nbsp;|&nbsp;
  <span id="uptimeText">Uptime: --</span>
  &nbsp;|&nbsp;
  Auto-refresh: 5s
</div>

<div class="cards">
  <div class="card temp">
    <span class="card-icon">&#x1F321;&#xFE0F;</span>
    <div class="card-label">Temperature</div>
    <div class="card-value" id="tempVal">--<span class="card-unit">°C</span></div>
    <div class="gauge-track"><div class="gauge-fill" id="tempGauge" style="width:0%"></div></div>
  </div>
  <div class="card hum">
    <span class="card-icon">&#x1F4A7;</span>
    <div class="card-label">Humidity</div>
    <div class="card-value" id="humVal">--<span class="card-unit">%</span></div>
    <div class="gauge-track"><div class="gauge-fill" id="humGauge" style="width:0%"></div></div>
  </div>
  <div class="card heat">
    <span class="card-icon">&#x1F9EE;</span>
    <div class="card-label">Heat Index</div>
    <div class="card-value" id="heatVal">--<span class="card-unit">°C</span></div>
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
        <th>TEMP (°C)</th>
        <th>HUMIDITY (%)</th>
        <th>UPTIME (s)</th>
      </tr>
    </thead>
    <tbody id="histTable">
      <tr><td colspan="4" style="color:var(--muted);text-align:center">Loading...</td></tr>
    </tbody>
  </table>
</div>

<footer>ESP8266 Weather Station &bull; 192.168.4.1 &bull; SSID: WeatherStation</footer>

<script>
// ─── Data fetch & update ──────────────────────────────────────────
function fmtUptime(s) {
  const h = Math.floor(s / 3600);
  const m = Math.floor((s % 3600) / 60);
  const sec = s % 60;
  return (h ? h + 'h ' : '') + (m ? m + 'm ' : '') + sec + 's';
}

async function fetchData() {
  try {
    const [liveRes, histRes] = await Promise.all([
      fetch('/live'),
      fetch('/history')
    ]);
    const live = await liveRes.json();
    const hist = await histRes.json();

    // ── Live cards ──
    document.getElementById('tempVal').innerHTML =
      live.ok ? live.temp.toFixed(1) + '<span class="card-unit">°C</span>' : '--';
    document.getElementById('humVal').innerHTML =
      live.ok ? live.hum.toFixed(1)  + '<span class="card-unit">%</span>'  : '--';
    document.getElementById('heatVal').innerHTML =
      live.ok ? live.heat.toFixed(1) + '<span class="card-unit">°C</span>' : '--';
    document.getElementById('countVal').textContent = hist.count;

    if (live.ok) {
      document.getElementById('tempGauge').style.width = Math.min(((live.temp + 10) / 60) * 100, 100) + '%';
      document.getElementById('humGauge').style.width  = Math.min(live.hum, 100) + '%';
    }

    document.getElementById('statusDot').style.background = live.ok ? '#00ff9d' : '#ff4444';
    document.getElementById('statusDot').style.boxShadow  = live.ok ? '0 0 8px #00ff9d' : '0 0 8px #ff4444';
    document.getElementById('statusText').textContent = live.ok ? 'Sensor OK' : 'Sensor Error';
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
          <td class="td-temp">${r.t.toFixed(1)}</td>
          <td class="td-hum">${r.h.toFixed(1)}</td>
          <td>${fmtUptime(r.s)}</td>
        </tr>`
      ).join('');
    }

  } catch(e) {
    document.getElementById('statusText').textContent = 'Connection error';
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
  float hi = NAN;
  if (sensorOK) {
    float t = currentTemp;
    float h = currentHum;
    float tf = t * 9.0 / 5.0 + 32.0;
    float hiF = -42.379
              + 2.04901523 * tf
              + 10.14333127 * h
              - 0.22475541 * tf * h
              - 6.83783e-3 * tf * tf
              - 5.48170e-2 * h * h
              + 1.22874e-3 * tf * tf * h
              + 8.52820e-4 * tf * h * h
              - 1.99000e-6 * tf * tf * h * h;
    hi = (hiF - 32.0) * 5.0 / 9.0;
  }

  String json = "{";
  json += "\"ok\":"   + String(sensorOK ? "true" : "false") + ",";
  json += "\"temp\":" + (sensorOK ? String(currentTemp, 2) : "null") + ",";
  json += "\"hum\":"  + (sensorOK ? String(currentHum,  2) : "null") + ",";
  json += "\"heat\":" + (sensorOK ? String(hi, 2)          : "null") + ",";
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
  
  // Setup LED Pin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Ensure it starts off
  
  Serial.println("\n\n=== ESP8266 Weather Station ===");
  Serial.println("[DHT11] Initialized on GPIO " + String(DHTPIN));

  // Configure Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, AP_SUBNET);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.println("[WiFi] AP started");
  Serial.println("  SSID    : " + String(AP_SSID));
  Serial.println("  Password: " + String(AP_PASSWORD));
  Serial.println("  IP      : " + AP_IP.toString());

  // Register routes
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

    int iTemp = 0, iHum = 0;
    int result = dht11.readTemperatureHumidity(iTemp, iHum);

    if (result != 0) {
      sensorOK = false;
      Serial.println("[DHT11] ERROR: " + String(DHT11::getErrorString(result)));
    } else {
      sensorOK     = true;
      currentTemp  = (float)iTemp;
      currentHum   = (float)iHum;
      addRecord(currentTemp, currentHum);
      Serial.printf("[DHT11] T=%d°C  H=%d%%  (samples: %u)\n", iTemp, iHum, histCount);
      
      // Blink LED upon successful read
      digitalWrite(LED_PIN, HIGH);
      delay(100);  // Quick 100ms flash
      digitalWrite(LED_PIN, LOW);
    }
  }

  // Yield to keep watchdog happy
  yield();
}

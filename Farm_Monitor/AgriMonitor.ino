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
  :root{--bg:#0a100d;--panel:#141c16;--border:#243627;--accent1:#4ade80;--accent2:#38bdf8;
    --accent3:#d97706;--text:#e2e8f0;--muted:#64748b;--glow1:0 0 20px rgba(74,222,128,0.25)}
  *,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
  body{background:var(--bg);color:var(--text);font-family:'Exo 2',sans-serif;min-height:100vh;padding:20px;
    background-image:radial-gradient(ellipse at 50% 0%,rgba(74,222,128,0.05) 0%,transparent 60%)}
  header{text-align:center;margin-bottom:24px}
  header h1{font-size:clamp(1.5rem,5vw,2.6rem);font-weight:800;letter-spacing:.06em;
    background:linear-gradient(135deg,var(--accent1),var(--accent2));-webkit-background-clip:text;
    -webkit-text-fill-color:transparent;background-clip:text;text-transform:uppercase}
  header p{color:var(--muted);font-family:'Share Tech Mono',monospace;font-size:.78rem;margin-top:6px;letter-spacing:.12em}
  .status-bar{display:flex;align-items:center;justify-content:center;gap:10px;margin-bottom:20px;
    font-family:'Share Tech Mono',monospace;font-size:.75rem;color:var(--muted)}
  .dot{width:8px;height:8px;border-radius:50%;background:var(--accent1);box-shadow:0 0 8px var(--accent1);animation:pulse 2s infinite}
  @keyframes pulse{0%,100%{opacity:1}50%{opacity:.3}}

  .plant-scene{display:flex;align-items:flex-end;justify-content:center;margin:0 auto 28px;
    max-width:320px;height:260px;position:relative}
  .plant-svg{width:200px;height:240px;transition:filter .5s}
  .stem{stroke:#4ade80;stroke-width:4;fill:none;stroke-linecap:round;transition:d .8s ease}
  .leaf{fill:#4ade80;transition:transform .8s ease,fill .5s;transform-origin:center}
  .leaf-l{transform-origin:50px 100px}
  .leaf-r{transform-origin:50px 100px}
  .pot{fill:#8B5E3C}
  .pot-rim{fill:#A0704B}
  .soil{fill:#5C3D2E;transition:fill .5s}
  .face{transition:opacity .3s}
  .drop{fill:var(--accent2);opacity:0;animation:drip 2s ease-in infinite}
  .drop:nth-child(2){animation-delay:.7s}
  .drop:nth-child(3){animation-delay:1.4s}
  @keyframes drip{0%{opacity:0;transform:translateY(0)}30%{opacity:.8}100%{opacity:0;transform:translateY(40px)}}
  .crack{stroke:#3a2518;stroke-width:2;fill:none;opacity:0}

  .thirsty .leaf{transform:rotate(25deg) scaleY(0.7);fill:#a3c47a}
  .thirsty .stem{stroke:#a3c47a}
  .thirsty .drop{opacity:1}
  .bonedry .leaf{transform:rotate(45deg) scaleY(0.5);fill:#8B7355}
  .bonedry .stem{stroke:#8B7355}
  .bonedry .soil{fill:#3a2518}
  .bonedry .crack{opacity:1}
  .bonedry .drop{animation:none;opacity:0}
  .waterlogged .leaf{fill:#22c55e;animation:sway 2s ease-in-out infinite}
  .waterlogged .stem{stroke:#22c55e}
  @keyframes sway{0%,100%{transform:rotate(0)}50%{transform:rotate(-3deg)}}
  .overflow-water{position:absolute;bottom:20px;width:180px;height:0;background:rgba(56,189,248,0.3);
    border-radius:50%;transition:height .5s;filter:blur(3px)}
  .waterlogged .overflow-water{height:20px;animation:wRipple 1.5s ease-in-out infinite}
  @keyframes wRipple{0%,100%{transform:scaleX(1)}50%{transform:scaleX(1.1)}}

  .healthy .leaf{animation:sway 3s ease-in-out infinite}

  .mood-text{text-align:center;font-family:'Share Tech Mono',monospace;font-size:1rem;margin-bottom:20px;
    letter-spacing:.1em;min-height:1.5em}

  .cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:16px;margin-bottom:24px}
  .card{background:var(--panel);border:1px solid var(--border);border-radius:16px;padding:22px 20px;
    position:relative;overflow:hidden;transition:border-color .3s,transform .3s}
  .card:hover{border-color:var(--accent1);transform:translateY(-2px)}
  .card::before{content:'';position:absolute;top:0;left:0;right:0;height:2px}
  .card.moist::before{background:linear-gradient(90deg,var(--accent1),transparent)}
  .card.raw::before{background:linear-gradient(90deg,var(--accent3),transparent)}
  .card.cond::before{background:linear-gradient(90deg,var(--accent2),transparent)}
  .card.count::before{background:linear-gradient(90deg,#8b5cf6,transparent)}
  .card-icon{font-size:1.8rem;margin-bottom:8px;display:block}
  .card-label{font-size:.7rem;letter-spacing:.18em;text-transform:uppercase;color:var(--muted);margin-bottom:6px}
  .card-value{font-family:'Share Tech Mono',monospace;font-size:clamp(1.6rem,4vw,2.2rem);font-weight:700;line-height:1}
  .card.moist .card-value{color:var(--accent1);text-shadow:var(--glow1)}
  .card.raw .card-value{color:var(--accent3)}
  .card.cond .card-value{color:var(--accent2);font-size:clamp(1.1rem,3vw,1.6rem)}
  .card-unit{font-size:.8rem;color:var(--muted);margin-left:4px}
  .gauge-track{margin-top:12px;height:8px;background:var(--border);border-radius:8px;overflow:hidden}
  .gauge-fill{height:100%;border-radius:8px;transition:width .6s ease}
  .card.moist .gauge-fill{background:linear-gradient(90deg,var(--accent3),var(--accent1),var(--accent2))}
  .card.raw .gauge-fill{background:linear-gradient(90deg,#b45309,var(--accent3))}

  .table-box{background:var(--panel);border:1px solid var(--border);border-radius:16px;padding:24px;overflow-x:auto}
  .table-box h2{font-size:.75rem;letter-spacing:.16em;text-transform:uppercase;color:var(--muted);margin-bottom:16px}
  table{width:100%;border-collapse:collapse;font-family:'Share Tech Mono',monospace;font-size:.82rem}
  th{text-align:left;color:var(--muted);font-size:.7rem;letter-spacing:.12em;padding:0 12px 10px;border-bottom:1px solid var(--border)}
  td{padding:9px 12px;border-bottom:1px solid rgba(36,54,39,0.5);color:var(--text)}
  tr:last-child td{border-bottom:none}
  tr:hover td{background:rgba(74,222,128,0.04)}
  .td-moist{color:var(--accent1);font-weight:bold}.td-raw{color:var(--accent3)}.td-idx{color:var(--muted);font-size:.7rem}
  footer{text-align:center;margin-top:20px;color:var(--muted);font-size:.72rem;font-family:'Share Tech Mono',monospace;letter-spacing:.1em}
  .college-info{background:var(--panel);border:1px solid var(--border);border-radius:16px;padding:20px 24px;margin-top:28px;text-align:center}
  .ci-name{font-size:.95rem;font-weight:700;letter-spacing:.06em;margin-bottom:4px}
  .ci-proj{font-size:.75rem;color:var(--accent2);margin-bottom:12px;font-family:'Share Tech Mono',monospace;letter-spacing:.08em}
  .ci-team{font-size:.72rem;color:var(--muted);line-height:2}
  .ci-team b{color:var(--text);font-weight:600}
  .ci-team .roll{color:var(--muted);font-size:.65rem}
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

<div class="plant-scene" id="plantScene">
  <div class="overflow-water"></div>
  <svg class="plant-svg" viewBox="0 0 100 120">
    <!-- Pot -->
    <rect class="pot-rim" x="20" y="78" width="60" height="6" rx="2"/>
    <polygon class="pot" points="24,84 76,84 72,115 28,115"/>
    <rect class="soil" x="28" y="84" width="44" height="10" rx="1"/>
    <!-- Cracks in dry soil -->
    <path class="crack" d="M35,88 L38,92 L33,94"/>
    <path class="crack" d="M55,87 L58,91 L53,93"/>
    <path class="crack" d="M65,89 L62,93"/>
    <!-- Stem -->
    <path class="stem" d="M50,82 Q50,55 48,35"/>
    <!-- Leaves -->
    <ellipse class="leaf leaf-l" cx="36" cy="50" rx="16" ry="7" transform="rotate(-30,36,50)"/>
    <ellipse class="leaf leaf-r" cx="62" cy="45" rx="16" ry="7" transform="rotate(25,62,45)"/>
    <ellipse class="leaf leaf-l" cx="38" cy="38" rx="12" ry="5" transform="rotate(-20,38,38)"/>
    <!-- Face -->
    <g class="face" id="plantFace">
      <circle cx="44" cy="28" r="2" fill="#1a1a2e"/>
      <circle cx="56" cy="28" r="2" fill="#1a1a2e"/>
      <path id="mouth" d="M44,33 Q50,38 56,33" stroke="#1a1a2e" stroke-width="1.5" fill="none"/>
    </g>
    <!-- Water drops for thirsty state -->
    <ellipse class="drop" cx="70" cy="30" rx="2" ry="3"/>
    <ellipse class="drop" cx="74" cy="25" rx="1.5" ry="2.5"/>
    <ellipse class="drop" cx="67" cy="22" rx="1.5" ry="2.5"/>
  </svg>
</div>
<div class="mood-text" id="moodText">Checking soil...</div>

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
    <thead><tr><th>#</th><th>MOISTURE (%)</th><th>RAW RESISTANCE</th><th>UPTIME (s)</th></tr></thead>
    <tbody id="histTable">
      <tr><td colspan="4" style="color:var(--muted);text-align:center">Loading...</td></tr>
    </tbody>
  </table>
</div>
<div class="college-info">
  <div class="ci-name">Karnatak Arts, Science and Commerce College, Bidar</div>
  <div class="ci-proj">Project: Smart Farm Monitor using ESP8266 &amp; Soil Sensor</div>
  <div class="ci-team">
    <b>Rahul Sharma</b> <span class="roll">(Roll No. 2024CS101)</span> &bull;
    <b>Priya Patil</b> <span class="roll">(Roll No. 2024CS102)</span> &bull;
    <b>Amit Kumar</b> <span class="roll">(Roll No. 2024CS103)</span> &bull;
    <b>Sneha Reddy</b> <span class="roll">(Roll No. 2024CS104)</span>
  </div>
</div>
<footer>ESP8266 Agri Monitor &bull; 192.168.4.1 &bull; SSID: AgriMonitor_AP</footer>

<script>
function fmtUptime(s){var h=Math.floor(s/3600);var m=Math.floor((s%3600)/60);var sec=s%60;return(h?h+'h ':'')+(m?m+'m ':'')+sec+'s'}
function setPlantState(cond,pct){
  var sc=document.getElementById('plantScene');
  var mood=document.getElementById('moodText');
  var mouth=document.getElementById('mouth');
  sc.className='plant-scene';
  if(cond==='Bone Dry'){
    sc.classList.add('bonedry');mood.innerHTML='&#x1F480; Completely parched! Water me NOW!';mood.style.color='#d97706';
    mouth.setAttribute('d','M44,35 Q50,31 56,35');
  }else if(cond==='Needs Water'){
    sc.classList.add('thirsty');mood.innerHTML='&#x1F4A6; Feeling thirsty... need water soon';mood.style.color='#38bdf8';
    mouth.setAttribute('d','M44,34 Q50,31 56,34');
  }else if(cond==='Waterlogged'){
    sc.classList.add('waterlogged');mood.innerHTML='&#x1F4A7; Too much water! I\'m drowning!';mood.style.color='#38bdf8';
    mouth.setAttribute('d','M44,34 Q50,31 56,34');
  }else{
    sc.classList.add('healthy');mood.innerHTML='&#x1F60A; Healthy & happy! Perfect moisture';mood.style.color='#4ade80';
    mouth.setAttribute('d','M44,33 Q50,38 56,33');
  }
}
async function fetchData(){
  try{
    var r=await Promise.all([fetch('/live'),fetch('/history')]);
    var live=await r[0].json();var hist=await r[1].json();
    document.getElementById('moistVal').innerHTML=live.percent+'<span class="card-unit">%</span>';
    document.getElementById('rawVal').textContent=live.raw;
    document.getElementById('condVal').textContent=live.cond;
    document.getElementById('countVal').textContent=hist.count;
    document.getElementById('moistGauge').style.width=Math.min(live.percent,100)+'%';
    document.getElementById('rawGauge').style.width=Math.max(0,100-(live.raw/1023*100))+'%';
    document.getElementById('uptimeText').textContent='Uptime: '+fmtUptime(live.uptime);
    var condEl=document.getElementById('condVal');
    if(live.cond==='Bone Dry')condEl.style.color='var(--accent3)';
    else if(live.cond==='Waterlogged')condEl.style.color='var(--accent2)';
    else condEl.style.color='var(--accent1)';
    setPlantState(live.cond,live.percent);
    var tbody=document.getElementById('histTable');
    if(hist.count===0){tbody.innerHTML='<tr><td colspan="4" style="color:var(--muted);text-align:center">No records yet</td></tr>';}
    else{var rows=hist.records.slice().reverse().slice(0,50);
      tbody.innerHTML=rows.map(function(r,i){return'<tr><td class="td-idx">'+(hist.count-i)+'</td><td class="td-moist">'+r.p+'%</td><td class="td-raw">'+r.r+'</td><td>'+fmtUptime(r.s)+'</td></tr>'}).join('');}
  }catch(e){
    document.getElementById('statusText').textContent='Connection error';
    document.getElementById('statusDot').style.background='#ff4444';
    document.getElementById('statusDot').style.boxShadow='0 0 8px #ff4444';
  }
}
fetchData();setInterval(fetchData,5000);
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
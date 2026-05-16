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
#define LED_PIN       2          // GPIO2 = D2 on NodeMCU

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
  :root{--bg:#0a0a0a;--panel:#121212;--border:#2a2a2a;--accent1:#ffcc00;--accent2:#fdb813;
    --accent3:#00ff9d;--text:#e0e0e0;--muted:#666;--glow1:0 0 20px rgba(255,204,0,0.25)}
  *,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
  body{background:var(--bg);color:var(--text);font-family:'Exo 2',sans-serif;min-height:100vh;padding:20px;
    background-image:radial-gradient(ellipse at 50% 0%,rgba(255,204,0,0.05) 0%,transparent 60%);
    transition:background-image .5s}
  header{text-align:center;margin-bottom:24px}
  header h1{font-size:clamp(1.5rem,5vw,2.6rem);font-weight:800;letter-spacing:.06em;
    background:linear-gradient(135deg,var(--accent1),#fff);-webkit-background-clip:text;
    -webkit-text-fill-color:transparent;background-clip:text;text-transform:uppercase}
  header p{color:var(--muted);font-family:'Share Tech Mono',monospace;font-size:.78rem;margin-top:6px;letter-spacing:.12em}
  .status-bar{display:flex;align-items:center;justify-content:center;gap:10px;margin-bottom:20px;
    font-family:'Share Tech Mono',monospace;font-size:.75rem;color:var(--muted)}
  .dot{width:8px;height:8px;border-radius:50%;background:var(--accent3);box-shadow:0 0 8px var(--accent3);animation:pulse 2s infinite}
  @keyframes pulse{0%,100%{opacity:1}50%{opacity:.3}}

  .sun-scene{display:flex;justify-content:center;margin:0 auto 24px;position:relative}
  .sun-container{position:relative;width:200px;height:200px}
  .sun-svg{width:100%;height:100%;animation:sunSpin 30s linear infinite}
  @keyframes sunSpin{to{transform:rotate(360deg)}}
  .sun-core{transition:fill .5s,r .3s}
  .sun-ray{stroke-linecap:round;transition:stroke .5s,opacity .5s,stroke-width .3s}
  .sun-glow{position:absolute;top:50%;left:50%;width:120px;height:120px;transform:translate(-50%,-50%);
    border-radius:50%;transition:box-shadow .5s,background .5s;pointer-events:none}
  .lux-display{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);text-align:center;z-index:2}
  .lux-big{font-family:'Share Tech Mono',monospace;font-size:1.8rem;font-weight:800;color:#fff;
    text-shadow:0 0 15px rgba(255,204,0,0.6);line-height:1;transition:color .5s}
  .lux-unit{font-size:.65rem;color:var(--accent1);letter-spacing:.2em;text-transform:uppercase}

  .light-cone{width:200px;height:60px;margin:-10px auto 20px;opacity:.3;transition:opacity .5s;
    background:linear-gradient(to bottom,rgba(255,204,0,0.4),transparent);
    clip-path:polygon(30% 0%,70% 0%,100% 100%,0% 100%)}

  .cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:16px;margin-bottom:24px}
  .card{background:var(--panel);border:1px solid var(--border);border-radius:16px;padding:22px 20px;
    position:relative;overflow:hidden;transition:border-color .3s,transform .3s}
  .card:hover{border-color:var(--accent1);transform:translateY(-2px)}
  .card::before{content:'';position:absolute;top:0;left:0;right:0;height:2px}
  .card.raw::before{background:linear-gradient(90deg,#a855f7,transparent)}
  .card.cond::before{background:linear-gradient(90deg,var(--accent3),transparent)}
  .card.count::before{background:linear-gradient(90deg,#4a6070,transparent)}
  .card-icon{font-size:1.8rem;margin-bottom:8px;display:block}
  .card-label{font-size:.7rem;letter-spacing:.18em;text-transform:uppercase;color:var(--muted);margin-bottom:6px}
  .card-value{font-family:'Share Tech Mono',monospace;font-size:clamp(1.6rem,4vw,2.2rem);font-weight:700;line-height:1}
  .card.raw .card-value{color:#a855f7}
  .card.cond .card-value{color:var(--accent3);font-size:clamp(1.2rem,3.5vw,1.8rem)}
  .card-unit{font-size:.8rem;color:var(--muted);margin-left:4px}
  .gauge-track{margin-top:12px;height:8px;background:var(--border);border-radius:8px;overflow:hidden}
  .gauge-fill{height:100%;border-radius:8px;transition:width .6s ease}
  .card.raw .gauge-fill{background:linear-gradient(90deg,#a855f7,#d8b4fe)}

  .table-box{background:var(--panel);border:1px solid var(--border);border-radius:16px;padding:24px;overflow-x:auto}
  .table-box h2{font-size:.75rem;letter-spacing:.16em;text-transform:uppercase;color:var(--muted);margin-bottom:16px}
  table{width:100%;border-collapse:collapse;font-family:'Share Tech Mono',monospace;font-size:.82rem}
  th{text-align:left;color:var(--muted);font-size:.7rem;letter-spacing:.12em;padding:0 12px 10px;border-bottom:1px solid var(--border)}
  td{padding:9px 12px;border-bottom:1px solid rgba(42,42,42,0.5);color:var(--text)}
  tr:last-child td{border-bottom:none}
  tr:hover td{background:rgba(255,204,0,0.04)}
  .td-lux{color:var(--accent1)}.td-raw{color:#a855f7}.td-idx{color:var(--muted);font-size:.7rem}
  footer{text-align:center;margin-top:20px;color:var(--muted);font-size:.72rem;font-family:'Share Tech Mono',monospace;letter-spacing:.1em}
  .college-info{background:var(--panel);border:1px solid var(--border);border-radius:16px;padding:20px 24px;margin-top:28px;text-align:center}
  .ci-name{font-size:.95rem;font-weight:700;letter-spacing:.06em;margin-bottom:4px}
  .ci-proj{font-size:.75rem;color:var(--accent1);margin-bottom:12px;font-family:'Share Tech Mono',monospace;letter-spacing:.08em}
  .ci-team{font-size:.72rem;color:var(--muted);line-height:2}
  .ci-team b{color:var(--text);font-weight:600}
  .ci-team .roll{color:var(--muted);font-size:.65rem}
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

<div class="sun-scene">
  <div class="sun-container">
    <div class="sun-glow" id="sunGlow"></div>
    <svg class="sun-svg" id="sunSvg" viewBox="0 0 200 200">
      <circle class="sun-core" id="sunCore" cx="100" cy="100" r="30" fill="#ffcc00"/>
      <line class="sun-ray" x1="100" y1="15" x2="100" y2="40" stroke="#ffcc00" stroke-width="4" opacity="0.8"/>
      <line class="sun-ray" x1="160" y1="40" x2="143" y2="57" stroke="#ffcc00" stroke-width="4" opacity="0.8"/>
      <line class="sun-ray" x1="185" y1="100" x2="160" y2="100" stroke="#ffcc00" stroke-width="4" opacity="0.8"/>
      <line class="sun-ray" x1="160" y1="160" x2="143" y2="143" stroke="#ffcc00" stroke-width="4" opacity="0.8"/>
      <line class="sun-ray" x1="100" y1="185" x2="100" y2="160" stroke="#ffcc00" stroke-width="4" opacity="0.8"/>
      <line class="sun-ray" x1="40" y1="160" x2="57" y2="143" stroke="#ffcc00" stroke-width="4" opacity="0.8"/>
      <line class="sun-ray" x1="15" y1="100" x2="40" y2="100" stroke="#ffcc00" stroke-width="4" opacity="0.8"/>
      <line class="sun-ray" x1="40" y1="40" x2="57" y2="57" stroke="#ffcc00" stroke-width="4" opacity="0.8"/>
    </svg>
    <div class="lux-display">
      <div class="lux-big" id="luxBig">--</div>
      <div class="lux-unit">LUX</div>
    </div>
  </div>
</div>
<div class="light-cone" id="lightCone"></div>

<div class="cards">
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
    <thead><tr><th>#</th><th>ILLUMINANCE (Lux)</th><th>RAW ANALOG</th><th>UPTIME (s)</th></tr></thead>
    <tbody id="histTable">
      <tr><td colspan="4" style="color:var(--muted);text-align:center">Loading...</td></tr>
    </tbody>
  </table>
</div>
<div class="college-info">
  <div class="ci-name">Karnatak Arts, Science and Commerce College, Bidar</div>
  <div class="ci-proj">Project: Lux Meter using ESP8266 &amp; TEMT6000 Sensor</div>
  <div class="ci-team">
    <b>Rahul Sharma</b> <span class="roll">(Roll No. 2024CS101)</span> &bull;
    <b>Priya Patil</b> <span class="roll">(Roll No. 2024CS102)</span> &bull;
    <b>Amit Kumar</b> <span class="roll">(Roll No. 2024CS103)</span> &bull;
    <b>Sneha Reddy</b> <span class="roll">(Roll No. 2024CS104)</span>
  </div>
</div>
<footer>ESP8266 Lux Meter &bull; 192.168.4.1 &bull; SSID: LuxMeter_AP</footer>

<script>
function fmtUptime(s){var h=Math.floor(s/3600);var m=Math.floor((s%3600)/60);var sec=s%60;return(h?h+'h ':'')+(m?m+'m ':'')+sec+'s'}
function updateSun(lux){
  var pct=Math.min(lux/1000,1);
  var rays=document.querySelectorAll('.sun-ray');
  var core=document.getElementById('sunCore');
  var glow=document.getElementById('sunGlow');
  var cone=document.getElementById('lightCone');
  var svg=document.getElementById('sunSvg');
  if(lux<10){
    core.setAttribute('fill','#444');core.setAttribute('r','25');
    rays.forEach(function(r){r.setAttribute('stroke','#333');r.setAttribute('opacity','0.2');r.setAttribute('stroke-width','2')});
    glow.style.boxShadow='none';glow.style.background='transparent';
    cone.style.opacity='0.05';svg.style.animationDuration='60s';
  }else if(lux<50){
    core.setAttribute('fill','#cc9900');core.setAttribute('r','28');
    rays.forEach(function(r){r.setAttribute('stroke','#cc9900');r.setAttribute('opacity','0.4');r.setAttribute('stroke-width','3')});
    glow.style.boxShadow='0 0 30px rgba(204,153,0,0.3)';glow.style.background='rgba(204,153,0,0.05)';
    cone.style.opacity='0.1';svg.style.animationDuration='40s';
  }else if(lux<300){
    core.setAttribute('fill','#ffcc00');core.setAttribute('r','30');
    rays.forEach(function(r){r.setAttribute('stroke','#ffcc00');r.setAttribute('opacity','0.7');r.setAttribute('stroke-width','4')});
    glow.style.boxShadow='0 0 40px rgba(255,204,0,0.4)';glow.style.background='rgba(255,204,0,0.08)';
    cone.style.opacity='0.25';svg.style.animationDuration='30s';
  }else if(lux<800){
    core.setAttribute('fill','#ffdd44');core.setAttribute('r','33');
    rays.forEach(function(r){r.setAttribute('stroke','#ffdd44');r.setAttribute('opacity','0.9');r.setAttribute('stroke-width','5')});
    glow.style.boxShadow='0 0 60px rgba(255,221,68,0.5)';glow.style.background='rgba(255,221,68,0.12)';
    cone.style.opacity='0.4';svg.style.animationDuration='20s';
  }else{
    core.setAttribute('fill','#fff8dc');core.setAttribute('r','35');
    rays.forEach(function(r){r.setAttribute('stroke','#fff8dc');r.setAttribute('opacity','1');r.setAttribute('stroke-width','6')});
    glow.style.boxShadow='0 0 80px rgba(255,248,220,0.6)';glow.style.background='rgba(255,248,220,0.15)';
    cone.style.opacity='0.6';svg.style.animationDuration='10s';
  }
}
async function fetchData(){
  try{
    var r=await Promise.all([fetch('/live'),fetch('/history')]);
    var live=await r[0].json();var hist=await r[1].json();
    document.getElementById('luxBig').textContent=live.lux.toFixed(0);
    document.getElementById('rawVal').textContent=live.raw;
    document.getElementById('condVal').textContent=live.cond;
    document.getElementById('countVal').textContent=hist.count;
    document.getElementById('rawGauge').style.width=Math.min((live.raw/1023)*100,100)+'%';
    document.getElementById('uptimeText').textContent='Uptime: '+fmtUptime(live.uptime);
    updateSun(live.lux);
    var tbody=document.getElementById('histTable');
    if(hist.count===0){tbody.innerHTML='<tr><td colspan="4" style="color:var(--muted);text-align:center">No data yet</td></tr>';}
    else{var rows=hist.records.slice().reverse().slice(0,50);
      tbody.innerHTML=rows.map(function(r,i){return'<tr><td class="td-idx">'+(hist.count-i)+'</td><td class="td-lux">'+r.l.toFixed(1)+'</td><td class="td-raw">'+r.r+'</td><td>'+fmtUptime(r.s)+'</td></tr>'}).join('');}
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
/*
 * ═══════════════════════════════════════════════════════════════
 * ESP8266 Smart Distance Monitoring System — Access Point + Web Dashboard
 * (Offline-Ready, Digital Ruler Theme, Live Distance Display)
 * * Wiring:
 * HC-SR04 VCC  → Vin (5V)
 * HC-SR04 GND  → GND
 * HC-SR04 TRIG → D5 (GPIO14)
 * HC-SR04 ECHO → D6 (GPIO12) -> Use Voltage Divider!
 * Buzzer       → D7 (GPIO13) -> Beeps on measurement lock
 * * Board: NodeMCU 1.0 (ESP-12E Module) or any ESP8266 board
 * Connect to WiFi SSID: "MeasureTool_AP"  Password: "12345678"
 * Then open browser → http://192.168.4.1
 * ═══════════════════════════════════════════════════════════════
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ── Configuration ────────────────────────────────────────────────
#define TRIG_PIN      14         // D5
#define ECHO_PIN      12         // D6
#define BUZZER_PIN    13         // D7

const char* AP_SSID     = "MeasureTool_AP";
const char* AP_PASSWORD = "12345678";
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

const uint16_t HISTORY_SIZE    = 100;
const uint32_t SAMPLE_INTERVAL = 300;  // ms between readings (fast for ruler)
const float MAX_RANGE_CM = 400.0;

// ── Globals ──────────────────────────────────────────────────────
ESP8266WebServer server(80);

struct Record {
  float    cm;
  float    inch;
  uint32_t ts;
};

Record   history[HISTORY_SIZE];
uint16_t histHead  = 0;
uint16_t histCount = 0;
uint32_t lastSample = 0;
uint32_t lastSaveTime = 0;

float currentCm   = 0.0;
float currentInch  = 0.0;
float minDist = 999.0;
float maxDist = 0.0;

// ── Helpers ──────────────────────────────────────────────────────
float getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return -1.0;
  return (duration * 0.0343) / 2.0;
}

void addRecord(float c, float i) {
  uint16_t idx = (histHead + histCount) % HISTORY_SIZE;
  history[idx].cm   = c;
  history[idx].inch = i;
  history[idx].ts   = millis() / 1000;
  if (histCount < HISTORY_SIZE) histCount++;
  else histHead = (histHead + 1) % HISTORY_SIZE;
}

String buildHistoryJSON() {
  String json = "{\"count\":" + String(histCount) + ",\"records\":[";
  for (uint16_t i = 0; i < histCount; i++) {
    uint16_t idx = (histHead + i) % HISTORY_SIZE;
    if (i > 0) json += ",";
    json += "{\"c\":" + String(history[idx].cm, 1) +
            ",\"i\":" + String(history[idx].inch, 1) +
            ",\"s\":" + String(history[idx].ts) + "}";
  }
  json += "]}";
  return json;
}

// ── HTML Dashboard ────────────────────────────────────────────────
const char HTML_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>Smart Distance Monitoring System</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Exo+2:wght@300;600;800&display=swap');
  :root{--bg:#0a0c10;--panel:#12151c;--border:#1e2533;--accent1:#00e5ff;--accent2:#7c4dff;
    --accent3:#00e676;--warn:#ff9100;--text:#e0e6f0;--muted:#5a6478;
    --glow:0 0 25px rgba(0,229,255,0.3)}
  *,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
  body{background:var(--bg);color:var(--text);font-family:'Exo 2',sans-serif;min-height:100vh;padding:20px;
    background-image:radial-gradient(ellipse at 50% 0%,rgba(0,229,255,0.06) 0%,transparent 60%)}
  header{text-align:center;margin-bottom:20px}
  header h1{font-size:clamp(1.5rem,5vw,2.6rem);font-weight:800;letter-spacing:.06em;
    background:linear-gradient(135deg,var(--accent1),var(--accent2));-webkit-background-clip:text;
    -webkit-text-fill-color:transparent;background-clip:text;text-transform:uppercase}
  header p{color:var(--muted);font-family:'Share Tech Mono',monospace;font-size:.78rem;margin-top:6px;letter-spacing:.12em}
  .college-info{background:var(--panel);border:1px solid var(--border);border-radius:16px;padding:16px 24px;margin-bottom:24px;text-align:center}
  .ci-name{font-size:.95rem;font-weight:700;letter-spacing:.06em;margin-bottom:4px}
  .ci-proj{font-size:.75rem;color:var(--accent1);margin-bottom:12px;font-family:'Share Tech Mono',monospace;letter-spacing:.08em}
  .ci-team{font-size:.72rem;color:var(--muted);line-height:2}
  .ci-team b{color:var(--text);font-weight:600}
  .ci-team .roll{color:var(--muted);font-size:.65rem}
  .status-bar{display:flex;align-items:center;justify-content:center;gap:10px;margin-bottom:20px;
    font-family:'Share Tech Mono',monospace;font-size:.75rem;color:var(--muted)}
  .dot{width:8px;height:8px;border-radius:50%;background:var(--accent3);box-shadow:0 0 8px var(--accent3);animation:pulse 2s infinite}
  @keyframes pulse{0%,100%{opacity:1}50%{opacity:.3}}

  .ruler-hero{text-align:center;margin-bottom:24px}
  .dist-display{font-family:'Share Tech Mono',monospace;font-size:clamp(3rem,10vw,5rem);font-weight:800;
    color:var(--accent1);text-shadow:var(--glow);line-height:1;margin-bottom:4px;transition:color .3s}
  .dist-unit{font-size:1rem;color:var(--muted);letter-spacing:.2em}
  .dist-secondary{font-family:'Share Tech Mono',monospace;font-size:1.4rem;color:var(--accent2);margin-top:8px}
  .dist-secondary .unit2{font-size:.7rem;color:var(--muted)}

  .ruler-bar{margin:20px auto;max-width:500px;position:relative;height:60px}
  .ruler-track{width:100%;height:30px;background:var(--panel);border:1px solid var(--border);border-radius:6px;
    position:relative;overflow:hidden}
  .ruler-fill{height:100%;background:linear-gradient(90deg,var(--accent1),var(--accent2));border-radius:6px;
    transition:width .3s ease;position:relative}
  .ruler-fill::after{content:'';position:absolute;right:0;top:0;width:3px;height:100%;background:#fff;
    box-shadow:0 0 10px var(--accent1);animation:blink 1s infinite}
  @keyframes blink{0%,100%{opacity:1}50%{opacity:.3}}
  .ruler-ticks{display:flex;justify-content:space-between;padding:4px 0;font-size:.6rem;color:var(--muted);
    font-family:'Share Tech Mono',monospace}
  .ruler-marks{position:absolute;top:0;width:100%;height:100%;display:flex;align-items:flex-end}
  .ruler-marks span{width:1px;background:rgba(255,255,255,0.15)}
  .ruler-marks span:nth-child(5n){height:60%;background:rgba(255,255,255,0.25)}
  .ruler-marks span:not(:nth-child(5n)){height:35%}

  .beam-visual{margin:0 auto 20px;width:200px;height:100px;position:relative}
  .beam-sensor{width:40px;height:20px;background:var(--panel);border:2px solid var(--accent1);border-radius:4px;
    margin:0 auto;position:relative;z-index:2;box-shadow:0 0 10px rgba(0,229,255,0.3)}
  .beam-cone{width:0;height:0;border-left:40px solid transparent;border-right:40px solid transparent;border-top:80px solid rgba(0,229,255,0.08);
    margin:0 auto;position:relative;top:-2px;transition:border-left-width .3s,border-right-width .3s}
  .beam-pulse{position:absolute;bottom:0;left:50%;transform:translateX(-50%);width:10px;height:10px;
    border-radius:50%;background:var(--accent1);animation:beamP 1s ease-out infinite}
  @keyframes beamP{0%{transform:translateX(-50%) scale(1);opacity:.8}100%{transform:translateX(-50%) scale(3);opacity:0}}

  .cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:14px;margin-bottom:24px}
  .card{background:var(--panel);border:1px solid var(--border);border-radius:14px;padding:18px 16px;
    position:relative;overflow:hidden;transition:border-color .3s,transform .3s}
  .card:hover{border-color:var(--accent1);transform:translateY(-2px)}
  .card::before{content:'';position:absolute;top:0;left:0;right:0;height:2px}
  .card.mn::before{background:linear-gradient(90deg,var(--accent3),transparent)}
  .card.mx::before{background:linear-gradient(90deg,var(--warn),transparent)}
  .card.cnt::before{background:linear-gradient(90deg,var(--accent2),transparent)}
  .card-label{font-size:.65rem;letter-spacing:.16em;text-transform:uppercase;color:var(--muted);margin-bottom:6px}
  .card-value{font-family:'Share Tech Mono',monospace;font-size:1.4rem;font-weight:700;line-height:1}
  .card.mn .card-value{color:var(--accent3)}.card.mx .card-value{color:var(--warn)}.card.cnt .card-value{color:var(--accent2)}
  .card-unit{font-size:.7rem;color:var(--muted);margin-left:3px}

  .table-box{background:var(--panel);border:1px solid var(--border);border-radius:16px;padding:20px;overflow-x:auto}
  .table-box h2{font-size:.72rem;letter-spacing:.16em;text-transform:uppercase;color:var(--muted);margin-bottom:14px}
  table{width:100%;border-collapse:collapse;font-family:'Share Tech Mono',monospace;font-size:.8rem}
  th{text-align:left;color:var(--muted);font-size:.65rem;letter-spacing:.12em;padding:0 10px 8px;border-bottom:1px solid var(--border)}
  td{padding:8px 10px;border-bottom:1px solid rgba(30,37,51,0.5);color:var(--text)}
  tr:last-child td{border-bottom:none}
  tr:hover td{background:rgba(0,229,255,0.04)}
  .td-cm{color:var(--accent1);font-weight:bold}.td-in{color:var(--accent2)}.td-idx{color:var(--muted);font-size:.65rem}
  footer{text-align:center;margin-top:24px;color:var(--muted);font-size:.7rem;font-family:'Share Tech Mono',monospace;letter-spacing:.1em}
</style>
</head>
<body>
<header>
  <h1>&#x1F4CF; Distance Monitor</h1>
  <p>ESP8266 &bull; HC-SR04 ULTRASONIC &bull; AP MODE</p>
</header>
<div class="college-info">
  <div class="ci-name">Karnatak Arts, Science and Commerce College, Bidar</div>
  <div class="ci-proj">Project: Smart Distance Monitoring System using Ultrasonic Sensor and NodeMCU ESP8266</div>
  <div class="ci-team">
    <b>Sumati</b> <span class="Bsc 6th sem">(Roll No. U27RE23S0114)</span> &bull;
    <b>Shilpa</b> <span class="Bsc 6th sem">(Roll No. U27RE23S0097)</span> &bull;
    <b>Vaishnavi</b> <span class="Bsc 6th sem">(Roll No. U27RE23S0144)</span> &bull;
    <b>Adnan</b> <span class="Bsc 6th sem">(Roll No. U27RE23S0001)</span>
    <b>Kishansingh </b> <span class="Bsc 6th sem">(Roll No. U27RE23S0027)</span>
  </div>
</div>
<div class="status-bar">
  <span class="dot" id="statusDot"></span>
  <span id="statusText">Measuring...</span>
  &nbsp;|&nbsp;
  <span id="uptimeText">Uptime: --</span>
  &nbsp;|&nbsp;
  Refresh: 0.5s
</div>

<div class="ruler-hero">
  <div class="beam-visual">
    <div class="beam-sensor"></div>
    <div class="beam-cone" id="beamCone"></div>
    <div class="beam-pulse"></div>
  </div>
  <div class="dist-display" id="distBig">--.-</div>
  <div class="dist-unit">CENTIMETERS</div>
  <div class="dist-secondary" id="distInch">--.- <span class="unit2">INCHES</span></div>
</div>

<div class="ruler-bar">
  <div class="ruler-track">
    <div class="ruler-fill" id="rulerFill" style="width:0%"></div>
  </div>
  <div class="ruler-ticks">
    <span>0</span><span>50</span><span>100</span><span>150</span><span>200</span><span>250</span><span>300</span><span>350</span><span>400cm</span>
  </div>
</div>

<div class="cards">
  <div class="card mn">
    <div class="card-label">&#x1F4C9; Min Distance</div>
    <div class="card-value" id="minVal">--<span class="card-unit">cm</span></div>
  </div>
  <div class="card mx">
    <div class="card-label">&#x1F4C8; Max Distance</div>
    <div class="card-value" id="maxVal">--<span class="card-unit">cm</span></div>
  </div>
  <div class="card cnt">
    <div class="card-label">&#x1F4CA; Saved Readings</div>
    <div class="card-value" id="countVal">0</div>
  </div>
</div>

<div class="table-box">
  <h2>&#x1F4CB; Measurement Log (newest first)</h2>
  <table>
    <thead><tr><th>#</th><th>CM</th><th>INCHES</th><th>UPTIME</th></tr></thead>
    <tbody id="histTable">
      <tr><td colspan="4" style="color:var(--muted);text-align:center">Loading...</td></tr>
    </tbody>
  </table>
</div>
<footer>Smart Distance Monitoring System &bull; 192.168.4.1 &bull; SSID: MeasureTool_AP</footer>

<script>
function fmtUptime(s){var h=Math.floor(s/3600);var m=Math.floor((s%3600)/60);var sec=s%60;return(h?h+'h ':'')+(m?m+'m ':'')+sec+'s'}
async function fetchData(){
  try{
    var r=await Promise.all([fetch('/live'),fetch('/history')]);
    var live=await r[0].json();var hist=await r[1].json();
    if(live.cm>=0){
      document.getElementById('distBig').textContent=live.cm.toFixed(1);
      document.getElementById('distInch').innerHTML=live.inch.toFixed(1)+' <span class="unit2">INCHES</span>';
      document.getElementById('rulerFill').style.width=Math.min((live.cm/400)*100,100)+'%';
      document.getElementById('minVal').innerHTML=live.min.toFixed(1)+'<span class="card-unit">cm</span>';
      document.getElementById('maxVal').innerHTML=live.max.toFixed(1)+'<span class="card-unit">cm</span>';
      var cone=document.getElementById('beamCone');
      var spread=Math.min(20+live.cm*0.15,60);
      cone.style.borderLeftWidth=spread+'px';cone.style.borderRightWidth=spread+'px';
      if(live.cm<10){document.getElementById('distBig').style.color='#ff5252';}
      else if(live.cm<30){document.getElementById('distBig').style.color='#ff9100';}
      else{document.getElementById('distBig').style.color='var(--accent1)';}
      document.getElementById('statusText').textContent='Measuring...';
      document.getElementById('statusDot').style.background='var(--accent3)';
    }else{
      document.getElementById('distBig').textContent='ERR';
      document.getElementById('statusText').textContent='Sensor Error';
      document.getElementById('statusDot').style.background='#ff4444';
    }
    document.getElementById('countVal').textContent=hist.count;
    document.getElementById('uptimeText').textContent='Uptime: '+fmtUptime(live.uptime);
    var tbody=document.getElementById('histTable');
    if(hist.count===0){tbody.innerHTML='<tr><td colspan="4" style="color:var(--muted);text-align:center">No data yet</td></tr>';}
    else{var rows=hist.records.slice().reverse().slice(0,50);
      tbody.innerHTML=rows.map(function(r,i){return'<tr><td class="td-idx">'+(hist.count-i)+'</td><td class="td-cm">'+r.c.toFixed(1)+'</td><td class="td-in">'+r.i.toFixed(1)+'</td><td>'+fmtUptime(r.s)+'</td></tr>'}).join('');}
  }catch(e){document.getElementById('statusText').textContent='Connection error';}
}
fetchData();setInterval(fetchData,500);
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
  json += "\"cm\":"    + String(currentCm, 1) + ",";
  json += "\"inch\":"  + String(currentInch, 1) + ",";
  json += "\"min\":"   + String(minDist, 1) + ",";
  json += "\"max\":"   + String(maxDist, 1) + ",";
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

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.println("\n\n=== Smart Distance Monitoring System ===");

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

    float dist = getDistance();

    if (dist > 0 && dist < MAX_RANGE_CM) {
      currentCm  = dist;
      currentInch = dist / 2.54;

      // Track min/max
      if (dist < minDist) minDist = dist;
      if (dist > maxDist) maxDist = dist;

      // Save to history every 5 seconds (not every 300ms)
      if (now - lastSaveTime >= 5000) {
        lastSaveTime = now;
        addRecord(currentCm, currentInch);

        // Short beep on save
        digitalWrite(BUZZER_PIN, HIGH);
        delay(50);
        digitalWrite(BUZZER_PIN, LOW);
      }

      Serial.printf("[MEASURE] %.1f cm | %.1f in\n", currentCm, currentInch);
    } else {
      currentCm = -1;
      currentInch = -1;
      Serial.println("[MEASURE] Out of range / Error");
    }
  }

  yield();
}

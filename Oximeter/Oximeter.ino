/*
 * ═══════════════════════════════════════════════════════════════
 * ESP8266 Heart Rate Monitor — Access Point + Web Dashboard
 * (Offline-Ready, Crimson Theme, Rounded Gauges, Live Heartbeat)
 * * Wiring:
 * Pulse Sensor Signal → A0 (Analog Pin)
 * Pulse Sensor VCC    → 3.3V
 * Pulse Sensor GND    → GND
 * Heartbeat LED       → D2 (GPIO4) -> 220Ω Resistor -> GND
 * * Board: NodeMCU 1.0 (ESP-12E Module) or any ESP8266 board
 * Connect to WiFi SSID: "HeartMonitor_AP"  Password: "12345678"
 * Then open browser → http://192.168.4.1
 * ═══════════════════════════════════════════════════════════════
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ── Configuration ────────────────────────────────────────────────
#define SENSOR_PIN    A0         // Analog input for Pulse Sensor
#define LED_PIN       2          // GPIO4 = D2 on NodeMCU

const char* AP_SSID     = "Oximeter";
const char* AP_PASSWORD = "12345678";
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

const uint16_t HISTORY_SIZE = 100;

// ── Globals ──────────────────────────────────────────────────────
ESP8266WebServer server(80);

struct Record {
  int      bpm;
  int      raw;
  uint32_t ts;
};

Record   history[HISTORY_SIZE];
uint16_t histHead  = 0;   
uint16_t histCount = 0;   

// Pulse Detection Variables
uint32_t lastSampleTime = 0;
uint32_t lastBeatTime   = 0;
int      threshold      = 550;   // Adjust this if your sensor rests higher/lower
bool     belowThreshold = true;

int currentBPM = 0;
int currentRaw = 0;
String currentStatus = "Waiting...";

// ── Helpers ──────────────────────────────────────────────────────
void addRecord(int b, int r) {
  uint16_t idx = (histHead + histCount) % HISTORY_SIZE;
  history[idx].bpm = b;
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
    json += "{\"b\":" + String(history[idx].bpm) +
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
<title>ESP8266 Pulse Monitor</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Exo+2:wght@300;600;800&display=swap');
  :root {
    --bg:#11050a;--panel:#1c0b12;--border:#3a1625;
    --accent1:#ff2a5f;--accent2:#ff758c;--accent3:#00f0ff;
    --text:#ffe6eb;--muted:#8a5a6a;
    --glow1:0 0 25px rgba(255,42,95,0.35);
  }
  *,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
  body{background:var(--bg);color:var(--text);font-family:'Exo 2',sans-serif;min-height:100vh;padding:20px;
    background-image:radial-gradient(ellipse at 50% 10%,rgba(255,42,95,0.08) 0%,transparent 60%)}
  header{text-align:center;margin-bottom:24px}
  header h1{font-size:clamp(1.5rem,5vw,2.6rem);font-weight:800;letter-spacing:.06em;
    background:linear-gradient(135deg,var(--accent1),var(--accent2));-webkit-background-clip:text;
    -webkit-text-fill-color:transparent;background-clip:text;text-transform:uppercase}
  header p{color:var(--muted);font-family:'Share Tech Mono',monospace;font-size:.78rem;margin-top:6px;letter-spacing:.12em}
  .status-bar{display:flex;align-items:center;justify-content:center;gap:10px;margin-bottom:20px;
    font-family:'Share Tech Mono',monospace;font-size:.75rem;color:var(--muted)}
  .dot{width:10px;height:10px;border-radius:50%;background:var(--accent1);box-shadow:0 0 10px var(--accent1);animation:hb 1s infinite}
  @keyframes hb{0%{transform:scale(1)}15%{transform:scale(1.3)}30%{transform:scale(1)}45%{transform:scale(1.3)}100%{transform:scale(1)}}

  .heart-hero{display:flex;align-items:center;justify-content:center;gap:30px;flex-wrap:wrap;margin-bottom:28px}
  .heart-wrap{position:relative;width:180px;height:180px;display:flex;align-items:center;justify-content:center}
  .heart-svg{width:120px;height:120px;animation:pump 1s infinite;filter:drop-shadow(0 0 20px rgba(255,42,95,0.6));z-index:2;position:relative}
  .heart-svg path{fill:var(--accent1)}
  @keyframes pump{0%{transform:scale(1)}10%{transform:scale(1.18)}20%{transform:scale(1)}35%{transform:scale(1.12)}50%{transform:scale(1)}100%{transform:scale(1)}}
  .pulse-ring{position:absolute;border:2px solid var(--accent1);border-radius:50%;width:100%;height:100%;top:0;left:0;opacity:0;animation:rp 2s infinite}
  .pulse-ring:nth-child(2){animation-delay:.5s}
  .pulse-ring:nth-child(3){animation-delay:1s}
  @keyframes rp{0%{transform:scale(.6);opacity:.7}100%{transform:scale(1.5);opacity:0}}
  .bpm-over{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);z-index:3;text-align:center}
  .bpm-big{font-family:'Share Tech Mono',monospace;font-size:2rem;font-weight:800;color:#fff;text-shadow:0 0 20px var(--accent1);line-height:1}
  .bpm-lbl{font-size:.6rem;color:var(--accent2);letter-spacing:.2em;text-transform:uppercase}
  .inactive .heart-svg{animation:none;filter:drop-shadow(0 0 5px rgba(255,42,95,0.2));opacity:.3}
  .inactive .heart-svg path{fill:#5a2535}
  .inactive .pulse-ring{animation:none;opacity:0}
  .inactive .ecg-line{stroke:#3a1625;filter:none}

  .ecg-box{flex:1;min-width:240px;max-width:500px;height:80px;background:var(--panel);border:1px solid var(--border);border-radius:12px;overflow:hidden;position:relative}
  .ecg-box svg{width:100%;height:100%}
  .ecg-line{stroke:var(--accent1);stroke-width:2;fill:none;filter:drop-shadow(0 0 4px var(--accent1));
    stroke-dasharray:600;stroke-dashoffset:600;animation:ecg 2s linear infinite}
  @keyframes ecg{to{stroke-dashoffset:0}}
  .ecg-glow{position:absolute;top:0;left:0;width:100%;height:100%;
    background:linear-gradient(90deg,transparent 60%,rgba(255,42,95,0.05) 80%,transparent 100%);
    animation:ecgG 2s linear infinite}
  @keyframes ecgG{0%{transform:translateX(-100%)}100%{transform:translateX(100%)}}

  .cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:16px;margin-bottom:24px}
  .card{background:var(--panel);border:1px solid var(--border);border-radius:16px;padding:22px 20px;
    position:relative;overflow:hidden;transition:border-color .3s,transform .3s}
  .card:hover{border-color:var(--accent1);transform:translateY(-2px)}
  .card::before{content:'';position:absolute;top:0;left:0;right:0;height:2px}
  .card.raw::before{background:linear-gradient(90deg,var(--accent3),transparent)}
  .card.cond::before{background:linear-gradient(90deg,var(--accent2),transparent)}
  .card.count::before{background:linear-gradient(90deg,#6b4c5a,transparent)}
  .card-icon{font-size:1.8rem;margin-bottom:8px;display:block}
  .card-label{font-size:.7rem;letter-spacing:.18em;text-transform:uppercase;color:var(--muted);margin-bottom:6px}
  .card-value{font-family:'Share Tech Mono',monospace;font-size:clamp(1.6rem,4vw,2.2rem);font-weight:700;line-height:1}
  .card.raw .card-value{color:var(--accent3)}
  .card.cond .card-value{color:var(--accent2);font-size:clamp(1.1rem,3vw,1.6rem)}
  .card-unit{font-size:.8rem;color:var(--muted);margin-left:4px}
  .gauge-track{margin-top:12px;height:8px;background:var(--border);border-radius:8px;overflow:hidden}
  .gauge-fill{height:100%;border-radius:8px;transition:width .3s ease}
  .card.raw .gauge-fill{background:linear-gradient(90deg,#007799,var(--accent3))}

  .table-box{background:var(--panel);border:1px solid var(--border);border-radius:16px;padding:24px;overflow-x:auto}
  .table-box h2{font-size:.75rem;letter-spacing:.16em;text-transform:uppercase;color:var(--muted);margin-bottom:16px}
  table{width:100%;border-collapse:collapse;font-family:'Share Tech Mono',monospace;font-size:.82rem}
  th{text-align:left;color:var(--muted);font-size:.7rem;letter-spacing:.12em;padding:0 12px 10px;border-bottom:1px solid var(--border)}
  td{padding:9px 12px;border-bottom:1px solid var(--border);color:var(--text)}
  tr:last-child td{border-bottom:none}
  tr:hover td{background:rgba(255,42,95,0.05)}
  .td-bpm{color:var(--accent1);font-weight:bold}
  .td-raw{color:var(--accent3)}
  .td-idx{color:var(--muted);font-size:.7rem}
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
  <h1>&#x1F493; Vitals Dash</h1>
  <p>ESP8266 &bull; PULSE SENSOR &bull; AP MODE</p>
</header>
<div class="college-info">
  <div class="ci-name">Karnatak Arts, Science and Commerce College, Bidar</div>
  <div class="ci-proj">Project: Heart Rate Monitor using ESP8266 &amp; Pulse Sensor</div>
  <div class="ci-team">
    <b>arati ganga</b> <span class="roll">(Roll No. U27RE23S0317)</span> &bull;
    <b>bhuvaneshwari</b> <span class="roll">(Roll No. U27RE23S0367)</span> &bull;
    <b>Maheen</b> <span class="roll">(Roll No. U27RE23S0312)</span> &bull;
    <b>Siddu</b> <span class="roll">(Roll No. U27RE23S0307)</span> &bull;
    <b>pavan</b> <span class="roll">(Roll No. U27RE23S0297)</span>
  </div>
</div>
<div class="status-bar">
  <span class="dot" id="statusDot"></span>
  <span id="statusText">Monitoring</span>
  &nbsp;|&nbsp;
  <span id="uptimeText">Uptime: --</span>
  &nbsp;|&nbsp;
  Refresh: 2s
</div>

<div class="heart-hero" id="heartHero">
  <div class="heart-wrap">
    <div class="pulse-ring"></div>
    <div class="pulse-ring"></div>
    <div class="pulse-ring"></div>
    <svg class="heart-svg" viewBox="0 0 100 90">
      <path d="M50 85C25 62,0 42,0 28A25 25 0 0 1 50 15 25 25 0 0 1 100 28C100 42,75 62,50 85Z"/>
    </svg>
    <div class="bpm-over">
      <div class="bpm-big" id="bpmBig">--</div>
      <div class="bpm-lbl">BPM</div>
    </div>
  </div>
  <div class="ecg-box">
    <div class="ecg-glow"></div>
    <svg viewBox="0 0 300 60" preserveAspectRatio="none">
      <polyline class="ecg-line" points="0,30 30,30 45,30 50,28 55,30 65,30 70,12 75,52 80,8 85,48 90,30 110,30 130,30 135,28 140,30 150,30 155,12 160,52 165,8 170,48 175,30 195,30 215,30 220,28 225,30 235,30 240,12 245,52 250,8 255,48 260,30 280,30 300,30"/>
    </svg>
  </div>
</div>

<div class="cards">
  <div class="card raw">
    <span class="card-icon">&#x1F4CA;</span>
    <div class="card-label">Sensor Raw (0-1023)</div>
    <div class="card-value" id="rawVal">--</div>
    <div class="gauge-track"><div class="gauge-fill" id="rawGauge" style="width:0%"></div></div>
  </div>
  <div class="card cond">
    <span class="card-icon">&#x1FA7A;</span>
    <div class="card-label">Signal Status</div>
    <div class="card-value" id="condVal">--</div>
  </div>
  <div class="card count">
    <span class="card-icon">&#x1F4BE;</span>
    <div class="card-label">Saved Beats</div>
    <div class="card-value" id="countVal">0</div>
  </div>
</div>

<div class="table-box">
  <h2>&#x1F4CB; Beat History (newest first)</h2>
  <table>
    <thead><tr><th>#</th><th>HEART RATE (BPM)</th><th>SIGNAL PEAK</th><th>UPTIME (s)</th></tr></thead>
    <tbody id="histTable">
      <tr><td colspan="4" style="color:var(--muted);text-align:center">Loading...</td></tr>
    </tbody>
  </table>
</div>
<footer>ESP8266 Pulse Monitor &bull; 192.168.4.1 &bull; SSID: Oximeter</footer>

<script>
function fmtUptime(s){const h=Math.floor(s/3600);const m=Math.floor((s%3600)/60);const sec=s%60;return(h?h+'h ':'')+(m?m+'m ':'')+sec+'s'}
async function fetchData(){
  try{
    const[liveRes,histRes]=await Promise.all([fetch('/live'),fetch('/history')]);
    const live=await liveRes.json();const hist=await histRes.json();
    const hero=document.getElementById('heartHero');
    const bb=document.getElementById('bpmBig');
    if(live.bpm>0){
      bb.textContent=live.bpm;hero.classList.remove('inactive');
      let sp=60/live.bpm;
      document.querySelector('.heart-svg').style.animationDuration=sp+'s';
      document.querySelectorAll('.pulse-ring').forEach(r=>r.style.animationDuration=(sp*2)+'s');
      document.querySelector('.ecg-line').style.animationDuration=sp+'s';
      document.getElementById('statusDot').style.animation='hb '+sp+'s infinite';
      document.getElementById('statusDot').style.opacity='1';
    }else{
      bb.textContent='--';hero.classList.add('inactive');
      document.getElementById('statusDot').style.animation='none';
      document.getElementById('statusDot').style.opacity='0.3';
    }
    document.getElementById('rawVal').textContent=live.raw;
    document.getElementById('condVal').textContent=live.cond;
    document.getElementById('countVal').textContent=hist.count;
    document.getElementById('rawGauge').style.width=Math.min((live.raw/1023)*100,100)+'%';
    document.getElementById('uptimeText').textContent='Uptime: '+fmtUptime(live.uptime);
    const tbody=document.getElementById('histTable');
    if(hist.count===0){tbody.innerHTML='<tr><td colspan="4" style="color:var(--muted);text-align:center">No beats recorded yet</td></tr>';}
    else{const rows=hist.records.slice().reverse().slice(0,50);
      tbody.innerHTML=rows.map((r,i)=>'<tr><td class="td-idx">'+(hist.count-i)+'</td><td class="td-bpm">'+r.b+' <span style="font-size:0.6rem">BPM</span></td><td class="td-raw">'+r.r+'</td><td>'+fmtUptime(r.s)+'</td></tr>').join('');}
  }catch(e){document.getElementById('statusText').textContent='Connection error';}
}
fetchData();setInterval(fetchData,2000);
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
  json += "\"bpm\":"  + String(currentBPM) + ",";
  json += "\"raw\":"  + String(currentRaw) + ",";
  json += "\"cond\":\"" + currentStatus + "\",";
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
  
  Serial.println("\n\n=== ESP8266 Pulse Monitor ===");

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

  uint32_t currentMillis = millis();
  
  // Non-blocking rapid sample (every 10ms)
  if (currentMillis - lastSampleTime >= 10) {
    lastSampleTime = currentMillis;
    currentRaw = analogRead(SENSOR_PIN);
    
    // Simple Peak Detection Algorithm
    if (currentRaw > threshold && belowThreshold) {
      // We found a peak (a heartbeat!)
      uint32_t beatTime = currentMillis;
      uint32_t ibi = beatTime - lastBeatTime; // Inter-Beat Interval in ms
      
      // Filter out noise: Only accept true human heart rate range
      // Normal resting HR: 60-100 BPM | Exercise: up to ~150-180 BPM
      // IBI range: 333ms (180 BPM) to 1200ms (50 BPM)
      if (ibi > 333 && ibi < 1200) {
        int computedBPM = 60000 / ibi; // Convert ms per beat to beats per minute
        
        // Secondary validation: Only accept BPM within true human range (50-180)
        if (computedBPM >= 50 && computedBPM <= 180) {
          currentBPM = computedBPM;
          addRecord(currentBPM, currentRaw);
          
          Serial.printf("[PULSE] Peak! Raw: %d | BPM: %d\n", currentRaw, currentBPM);
          
          // Flash LED to the heartbeat
          digitalWrite(LED_PIN, HIGH);
        }
      }
      
      lastBeatTime = beatTime;
      belowThreshold = false;
      currentStatus = "Reading Pulse";
      
    } else if (currentRaw < threshold - 30) {
      // Hysteresis: Signal dropped enough to look for the next peak
      belowThreshold = true;
      digitalWrite(LED_PIN, LOW);
    }
    
    // Auto-reset if no finger detected for a while
    if (currentMillis - lastBeatTime > 3000) {
      currentBPM = 0;
      currentStatus = "No Finger / Weak";
    }
  }
  
  yield();
}

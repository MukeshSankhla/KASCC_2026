/*
 * ═══════════════════════════════════════════════════════════════
 * ESP8266 Water Tank Monitor — Access Point + Web Dashboard
 * (Offline-Ready, Deep Aqua Theme, Buzzer Alarms)
 * * Wiring:
 * HC-SR04 VCC  → Vin (5V)
 * HC-SR04 GND  → GND
 * HC-SR04 TRIG → D5 (GPIO14)
 * HC-SR04 ECHO → D6 (GPIO12) -> Use Voltage Divider!
 * Buzzer       → D7 (GPIO13) -> Connect to positive leg, GND to negative
 * * Board: NodeMCU 1.0 (ESP-12E Module) or any ESP8266 board
 * Connect to WiFi SSID: "WaterTank_AP"  Password: "12345678"
 * Then open browser → http://192.168.4.1
 * ═══════════════════════════════════════════════════════════════
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ── Configuration ────────────────────────────────────────────────
#define TRIG_PIN      14         // D5
#define ECHO_PIN      12         // D6
#define BUZZER_PIN    13         // D7

const char* AP_SSID     = "WaterTank_AP";
const char* AP_PASSWORD = "12345678";
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

const uint16_t HISTORY_SIZE    = 100;
const uint32_t SAMPLE_INTERVAL = 5000;  // ms between readings

// ── Tank Dimensions (Customize these!) ───────────────────────────
const float TANK_EMPTY_CM = 150.0; // Sensor distance to bottom of empty tank
const float TANK_FULL_CM  = 20.0;  // Sensor distance to max water line (don't let water touch sensor!)

// ── Globals ──────────────────────────────────────────────────────
ESP8266WebServer server(80);

struct Record {
  int      percent;
  float    distance;
  uint32_t ts;
};

Record   history[HISTORY_SIZE];
uint16_t histHead  = 0;   
uint16_t histCount = 0;   
uint32_t lastSample = 0;

float currentDistance = 0.0;
int   currentPercent  = 0;
String currentStatus  = "Unknown";

// ── Helpers ──────────────────────────────────────────────────────
void addRecord(int p, float d) {
  uint16_t idx = (histHead + histCount) % HISTORY_SIZE;
  history[idx].percent  = p;
  history[idx].distance = d;
  history[idx].ts       = millis() / 1000;

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
            ",\"d\":" + String(history[idx].distance, 1) +
            ",\"s\":" + String(history[idx].ts)  + "}";
  }
  json += "]}";
  return json;
}

// Read HC-SR04
float getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout (~5 meters)
  if (duration == 0) return -1.0; // Out of range or error
  
  // Speed of sound is 0.034 cm/us. Divide by 2 for round trip.
  return (duration * 0.0343) / 2.0; 
}

// Sound the buzzer based on status
void triggerBuzzerAlarm() {
  if (currentStatus == "OVERFLOW WARNING") {
    // 3 Fast Beeps (Urgent)
    for(int i=0; i<3; i++) {
      digitalWrite(BUZZER_PIN, HIGH);
      delay(150);
      digitalWrite(BUZZER_PIN, LOW);
      delay(100);
    }
  } 
  else if (currentStatus == "CRITICAL LOW") {
    // 1 Long Beep (Warning)
    digitalWrite(BUZZER_PIN, HIGH);
    delay(800);
    digitalWrite(BUZZER_PIN, LOW);
  }
}

// ── HTML Dashboard (stored in PROGMEM) ────────────────────────────
const char HTML_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>ESP8266 Tank Monitor</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Exo+2:wght@300;600;800&display=swap');
  :root{--bg:#061325;--panel:#0b1e36;--border:#1a365d;--accent1:#00f0ff;--accent2:#3b82f6;
    --accent3:#10b981;--danger:#ef4444;--text:#e2e8f0;--muted:#64748b;--glow:0 0 25px rgba(0,240,255,0.4)}
  *,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
  body{background:var(--bg);color:var(--text);font-family:'Exo 2',sans-serif;min-height:100vh;padding:20px;
    background-image:radial-gradient(ellipse at 50% 0%,rgba(0,240,255,0.08) 0%,transparent 60%)}
  header{text-align:center;margin-bottom:28px}
  header h1{font-size:clamp(1.5rem,5vw,2.6rem);font-weight:800;letter-spacing:.06em;
    background:linear-gradient(135deg,var(--accent1),var(--accent2));-webkit-background-clip:text;
    -webkit-text-fill-color:transparent;background-clip:text;text-transform:uppercase}
  header p{color:var(--muted);font-family:'Share Tech Mono',monospace;font-size:.78rem;margin-top:6px;letter-spacing:.12em}

  .dashboard{display:grid;grid-template-columns:1fr;gap:20px;max-width:1000px;margin:0 auto}
  @media(min-width:768px){.dashboard{grid-template-columns:300px 1fr}}

  .tank-container{background:var(--panel);border:1px solid var(--border);border-radius:16px;padding:30px;
    display:flex;flex-direction:column;align-items:center;justify-content:center}
  .tank-visual{width:160px;height:260px;border:4px solid #1e3a8a;border-radius:10px 10px 25px 25px;
    position:relative;overflow:hidden;background:#020617;box-shadow:inset 0 0 20px rgba(0,0,0,0.8);margin-bottom:20px}
  .water{position:absolute;bottom:0;left:0;width:100%;height:0%;
    background:linear-gradient(to bottom,var(--accent1),var(--accent2));
    transition:height 1.5s cubic-bezier(0.4,0,0.2,1),background 1s;box-shadow:var(--glow)}
  .water::before,.water::after{content:'';position:absolute;width:400px;height:400px;top:-390px;left:50%;
    transform:translateX(-50%);background:#020617;border-radius:40%;animation:wave 6s linear infinite}
  .water::after{border-radius:42%;background:rgba(2,6,23,0.5);animation:wave 8s linear infinite}
  @keyframes wave{0%{transform:translateX(-50%) rotate(0)}100%{transform:translateX(-50%) rotate(360deg)}}

  .bubble{position:absolute;border-radius:50%;background:rgba(0,240,255,0.2);border:1px solid rgba(0,240,255,0.3);
    animation:rise 3s ease-in infinite}
  .bubble:nth-child(2){animation-delay:.8s;animation-duration:3.5s}
  .bubble:nth-child(3){animation-delay:1.6s;animation-duration:2.8s}
  .bubble:nth-child(4){animation-delay:2.2s;animation-duration:3.2s}
  @keyframes rise{0%{bottom:5%;opacity:0;transform:translateX(0) scale(0.5)}
    20%{opacity:1}80%{opacity:.6}100%{bottom:95%;opacity:0;transform:translateX(10px) scale(1)}}

  .ripple{position:absolute;top:0;left:50%;transform:translateX(-50%);width:60px;height:6px;
    border:1px solid rgba(0,240,255,0.4);border-radius:50%;opacity:0;animation:rippleOut 2s ease-out infinite}
  .ripple:nth-child(6){animation-delay:1s}
  @keyframes rippleOut{0%{width:20px;opacity:.8}100%{width:100px;opacity:0}}

  .fish{position:absolute;font-size:14px;animation:swim 4s ease-in-out infinite;opacity:.7}
  @keyframes swim{0%,100%{left:10%;transform:scaleX(1)}50%{left:70%;transform:scaleX(-1)}}

  .tank-percent{font-family:'Share Tech Mono',monospace;font-size:3rem;font-weight:700;color:var(--text);
    text-shadow:0 0 10px rgba(255,255,255,0.3)}

  .stats-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:16px}
  .card{background:var(--panel);border:1px solid var(--border);border-radius:16px;padding:22px;
    position:relative;overflow:hidden;transition:border-color .3s,transform .3s}
  .card:hover{border-color:var(--accent1);transform:translateY(-2px)}
  .card::before{content:'';position:absolute;top:0;left:0;right:0;height:2px}
  .card.dist::before{background:linear-gradient(90deg,#8b5cf6,transparent)}
  .card.cond::before{background:linear-gradient(90deg,var(--accent3),transparent)}
  .card.time::before{background:linear-gradient(90deg,var(--muted),transparent)}
  .card-label{font-size:.7rem;letter-spacing:.18em;text-transform:uppercase;color:var(--muted);margin-bottom:8px}
  .card-value{font-family:'Share Tech Mono',monospace;font-size:1.8rem;font-weight:700;line-height:1.2}
  .card-unit{font-size:.8rem;color:var(--muted);margin-left:4px}
  .val-cond{color:var(--accent1)}
  .val-alert{color:var(--danger);animation:pulseAlert 1.5s infinite}
  @keyframes pulseAlert{0%,100%{opacity:1}50%{opacity:.5}}

  .table-box{background:var(--panel);border:1px solid var(--border);border-radius:16px;padding:24px;
    overflow-x:auto;margin-top:20px}
  .table-box h2{font-size:.75rem;letter-spacing:.16em;text-transform:uppercase;color:var(--muted);margin-bottom:16px}
  table{width:100%;border-collapse:collapse;font-family:'Share Tech Mono',monospace;font-size:.82rem}
  th{text-align:left;color:var(--muted);font-size:.7rem;letter-spacing:.12em;padding:0 12px 10px;border-bottom:1px solid var(--border)}
  td{padding:9px 12px;border-bottom:1px solid rgba(26,54,93,0.5);color:var(--text)}
  tr:last-child td{border-bottom:none}
  tr:hover td{background:rgba(0,240,255,0.04)}
  .td-lvl{color:var(--accent1);font-weight:bold}
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
  <h1>&#x1F4A7; Reservoir Dash</h1>
  <p>ESP8266 &bull; ULTRASONIC &bull; AP MODE</p>
</header>

<div class="dashboard">
  <div class="tank-container">
    <div class="tank-visual">
      <div class="water" id="waterFill">
        <div class="bubble" style="left:20%;width:8px;height:8px"></div>
        <div class="bubble" style="left:50%;width:6px;height:6px"></div>
        <div class="bubble" style="left:70%;width:10px;height:10px"></div>
        <div class="bubble" style="left:35%;width:5px;height:5px"></div>
        <div class="ripple"></div>
        <div class="ripple"></div>
        <div class="fish" id="tankFish" style="bottom:30%">&#x1F41F;</div>
      </div>
    </div>
    <div class="tank-percent" id="tankPct">--%</div>
    <div style="color:var(--muted);font-size:.8rem;margin-top:5px" id="statusText">Connecting...</div>
  </div>

  <div class="stats-grid">
    <div class="card dist">
      <div class="card-label">Raw Distance</div>
      <div class="card-value" id="distVal">--<span class="card-unit">cm</span></div>
    </div>
    <div class="card cond">
      <div class="card-label">Tank Status</div>
      <div class="card-value val-cond" id="condVal">--</div>
    </div>
    <div class="card time">
      <div class="card-label">System Uptime</div>
      <div class="card-value" id="uptimeText" style="font-size:1.4rem">--</div>
    </div>
    <div class="card dist">
      <div class="card-label">Samples Logged</div>
      <div class="card-value" id="countVal">0</div>
    </div>
  </div>
</div>

<div class="table-box">
  <h2>&#x1F4CB; Level Logs (newest first)</h2>
  <table>
    <thead><tr><th>#</th><th>FILL LEVEL (%)</th><th>DISTANCE (cm)</th><th>UPTIME (s)</th></tr></thead>
    <tbody id="histTable">
      <tr><td colspan="4" style="color:var(--muted);text-align:center">Loading...</td></tr>
    </tbody>
  </table>
</div>
<div class="college-info">
  <div class="ci-name">Karnatak Arts, Science and Commerce College, Bidar</div>
  <div class="ci-proj">Project: Water Tank Monitor using ESP8266 &amp; Ultrasonic Sensor</div>
  <div class="ci-team">
    <b>Rahul Sharma</b> <span class="roll">(Roll No. 2024CS101)</span> &bull;
    <b>Priya Patil</b> <span class="roll">(Roll No. 2024CS102)</span> &bull;
    <b>Amit Kumar</b> <span class="roll">(Roll No. 2024CS103)</span> &bull;
    <b>Sneha Reddy</b> <span class="roll">(Roll No. 2024CS104)</span>
  </div>
</div>
<footer>ESP8266 Tank Monitor &bull; 192.168.4.1 &bull; SSID: WaterTank_AP</footer>

<script>
function fmtUptime(s){var h=Math.floor(s/3600);var m=Math.floor((s%3600)/60);var sec=s%60;return(h?h+'h ':'')+(m?m+'m ':'')+sec+'s'}
function setWaterColor(pct,cond){
  var w=document.getElementById('waterFill');
  var fish=document.getElementById('tankFish');
  if(cond==='CRITICAL LOW'){w.style.background='linear-gradient(to bottom,#ef4444,#991b1b)';fish.style.display='none';}
  else if(cond==='OVERFLOW WARNING'){w.style.background='linear-gradient(to bottom,#60a5fa,#1e3a8a)';
    w.style.boxShadow='0 0 30px rgba(239,68,68,0.5)';fish.style.display='block';}
  else if(pct>50){w.style.background='linear-gradient(to bottom,#00f0ff,#3b82f6)';w.style.boxShadow='0 0 25px rgba(0,240,255,0.4)';fish.style.display='block';}
  else{w.style.background='linear-gradient(to bottom,#06b6d4,#1e40af)';w.style.boxShadow='0 0 15px rgba(6,182,212,0.3)';fish.style.display=pct>20?'block':'none';}
}
async function fetchData(){
  try{
    var r=await Promise.all([fetch('/live'),fetch('/history')]);
    var live=await r[0].json();var hist=await r[1].json();
    document.getElementById('tankPct').textContent=live.percent+'%';
    document.getElementById('distVal').innerHTML=live.distance.toFixed(1)+'<span class="card-unit">cm</span>';
    document.getElementById('condVal').textContent=live.cond;
    document.getElementById('countVal').textContent=hist.count;
    document.getElementById('uptimeText').textContent=fmtUptime(live.uptime);
    document.getElementById('statusText').textContent=live.distance<0?'Sensor Error':'Live Reading';
    var vh=Math.max(5,live.percent);
    document.getElementById('waterFill').style.height=vh+'%';
    setWaterColor(live.percent,live.cond);
    var condEl=document.getElementById('condVal');
    condEl.className='card-value '+((live.cond==='CRITICAL LOW'||live.cond==='OVERFLOW WARNING')?'val-alert':'val-cond');
    var tbody=document.getElementById('histTable');
    if(hist.count===0){tbody.innerHTML='<tr><td colspan="4" style="color:var(--muted);text-align:center">No records yet</td></tr>';}
    else{var rows=hist.records.slice().reverse().slice(0,50);
      tbody.innerHTML=rows.map(function(r,i){return'<tr><td class="td-idx">'+(hist.count-i)+'</td><td class="td-lvl">'+r.p+'%</td><td>'+r.d.toFixed(1)+'</td><td>'+fmtUptime(r.s)+'</td></tr>'}).join('');}
  }catch(e){document.getElementById('statusText').textContent='Connection Error';}
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
  json += "\"distance\":" + String(currentDistance, 1) + ",";
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
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  // Setup Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Ensure it is silent on boot
  
  Serial.println("\n\n=== ESP8266 Water Tank Monitor ===");

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

    // 1. Read Sensor
    currentDistance = getDistance();
    
    if (currentDistance > 0) {
      // 2. Map distance to a 0-100% capacity
      float range = TANK_EMPTY_CM - TANK_FULL_CM;
      float fillHeight = TANK_EMPTY_CM - currentDistance;
      
      currentPercent = (fillHeight / range) * 100;
      currentPercent = constrain(currentPercent, 0, 100); 

      // 3. Determine textual condition
      if (currentPercent <= 10)       currentStatus = "CRITICAL LOW";
      else if (currentPercent < 30)   currentStatus = "Low";
      else if (currentPercent < 80)   currentStatus = "Normal";
      else if (currentPercent < 95)   currentStatus = "High";
      else                            currentStatus = "OVERFLOW WARNING";

      // 4. Save and output
      addRecord(currentPercent, currentDistance);
      Serial.printf("[TANK] Dist: %.1fcm | Fill: %d%% | Status: %s\n", currentDistance, currentPercent, currentStatus.c_str());
      
      // 5. Check and trigger alarms
      triggerBuzzerAlarm();

    } else {
      currentStatus = "Read Error";
      Serial.println("[TANK] Sensor Read Error / Timeout");
    }
  }

  yield();
}
/*
 * ═══════════════════════════════════════════════════════════════
 * ESP8266 Smart Blind Stick — Obstacle Detection + Web Dashboard
 * (Ultrasonic Cane for Visually Impaired, AP Mode)
 * * Wiring:
 * HC-SR04 VCC  → Vin (5V)
 * HC-SR04 GND  → GND
 * HC-SR04 TRIG → D5 (GPIO14)
 * HC-SR04 ECHO → D6 (GPIO12) -> Use Voltage Divider!
 * Buzzer       → D7 (GPIO13)
 * * Board: NodeMCU 1.0 (ESP-12E Module)
 * Connect to WiFi SSID: "BlindStick_AP"  Password: "12345678"
 * Then open browser → http://192.168.4.1
 * ═══════════════════════════════════════════════════════════════
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ── Configuration ────────────────────────────────────────────────
#define TRIG_PIN      14         // D5
#define ECHO_PIN      12         // D6
#define BUZZER_PIN    13         // D7

const char* AP_SSID     = "BlindStick_AP";
const char* AP_PASSWORD = "12345678";
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

const uint16_t HISTORY_SIZE    = 100;
const uint32_t SAMPLE_INTERVAL = 200;  // ms between readings

// ── Globals ──────────────────────────────────────────────────────
ESP8266WebServer server(80);

struct Record {
  int      dist;
  String   zone;
  uint32_t ts;
};

Record   history[HISTORY_SIZE];
uint16_t histHead  = 0;
uint16_t histCount = 0;
uint32_t lastSample = 0;
uint32_t lastSave   = 0;

int currentDist  = 0;
String currentZone = "Clear";
int alertCount = 0;  // total times danger zone triggered

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

void addRecord(int d, String z) {
  uint16_t idx = (histHead + histCount) % HISTORY_SIZE;
  history[idx].dist = d;
  history[idx].zone = z;
  history[idx].ts   = millis() / 1000;
  if (histCount < HISTORY_SIZE) histCount++;
  else histHead = (histHead + 1) % HISTORY_SIZE;
}

String buildHistoryJSON() {
  String json = "{\"count\":" + String(histCount) + ",\"records\":[";
  for (uint16_t i = 0; i < histCount; i++) {
    uint16_t idx = (histHead + i) % HISTORY_SIZE;
    if (i > 0) json += ",";
    json += "{\"d\":" + String(history[idx].dist) +
            ",\"z\":\"" + history[idx].zone + "\"" +
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
<title>Smart Blind Stick</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Exo+2:wght@300;600;800&display=swap');
  :root{--bg:#0c0c14;--panel:#13131f;--border:#252540;--safe:#00e676;--caution:#ffab00;
    --danger:#ff1744;--accent:#7c4dff;--text:#e0e0f0;--muted:#5a5a80;
    --glow-safe:0 0 20px rgba(0,230,118,0.3);--glow-danger:0 0 30px rgba(255,23,68,0.4)}
  *,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
  body{background:var(--bg);color:var(--text);font-family:'Exo 2',sans-serif;min-height:100vh;padding:20px;
    background-image:radial-gradient(ellipse at 50% 0%,rgba(124,77,253,0.06) 0%,transparent 60%)}
  header{text-align:center;margin-bottom:16px}
  header h1{font-size:clamp(1.4rem,5vw,2.4rem);font-weight:800;letter-spacing:.06em;
    background:linear-gradient(135deg,var(--accent),var(--safe));-webkit-background-clip:text;
    -webkit-text-fill-color:transparent;background-clip:text;text-transform:uppercase}
  header p{color:var(--muted);font-family:'Share Tech Mono',monospace;font-size:.75rem;margin-top:5px;letter-spacing:.12em}
  .college-info{background:var(--panel);border:1px solid var(--border);border-radius:16px;padding:16px 24px;margin-bottom:20px;text-align:center}
  .ci-name{font-size:.92rem;font-weight:700;letter-spacing:.06em;margin-bottom:4px}
  .ci-proj{font-size:.72rem;color:var(--accent);margin-bottom:10px;font-family:'Share Tech Mono',monospace;letter-spacing:.08em}
  .ci-team{font-size:.7rem;color:var(--muted);line-height:2}
  .ci-team b{color:var(--text);font-weight:600}
  .ci-team .roll{color:var(--muted);font-size:.62rem}
  .status-bar{display:flex;align-items:center;justify-content:center;gap:10px;margin-bottom:18px;
    font-family:'Share Tech Mono',monospace;font-size:.72rem;color:var(--muted)}
  .dot{width:8px;height:8px;border-radius:50%;background:var(--safe);box-shadow:0 0 8px var(--safe);animation:blink 2s infinite}
  @keyframes blink{0%,100%{opacity:1}50%{opacity:.3}}

  .scene{display:flex;justify-content:center;align-items:flex-end;margin:0 auto 20px;
    max-width:420px;height:280px;position:relative;overflow:hidden}
  .ground{position:absolute;bottom:0;width:100%;height:40px;background:linear-gradient(to top,#1a1a2e,transparent);border-top:1px solid var(--border)}
  .ground-dots{position:absolute;bottom:8px;width:100%;display:flex;justify-content:space-around}
  .ground-dots span{width:3px;height:3px;border-radius:50%;background:var(--muted);opacity:.4}

  .person{position:relative;width:80px;height:200px;margin-bottom:40px;z-index:2;transition:filter .5s}

  .p-head{width:28px;height:28px;background:var(--text);border-radius:50%;margin:0 auto 2px;position:relative}
  .p-glasses{position:absolute;top:10px;left:2px;width:24px;height:8px;border:2px solid #333;border-radius:3px;background:rgba(0,0,0,0.5)}
  .p-body{width:20px;height:50px;background:linear-gradient(to bottom,#4a4a8a,#3a3a6a);margin:0 auto;border-radius:6px 6px 2px 2px;position:relative}
  .p-arm-l,.p-arm-r{position:absolute;width:8px;height:40px;background:#4a4a8a;border-radius:4px;top:5px}
  .p-arm-l{left:-10px;transform:rotate(8deg)}
  .p-arm-r{right:-10px;transform:rotate(-15deg);transform-origin:top center}
  .p-legs{display:flex;justify-content:center;gap:4px;margin-top:-2px}
  .p-leg{width:9px;height:48px;background:#3a3a6a;border-radius:3px 3px 5px 5px}
  .p-leg-l{animation:walkL 0.8s ease-in-out infinite}
  .p-leg-r{animation:walkR 0.8s ease-in-out infinite}
  @keyframes walkL{0%,100%{transform:rotate(10deg)}50%{transform:rotate(-10deg)}}
  @keyframes walkR{0%,100%{transform:rotate(-10deg)}50%{transform:rotate(10deg)}}
  .p-cane{position:absolute;right:-18px;top:45px;width:4px;height:90px;background:linear-gradient(to bottom,#eee,#ccc);
    border-radius:2px;transform:rotate(-12deg);transform-origin:top center}
  .p-cane::after{content:'';position:absolute;bottom:0;left:-3px;width:10px;height:10px;border-radius:50%;
    background:#ddd;box-shadow:0 0 8px rgba(255,255,255,0.3)}

  .sonar-waves{position:absolute;right:30px;bottom:100px;z-index:1}
  .sonar-wave{position:absolute;width:20px;height:20px;border:2px solid var(--safe);border-radius:50%;
    animation:sonarPing 1.5s ease-out infinite;opacity:0}
  .sonar-wave:nth-child(1){animation-delay:0s}
  .sonar-wave:nth-child(2){animation-delay:.5s}
  .sonar-wave:nth-child(3){animation-delay:1s}
  @keyframes sonarPing{0%{width:10px;height:10px;opacity:.8;top:0;left:0}
    100%{width:120px;height:120px;opacity:0;top:-55px;left:-55px}}

  .obstacle-wall{position:absolute;right:20px;bottom:40px;width:30px;height:0;
    background:linear-gradient(to top,#555,#777);border-radius:4px 4px 0 0;
    transition:height 1s ease,box-shadow .5s;z-index:1}

  .zone-badge{text-align:center;margin-bottom:16px}
  .zone-text{font-family:'Share Tech Mono',monospace;font-size:1.6rem;font-weight:700;letter-spacing:.12em;
    padding:10px 30px;border-radius:12px;display:inline-block;transition:all .5s}
  .zone-safe{color:var(--safe);background:rgba(0,230,118,0.08);border:1px solid rgba(0,230,118,0.2);text-shadow:var(--glow-safe)}
  .zone-caution{color:var(--caution);background:rgba(255,171,0,0.08);border:1px solid rgba(255,171,0,0.2);
    text-shadow:0 0 15px rgba(255,171,0,0.4);animation:cautionFlash 1s infinite}
  .zone-danger{color:var(--danger);background:rgba(255,23,68,0.1);border:1px solid rgba(255,23,68,0.3);
    text-shadow:var(--glow-danger);animation:dangerFlash .5s infinite}
  @keyframes cautionFlash{0%,100%{opacity:1}50%{opacity:.7}}
  @keyframes dangerFlash{0%,100%{opacity:1}50%{opacity:.5}}

  .dist-bar{margin:0 auto 20px;max-width:450px}
  .bar-track{height:18px;background:var(--panel);border:1px solid var(--border);border-radius:10px;overflow:hidden;position:relative}
  .bar-fill{height:100%;border-radius:10px;transition:width .4s ease,background .4s}
  .bar-labels{display:flex;justify-content:space-between;font-size:.6rem;color:var(--muted);margin-top:4px;
    font-family:'Share Tech Mono',monospace}
  .bar-zones{display:flex;justify-content:space-between;font-size:.55rem;margin-top:2px}
  .bar-zones span{padding:2px 8px;border-radius:4px;font-family:'Share Tech Mono',monospace}
  .bz-d{color:var(--danger);background:rgba(255,23,68,0.1)}
  .bz-c{color:var(--caution);background:rgba(255,171,0,0.1)}
  .bz-s{color:var(--safe);background:rgba(0,230,118,0.1)}

  .cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(155px,1fr));gap:14px;margin-bottom:20px}
  .card{background:var(--panel);border:1px solid var(--border);border-radius:14px;padding:16px;
    position:relative;overflow:hidden;transition:border-color .3s,transform .3s}
  .card:hover{border-color:var(--accent);transform:translateY(-2px)}
  .card::before{content:'';position:absolute;top:0;left:0;right:0;height:2px}
  .card.cd::before{background:linear-gradient(90deg,var(--accent),transparent)}
  .card.ca::before{background:linear-gradient(90deg,var(--danger),transparent)}
  .card.ct::before{background:linear-gradient(90deg,var(--muted),transparent)}
  .card-label{font-size:.62rem;letter-spacing:.16em;text-transform:uppercase;color:var(--muted);margin-bottom:6px}
  .card-value{font-family:'Share Tech Mono',monospace;font-size:1.4rem;font-weight:700;line-height:1}
  .card.cd .card-value{color:var(--accent)}.card.ca .card-value{color:var(--danger)}.card.ct .card-value{color:var(--text)}
  .card-unit{font-size:.7rem;color:var(--muted);margin-left:3px}

  .table-box{background:var(--panel);border:1px solid var(--border);border-radius:14px;padding:18px;overflow-x:auto}
  .table-box h2{font-size:.7rem;letter-spacing:.14em;text-transform:uppercase;color:var(--muted);margin-bottom:12px}
  table{width:100%;border-collapse:collapse;font-family:'Share Tech Mono',monospace;font-size:.78rem}
  th{text-align:left;color:var(--muted);font-size:.62rem;letter-spacing:.1em;padding:0 10px 8px;border-bottom:1px solid var(--border)}
  td{padding:7px 10px;border-bottom:1px solid rgba(37,37,64,0.5);color:var(--text)}
  tr:last-child td{border-bottom:none}
  tr:hover td{background:rgba(124,77,253,0.04)}
  .td-d{font-weight:bold}.td-z{font-size:.7rem}.td-idx{color:var(--muted);font-size:.62rem}
  footer{text-align:center;margin-top:24px;color:var(--muted);font-size:.68rem;font-family:'Share Tech Mono',monospace;letter-spacing:.1em}
</style>
</head>
<body>
<header>
  <h1>&#x1F9AF; Smart Blind Stick</h1>
  <p>ESP8266 &bull; ULTRASONIC &bull; ASSISTIVE TECH</p>
</header>
<div class="college-info">
  <div class="ci-name">Karnatak Arts, Science and Commerce College, Bidar</div>
  <div class="ci-proj">Project: Smart Blind Stick using ESP8266 &amp; Ultrasonic Sensor</div>
  <div class="ci-team">
    <b>Rahul Sharma</b> <span class="roll">(Roll No. 2024CS101)</span> &bull;
    <b>Priya Patil</b> <span class="roll">(Roll No. 2024CS102)</span> &bull;
    <b>Amit Kumar</b> <span class="roll">(Roll No. 2024CS103)</span> &bull;
    <b>Sneha Reddy</b> <span class="roll">(Roll No. 2024CS104)</span>
  </div>
</div>
<div class="status-bar">
  <span class="dot" id="statusDot"></span>
  <span id="statusText">Scanning...</span>
  &nbsp;|&nbsp;
  <span id="uptimeText">Uptime: --</span>
  &nbsp;|&nbsp;
  Live
</div>

<div class="scene" id="scene">
  <div class="ground"><div class="ground-dots"><span></span><span></span><span></span><span></span><span></span><span></span><span></span><span></span></div></div>
  <div class="person" id="person">
    <div class="p-head"><div class="p-glasses"></div></div>
    <div class="p-body">
      <div class="p-arm-l"></div>
      <div class="p-arm-r"></div>
      <div class="p-cane"></div>
    </div>
    <div class="p-legs">
      <div class="p-leg p-leg-l"></div>
      <div class="p-leg p-leg-r"></div>
    </div>
  </div>
  <div class="sonar-waves" id="sonarWaves">
    <div class="sonar-wave"></div>
    <div class="sonar-wave"></div>
    <div class="sonar-wave"></div>
  </div>
  <div class="obstacle-wall" id="obstacleWall"></div>
</div>

<div class="zone-badge">
  <span class="zone-text zone-safe" id="zoneBadge">&#x2705; PATH CLEAR</span>
</div>

<div class="dist-bar">
  <div class="bar-track"><div class="bar-fill" id="barFill" style="width:0%;background:var(--safe)"></div></div>
  <div class="bar-labels"><span>0cm</span><span>40cm</span><span>100cm</span><span>200cm+</span></div>
  <div class="bar-zones"><span class="bz-d">DANGER</span><span class="bz-c">CAUTION</span><span class="bz-s">SAFE</span></div>
</div>

<div class="cards">
  <div class="card cd">
    <div class="card-label">&#x1F4CF; Distance</div>
    <div class="card-value" id="distVal">--<span class="card-unit">cm</span></div>
  </div>
  <div class="card ca">
    <div class="card-label">&#x26A0; Alert Count</div>
    <div class="card-value" id="alertVal">0</div>
  </div>
  <div class="card ct">
    <div class="card-label">&#x1F4CA; Readings Saved</div>
    <div class="card-value" id="countVal">0</div>
  </div>
</div>

<div class="table-box">
  <h2>&#x1F4CB; Detection Log (newest first)</h2>
  <table>
    <thead><tr><th>#</th><th>DISTANCE</th><th>ZONE</th><th>UPTIME</th></tr></thead>
    <tbody id="histTable">
      <tr><td colspan="4" style="color:var(--muted);text-align:center">Loading...</td></tr>
    </tbody>
  </table>
</div>
<footer>ESP8266 Smart Blind Stick &bull; 192.168.4.1 &bull; SSID: BlindStick_AP</footer>

<script>
function fmtUptime(s){var h=Math.floor(s/3600);var m=Math.floor((s%3600)/60);var sec=s%60;return(h?h+'h ':'')+(m?m+'m ':'')+sec+'s'}
function updateScene(dist,zone){
  var badge=document.getElementById('zoneBadge');
  var bar=document.getElementById('barFill');
  var wall=document.getElementById('obstacleWall');
  var person=document.getElementById('person');
  var waves=document.querySelectorAll('.sonar-wave');
  var legs=document.querySelectorAll('.p-leg');
  if(zone==='Danger'){
    badge.className='zone-text zone-danger';badge.innerHTML='\u26D4 OBSTACLE DETECTED!';
    bar.style.width='100%';bar.style.background='var(--danger)';
    wall.style.height='120px';wall.style.boxShadow='0 0 20px rgba(255,23,68,0.5)';
    person.style.filter='drop-shadow(0 0 10px rgba(255,23,68,0.4))';
    waves.forEach(function(w){w.style.borderColor='var(--danger)'});
    legs.forEach(function(l){l.style.animationDuration='0.4s'});
  }else if(zone==='Caution'){
    badge.className='zone-text zone-caution';badge.innerHTML='\u26A0\uFE0F SLOW DOWN — Object Ahead';
    var pct=Math.min(((100-dist)/100)*100,80);
    bar.style.width=pct+'%';bar.style.background='var(--caution)';
    wall.style.height=(60+dist*0.3)+'px';wall.style.boxShadow='0 0 10px rgba(255,171,0,0.3)';
    person.style.filter='none';
    waves.forEach(function(w){w.style.borderColor='var(--caution)'});
    legs.forEach(function(l){l.style.animationDuration='0.8s'});
  }else{
    badge.className='zone-text zone-safe';badge.innerHTML='\u2705 PATH CLEAR';
    bar.style.width='20%';bar.style.background='var(--safe)';
    wall.style.height='0px';wall.style.boxShadow='none';
    person.style.filter='none';
    waves.forEach(function(w){w.style.borderColor='var(--safe)'});
    legs.forEach(function(l){l.style.animationDuration='0.8s'});
  }
}
async function fetchData(){
  try{
    var r=await Promise.all([fetch('/live'),fetch('/history')]);
    var live=await r[0].json();var hist=await r[1].json();
    document.getElementById('distVal').innerHTML=live.dist+'<span class="card-unit">cm</span>';
    document.getElementById('alertVal').textContent=live.alerts;
    document.getElementById('countVal').textContent=hist.count;
    document.getElementById('uptimeText').textContent='Uptime: '+fmtUptime(live.uptime);
    updateScene(live.dist,live.zone);
    var distEl=document.getElementById('distVal');
    if(live.zone==='Danger')distEl.style.color='var(--danger)';
    else if(live.zone==='Caution')distEl.style.color='var(--caution)';
    else distEl.style.color='var(--safe)';
    var tbody=document.getElementById('histTable');
    if(hist.count===0){tbody.innerHTML='<tr><td colspan="4" style="color:var(--muted);text-align:center">No data yet</td></tr>';}
    else{var rows=hist.records.slice().reverse().slice(0,50);
      tbody.innerHTML=rows.map(function(r,i){
        var zc=r.z==='Danger'?'var(--danger)':r.z==='Caution'?'var(--caution)':'var(--safe)';
        return'<tr><td class="td-idx">'+(hist.count-i)+'</td><td class="td-d" style="color:'+zc+'">'+r.d+' cm</td><td class="td-z" style="color:'+zc+'">'+r.z+'</td><td>'+fmtUptime(r.s)+'</td></tr>'}).join('');}
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
  json += "\"dist\":"   + String(currentDist) + ",";
  json += "\"zone\":\"" + currentZone + "\",";
  json += "\"alerts\":" + String(alertCount) + ",";
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

  Serial.println("\n\n=== ESP8266 Smart Blind Stick ===");

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

    if (dist > 0 && dist < 400) {
      currentDist = (int)dist;

      // Zone classification (matching original buzzer logic)
      if (currentDist <= 40) {
        currentZone = "Danger";
        alertCount++;
        // Fast beep
        digitalWrite(BUZZER_PIN, HIGH);
        delay(80);
        digitalWrite(BUZZER_PIN, LOW);
      }
      else if (currentDist <= 100) {
        currentZone = "Caution";
        // Slow beep
        if (now % 600 < 200) {
          digitalWrite(BUZZER_PIN, HIGH);
        } else {
          digitalWrite(BUZZER_PIN, LOW);
        }
      }
      else {
        currentZone = "Clear";
        digitalWrite(BUZZER_PIN, LOW);
      }

      // Save to history every 3 seconds
      if (now - lastSave >= 3000) {
        lastSave = now;
        addRecord(currentDist, currentZone);
      }

      Serial.printf("[STICK] %dcm | Zone: %s\n", currentDist, currentZone.c_str());
    } else {
      currentDist = -1;
      currentZone = "Error";
      digitalWrite(BUZZER_PIN, LOW);
    }
  }

  yield();
}
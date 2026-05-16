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
  :root{--bg:#020804;--panel:#05140a;--border:#0a2e16;--radar:#00ff41;--radar-dim:#005515;--blip:#ff3333;--text:#ccffcc}
  *,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
  body{background:var(--bg);color:var(--text);font-family:'Share Tech Mono',monospace;min-height:100vh;padding:20px;
    background-image:linear-gradient(rgba(0,255,65,0.03) 1px,transparent 1px),
    linear-gradient(90deg,rgba(0,255,65,0.03) 1px,transparent 1px);background-size:30px 30px;position:relative}
  body::after{content:'';position:fixed;top:0;left:0;width:100%;height:100%;pointer-events:none;z-index:99;
    background:repeating-linear-gradient(0deg,transparent,transparent 2px,rgba(0,0,0,0.06) 2px,rgba(0,0,0,0.06) 4px)}

  header{text-align:center;margin-bottom:20px;text-transform:uppercase}
  header h1{font-size:2rem;color:var(--radar);text-shadow:0 0 10px var(--radar),0 0 30px rgba(0,255,65,0.3);letter-spacing:2px}
  header p{color:var(--radar-dim);font-size:.8rem;letter-spacing:4px}

  .stats{display:flex;justify-content:center;gap:20px;margin-bottom:25px;flex-wrap:wrap}
  .stat-box{background:var(--panel);border:1px solid var(--border);padding:15px 25px;border-radius:8px;
    text-align:center;min-width:140px;box-shadow:inset 0 0 15px rgba(0,255,65,0.05);transition:border-color .3s}
  .stat-box:hover{border-color:var(--radar)}
  .stat-label{font-size:.7rem;color:var(--radar-dim);margin-bottom:5px}
  .stat-val{font-size:1.8rem;font-weight:bold;color:var(--radar);text-shadow:0 0 8px rgba(0,255,65,0.4)}
  .stat-val.alert{color:var(--blip);text-shadow:0 0 10px var(--blip);animation:blipPulse 1s infinite}
  @keyframes blipPulse{0%,100%{opacity:1}50%{opacity:.6}}

  .radar-container{display:flex;justify-content:center;position:relative;margin:0 auto;width:100%;max-width:600px}
  canvas{background:var(--panel);border:2px solid var(--radar-dim);border-radius:50% 50% 0 0;
    box-shadow:0 0 30px rgba(0,255,65,0.1),inset 0 0 40px rgba(0,255,65,0.05);max-width:100%;height:auto}

  .scope-info{display:flex;justify-content:center;gap:30px;margin-top:15px;font-size:.7rem;color:var(--radar-dim)}
  .scope-dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:5px;vertical-align:middle}
  .scope-dot.green{background:var(--radar);box-shadow:0 0 5px var(--radar)}
  .scope-dot.red{background:var(--blip);box-shadow:0 0 5px var(--blip)}

  footer{text-align:center;margin-top:20px;color:var(--radar-dim);font-size:.7rem}
  .college-info{background:var(--panel);border:1px solid var(--border);border-radius:8px;padding:14px 22px;margin-bottom:20px;text-align:center}
  .ci-name{font-size:.9rem;font-weight:700;letter-spacing:.06em;margin-bottom:4px;color:var(--text)}
  .ci-proj{font-size:.72rem;color:var(--radar);margin-bottom:10px;letter-spacing:.08em}
  .ci-team{font-size:.68rem;color:var(--radar-dim);line-height:2}
  .ci-team b{color:var(--text);font-weight:600}
  .ci-team .roll{color:var(--radar-dim);font-size:.62rem}
</style>
</head>
<body>
<header>
  <h1>SYS.RADAR_SCAN</h1>
  <p>AP-MODE // 192.168.4.1 // SECURE</p>
</header>
<div class="college-info">
  <div class="ci-name">Karnatak Arts, Science and Commerce College, Bidar</div>
  <div class="ci-proj">Project: Radar Scanner using ESP8266, Servo &amp; Ultrasonic</div>
  <div class="ci-team">
    <b>Rahul Sharma</b> <span class="roll">(Roll No. 2024CS101)</span> &bull;
    <b>Priya Patil</b> <span class="roll">(Roll No. 2024CS102)</span> &bull;
    <b>Amit Kumar</b> <span class="roll">(Roll No. 2024CS103)</span> &bull;
    <b>Sneha Reddy</b> <span class="roll">(Roll No. 2024CS104)</span>
  </div>
</div>

<div class="stats">
  <div class="stat-box">
    <div class="stat-label">SCAN ANGLE</div>
    <div class="stat-val" id="angleVal">--&deg;</div>
  </div>
  <div class="stat-box">
    <div class="stat-label">PROXIMITY ALERT</div>
    <div class="stat-val" id="distVal">-- cm</div>
  </div>
  <div class="stat-box">
    <div class="stat-label">OBJECTS DETECTED</div>
    <div class="stat-val" id="objCount">0</div>
  </div>
</div>

<div class="radar-container">
  <canvas id="radarCanvas" width="600" height="300"></canvas>
</div>
<div class="scope-info">
  <span><span class="scope-dot green"></span> Detected (&gt;20cm)</span>
  <span><span class="scope-dot red"></span> Close (&lt;20cm)</span>
</div>

<footer>AEROSPACE DEFENSE DASHBOARD &bull; SYSTEM ONLINE</footer>

<script>
var canvas=document.getElementById('radarCanvas');
var ctx=canvas.getContext('2d');
var cx=canvas.width/2,cy=canvas.height;
var radius=canvas.width/2-20;
var MAX_RANGE=100;
var prevAngle=0;
var trailData=[];

function drawRadarGrid(){
  ctx.fillStyle='rgba(2,8,4,0.15)';
  ctx.fillRect(0,0,canvas.width,canvas.height);
  ctx.strokeStyle='rgba(0,255,65,0.2)';ctx.lineWidth=1;
  for(var r=0.25;r<=1;r+=0.25){
    ctx.beginPath();ctx.arc(cx,cy,radius*r,Math.PI,0);ctx.stroke();
    ctx.fillStyle='rgba(0,255,65,0.3)';ctx.font='10px Share Tech Mono';ctx.textAlign='center';
    ctx.fillText(Math.round(r*MAX_RANGE)+'cm',cx+radius*r+2,cy-5);
  }
  for(var a=0;a<=180;a+=30){
    var rad=a*Math.PI/180;ctx.beginPath();ctx.moveTo(cx,cy);
    ctx.lineTo(cx-radius*Math.cos(rad),cy-radius*Math.sin(rad));ctx.stroke();
    if(a>0&&a<180){ctx.fillStyle='rgba(0,255,65,0.25)';ctx.font='10px Share Tech Mono';
      var lx=cx-(radius+12)*Math.cos(rad);var ly=cy-(radius+12)*Math.sin(rad);ctx.textAlign='center';ctx.fillText(a+'°',lx,ly);}
  }
}

function drawSweep(angle,dataArray){
  var beamRad=angle*Math.PI/180;
  trailData.push({angle:angle,time:Date.now()});
  var now=Date.now();
  trailData=trailData.filter(function(t){return now-t.time<2000});
  for(var i=0;i<trailData.length;i++){
    var age=(now-trailData[i].time)/2000;
    var alpha=0.3*(1-age);
    var tRad=trailData[i].angle*Math.PI/180;
    ctx.beginPath();ctx.moveTo(cx,cy);
    ctx.lineTo(cx-radius*Math.cos(tRad),cy-radius*Math.sin(tRad));
    ctx.strokeStyle='rgba(0,255,65,'+alpha+')';ctx.lineWidth=2;ctx.stroke();
  }
  var grad=ctx.createLinearGradient(cx,cy,cx-radius*Math.cos(beamRad),cy-radius*Math.sin(beamRad));
  grad.addColorStop(0,'rgba(0,255,65,0.8)');grad.addColorStop(1,'rgba(0,255,65,0.1)');
  ctx.beginPath();ctx.moveTo(cx,cy);
  ctx.lineTo(cx-radius*Math.cos(beamRad),cy-radius*Math.sin(beamRad));
  ctx.strokeStyle=grad;ctx.lineWidth=3;ctx.stroke();
  ctx.beginPath();
  var sw=8*Math.PI/180;
  ctx.moveTo(cx,cy);ctx.arc(cx,cy,radius,-(beamRad+sw),-(beamRad-sw));ctx.closePath();
  ctx.fillStyle='rgba(0,255,65,0.08)';ctx.fill();
}

function drawBlips(dataArray,currentAngle){
  var objCount=0;
  for(var i=0;i<dataArray.length;i++){
    var dist=dataArray[i];
    if(dist<MAX_RANGE){
      objCount++;var angle=i*5;var rad=angle*Math.PI/180;
      var sd=(dist/MAX_RANGE)*radius;
      var x=cx-sd*Math.cos(rad);var y=cy-sd*Math.sin(rad);
      var isClose=dist<20;var col=isClose?'#ff3333':'#00ff41';
      var pulse=1+0.3*Math.sin(Date.now()/300);
      ctx.beginPath();ctx.arc(x,y,4*pulse,0,Math.PI*2);ctx.fillStyle=col;
      ctx.shadowBlur=isClose?15:10;ctx.shadowColor=col;ctx.fill();ctx.shadowBlur=0;
      ctx.beginPath();ctx.arc(x,y,8,0,Math.PI*2);ctx.strokeStyle=col.replace(')',',0.3)').replace('rgb','rgba');
      ctx.lineWidth=1;ctx.stroke();
    }
  }
  document.getElementById('objCount').textContent=objCount;
}

async function fetchRadarData(){
  try{
    var res=await fetch('/data');var data=await res.json();
    document.getElementById('angleVal').innerHTML=data.angle+'&deg;';
    var distEl=document.getElementById('distVal');
    if(data.closest<MAX_RANGE){distEl.textContent=data.closest+' cm';
      distEl.className=data.closest<20?'stat-val alert':'stat-val';}
    else{distEl.textContent='CLEAR';distEl.className='stat-val';}
    drawRadarGrid();drawSweep(data.angle,data.map);drawBlips(data.map,data.angle);
  }catch(e){console.error('Radar connection lost');}
}
drawRadarGrid();setInterval(fetchRadarData,250);
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
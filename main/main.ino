#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>

// --- WiFi ---
const char* ssid = "Xia";
const char* password = "poiuytrewqasd";

// --- Pins ---
#define RELAY_FAN1 14 // D5
#define RELAY_FAN2 12 // D6
#define RELAY_FAN3 16 // D0
#define RELAY_COMP 5  // D1
#define ONE_WIRE_AIR 4   // D2
#define ONE_WIRE_COMP 13 // D7

// --- Sensors ---
OneWire oneWireAir(ONE_WIRE_AIR);
DallasTemperature airSensor(&oneWireAir);
OneWire oneWireComp(ONE_WIRE_COMP);
DallasTemperature compSensor(&oneWireComp);

// --- Web server ---
ESP8266WebServer server(80);

// --- State ---
struct State {
  bool systemOn = true;
  bool compOn = false;
  int fanSpeed = 0;
  unsigned long lastCompOff = 0;
} state;

// --- Settings ---
struct Settings {
  float airSetpoint = 25.0;
  float compSetpoint = 50.0;
  float hysteresis = 1.0;
  int compDelay = 60; // sec
} settings;

// --- HTML page ---
const char PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP8266 AC Controller</title>
<style>
body{margin:0;font-family:system-ui;background:#0f1221;color:#e6e8f2}.wrap{max-width:1120px;margin:0 auto;padding:18px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:14px}.card{background:#171a2e;border:1px solid #2b2f55;border-radius:14px;padding:14px;position:relative}
.pill{border-radius:999px;padding:4px 10px;font-weight:700;font-size:12px;border:1px solid transparent}.on{color:#4ee07d;background:#16361d;border-color:#1f5b31}.wait{color:#ffd76a;background:#3a3217;border-color:#6b571c}.off{color:#c8cbe0;background:#2b2d3f;border-color:#3a3d57}.btn{background:#5865f2;color:#fff;border:none;border-radius:10px;padding:8px 12px;font-weight:700;cursor:pointer;margin-top:6px}.form input{background:#0e1022;color:#e6e8f2;border:1px solid #353a66;border-radius:8px;padding:6px 8px;min-width:60px;margin-right:6px}select{background:#0e1022;color:#e6e8f2;border:1px solid #353a66;border-radius:8px;padding:6px}#fanVisual{position:absolute;top:10px;right:10px;width:30px;height:30px;border:3px solid #6f82ff;border-radius:50%;transform-origin:50% 50%}#compVisual{position:absolute;top:10px;right:10px;width:30px;height:30px;border:3px solid #ff4e4e;border-radius:50%;opacity:0.2}.spin{animation:spin 1s linear infinite}@keyframes spin{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}.pulse{animation:pulse 1s linear infinite}@keyframes pulse{0%{opacity:1}50%{opacity:0.2}100%{opacity:1}}canvas{background:#20265f;border-radius:14px;margin-top:10px}
</style>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body>
<div class="wrap">
<h1>ESP8266 AC Controller</h1>
<div class="grid">
<div class="card">Система: <span id="sysPill" class="pill on">ON</span><br><button id="sysBtn" class="btn">Вкл/Выкл</button></div>
<div class="card">Компрессор: <span id="compPill" class="pill off">OFF</span><span id="cd"></span><div id="compVisual"></div><br><button id="compBtn" class="btn">Вкл/Выкл</button></div>
<div class="card">Вентилятор: <span id="fanPill" class="pill off">OFF</span><div id="fanVisual"></div><br>
<select id="fanSpeed"><option value="0">Выкл</option><option value="1">Низкая</option><option value="2">Средняя</option><option value="3">Высокая</option></select>
</div>
<div class="card">
<h3>Настройки</h3>
<form id="f1">Темп. воздуха: <input id="setT" type="number" step="0.1"><button>Сохранить</button></form>
<form id="f2">Компрессор отсечка: <input id="setC" type="number" step="0.1"><button>Сохранить</button></form>
<form id="f3">Гистерезис: <input id="setH" type="number" step="0.1"><button>Сохранить</button></form>
<form id="f4">Задержка компрессора (с): <input id="setD" type="number" step="1" min="5" max="3600"><button>Сохранить</button></form>
</div>
<div class="card">
<div>Температура компрессора: <span id="t1">--</span> °C</div>
<div>Температура воздуха: <span id="t2">--</span> °C</div>
<canvas id="chart" height="120"></canvas>
</div>
</div>
<script>
const sysPill=document.getElementById('sysPill');const sysBtn=document.getElementById('sysBtn');
const compPill=document.getElementById('compPill');const compBtn=document.getElementById('compBtn');
const fanPill=document.getElementById('fanPill');const fanSpeedSel=document.getElementById('fanSpeed');
const t1El=document.getElementById('t1'); const t2El=document.getElementById('t2');
const setT=document.getElementById('setT');const setC=document.getElementById('setC');const setH=document.getElementById('setH');const setD=document.getElementById('setD');
const cd=document.getElementById('cd'); const fanVisual=document.getElementById('fanVisual'); const compVisual=document.getElementById('compVisual');
sysBtn.onclick=()=>fetch('/toggleSystem').then(updateStatus);
compBtn.onclick=()=>fetch('/toggleComp').then(updateStatus);
fanSpeedSel.onchange=()=>fetch('/setFan?f='+fanSpeedSel.value).then(updateStatus);
document.getElementById('f1').onsubmit=e=>{e.preventDefault();fetch('/setTemp?t='+setT.value).then(updateStatus);}
document.getElementById('f2').onsubmit=e=>{e.preventDefault();fetch('/setCompTemp?c='+setC.value).then(updateStatus);}
document.getElementById('f3').onsubmit=e=>{e.preventDefault();fetch('/setHyst?h='+setH.value).then(updateStatus);}
document.getElementById('f4').onsubmit=e=>{e.preventDefault();fetch('/setDelay?d='+setD.value).then(updateStatus);}
const ctx=document.getElementById('chart').getContext('2d');
const airData=new Array(288).fill(0); const compData=new Array(288).fill(0);
const chart=new Chart(ctx,{type:'line',data:{labels:new Array(288).fill(''),datasets:[{label:'Air',borderColor:'blue',fill:false,data:airData},{label:'Comp',borderColor:'red',fill:false,data:compData}]},options:{animation:false,responsive:true,plugins:{legend:{display:true}}}});
function setPill(el,state){el.className='pill '+(state==='ON'?'on':state==='WAIT'?'wait':'off');el.textContent=state;}
function updateStatus(){fetch('/status').then(r=>r.json()).then(d=>{setPill(sysPill,d.system); setPill(compPill,d.compState); cd.textContent=d.countdown.toFixed(0)+'s'; setPill(fanPill,d.fan); fanSpeedSel.value=d.fanSpeed; t1El.textContent=d.t1.toFixed(1); t2El.textContent=d.t2.toFixed(1); setT.value=d.airSetpoint; setC.value=d.compSetpoint; setH.value=d.hyst; setD.value=d.delay; fanVisual.className=(d.fanSpeed>0?'spin':''); compVisual.className=(d.compState==='ON'?'pulse':''); airData.shift(); airData.push(d.t2); compData.shift(); compData.push(d.t1); chart.update();});}
setInterval(updateStatus,2000); updateStatus();
</script>
</body>
</html>
)HTML";

// --- Relay control ---
void updateRelays(){
  if(!state.systemOn){
    digitalWrite(RELAY_COMP,HIGH);
    digitalWrite(RELAY_FAN1,HIGH);
    digitalWrite(RELAY_FAN2,HIGH);
    digitalWrite(RELAY_FAN3,HIGH);
    return;
  }
  digitalWrite(RELAY_COMP,state.compOn?LOW:HIGH);
  digitalWrite(RELAY_FAN1,(state.fanSpeed>=1?LOW:HIGH));
  digitalWrite(RELAY_FAN2,(state.fanSpeed>=2?LOW:HIGH));
  digitalWrite(RELAY_FAN3,(state.fanSpeed>=3?LOW:HIGH));
}

// --- Handlers ---
void handleRoot(){ server.sendHeader("Connection","close"); server.send(200,"text/html",PAGE); }
void handleStatus(){
  airSensor.requestTemperatures(); compSensor.requestTemperatures();
  float airTemp=airSensor.getTempCByIndex(0);
  float compTemp=compSensor.getTempCByIndex(0);
  DynamicJsonDocument doc(1024);
  doc["system"] = state.systemOn?"ON":"OFF";
  doc["compState"] = state.compOn?"ON":"OFF";
  doc["fan"] = state.fanSpeed>0?"ON":"OFF";
  doc["fanSpeed"] = state.fanSpeed;
  doc["airSetpoint"] = settings.airSetpoint;
  doc["compSetpoint"] = settings.compSetpoint;
  doc["hyst"] = settings.hysteresis;
  doc["delay"] = settings.compDelay;
  doc["t1"] = compTemp;
  doc["t2"] = airTemp;
  doc["countdown"] = state.compOn?0:max(0,(int)(settings.compDelay-(millis()-state.lastCompOff)/1000UL));
  String json; serializeJson(doc,json); server.send(200,"application/json",json);
}
void handleToggleSystem(){ state.systemOn=!state.systemOn; updateRelays(); server.send(200,"text/plain","OK");}
void handleToggleComp(){ if(!state.compOn && millis()-state.lastCompOff>=settings.compDelay*1000UL) state.compOn=true; else {state.compOn=false; state.lastCompOff=millis();} updateRelays(); server.send(200,"text/plain","OK");}
void handleSetFan(){ state.fanSpeed=server.arg("f").toInt(); updateRelays(); server.send(200,"text/plain","OK");}
void handleSetTemp(){ settings.airSetpoint=server.arg("t").toFloat(); server.send(200,"text/plain","OK");}
void handleSetCompTemp(){ settings.compSetpoint=server.arg("c").toFloat(); server.send(200,"text/plain","OK");}
void handleSetHyst(){ settings.hysteresis=server.arg("h").toFloat(); server.send(200,"text/plain","OK");}
void handleSetDelay(){ settings.compDelay=server.arg("d").toInt(); server.send(200,"text/plain","OK");}

void setup(){
  Serial.begin(115200);
  pinMode(RE

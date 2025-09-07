// --- Web page (PROGMEM) ---
const char PAGE[] PROGMEM = R"HTML(
<!doctype html><html><head><meta name=viewport content="width=device-width,initial-scale=1">
<title>ESP8266 — Compressor Controller</title>
<style>
:root{--bg:#0f1221;--card:#171a2e;--muted:#b7bddb;--txt:#e6e8f2;--ok:#4ee07d;--ok-bg:#16361d;--wait:#ffd76a;--wait-bg:#3a3217;--off:#c8cbe0;--off-bg:#2b2d3f;--accent:#5865f2;--border:#2b2f55}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--txt);font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial}
.wrap{max-width:1120px;margin:0 auto;padding:18px}h1{margin:0 0 16px;font-size:22px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:14px}
.card{background:var(--card);border:1px solid var(--border);border-radius:14px;padding:14px;box-shadow:0 8px 24px rgba(0,0,0,.35)}
.title{display:flex;align-items:center;gap:10px;margin:0 0 10px;font-size:15px}
.pill{display:inline-block;border-radius:999px;padding:4px 10px;font-weight:700;font-size:12px;border:1px solid transparent}
.on{color:var(--ok);background:var(--ok-bg);border-color:#1f5b31}
.wait{color:var(--wait);background:var(--wait-bg);border-color:#6b571c}
.off{color:var(--off);background:var(--off-bg);border-color:#3a3d57}
.svg{width:36px;height:36px}.row{display:flex;gap:12px;align-items:center;justify-content:space-between}
.val{font-size:28px;font-weight:700}.units{opacity:.8;margin-left:6px;font-size:14px}
.btn{background:var(--accent);color:#fff;border:none;border-radius:10px;padding:8px 12px;font-weight:700;cursor:pointer}
.btn[disabled]{opacity:.45;cursor:not-allowed}
.form input{background:#0e1022;color:var(--txt);border:1px solid #353a66;border-radius:8px;padding:6px 8px;min-width:80px}
.muted{color:var(--muted)}.mono{font-family:ui-monospace,Consolas,Monaco,monospace}
.spin{animation:spin 1.1s linear infinite;transform-origin:50% 50%}@keyframes spin{to{transform:rotate(360deg)}}
.pulse{animation:pulse 1s linear infinite}@keyframes pulse{0%{opacity:1}50%{opacity:.5}100%{opacity:1}}
.chartWrap{height:260px;position:relative}canvas{width:100%;height:100%;display:block}
.switch{position:relative;width:56px;height:30px;background:#2a2f58;border:1px solid #3d4380;border-radius:999px;cursor:pointer}
.knob{position:absolute;top:3px;left:3px;width:24px;height:24px;background:#fff;border-radius:50%;transition:.25s}
.switch.on{background:#1f5b31;border-color:#3cae67}.switch.on .knob{left:29px}
.small{font-size:13px;color:var(--muted)}
</style></head><body>
<div class="wrap">
  <h1>ESP8266 — Compressor Controller</h1>
  <div class="grid">
    <div class="card" id="sysCard">
      <div class="title"><svg class="svg" viewBox="0 0 64 64"><path d="M30 6 v22" stroke="#9fb3ff" stroke-width="6" stroke-linecap="round"/><circle cx="32" cy="40" r="18" fill="#20265f" stroke="#7e8aff" stroke-width="4"/></svg>
      <div><div>System Power</div><div class="small">Master ON / OFF</div><div class="status"><span id="sysPill" class="pill on">ON</span></div></div></div>
      <div style="margin-top:8px" class="row"><button class="btn" id="sysBtn">Toggle</button></div>
    </div>

    <div class="card" id="compCard">
      <div class="title">
        <svg class="svg" viewBox="0 0 64 64"><g id="compRotor"><rect x="10" y="12" rx="6" ry="6" width="44" height="40" fill="#3740b8"/><rect x="16" y="18" rx="4" ry="4" width="32" height="18" fill="#20265f"/><circle cx="24" cy="45" r="6" fill="#0e1022" stroke="#7e8aff" stroke-width="2"/><circle cx="40" cy="45" r="6" fill="#0e1022" stroke="#7e8aff" stroke-width="2"/></g></svg>
        <div><div>Compressor (Pump)</div><div class="small">Restart delay: <b id="delayLabel">120</b> s</div>
        <div class="status"><span id="compPill" class="pill off">OFF</span> • <span class="mono" id="cd">--</span></div></div>
      </div>
      <div style="margin-top:8px" class="row"><button class="btn" id="compBtn">Toggle</button>
      <div style="display:flex;align-items:center;gap:10px"><div class="small">Mode</div><div id="modeSwitch" class="switch"><div class="knob"></div></div><div id="mode" class="small">Cooling</div></div></div>
    </div>

    <div class="card" id="fanCard">
      <div class="title">
        <svg class="svg" viewBox="0 0 64 64"><circle cx="32" cy="32" r="6" fill="#9fb3ff"/><g id="fanBlades"><path d="M32 8c8 0 12 6 12 10s-6 6-12 6-8-2-8-6 4-10 8-10z" fill="#6f82ff"/><path d="M56 32c0 8-6 12-10 12s-6-6-6-12 2-8 6-8 10 4 10 8z" fill="#6f82ff"/><path d="M32 56c-8 0-12-6-12-10s6-6 12-6 8 2 8 6-4 10-8 10z" fill="#6f82ff"/><path d="M8 32c0-8 6-12 10-12s6 6 6 12-2 8-6 8S8 36 8 32z" fill="#6f82ff"/></g></svg>
        <div><div>Fan</div><div class="small">Blows air</div><div class="status"><span id="fanPill" class="pill off">OFF</span></div></div>
      </div>
      <div style="margin-top:8px"><button class="btn" id="fanBtn">Toggle</button></div>
    </div>

    <div class="card" id="t1Card">
      <div class="title"><svg class="svg" viewBox="0 0 64 64"><rect x="28" y="8" width="8" height="30" rx="4" fill="#6b718f"/><circle cx="32" cy="48" r="10" fill="#ff6b6b"/></svg>
        <div><div>Sensor 1 (Compressor)</div><div class="small">Cutoff at: <span id="cutComp">--</span> °C</div></div></div>
      <div style="margin-top:6px" class="row"><div class="val"><span id="t1">--</span></div><div class="units">°C</div></div>
    </div>

    <div class="card" id="t2Card">
      <div class="title"><svg class="svg" viewBox="0 0 64 64"><rect x="28" y="8" width="8" height="30" rx="4" fill="#6b718f"/><circle cx="32" cy="48" r="10" fill="#7dd3fc"/></svg>
        <div><div>Sensor 2 (Air)</div><div class="small">Setpoint: <span id="cutAir">--</span> °C</div></div></div>
      <div style="margin-top:6px" class="row"><div class="val"><span id="t2">--</span></div><div class="units">°C</div></div>
    </div>
  </div>

  <div class="grid" style="margin-top:14px">
    <div class="card">
      <div class="title">Settings</div>
      <form id="f1" class="form">Air setpoint: <input id="setT" name="t" type="number" step="0.1"> <button class="btn" type="submit">Save</button></form><hr>
      <form id="f2" class="form">Compressor cutoff: <input id="setC" name="c" type="number" step="0.1"> <button class="btn" type="submit">Save</button></form><hr>
      <form id="f3" class="form">Hysteresis: <input id="setH" name="h" type="number" step="0.1"> <button class="btn" type="submit">Save</button></form><hr>
      <form id="f4" class="form">Delay (s): <input id="setD" name="d" type="number" step="1" min="5" max="3600"> <button class="btn" type="submit">Save</button></form>
    </div>

    <div class="card">
      <div class="title">Device Info</div>
      <div class="mono">IP: <span id="ip">-</span></div>
      <div class="mono">RSSI: <span id="rssi">-</span> dBm</div>
      <div class="mono">Uptime: <span id="uptime">-</span></div>
    </div>
  </div>

  <div class="card" style="margin-top:14px">
    <div class="title">Temperature History (24 hours, every 5 min)</div>
    <div class="chartWrap"><canvas id="chart"></canvas></div>
    <div class="small" style="margin-top:8px">Red = Compressor, Blue = Air</div>
  </div>

</div>

<script>
(function(){
  const sysPill = document.getElementById('sysPill');
  const sysBtn  = document.getElementById('sysBtn');
  const compBtn = document.getElementById('compBtn');
  const fanBtn  = document.getElementById('fanBtn');
  const setT    = document.getElementById('setT');
  const setC    = document.getElementById('setC');
  const setH    = document.getElementById('setH');
  const setD    = document.getElementById('setD');
  const modeSwitch = document.getElementById('modeSwitch');

  const compPill = document.getElementById('compPill');
  const fanPill  = document.getElementById('fanPill');
  const t1El     = document.getElementById('t1');
  const t2El     = document.getElementById('t2');
  const cutComp  = document.getElementById('cutComp');
  const cutAir   = document.getElementById('cutAir');
  const delayLabel = document.getElementById('delayLabel');
  const cd = document.getElementById('cd');
  const ip = document.getElementById('ip');
  const rssi = document.getElementById('rssi');
  const uptime = document.getElementById('uptime');
  const mode = document.getElementById('mode');
  const compRotor = document.getElementById('compRotor');
  const fanBlades = document.getElementById('fanBlades');

  sysBtn.addEventListener('click', ()=>{ fetch('/toggleSystem').then(()=>setTimeout(update,200)); });
  compBtn.addEventListener('click', ()=>{ fetch('/toggleComp').then(()=>setTimeout(update,200)); });
  fanBtn.addEventListener('click', ()=>{ fetch('/toggleFan').then(()=>setTimeout(update,200)); });
  modeSwitch.addEventListener('click', ()=>{ fetch('/toggleMode').then(()=>setTimeout(update,200)); });

  document.getElementById('f1').addEventListener('submit', e=>{ e.preventDefault(); fetch('/setTemp?t='+encodeURIComponent(setT.value)).then(()=>setTimeout(update,200)); });
  document.getElementById('f2').addEventListener('submit', e=>{ e.preventDefault(); fetch('/setCompTemp?c='+encodeURIComponent(setC.value)).then(()=>setTimeout(update,200)); });
  document.getElementById('f3').addEventListener('submit', e=>{ e.preventDefault(); fetch('/setHyst?h='+encodeURIComponent(setH.value)).then(()=>setTimeout(update,200)); });
  document.getElementById('f4').addEventListener('submit', e=>{ e.preventDefault(); fetch('/setDelay?d='+encodeURIComponent(setD.value)).then(()=>setTimeout(update,200)); });

  function setCardState(el, state){ el.className='pill ' + (state==='ON'?'on':state==='WAIT'?'wait':'off'); }
  function setDisabled(el, dis){ if(!el) return; el.disabled = !!dis; if(dis) el.setAttribute('disabled',''); else el.removeAttribute('disabled'); }

  function update(){
    fetch('/status').then(r=>r.json()).then(d=>{
      ip.textContent = d.ip; rssi.textContent = d.rssi; uptime.textContent = d.uptime;

      sysPill.textContent = d.system; setCardState(sysPill, d.system==='ON'?'ON':'OFF');
      const sysOff = d.system!=='ON';
      setDisabled(compBtn, sysOff); setDisabled(fanBtn, sysOff);

      t1El.textContent = d.t1; t2El.textContent = d.t2;
      cutComp.textContent = d.compShutdown; cutAir.textContent = d.airSetpoint;
      delayLabel.textContent = d.delay;
      cd.textContent = "";

      if (document.activeElement !== setT) setT.value = d.airSetpoint;
      if (document.activeElement !== setC) setC.value = d.compShutdown;
      if (document.activeElement !== setH) setH.value = d.hyst;
      if (document.activeElement !== setD) setD.value = d.delay;

      compPill.textContent = d.compState;
      if (d.compState === "ON") {
        setCardState(compPill, "ON");
        compRotor.classList.add("pulse");
        setDisabled(compBtn, false);
      } else if (d.compState === "WAIT") {
        setCardState(compPill, "WAIT");
        compRotor.classList.remove("pulse");
        setDisabled(compBtn, true);
        cd.textContent = d.countdown + "s";
      } else {
        setCardState(compPill, "OFF");
        compRotor.classList.remove("pulse");
        setDisabled(compBtn, sysOff);
      }

      fanPill.textContent = d.fan;
      setCardState(fanPill, d.fan==='ON'?'ON':'OFF');
      if (d.fan==='ON') fanBlades.classList.add("spin"); else fanBlades.classList.remove("spin");

      mode.textContent = d.mode;
      if (d.mode==='Heating') modeSwitch.classList.add('on'); else modeSwitch.classList.remove('on');
    });
  }

  setInterval(update, 2000);
  update();
})();
</script>
</body></html>
)HTML";

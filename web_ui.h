/*
 * web_ui.h  –  Gateway Diagnostic Web Interface
 *
 * Single-page HTML/CSS/JS application stored in ESP32 PROGMEM flash.
 * Served at GET /  by the WebServer.
 *
 * Features
 *  • Dark glassmorphism dashboard
 *  • Run All Tests button + per-module Run buttons
 *  • Live result cards (polls /results every 2 s)
 *  • Scrollable diagnostic log console (polls /log)
 *  • Drag-and-drop OTA firmware upload with progress bar
 */

#pragma once

const char index_html[] PROGMEM = R"HTMLEOF(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Gateway Diagnostic</title>
<style>
:root{
  --bg:#070b14;--surf:#0d1424;--card:rgba(255,255,255,0.04);
  --border:rgba(255,255,255,0.08);--text:#e2e8f0;--muted:#64748b;
  --accent:#38bdf8;--pass:#34d399;--warn:#fbbf24;--fail:#f87171;
  --pend:#94a3b8;--r:12px;
}
*{box-sizing:border-box;margin:0;padding:0}
html{scroll-behavior:smooth}
body{background:var(--bg);color:var(--text);font-family:'Segoe UI',system-ui,sans-serif;min-height:100vh;overflow-x:hidden}
body::before{content:'';position:fixed;inset:0;background:
  radial-gradient(ellipse 80% 40% at 50% -10%,rgba(56,189,248,.10) 0,transparent 70%),
  radial-gradient(ellipse 40% 30% at 90% 80%,rgba(129,140,248,.06) 0,transparent 60%);
  pointer-events:none;z-index:0}

/* ─ HEADER ─ */
header{position:sticky;top:0;z-index:100;
  background:rgba(7,11,20,.92);backdrop-filter:blur(24px);
  border-bottom:1px solid var(--border);
  padding:14px 24px;display:flex;align-items:center;justify-content:space-between}
.logo{display:flex;align-items:center;gap:12px}
.logo-icon{width:38px;height:38px;border-radius:10px;
  background:linear-gradient(135deg,#38bdf8,#818cf8);
  display:flex;align-items:center;justify-content:center;font-size:20px;flex-shrink:0}
.logo-name{font-size:15px;font-weight:700;letter-spacing:-.3px}
.logo-sub{font-size:11px;color:var(--muted);margin-top:2px}
.conn-pill{display:flex;align-items:center;gap:6px;
  padding:5px 12px;border-radius:20px;font-size:12px;
  background:rgba(52,211,153,.10);border:1px solid rgba(52,211,153,.25);color:var(--pass)}
.conn-dot{width:6px;height:6px;border-radius:50%;background:var(--pass);
  animation:blink 2s infinite}
@keyframes blink{0%,100%{opacity:1}50%{opacity:.3}}

/* ─ MAIN ─ */
main{max-width:920px;margin:0 auto;padding:28px 16px;position:relative;z-index:1}

/* ─ HERO ─ */
.hero{text-align:center;margin:20px 0 36px}
.hero h1{font-size:26px;font-weight:800;letter-spacing:-.5px;
  background:linear-gradient(135deg,#e2e8f0 30%,#64748b);
  -webkit-background-clip:text;-webkit-text-fill-color:transparent;margin-bottom:6px}
.hero p{color:var(--muted);font-size:13px;margin-bottom:22px}
.btn-all{padding:13px 42px;font-size:15px;font-weight:700;
  background:linear-gradient(135deg,#38bdf8,#818cf8);
  color:#fff;border:none;border-radius:50px;cursor:pointer;letter-spacing:.3px;
  box-shadow:0 0 36px rgba(56,189,248,.22);transition:all .25s}
.btn-all:hover:not(:disabled){transform:translateY(-3px);box-shadow:0 4px 48px rgba(56,189,248,.38)}
.btn-all:active{transform:translateY(0)}
.btn-all:disabled{opacity:.45;cursor:not-allowed;transform:none}
.btn-all.busy{background:linear-gradient(135deg,#334155,#1e293b)}
.spin{display:inline-block;width:14px;height:14px;margin-right:8px;
  border:2px solid rgba(255,255,255,.3);border-top-color:#fff;
  border-radius:50%;animation:rot .65s linear infinite;vertical-align:middle}
@keyframes rot{to{transform:rotate(360deg)}}

/* ─ TEST GRID ─ */
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(230px,1fr));gap:14px;margin-bottom:28px}

.card{background:var(--card);border:1px solid var(--border);
  border-radius:var(--r);padding:18px 18px 14px;
  transition:border-color .3s,transform .25s,box-shadow .25s;
  position:relative;overflow:hidden}
.card::after{content:'';position:absolute;top:0;left:0;right:0;height:2px;
  background:var(--border);transition:background .4s}
.card.pass::after{background:var(--pass)}
.card.warn::after{background:var(--warn)}
.card.fail::after{background:var(--fail)}
.card.pending::after{background:var(--pend);animation:shimmer 1.8s ease-in-out infinite}
@keyframes shimmer{0%,100%{opacity:.3}50%{opacity:1}}
.card:hover{border-color:rgba(255,255,255,.14);transform:translateY(-3px);
  box-shadow:0 10px 36px rgba(0,0,0,.35)}

.card-head{display:flex;align-items:center;justify-content:space-between;margin-bottom:10px}
.card-label{font-size:11px;font-weight:700;letter-spacing:.8px;
  text-transform:uppercase;color:var(--muted)}
.badge{padding:3px 9px;border-radius:20px;font-size:10px;font-weight:700;letter-spacing:.6px}
.badge.PASS  {background:rgba(52,211,153,.14);color:var(--pass);border:1px solid rgba(52,211,153,.3)}
.badge.WARN  {background:rgba(251,191,36,.14);color:var(--warn);border:1px solid rgba(251,191,36,.3)}
.badge.FAIL  {background:rgba(248,113,113,.14);color:var(--fail);border:1px solid rgba(248,113,113,.3)}
.badge.PENDING{background:rgba(148,163,184,.08);color:var(--pend);border:1px solid rgba(148,163,184,.2)}
.badge.SKIP  {background:rgba(100,116,139,.1);color:var(--muted);border:1px solid rgba(100,116,139,.2)}

.card-icon{font-size:26px;margin-bottom:6px;display:block}
.card-detail{font-size:11.5px;color:var(--muted);min-height:34px;
  line-height:1.55;margin-bottom:12px;word-break:break-all}
.btn-run{width:100%;padding:7px;font-size:12px;font-weight:600;
  background:rgba(255,255,255,.04);color:var(--muted);
  border:1px solid var(--border);border-radius:8px;cursor:pointer;
  transition:all .2s;letter-spacing:.3px}
.btn-run:hover:not(:disabled){background:rgba(56,189,248,.10);
  border-color:rgba(56,189,248,.35);color:var(--accent)}
.btn-run:disabled{opacity:.35;cursor:not-allowed}

/* ─ SECTION TITLE ─ */
.sec-title{font-size:12px;font-weight:700;letter-spacing:.7px;
  text-transform:uppercase;color:var(--muted);margin-bottom:10px;
  display:flex;align-items:center;justify-content:space-between}

/* ─ LOG CONSOLE ─ */
.log-wrap{margin-bottom:24px}
.log-box{background:#05080f;border:1px solid var(--border);border-radius:var(--r);
  padding:14px 16px;font-family:'Cascadia Code','Fira Code','Courier New',monospace;
  font-size:11.5px;line-height:1.75;height:230px;overflow-y:auto;
  color:#7c8fa8;white-space:pre-wrap;word-break:break-all}
.log-box .lp{color:#34d399}.log-box .lw{color:#fbbf24}.log-box .lf{color:#f87171}

/* ─ SMALL BUTTONS ─ */
.btn-xs{padding:4px 10px;font-size:11px;font-weight:600;
  background:rgba(255,255,255,.05);color:var(--muted);
  border:1px solid var(--border);border-radius:6px;cursor:pointer;transition:all .2s}
.btn-xs:hover{background:rgba(255,255,255,.09);color:var(--text)}

/* ─ OTA ─ */
.ota-wrap{margin-bottom:28px}
.ota-drop{border:2px dashed var(--border);border-radius:var(--r);
  padding:36px 24px;text-align:center;cursor:pointer;
  transition:all .3s;background:rgba(255,255,255,.02)}
.ota-drop.over{border-color:var(--accent);background:rgba(56,189,248,.06)}
.ota-drop input[type=file]{display:none}
.ota-drop .drop-icon{font-size:34px;margin-bottom:8px;display:block}
.ota-drop .drop-text{color:var(--muted);font-size:13px;margin-bottom:3px}
.ota-drop .drop-hint{color:var(--muted);font-size:11px;opacity:.55}
.ota-file{color:var(--accent);font-size:13px;font-weight:600;
  margin-top:10px;min-height:18px}

.prog-wrap{display:none;margin-top:16px}
.prog-track{background:rgba(255,255,255,.06);border-radius:20px;height:8px;overflow:hidden}
.prog-fill{height:100%;border-radius:20px;width:0;
  background:linear-gradient(90deg,#38bdf8,#818cf8);transition:width .3s}
.prog-label{font-size:12px;color:var(--muted);margin-top:6px}

.btn-flash{display:none;margin-top:14px;padding:10px 28px;
  font-size:13px;font-weight:700;background:linear-gradient(135deg,#38bdf8,#818cf8);
  color:#fff;border:none;border-radius:8px;cursor:pointer;transition:opacity .2s}
.btn-flash:hover{opacity:.88}
.btn-flash:disabled{opacity:.4;cursor:not-allowed}
.ota-status{display:none;margin-top:10px;font-size:13px;font-weight:600}

/* ─ FOOTER ─ */
footer{text-align:center;padding:20px;color:var(--muted);font-size:11px;
  border-top:1px solid var(--border);margin-top:4px}
footer a{color:var(--accent);text-decoration:none}

/* ─ RESPONSIVE ─ */
@media(max-width:500px){
  .hero h1{font-size:20px}
  .btn-all{padding:11px 30px;font-size:14px}
  main{padding:20px 12px}
}
</style>
</head>
<body>

<header>
  <div class="logo">
    <div class="logo-icon">⚡</div>
    <div>
      <div class="logo-name">Gateway Diagnostic</div>
      <div class="logo-sub">ESP32-S3 &nbsp;·&nbsp; 192.168.4.1 &nbsp;·&nbsp; GatewayDiag</div>
    </div>
  </div>
  <div class="conn-pill"><span class="conn-dot"></span>Connected</div>
</header>

<main>

  <!-- HERO -->
  <div class="hero">
    <h1>Hardware Diagnostic Suite</h1>
    <p id="last-run">Tests not yet run — press the button to begin</p>
    <button class="btn-all" id="btn-all" onclick="runAll()">&#9654;&nbsp; Run All Tests</button>
  </div>

  <!-- TEST CARDS -->
  <div class="grid" id="grid"></div>

  <!-- LOG -->
  <div class="log-wrap">
    <div class="sec-title">
      <span>&#128203; Diagnostic Log</span>
      <span>
        <button class="btn-xs" onclick="fetchLog()">Refresh</button>
        &nbsp;
        <button class="btn-xs" onclick="document.getElementById('log').textContent=''">Clear</button>
      </span>
    </div>
    <div class="log-box" id="log">Waiting for test output...</div>
  </div>

  <!-- OTA -->
  <div class="ota-wrap">
    <div class="sec-title">&#11014; OTA Firmware Update</div>
    <div class="ota-drop" id="ota-drop"
         onclick="document.getElementById('ota-file').click()"
         ondragover="event.preventDefault();this.classList.add('over')"
         ondragleave="this.classList.remove('over')"
         ondrop="handleDrop(event)">
      <input type="file" id="ota-file" accept=".bin" onchange="onFilePick(this)">
      <span class="drop-icon">&#128230;</span>
      <div class="drop-text">Drop <code>.bin</code> firmware here or click to browse</div>
      <div class="drop-hint">Device reboots automatically after a successful flash</div>
      <div class="ota-file" id="ota-fname"></div>
    </div>
    <div class="prog-wrap" id="prog-wrap">
      <div class="prog-track"><div class="prog-fill" id="prog-fill"></div></div>
      <div class="prog-label" id="prog-label">Uploading…</div>
    </div>
    <button class="btn-flash" id="btn-flash" onclick="doFlash()">&#11014; Flash Firmware</button>
    <div class="ota-status" id="ota-status"></div>
  </div>

</main>

<footer>
  ESP32-S3 Gateway Diagnostic v2.0 &nbsp;&#183;&nbsp;
  WiFi: <strong>GatewayDiag</strong> / <strong>gateway123</strong> &nbsp;&#183;&nbsp;
  IP: <a href="http://192.168.4.1">192.168.4.1</a>
</footer>

<script>
'use strict';

/* ── Module definitions ─────────────────────────────────────── */
const MODS = [
  {id:'rs232',  name:'RS232',    icon:'&#128225;', desc:'Modbus RTU · FR Meter · Serial2'},
  {id:'rs485',  name:'RS485',    icon:'&#128268;', desc:'Modbus RTU · bus test · Serial2'},
  {id:'gprs',   name:'GPRS/LTE', icon:'&#128246;', desc:'SIM module · AT commands · Serial1'},
  {id:'di',     name:'DI',       icon:'&#128294;', desc:'Digital inputs · GPIO 38-41'},
  {id:'psram',  name:'PSRAM',    icon:'&#128190;', desc:'External PSRAM · alloc/write/read'},
  {id:'rtc',    name:'RTC',      icon:'&#128336;', desc:'DS1307 · I2C clock read'},
  {id:'winbond',name:'Winbond',  icon:'&#128439;', desc:'SPI flash · JEDEC ID check'},
];

let pollTimer = null;
let selectedFile = null;

/* ── Build cards ────────────────────────────────────────────── */
function buildCards() {
  document.getElementById('grid').innerHTML = MODS.map(m => `
    <div class="card pending" id="card-${m.id}">
      <div class="card-head">
        <span class="card-label">${m.name}</span>
        <span class="badge PENDING" id="badge-${m.id}">PENDING</span>
      </div>
      <span class="card-icon">${m.icon}</span>
      <div class="card-detail" id="detail-${m.id}">${m.desc}</div>
      <button class="btn-run" id="btn-${m.id}" onclick="runOne('${m.id}')">&#9654; Run</button>
    </div>`).join('');
}

/* ── Fetch results and refresh UI ───────────────────────────── */
function fetchResults() {
  fetch('/results')
    .then(r => r.json())
    .then(data => applyResults(data))
    .catch(() => {});
}

function applyResults(data) {
  const busy = data.running;
  const allBtn = document.getElementById('btn-all');
  if (busy) {
    allBtn.disabled = true;
    allBtn.innerHTML = '<span class="spin"></span>Running…';
    allBtn.classList.add('busy');
  } else {
    allBtn.disabled = false;
    allBtn.innerHTML = '&#9654;&nbsp; Run All Tests';
    allBtn.classList.remove('busy');
  }

  data.tests.forEach(t => {
    const key = t.name.toLowerCase();
    const card   = document.getElementById('card-' + key);
    const badge  = document.getElementById('badge-' + key);
    const detail = document.getElementById('detail-' + key);
    const btn    = document.getElementById('btn-' + key);
    if (!card) return;
    const st = t.status.toLowerCase();
    card.className  = 'card ' + st;
    badge.className = 'badge ' + t.status;
    badge.textContent = t.status;
    detail.textContent = t.detail;
    if (btn) btn.disabled = busy;
  });

  if (!busy && pollTimer) {
    clearInterval(pollTimer);
    pollTimer = null;
    document.getElementById('last-run').textContent =
      'Last run: ' + new Date().toLocaleTimeString();
  }
}

/* ── Run controls ───────────────────────────────────────────── */
function runAll() {
  fetch('/run?test=all')
    .then(() => {
      document.getElementById('last-run').textContent = 'Running all tests…';
      startPoll();
    })
    .catch(() => alert('Could not reach device'));
}

function runOne(id) {
  fetch('/run?test=' + id)
    .then(() => startPoll())
    .catch(() => {});
}

function startPoll() {
  if (pollTimer) return;
  fetchResults(); fetchLog();
  pollTimer = setInterval(() => { fetchResults(); fetchLog(); }, 1800);
}

/* ── Log ────────────────────────────────────────────────────── */
function fetchLog() {
  fetch('/log')
    .then(r => r.text())
    .then(txt => {
      const el = document.getElementById('log');
      el.textContent = txt || 'No log output yet.';
      el.scrollTop = el.scrollHeight;
    })
    .catch(() => {});
}

/* ── OTA helpers ────────────────────────────────────────────── */
function handleDrop(e) {
  e.preventDefault();
  document.getElementById('ota-drop').classList.remove('over');
  const f = e.dataTransfer.files[0];
  if (f) useFile(f);
}

function onFilePick(input) {
  if (input.files && input.files[0]) useFile(input.files[0]);
}

function useFile(f) {
  selectedFile = f;
  const sz = f.size < 1024*1024
    ? (f.size/1024).toFixed(1)+' KB'
    : (f.size/1024/1024).toFixed(2)+' MB';
  document.getElementById('ota-fname').textContent = '&#128196; ' + f.name + ' (' + sz + ')';
  document.getElementById('btn-flash').style.display = 'inline-block';
  document.getElementById('ota-status').style.display = 'none';
  document.getElementById('prog-wrap').style.display = 'none';
  document.getElementById('prog-fill').style.width = '0';
}

function doFlash() {
  if (!selectedFile) return;
  const fd = new FormData();
  fd.append('firmware', selectedFile, selectedFile.name);
  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/ota');

  document.getElementById('prog-wrap').style.display = 'block';
  document.getElementById('btn-flash').disabled = true;
  document.getElementById('ota-status').style.display = 'none';

  xhr.upload.addEventListener('progress', e => {
    if (!e.lengthComputable) return;
    const pct = Math.round(e.loaded / e.total * 100);
    document.getElementById('prog-fill').style.width = pct + '%';
    document.getElementById('prog-label').textContent = 'Uploading… ' + pct + '%';
  });

  xhr.addEventListener('load', () => {
    const ok = xhr.status === 200;
    const st = document.getElementById('ota-status');
    st.style.display = 'block';
    st.style.color = ok ? '#34d399' : '#f87171';
    st.textContent  = ok
      ? '&#10003; OTA successful! Device rebooting in 2 s…'
      : '&#10007; OTA failed: ' + xhr.responseText;
    document.getElementById('prog-label').textContent = ok ? 'Upload complete!' : 'Upload failed';
    if (!ok) document.getElementById('btn-flash').disabled = false;
  });

  xhr.addEventListener('error', () => {
    const st = document.getElementById('ota-status');
    st.style.display = 'block';
    st.style.color = '#f87171';
    st.textContent = '&#10007; Network error — check WiFi connection';
    document.getElementById('btn-flash').disabled = false;
  });

  xhr.send(fd);
}

/* ── Init ────────────────────────────────────────────────────── */
buildCards();
fetchResults();
fetchLog();
setInterval(fetchResults, 2500);
setInterval(fetchLog, 6000);
</script>
</body>
</html>
)HTMLEOF";

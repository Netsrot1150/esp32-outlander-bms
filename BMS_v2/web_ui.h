#pragma once
// Webbsidan för BMS v2. HTML, CSS och JS är inbäddat – inga externa resurser, eftersom BMS:ens
// eget WiFi saknar internet. Förhandsvisa på PC med:  python tools/mock_server.py
static const char WEB_UI_HTML[] = R"BMSUI(<!doctype html>
<html lang="sv">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#0d1117">
<title>BMS Transporter</title>
<style>
:root{--bg:#0d1117;--panel:#161b22;--panel2:#1f2632;--line:#2b3442;--text:#e6edf3;--muted:#8d99aa;--accent:#4c9aff;--ok:#3fb950;--warn:#d29922;--bad:#f85149;--low:#58c4ff;--high:#ff7b72;--bleed:#e3b341}
*{box-sizing:border-box}
html,body{margin:0;background:var(--bg);color:var(--text);font:15px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
header{position:sticky;top:0;z-index:5;background:rgba(13,17,23,.94);backdrop-filter:blur(6px);border-bottom:1px solid var(--line);padding:10px 16px;display:flex;align-items:center;gap:10px;flex-wrap:wrap}
h1{font-size:17px;margin:0;font-weight:650}
.sub{color:var(--muted);font-size:12px}
.dot{width:10px;height:10px;border-radius:50%;background:var(--bad);flex:none}
.dot.ok{background:var(--ok)}
nav{display:flex;gap:4px;margin-left:auto}
nav button{background:none;border:1px solid transparent;color:var(--muted);padding:6px 12px;border-radius:8px;font:inherit;cursor:pointer}
nav button.on{color:var(--text);background:var(--panel2);border-color:var(--line)}
main{padding:14px 16px 48px;max-width:1320px;margin:0 auto}
.alarmbar{display:none;margin-bottom:12px;padding:10px 14px;border-radius:10px;font-weight:600}
.alarmbar.warn{display:block;background:rgba(210,153,34,.14);border:1px solid var(--warn);color:#f2cc60}
.alarmbar.bad{display:block;background:rgba(248,81,73,.14);border:1px solid var(--bad);color:#ffa198}
.tiles{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:10px;margin-bottom:14px}
.tile{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:10px 12px;min-width:0}
.tile.wide{grid-column:span 2}
.k{color:var(--muted);font-size:12px;text-transform:uppercase;letter-spacing:.6px}
.v{font-size:24px;font-weight:650;font-variant-numeric:tabular-nums;margin-top:2px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.s{color:var(--muted);font-size:13px;margin-top:2px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.badge{display:inline-block;padding:2px 9px;border-radius:999px;font-size:14px;font-weight:600;border:1px solid var(--line);color:var(--muted);vertical-align:3px}
.badge.ACTIVE,.badge.MANUAL{color:var(--bleed);border-color:var(--bleed)}
.badge.BLOCKED{color:var(--bad);border-color:var(--bad)}
.badge.WAITING{color:var(--accent);border-color:var(--accent)}
.legend{display:flex;gap:16px;flex-wrap:wrap;color:var(--muted);font-size:12px;margin:0 0 10px}
.legend i{display:inline-block;width:12px;height:12px;border-radius:3px;margin-right:6px;vertical-align:-2px}
.mods{display:grid;grid-template-columns:repeat(auto-fill,minmax(290px,1fr));gap:12px}
.mod{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:10px 12px}
.mod.off{opacity:.5;border-style:dashed}
.mod h3{margin:0;font-size:15px;display:flex;justify-content:space-between;gap:8px;font-weight:600}
.mod h3 small{color:var(--muted);font-weight:400}
.temps{display:flex;gap:6px;flex-wrap:wrap;margin:6px 0 8px;min-height:20px}
.temps span{font-size:12px;padding:1px 7px;border-radius:6px;background:var(--panel2);color:var(--muted);font-variant-numeric:tabular-nums}
.cells{display:grid;gap:5px;height:132px}
.cell{display:flex;flex-direction:column;min-width:0;text-align:center}
.cell .mv{font-size:11px;color:var(--muted);font-variant-numeric:tabular-nums;height:16px}
.cell .track{flex:1;display:flex;align-items:flex-end;background:rgba(255,255,255,.03);border-radius:4px}
.cell .bar{width:100%;border-radius:4px;background:var(--accent);transition:height .4s}
.cell.min .bar{background:var(--low)}
.cell.min .mv{color:var(--low);font-weight:600}
.cell.max .bar{background:var(--high)}
.cell.max .mv{color:var(--high);font-weight:600}
.cell.bleed .bar{background-image:repeating-linear-gradient(135deg,var(--bleed) 0 4px,transparent 4px 8px)}
.cell .n{font-size:11px;color:var(--muted);height:16px;line-height:16px}
.empty{color:var(--muted);padding:40px 10px;text-align:center;grid-column:1/-1}
.panel{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:12px 14px;margin-bottom:12px}
.panel h2{font-size:15px;margin:0 0 10px;font-weight:600}
.form{display:grid;grid-template-columns:repeat(auto-fill,minmax(220px,1fr));gap:10px 16px}
label{display:flex;flex-direction:column;gap:4px;font-size:13px;color:var(--muted)}
label.chk{flex-direction:row;align-items:center;gap:8px;color:var(--text);font-size:14px}
input,select{background:var(--bg);color:var(--text);border:1px solid var(--line);border-radius:8px;padding:8px 10px;font:inherit;width:100%}
label.chk input{width:18px;height:18px}
input:focus,select:focus{outline:2px solid var(--accent);outline-offset:-1px}
input.bad,select.bad{border-color:var(--bad)}
.hint{font-size:12.5px;color:var(--muted);margin:8px 0 0}
.btn{display:inline-block;background:var(--accent);color:#fff;border:0;border-radius:8px;padding:9px 16px;font:inherit;font-weight:600;cursor:pointer;text-decoration:none}
.btn.sec{background:var(--panel2);color:var(--text);border:1px solid var(--line)}
.btn.danger{background:#b62324}
.btn:disabled,.btn.disabled{opacity:.45;pointer-events:none}
.row{display:flex;gap:10px;flex-wrap:wrap;align-items:center}
.savebar{position:sticky;bottom:0;background:var(--bg);padding:10px 0;border-top:1px solid var(--line)}
.msg{font-size:14px;margin:0}
.msg.ok{color:var(--ok)}
.msg.err{color:var(--bad)}
table{width:100%;border-collapse:collapse;font-size:13px}
th,td{text-align:left;padding:6px 8px;border-bottom:1px solid var(--line);vertical-align:top}
th{color:var(--muted);font-weight:500}
.mono{font-family:ui-monospace,SFMono-Regular,Consolas,monospace;font-size:12.5px;white-space:nowrap}
.kv{display:grid;grid-template-columns:repeat(auto-fill,minmax(240px,1fr));gap:2px 18px;font-size:14px}
.kv div{display:flex;justify-content:space-between;gap:10px;border-bottom:1px dashed var(--line);padding:4px 0}
.kv span:first-child{color:var(--muted)}
.scroll{overflow-x:auto}
progress{width:100%;height:10px;margin-top:10px}
.socbox{display:flex;align-items:center;gap:14px}
.batt{position:relative;width:64px;height:30px;border:2px solid var(--muted);border-radius:6px;flex:none}
.batt:after{content:'';position:absolute;right:-6px;top:8px;width:4px;height:10px;background:var(--muted);border-radius:0 2px 2px 0}
.batt i{position:absolute;left:2px;top:2px;bottom:2px;width:0;border-radius:3px;background:var(--ok);transition:width .6s}
.charts{display:grid;gap:12px}
.chart{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:10px 12px}
.chart h3{margin:0 0 6px;font-size:14px;font-weight:600;display:flex;justify-content:space-between;gap:8px;flex-wrap:wrap}
.chart h3 span{color:var(--muted);font-weight:400;font-variant-numeric:tabular-nums;font-size:13px}
.chart canvas{width:100%;height:190px;display:block;touch-action:pan-y}
.seg{display:flex;gap:4px;margin-bottom:12px;flex-wrap:wrap}
.seg button{background:var(--panel2);border:1px solid var(--line);color:var(--muted);padding:6px 12px;border-radius:8px;font:inherit;cursor:pointer}
.seg button.on{color:var(--text);border-color:var(--accent)}
.hidden{display:none!important}
.avg{font-size:14px;font-weight:500;color:var(--muted);margin-left:6px}
#cC{height:240px}
.sw{display:inline-block;width:10px;height:10px;border-radius:2px;margin-right:6px}
#cellTab tr{cursor:pointer}
#cellTab td,#cellTab th{white-space:nowrap}
#cellTab tr.sel td{background:var(--panel2)}
.neg{color:var(--bad)}
@media (max-width:560px){.v{font-size:20px}.tile.wide{grid-column:1/-1}.cells{height:118px}.cell .mv{font-size:10px}nav{margin-left:0;width:100%}nav button{flex:1;padding:6px 4px;font-size:14px}}
</style>
</head>
<body>
<header>
  <span class="dot" id="conn" title="Ingen kontakt med BMS:en"></span>
  <h1>BMS Transporter</h1>
  <span class="sub" id="hdrUp"></span>
  <nav id="nav">
    <button data-tab="ov" class="on">Översikt</button>
    <button data-tab="graf">Grafer</button>
    <button data-tab="set">Inställningar</button>
    <button data-tab="diag">Diagnostik</button>
  </nav>
</header>
<main>
  <div class="alarmbar" id="alarmbar"></div>

  <div id="tab-ov">
    <div class="tiles">
      <div class="tile wide"><div class="k">Laddnivå</div><div class="socbox"><div class="batt"><i id="tSocBar"></i></div><div style="min-width:0"><div class="v"><span id="tSoc">–</span> <small class="avg" id="tSocAvg" title="Laddnivå räknad från medelcellens spänning. Huvudvärdet räknas från lägsta cellen, som avgör när packet är tomt."></small></div><div class="s" id="tSocS"></div></div></div></div>
      <div class="tile hidden" id="tCurTile"><div class="k">Ström</div><div class="v" id="tCur">–</div><div class="s" id="tCurS"></div></div>
      <div class="tile"><div class="k">Pack</div><div class="v" id="tPack">–</div><div class="s" id="tPackS"></div></div>
      <div class="tile"><div class="k">Lägsta cell</div><div class="v" id="tMin">–</div><div class="s" id="tMinS"></div></div>
      <div class="tile"><div class="k">Högsta cell</div><div class="v" id="tMax">–</div><div class="s" id="tMaxS"></div></div>
      <div class="tile"><div class="k">Delta</div><div class="v" id="tDelta">–</div><div class="s">högsta − lägsta</div></div>
      <div class="tile"><div class="k">Temperatur</div><div class="v" id="tTemp">–</div><div class="s">min – max</div></div>
      <div class="tile"><div class="k">CMU</div><div class="v" id="tCmu">–</div><div class="s">online / förväntade</div></div>
      <div class="tile wide"><div class="k">Balansering</div><div class="v"><span class="badge" id="tBal">–</span> <span id="tBalT" class="sub" style="font-size:15px"></span></div><div class="s" id="tBalR"></div></div>
    </div>
    <div class="legend">
      <span><i style="background:var(--low)"></i>Lägsta cell</span>
      <span><i style="background:var(--high)"></i>Högsta cell</span>
      <span><i style="background:var(--accent);background-image:repeating-linear-gradient(135deg,var(--bleed) 0 3px,transparent 3px 6px)"></i>Bleed pågår</span>
    </div>
    <div class="mods" id="mods"><div class="empty">Väntar på data från CMU:erna…</div></div>
  </div>

  <div id="tab-graf" class="hidden">
    <div class="seg" id="rangeSeg">
      <button data-r="10m">10 min</button><button data-r="1h" class="on">1 h</button><button data-r="6h">6 h</button><button data-r="24h">24 h</button>
    </div>
    <div class="charts">
      <div class="chart"><h3>Packspänning <span id="cv0"></span></h3><canvas id="c0"></canvas></div>
      <div class="chart"><h3>Lägsta / högsta cell <span id="cv1"></span></h3><canvas id="c1"></canvas></div>
      <div class="chart"><h3>Ström <span id="cv2"></span></h3><canvas id="c2"></canvas></div>
      <div class="chart"><h3>Laddnivå och temperatur <span id="cv3"></span></h3><canvas id="c3"></canvas></div>
    </div>
    <p class="hint" id="grafHint"></p>
    <div class="chart" style="margin-top:12px">
      <h3>Celler – avvikelse från median <span id="cvC"></span></h3>
      <div class="seg" id="cellSeg"><button data-d="1">1 dygn</button><button data-d="3">3 dygn</button><button data-d="14" class="on">14 dygn</button></div>
      <canvas id="cC"></canvas>
      <div class="scroll"><table id="cellTab"></table></div>
      <p class="hint">Timmedel per cell. De 5 lägsta (blå) och 5 högsta (röda) vid senaste punkten är färgade, och den sista punkten är timmen som pågår. Drift är hur snabbt cellen glider från medianen under vald period. Jämför helst perioder när bilen står still: en cell som tappar mer än ca 3 mV/dygn självurladdar troligen. Tryck på en rad för att markera cellen.</p>
    </div>
  </div>

  <div id="tab-set" class="hidden">
    <form id="setForm" autocomplete="off" onsubmit="return false"></form>
    <section class="panel"><h2>Nolla strömsensorn</h2>
      <div class="row"><button class="btn sec" id="zeroBtn" type="button">Nolla ström nu</button><span class="msg" id="zeroMsg"></span></div>
      <p class="hint">Gör detta när ingen ström går genom kabeln (laddare och last av). Strömsensorn måste vara aktiverad och sparad först.</p>
    </section>
    <div class="row savebar">
      <button class="btn" id="saveBtn" type="button">Spara</button>
      <button class="btn sec" id="reloadBtn" type="button">Ångra ändringar</button>
      <span class="msg" id="setMsg"></span>
    </div>
  </div>

  <div id="tab-diag" class="hidden">
    <section class="panel"><h2>System</h2><div class="kv" id="dSys"></div></section>
    <section class="panel"><h2>CAN</h2><div class="kv" id="dCan"></div></section>
    <section class="panel"><h2>Nätverk</h2><div class="kv" id="dNet"></div></section>
    <section class="panel"><h2>Ström, klocka och historik</h2><div class="kv" id="dMisc"></div></section>
    <section class="panel"><h2>Rå CAN per CMU</h2><div class="scroll"><table id="dRaw"></table></div>
      <p class="hint">Sub 1: byte 0 = bleed-bitar, byte 1 = status, byte 2–7 = temperaturgivare (FF = ingen givare). Sub 2–3: cellspänningar i mV, råa (översikten visar utjämnade värden, tidskonstant ca 4 s). Sub 4 (alltid FF) och sub 5 filtreras bort utom under CAN-inspelning. Sub 5: byte 6–7 = CMU:ns egen mätning av modulspänningen i mV.</p></section>
    <section class="panel"><h2>Verktyg</h2>
      <div class="row">
        <button class="btn" id="capBtn" type="button">Spela in CAN 10 s</button>
        <a class="btn sec disabled" id="capDl" href="/api/capture.csv" download="bms-can.csv">Ladda ner CSV</a>
        <a class="btn sec" href="/api/report" download="bms-rapport.json">Ladda ner rapport</a>
        <span class="msg" id="capMsg"></span>
      </div>
      <div class="row" style="margin-top:10px">
        <button class="btn sec" id="bleedBtn" type="button">Nollställ bleed-tid</button>
        <button class="btn danger" id="restartBtn" type="button">Starta om BMS</button>
      </div>
    </section>
    <section class="panel"><h2>Firmware-uppdatering</h2>
      <div class="row">
        <input type="file" id="fwFile" accept=".bin" style="max-width:340px">
        <button class="btn" id="fwBtn" type="button">Ladda upp</button>
      </div>
      <progress id="fwProg" value="0" max="1" class="hidden"></progress>
      <p class="msg" id="fwMsg"></p>
      <p class="hint">Använd filen som slutar på <b>-ota.bin</b>. Inställningarna behålls.</p>
    </section>
    <section class="panel"><h2>Larmlogg</h2><div class="scroll"><table id="dLog"></table></div></section>
  </div>
</main>
<script>
'use strict';
const $ = id => document.getElementById(id);
const esc = s => String(s).replace(/[&<>"]/g, c => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;'}[c]));
const V = (mV, d = 3) => (mV / 1000).toFixed(d).replace('.', ',');
const clock = s => Math.floor(s / 3600) + ':' + String(Math.floor(s % 3600 / 60)).padStart(2, '0') + ':' + String(s % 60).padStart(2, '0');
const dur = s => { s = Math.round(s); const h = Math.floor(s / 3600), m = Math.floor(s % 3600 / 60); return (h ? h + ' h ' : '') + (h || m ? m + ' min ' : '') + (s % 60) + ' s'; };
const BAL = {OFF: 'Av', WAITING: 'Väntar', ACTIVE: 'Balanserar', MANUAL: 'Manuell', BLOCKED: 'Blockerad'};
const hm = m => Math.floor(m / 60) + ' h ' + String(m % 60).padStart(2, '0') + ' min';
const dec = (v, d) => v.toFixed(d).replace('.', ',');
let lastState = null;

let tab = 'ov', lastOk = 0, modKey = '', loaded = null, scale = null;

async function getJSON(url) {
  const c = new AbortController();
  const t = setTimeout(() => c.abort(), 2500);
  try {
    const r = await fetch(url, {signal: c.signal, cache: 'no-store'});
    if (!r.ok) throw new Error('HTTP ' + r.status);
    return await r.json();
  } finally { clearTimeout(t); }
}
async function post(url, body) {
  const r = await fetch(url, {method: 'POST', headers: body ? {'Content-Type': 'application/json'} : {}, body: body ? JSON.stringify(body) : null});
  let j = null;
  try { j = await r.json(); } catch (e) {}
  return {ok: r.ok, status: r.status, json: j};
}
function msg(el, cls, text) { el.className = 'msg ' + cls; el.textContent = text; }

// ---------- Flikar ----------
function showTab(t) {
  tab = t;
  for (const b of document.querySelectorAll('#nav button')) b.classList.toggle('on', b.dataset.tab === t);
  for (const id of ['ov', 'graf', 'set', 'diag']) $('tab-' + id).classList.toggle('hidden', id !== t);
  if (t === 'set') loadSettings();
  if (t === 'graf') { loadHist(); loadCells(); }
  try { localStorage.setItem('tab', t); } catch (e) {}
}
$('nav').addEventListener('click', e => { const b = e.target.closest('button'); if (b) showTab(b.dataset.tab); });

// ---------- Översikt ----------
function alarmValue(a) {
  switch (a.id) {
    case 0: case 1: return V(a.value) + ' V';
    case 2: return a.value + ' mV';
    case 3: case 4: return a.value + ' °C';
    case 5: return a.value + ' saknas';
    case 7: return a.value + ' A';
    default: return '';
  }
}
function alarmText(a) { const v = alarmValue(a); return a.name + (v ? ' ' + v : '') + (a.loc ? ' (' + a.loc + ')' : ''); }
function renderAlarms(al) {
  const bar = $('alarmbar');
  if (!al.active.length) { bar.className = 'alarmbar'; bar.textContent = ''; return; }
  const bad = al.active.some(a => a.level === 'ALARM');
  bar.className = 'alarmbar ' + (bad ? 'bad' : 'warn');
  bar.textContent = al.active.map(a => (a.level === 'ALARM' ? 'LARM: ' : 'Varning: ') + alarmText(a)).join('   ·   ');
}
function makeModule(c, i) {
  const el = document.createElement('div');
  el.className = 'mod';
  let h = '<h3><span>Modul ' + (i + 1) + ' <small>CMU ' + c.id + '</small></span><span><b class="tot"></b> <small class="st"></small></span></h3>' +
          '<div class="temps"></div><div class="cells" style="grid-template-columns:repeat(' + c.cells.length + ',1fr)">';
  for (let k = 0; k < c.cells.length; k++) h += '<div class="cell"><div class="mv"></div><div class="track"><div class="bar"></div></div><div class="n">' + (k + 1) + '</div></div>';
  el.innerHTML = h + '</div>';
  return el;
}
function renderModules(s) {
  const box = $('mods');
  if (!s.cmus.length) {
    if (modKey !== 'none') { box.innerHTML = '<div class="empty">Väntar på data från CMU:erna…</div>'; modKey = 'none'; }
    return;
  }
  const key = s.cmus.map(c => c.id + ':' + c.cells.length).join(',');
  if (key !== modKey) { modKey = key; box.innerHTML = ''; s.cmus.forEach((c, i) => box.appendChild(makeModule(c, i))); }
  const p = s.pack;
  // Skalan avrundas till 10 mV och byts bara när en cell hamnar nära kanten eller långt innanför,
  // så att staplarna inte skalas om varje gång lägsta eller högsta cell ändras 1 mV.
  if (p.cells && (!scale || p.minmV < scale.lo + 3 || p.maxmV > scale.hi - 3 || p.minmV - scale.lo > 40 || scale.hi - p.maxmV > 40))
    scale = {lo: Math.floor(p.minmV / 10) * 10 - 10, hi: Math.ceil(p.maxmV / 10) * 10 + 10};
  const lo = scale ? scale.lo : 3000, hi = scale ? scale.hi : 4200, span = Math.max(hi - lo, 20);
  s.cmus.forEach((c, i) => {
    const el = box.children[i];
    el.classList.toggle('off', !c.online);
    el.querySelector('.tot').textContent = V(c.mV, 2) + ' V';
    el.querySelector('.tot').title = c.cmuMv ? 'Summa av cellerna. CMU:ns egen mätning: ' + V(c.cmuMv, 2) + ' V' : 'Summa av cellerna';
    el.querySelector('.st').textContent = c.online ? '' : 'offline ' + Math.round(c.age / 1000) + ' s';
    el.querySelector('.temps').innerHTML = c.tC.map((t, j) => t === null ? '' : '<span title="Rå ' + c.tRaw[j] + '">T' + (j + 1) + ' ' + t + '°</span>').join('');
    const cells = el.querySelectorAll('.cell');
    c.cells.forEach((mv, k) => {
      const ce = cells[k];
      ce.querySelector('.mv').textContent = mv || '–';
      ce.querySelector('.bar').style.height = (mv ? Math.max(4, Math.min(100, (mv - lo) / span * 100)) : 0) + '%';
      ce.classList.toggle('min', p.cells > 0 && p.minIdx[0] === i && p.minIdx[1] === k);
      ce.classList.toggle('max', p.cells > 0 && p.maxIdx[0] === i && p.maxIdx[1] === k);
      ce.classList.toggle('bleed', !!c.bal[k]);
      ce.title = 'Cell ' + (k + 1) + ': ' + mv + ' mV' + (c.bal[k] ? ' · bleed pågår' : '') + ' · bleed-tid ' + dur(c.bleedS[k]);
    });
  });
}
function renderState(s) {
  const p = s.pack, has = p.cells > 0;
  $('tPack').textContent = has ? V(p.mV, 1) + ' V' : '–';
  $('tPackS').textContent = p.cells + ' celler';
  $('tMin').textContent = has ? V(p.minmV) + ' V' : '–';
  $('tMinS').textContent = has ? p.minLoc : '';
  $('tMax').textContent = has ? V(p.maxmV) + ' V' : '–';
  $('tMaxS').textContent = has ? p.maxLoc : '';
  $('tDelta').textContent = has ? p.deltamV + ' mV' : '–';
  $('tTemp').textContent = p.minT === null ? '–' : (p.minT === p.maxT ? p.minT : p.minT + '–' + p.maxT) + ' °C';
  $('tCmu').textContent = p.online + ' / ' + p.expected;
  $('tCmu').style.color = p.online < p.expected ? 'var(--bad)' : '';
  const b = s.bal, badge = $('tBal');
  badge.textContent = BAL[b.state] || b.state;
  badge.className = 'badge ' + b.state;
  $('tBalT').textContent = b.cmdMode ? 'mål ' + V(b.targetmV) + ' V' : '';
  $('tBalR').textContent = b.reason;
  $('hdrUp').textContent = 'upptid ' + clock(s.up);
  lastState = s;
  const so = s.soc;
  $('tSoc').textContent = so.valid ? dec(so.pct, 0) + ' %' : '–';
  const bar = $('tSocBar');
  bar.style.width = so.valid ? (Math.max(0, Math.min(100, so.pct)) / 100 * 56) + 'px' : '0';
  bar.style.background = so.pct < 15 ? 'var(--bad)' : so.pct < 30 ? 'var(--warn)' : 'var(--ok)';
  $('tSocS').textContent = so.valid ? dec(so.kWhLeft, 1) + ' kWh kvar · ' + (so.mode === 'ah' ? 'Ah-räkning' : 'uppskattad') : '';
  $('tSocAvg').textContent = so.avgPct === null || so.avgPct === undefined ? '' : 'medelcell ' + dec(so.avgPct, 0) + ' %';
  $('tSoc').title = so.valid ? 'Andel av energin mellan tom- och fullgränsen. Laddning (Ah): ' + dec(so.chargePct, 1) + ' %' : '';
  const cu = s.cur;
  $('tCurTile').classList.toggle('hidden', !cu.enabled);
  if (cu.enabled) {
    $('tCur').textContent = (cu.A > 0 ? '+' : '') + dec(cu.A, 1) + ' A';
    $('tCurS').textContent = dec(cu.kW, 2) + ' kW' + (cu.minutesLeft !== null ? ' · ' + (cu.A < 0 ? 'tomt om ' : 'fullt om ') + hm(cu.minutesLeft) : '') + (cu.saturated ? ' · mättad' : '');
  }
  renderAlarms(s.alarms);
  renderModules(s);
}

// ---------- Inställningar ----------
const SCHEMA = [
  ['Balansering', [
    ['balMode', 'Läge', 'sel', [[0, 'Av'], [1, 'Auto'], [2, 'Manuell (test)']]],
    ['balStartmV', 'Startspänning', 'num', 'mV'],
    ['balStartDeltamV', 'Starta vid delta ≥', 'num', 'mV'],
    ['balStopDeltamV', 'Stoppa vid delta ≤', 'num', 'mV'],
    ['balMaxTempC', 'Max temperatur', 'num', '°C'],
    ['balTargetOffsetmV', 'Mål = lägsta cell +', 'num', 'mV'],
    ['manualTargetmV', 'Manuellt mål', 'num', 'mV'],
    ['manualTimeoutMin', 'Manuell stängs av efter', 'num', 'min']],
    'Auto balanserar bara när alla CMU:er är online, högsta cell ≥ startspänningen och temperaturen är under max. Målet blir max(lägsta cell + tillägg, startspänning).'],
  ['Larmgränser', [
    ['cellHighWarnmV', 'Cell hög – varning', 'num', 'mV'], ['cellHighAlarmmV', 'Cell hög – larm', 'num', 'mV'],
    ['cellLowWarnmV', 'Cell låg – varning', 'num', 'mV'], ['cellLowAlarmmV', 'Cell låg – larm', 'num', 'mV'],
    ['deltaWarnmV', 'Delta – varning', 'num', 'mV'], ['deltaAlarmmV', 'Delta – larm', 'num', 'mV'],
    ['tempHighWarnC', 'Temp hög – varning', 'num', '°C'], ['tempHighAlarmC', 'Temp hög – larm', 'num', '°C'],
    ['tempLowWarnC', 'Temp låg – varning', 'num', '°C']]],
  ['Temperatur och CAN', [
    ['tempOffset', 'Temperaturförskjutning (T = rå − x)', 'num', ''],
    ['canPollMs', 'Pollintervall 0x3C3', 'num', 'ms'],
    ['expectedCmus', 'Förväntat antal CMU', 'num', ''],
    ['cmuTimeoutMs', 'CMU offline efter', 'num', 'ms']],
    'Kalibrering: jämför rådata i Diagnostik med uppmätt temperatur. Exempel: rå 59 och uppmätt 19 °C ger förskjutning 40.'],
  ['Ström och laddnivå', [
    ['curEnabled', 'Strömsensor ansluten (WCS1700 på GPIO3)', 'chk'],
    ['curInvert', 'Vänd strömriktningen', 'chk'],
    ['curSensX100', 'Känslighet', 'num', 'mV/A', 100],
    ['curZeromV', 'Nollpunkt', 'num', 'mV'],
    ['curFactorX10', 'Faktor (ledare genom sensorn)', 'num', '×', 10],
    ['curWarnA', 'Urladdningsström – varning', 'num', 'A'],
    ['curAlarmA', 'Urladdningsström – larm', 'num', 'A'],
    ['capacityAhX10', 'Kapacitet', 'num', 'Ah', 10],
    ['socEmptymV', '0 % vid lägsta cell', 'num', 'mV'],
    ['socFullmV', '100 % vid lägsta cell', 'num', 'mV']],
    'Positiv ström = laddning. Med strömsensor räknas laddnivån i Ah och rättas automatiskt till 100 % när laddaren slutat och mot vilospänningen efter 30 min vila.'],
  ['WiFi och skärm (kräver omstart)', [
    ['apSsid', 'Eget nät – namn', 'txt'], ['apPass', 'Eget nät – lösenord', 'pw'],
    ['staSsid', 'Hemnät – namn (tomt = av)', 'txt'], ['staPass', 'Hemnät – lösenord', 'pw'],
    ['staStatic', 'Fast IP i hemnätet', 'chk'], ['staIp', 'IP-adress', 'txt'], ['staGw', 'Gateway', 'txt'], ['staMask', 'Nätmask', 'txt'],
    ['espnowEnabled', 'Skicka data till skärm (ESP-NOW)', 'chk']]]
];
const LABEL = {};
const NET_KEYS = ['apSsid', 'apPass', 'staSsid', 'staPass', 'staStatic', 'staIp', 'staGw', 'staMask', 'espnowEnabled'];
function buildForm() {
  let h = '';
  for (const [title, fields, hint] of SCHEMA) {
    h += '<section class="panel"><h2>' + title + '</h2><div class="form">';
    for (const [key, label, type, extra, scale] of fields) {
      LABEL[key] = label;
      if (type === 'chk') { h += '<label class="chk"><input type="checkbox" name="' + key + '"> ' + label + '</label>'; continue; }
      h += '<label>' + label + (type === 'num' && extra ? ' (' + extra + ')' : '');
      if (type === 'sel') h += '<select name="' + key + '">' + extra.map(([v, t]) => '<option value="' + v + '">' + t + '</option>').join('') + '</select>';
      else h += '<input name="' + key + '" type="' + (type === 'num' ? 'number' : type === 'pw' ? 'password' : 'text') + '"' + (type === 'num' ? ' step="' + (1 / (scale || 1)) + '" data-scale="' + (scale || 1) + '" inputmode="decimal"' : '') + '>';
      h += '</label>';
    }
    h += '</div>' + (hint ? '<p class="hint">' + hint + '</p>' : '') + '</section>';
  }
  $('setForm').innerHTML = h;
}
async function loadSettings() {
  try {
    const s = await getJSON('/api/settings');
    loaded = s;
    for (const el of $('setForm').elements) {
      if (!(el.name in s)) continue;
      if (el.type === 'checkbox') el.checked = !!s[el.name];
      else { const sc = Number(el.dataset.scale || 1); el.value = sc === 1 ? s[el.name] : s[el.name] / sc; }
      el.classList.remove('bad');
    }
    msg($('setMsg'), '', '');
  } catch (e) { msg($('setMsg'), 'err', 'Kunde inte läsa inställningarna'); }
}
async function saveSettings() {
  const body = {};
  for (const el of $('setForm').elements) {
    if (!el.name) continue;
    el.classList.remove('bad');
    const sc = Number(el.dataset.scale || 1);
    body[el.name] = el.type === 'checkbox' ? el.checked : el.tagName === 'SELECT' ? Number(el.value) : el.type === 'number' ? Math.round(Number(el.value) * sc) : el.value;
  }
  const r = await post('/api/settings', body).catch(() => null);
  if (!r) return msg($('setMsg'), 'err', 'Ingen kontakt med BMS:en');
  if (!r.ok) {
    const f = r.json && r.json.field;
    const el = f && $('setForm').elements[f];
    if (el) { el.classList.add('bad'); el.focus(); }
    return msg($('setMsg'), 'err', 'Ogiltigt värde: ' + (LABEL[f] || f || 'okänt fält'));
  }
  const netChanged = loaded && NET_KEYS.some(k => String(body[k]) !== String(loaded[k]));
  loaded = r.json;
  msg($('setMsg'), 'ok', netChanged ? 'Sparat ✓ – nätverksändringar gäller efter omstart (Diagnostik → Starta om BMS)' : 'Sparat ✓');
}

// ---------- Diagnostik ----------
function kv(el, rows) { el.innerHTML = rows.map(([k, v]) => '<div><span>' + esc(k) + '</span><span>' + esc(v) + '</span></div>').join(''); }
function renderDiag(d) {
  kv($('dSys'), [['Firmware', d.fw + (d.sim ? ' (SIMULERING)' : '')], ['Upptid', clock(d.up)], ['Senaste omstart', d.reset || '?'],
    ['Ledigt minne', Math.round(d.heap / 1024) + ' kB (lägst ' + Math.round(d.minHeap / 1024) + ' kB)']]);
  const c = d.can;
  const hex = v => '0x' + v.toString(16).toUpperCase().padStart(2, '0');
  const MCP = {0: 'ej initierad', 1: 'OK', 2: 'konfigurerad, väntar på normalläge', 3: 'går inte att konfigurera', 4: 'register går inte att skriva'};
  const MODE = {0: 'normal', 1: 'sleep', 2: 'loopback', 3: 'listen-only', 4: 'konfiguration'};
  kv($('dCan'), [['MCP2515', (MCP[c.mcpStep] || c.mcpStep) + (c.ok ? '' : ' · försök ' + c.initTries)],
    ['Läge (CANSTAT)', (MODE[c.canstat >> 5] || '?') + ' · ' + hex(c.canstat) + ' · CANCTRL ' + hex(c.canctrl)],
    ['CANSTAT efter reset', hex(c.initCanstat)], ['SPI-test', c.spiOk ? 'OK' : 'FEL'], ['Återstarter av MCP2515', c.reinits], ['CMU-ramar', c.rx], ['Övriga ramar', c.other + ' (filtreras bort utom under inspelning)'],
    ['0x3C3 skickade', c.txOk], ['TX upptagen', c.txBusy], ['TX avbrutna', c.txAborts], ['RX-overflow', c.overflows],
    ['TEC / REC', c.tec + ' / ' + c.rec], ['EFLG', '0b' + c.eflg.toString(2).padStart(8, '0')],
    ['Senaste CAN-fel', c.lastErrS === null ? 'aldrig' : dur(c.lastErrS) + ' sedan'], ['Pollintervall', c.pollMs + ' ms']]);
  const n = d.net;
  kv($('dNet'), [['Eget nät', n.apSsid + ' · ' + n.apIp], ['Klienter på eget nät', n.apClients],
    ['Hemnät', n.staSsid ? n.staSsid + (n.staConnected ? ' · ansluten ' + n.staIp + ' (' + n.rssi + ' dBm)' : ' · ej ansluten') : 'av'],
    ['WiFi-kanal', n.channel], ['ESP-NOW', n.espnow ? 'på · ' + n.espnowSent + ' paket' : 'av']]);
  let h = '<tr><th>CMU</th><th>Ram</th><th>Data (hex)</th><th>Ålder</th><th>Ramar/s</th><th>Antal</th></tr>';
  for (const m of d.cmus) m.subs.forEach((x, i) => {
    h += '<tr>' + (i === 0 ? '<td rowspan="' + m.subs.length + '"><b>' + m.id + '</b><br><span class="sub">status 0x' + m.status.toString(16).padStart(2, '0') + '<br>ogiltiga ' + m.invalid + '</span></td>' : '') +
         '<td>0x6' + m.id.toString(16).toUpperCase() + x.sub + '</td><td class="mono">' + (x.hex || '–') + '</td><td>' + (x.age === null ? '–' : x.age + ' ms') +
         '</td><td>' + x.fps + '</td><td>' + x.n + '</td></tr>';
  });
  $('dRaw').innerHTML = d.cmus.length ? h : '<tr><td>Inga CMU-ramar mottagna ännu.</td></tr>';
  const cap = d.capture;
  $('capBtn').disabled = cap.running;
  $('capDl').classList.toggle('disabled', cap.running || !cap.frames);
  if (cap.running) msg($('capMsg'), '', 'Spelar in… ' + cap.frames + ' ramar');
  else if (cap.frames) msg($('capMsg'), 'ok', cap.frames + ' ramar inspelade');
  let l = '<tr><th>Tid</th><th>Larm</th><th>Nivå</th><th>Händelse</th><th>Värde</th></tr>';
  for (const e of d.alarmLog) {
    l += '<tr><td>' + clock(e.t) + '</td><td>' + esc(e.name) + (e.loc ? ' (' + esc(e.loc) + ')' : '') + '</td><td>' + (e.level === 'ALARM' ? 'Larm' : 'Varning') +
         '</td><td>' + (e.active ? 'kom' : 'släppte') + '</td><td>' + esc(alarmValue(e)) + '</td></tr>';
  }
  $('dLog').innerHTML = d.alarmLog.length ? l : '<tr><td>Inga larm sedan start.</td></tr>';
  if (d.cur) kv($('dMisc'), [
    ['Strömsensor', d.cur.enabled ? d.cur.mV + ' mV · nollpunkt ' + d.cur.zeromV + ' mV' + (d.cur.saturated ? ' · MÄTTAD' : '') : 'av'],
    ['Klocka', d.time.known ? new Date(d.time.epoch * 1000).toLocaleString('sv-SE') : 'okänd'],
    ['Historik', d.hist.n24 + ' minutposter · ' + d.hist.nLive + ' sekundposter · flash ' + (d.hist.fsOk ? 'OK' : 'FEL')],
    ['Cellogg', (d.hist.nCells || 0) + ' timposter (max 336 = 14 dygn)']]);
}
$('capBtn').onclick = async () => {
  const r = await post('/api/capture?s=10').catch(() => null);
  if (!r || !r.ok) msg($('capMsg'), 'err', 'Kunde inte starta inspelningen' + (r && r.status === 507 ? ' (minne saknas)' : ''));
  else { msg($('capMsg'), '', 'Spelar in…'); $('capBtn').disabled = true; $('capDl').classList.add('disabled'); }
};
$('bleedBtn').onclick = async () => { if (confirm('Nollställa bleed-tiden för alla celler?')) await post('/api/bleed/reset').catch(() => {}); };
$('restartBtn').onclick = async () => {
  if (!confirm('Starta om BMS:en?')) return;
  await post('/api/restart').catch(() => {});
  msg($('capMsg'), '', 'Startar om…');
};
$('fwBtn').onclick = () => {
  const f = $('fwFile').files[0], m = $('fwMsg'), pr = $('fwProg');
  if (!f) return msg(m, 'err', 'Välj en .bin-fil först');
  if (!confirm('Ladda upp ' + f.name + ' (' + Math.round(f.size / 1024) + ' kB) och starta om BMS:en?')) return;
  const x = new XMLHttpRequest(), fd = new FormData();
  fd.append('firmware', f, f.name);
  pr.classList.remove('hidden'); pr.value = 0; $('fwBtn').disabled = true; msg(m, '', 'Laddar upp…');
  x.upload.onprogress = e => { if (e.lengthComputable) pr.value = e.loaded / e.total; };
  x.onload = () => { $('fwBtn').disabled = false; msg(m, x.status === 200 ? 'ok' : 'err', x.responseText || ('HTTP ' + x.status)); };
  x.onerror = () => { $('fwBtn').disabled = false; msg(m, 'err', 'Uppladdningen avbröts'); };
  x.open('POST', '/update');
  x.send(fd);
};
$('saveBtn').onclick = saveSettings;
$('reloadBtn').onclick = loadSettings;
$('zeroBtn').onclick = async () => {
  const r = await post('/api/current/zero').catch(() => null);
  if (!r || !r.ok) return msg($('zeroMsg'), 'err', (r && r.json && r.json.error) || 'Ingen kontakt');
  msg($('zeroMsg'), 'ok', 'Nollpunkt satt till ' + r.json.zeromV + ' mV');
  const el = $('setForm').elements['curZeromV'];
  if (el) el.value = r.json.zeromV;
};

// ---------- Grafer ----------
const RANGES = {'10m': 600, '1h': 3600, '6h': 21600, '24h': 86400};
let range = '1h', hist = [], histLoadedAt = 0, hoverT = null;
const css = v => v.startsWith('#') ? v : getComputedStyle(document.documentElement).getPropertyValue(v.startsWith('var(') ? v.slice(4, -1) : v).trim();
const CHARTS = [
  {id: 'c0', v: 'cv0', s: [{f: r => r.packdV / 10, c: 'var(--accent)'}],
   txt: r => dec(r.packdV / 10, 1) + ' V'},
  {id: 'c1', v: 'cv1', s: [{f: r => r.minmV, c: 'var(--low)'}, {f: r => r.maxmV, c: 'var(--high)'}],
   txt: r => r.minmV + ' / ' + r.maxmV + ' mV · Δ ' + (r.maxmV - r.minmV)},
  {id: 'c2', v: 'cv2', s: [{f: r => r.curdA === -32768 ? null : r.curdA / 10, c: 'var(--bleed)'}],
   txt: r => r.curdA === -32768 ? 'ingen sensor' : dec(r.curdA / 10, 1) + ' A · ' + dec(r.curdA / 10 * r.packdV / 10 / 1000, 2) + ' kW'},
  {id: 'c3', v: 'cv3', s: [{f: r => r.soc / 10, c: 'var(--ok)'}, {f: r => r.maxT === -128 ? null : r.maxT, c: 'var(--warn)', right: true}],
   txt: r => dec(r.soc / 10, 0) + ' %' + (r.maxT === -128 ? '' : ' · ' + r.minT + '–' + r.maxT + ' °C')},
];

function parseHist(buf) {
  const dv = new DataView(buf), n = dv.getUint32(0, true), out = [];
  const nowS = Date.now() / 1000, up = lastState ? lastState.up : 0;
  for (let i = 0; i < n; i++) {
    const o = 4 + i * 16;
    let t = dv.getUint32(o, true);
    if (t & 0x80000000) t = nowS - (up - (t & 0x7fffffff));   // relativ tid → uppskattad klocka
    out.push({t, packdV: dv.getUint16(o + 4, true), minmV: dv.getUint16(o + 6, true), maxmV: dv.getUint16(o + 8, true),
              minT: dv.getInt8(o + 10), maxT: dv.getInt8(o + 11), curdA: dv.getInt16(o + 12, true), soc: dv.getUint16(o + 14, true)});
  }
  return out;
}

async function loadHist() {
  try {
    const r = await fetch('/api/history?range=' + (range === '10m' ? '10m' : '24h'), {cache: 'no-store'});
    hist = parseHist(await r.arrayBuffer());
    histLoadedAt = Date.now();
  } catch (e) { }
  drawAll();
}

function fmtTime(t) {
  const o = range === '10m' ? {hour: '2-digit', minute: '2-digit', second: '2-digit'} : {hour: '2-digit', minute: '2-digit'};
  return new Date(t * 1000).toLocaleTimeString('sv-SE', o);
}

function drawAll() {
  const t1 = Date.now() / 1000, t0 = t1 - RANGES[range];
  const recs = hist.filter(r => r.t >= t0 && r.t <= t1 + 60);
  $('grafHint').textContent = recs.length ? '' : (range === '10m' ? 'Ingen data ännu.' : 'Ingen historik ännu – en post sparas per minut.');
  const gap = range === '10m' ? 5 : 180;
  let hov = null;
  if (hoverT !== null && recs.length) hov = recs.reduce((a, b) => Math.abs(b.t - hoverT) < Math.abs(a.t - hoverT) ? b : a);
  for (const cfg of CHARTS) drawChart(cfg, recs, t0, t1, gap, hov);
}

function drawChart(cfg, recs, t0, t1, gap, hov) {
  const cv = $(cfg.id), dpr = window.devicePixelRatio || 1, w = cv.clientWidth, h = cv.clientHeight;
  if (!w) return;
  if (cv.width !== Math.round(w * dpr)) { cv.width = Math.round(w * dpr); cv.height = Math.round(h * dpr); }
  const g = cv.getContext('2d');
  g.setTransform(dpr, 0, 0, dpr, 0, 0);
  g.clearRect(0, 0, w, h);
  const hasR = cfg.s.some(s => s.right), L = 46, R = hasR ? 38 : 10, T = 8, B = 20, pw = w - L - R, ph = h - T - B;
  const X = t => L + (t - t0) / (t1 - t0) * pw;
  const ax = {};
  for (const s of cfg.s) {
    const k = s.right ? 'r' : 'l';
    for (const r of recs) {
      const v = s.f(r);
      if (v === null) continue;
      const a = ax[k] || (ax[k] = {lo: v, hi: v});
      a.lo = Math.min(a.lo, v); a.hi = Math.max(a.hi, v);
    }
  }
  for (const k in ax) {
    const a = ax[k];
    if (a.hi - a.lo < 1e-6) { a.lo -= 1; a.hi += 1; }
    const p = (a.hi - a.lo) * 0.08; a.lo -= p; a.hi += p;
  }
  const Y = (v, k) => T + ph - (v - ax[k].lo) / (ax[k].hi - ax[k].lo) * ph;
  g.font = '11px system-ui, sans-serif';
  g.lineWidth = 1;
  g.strokeStyle = css('--line');
  g.fillStyle = css('--muted');
  for (let i = 0; i <= 4; i++) {
    const y = T + ph * i / 4;
    g.beginPath(); g.moveTo(L, y); g.lineTo(L + pw, y); g.stroke();
    for (const k in ax) {
      const v = ax[k].hi - (ax[k].hi - ax[k].lo) * i / 4;
      g.textAlign = k === 'l' ? 'right' : 'left';
      const span = ax[k].hi - ax[k].lo;
      g.fillText(dec(v, span >= 20 ? 0 : span >= 2 ? 1 : 2), k === 'l' ? L - 5 : L + pw + 5, y + 4);
    }
  }
  g.textAlign = 'center';
  const nx = Math.max(2, Math.min(4, Math.floor(pw / 90)));   // färre tidsetiketter på smal skärm
  for (let i = 0; i <= nx; i++) {
    const t = t0 + (t1 - t0) * i / nx;
    g.fillText((cfg.fmt || fmtTime)(t), Math.min(Math.max(X(t), L + 28), L + pw - 28), h - 5);
  }
  for (const s of cfg.s) {
    const k = s.right ? 'r' : 'l';
    if (!ax[k]) continue;
    g.strokeStyle = css(s.c);
    g.lineWidth = s.w || 1.6;
    g.globalAlpha = s.a || 1;
    g.beginPath();
    let pen = false, lastT = 0;
    for (const r of recs) {
      const v = s.f(r);
      if (v === null) { pen = false; continue; }
      const x = X(r.t), y = Y(v, k);
      if (!pen || r.t - lastT > gap) g.moveTo(x, y); else g.lineTo(x, y);
      pen = true; lastT = r.t;
    }
    g.stroke();
    g.globalAlpha = 1;
  }
  const show = hov || recs[recs.length - 1];
  $(cfg.v).textContent = show ? cfg.txt(show) + (hov ? ' · ' + (cfg.fmtHov || cfg.fmt || fmtTime)(hov.t) : '') : '';
  if (hov) {
    const x = X(hov.t);
    g.strokeStyle = css('--muted'); g.lineWidth = 1;
    g.beginPath(); g.moveTo(x, T); g.lineTo(x, T + ph); g.stroke();
    for (const s of cfg.s) {
      const k = s.right ? 'r' : 'l', v = s.f(hov);
      if (v === null || !ax[k] || s.nodot) continue;
      g.fillStyle = css(s.c);
      g.beginPath(); g.arc(x, Y(v, k), 3.5, 0, 7); g.fill();
    }
  }
}

$('rangeSeg').addEventListener('click', e => {
  const b = e.target.closest('button');
  if (!b) return;
  range = b.dataset.r;
  for (const x of $('rangeSeg').children) x.classList.toggle('on', x === b);
  loadHist();
});
for (const c of CHARTS) {
  const cv = $(c.id);
  const move = e => {
    const rect = cv.getBoundingClientRect(), t1 = Date.now() / 1000, t0 = t1 - RANGES[range];
    hoverT = t0 + (e.clientX - rect.left - 46) / (rect.width - 46 - (c.s.some(s => s.right) ? 38 : 10)) * (t1 - t0);
    drawAll();
  };
  cv.addEventListener('pointermove', move);
  cv.addEventListener('pointerdown', move);
  cv.addEventListener('pointerleave', () => { hoverT = null; drawAll(); });
}
window.addEventListener('resize', () => { if (tab === 'graf') { drawAll(); drawCells(); } });

// ---------- Cellogg ----------
const LOWC = ['#58c4ff', '#4c9aff', '#8fd8ff', '#2f6fd0', '#b4e4ff'], HIGHC = ['#ff7b72', '#ffa657', '#f85149', '#ffc2b8', '#d29922'];
let cellRecs = [], cellDays = 14, cellLoadedAt = 0, cellSel = null, cellHoverT = null;
const cellName = i => 'M' + (Math.floor(i / 8) + 1) + 'C' + (i % 8 + 1);
const sgn = v => { const d = Math.abs(v) < 9.95 ? 1 : 0, r = Number(v.toFixed(d)); return (r > 0 ? '+' : r < 0 ? '−' : '') + dec(Math.abs(r), d); };
const fmtFull = t => new Date(t * 1000).toLocaleString('sv-SE', {day: 'numeric', month: 'numeric', hour: '2-digit', minute: '2-digit'});
const fmtDay = t => cellDays > 1 ? new Date(t * 1000).toLocaleDateString('sv-SE', {day: 'numeric', month: 'numeric'})
                                 : new Date(t * 1000).toLocaleTimeString('sv-SE', {hour: '2-digit', minute: '2-digit'});

function parseCells(buf) {
  const dv = new DataView(buf), n = dv.getUint32(0, true), out = [];
  const nowS = Date.now() / 1000, up = lastState ? lastState.up : 0;
  for (let i = 0; i < n; i++) {
    const o = 4 + i * 72;
    let t = dv.getUint32(o, true);
    if (t & 0x80000000) t = nowS - (up - (t & 0x7fffffff));
    const base = dv.getUint16(o + 4, true), cn = dv.getUint8(o + 6), v = [];
    for (let k = 0; k < cn; k++) { const b = dv.getUint8(o + 8 + k); v.push(b === 255 ? null : base + b); }
    const ok = v.filter(x => x !== null).sort((a, b) => a - b), m = ok.length >> 1;
    const med = ok.length ? (ok.length % 2 ? ok[m] : (ok[m - 1] + ok[m]) / 2) : 0;
    out.push({t, v, dev: v.map(x => x === null ? null : x - med)});
  }
  return out;
}

async function loadCells() {
  try {
    const r = await fetch('/api/cells', {cache: 'no-store'});
    cellRecs = parseCells(await r.arrayBuffer());
    cellLoadedAt = Date.now();
  } catch (e) { }
  drawCells();
}

// Lutning i mV/dygn (minsta kvadrat) för cell i, null om perioden är kortare än 12 h.
function drift(recs, i) {
  const p = recs.filter(r => r.dev[i] != null).map(r => [r.t / 86400, r.dev[i]]);
  if (p.length < 3 || p[p.length - 1][0] - p[0][0] < 0.5) return null;
  const mx = p.reduce((a, q) => a + q[0], 0) / p.length, my = p.reduce((a, q) => a + q[1], 0) / p.length;
  let num = 0, den = 0;
  for (const q of p) { num += (q[0] - mx) * (q[1] - my); den += (q[0] - mx) ** 2; }
  return den ? num / den : null;
}

function drawCells() {
  const t1 = Date.now() / 1000, t0 = t1 - cellDays * 86400;
  const recs = cellRecs.filter(r => r.t >= t0 && r.t <= t1 + 3600), last = recs[recs.length - 1];
  const tabEl = $('cellTab');
  if (!last) {
    tabEl.innerHTML = '<tr><td>Ingen cellogg ännu. Värdena samlas varje sekund och ett timmedel sparas varje timme.</td></tr>';
    drawChart({id: 'cC', v: 'cvC', s: [], fmt: fmtDay, txt: () => ''}, [], t0, t1, 0, null);
    return;
  }
  const idx = last.dev.map((d, i) => ({d, i})).filter(x => x.d !== null).sort((a, b) => a.d - b.d);
  const lows = idx.slice(0, 5).map(x => x.i), highs = idx.slice(-5).reverse().map(x => x.i).filter(i => !lows.includes(i));
  const color = {};
  lows.forEach((i, k) => color[i] = LOWC[k]);
  highs.forEach((i, k) => color[i] = HIGHC[k]);
  const series = [];
  last.v.forEach((_, i) => { if (!(i in color) && i !== cellSel) series.push({f: r => r.dev[i] ?? null, c: '--muted', w: 1, a: .35, nodot: true}); });
  for (const i of [...lows, ...highs]) if (i !== cellSel) series.push({f: r => r.dev[i] ?? null, c: color[i], w: 1.8});
  if (cellSel !== null) series.push({f: r => r.dev[cellSel] ?? null, c: '--text', w: 2.8});
  const focus = cellSel !== null ? cellSel : lows[0];
  let hov = null;
  if (cellHoverT !== null && recs.length) hov = recs.reduce((a, b) => Math.abs(b.t - cellHoverT) < Math.abs(a.t - cellHoverT) ? b : a);
  drawChart({id: 'cC', v: 'cvC', s: series, fmt: fmtDay, fmtHov: fmtFull,
             txt: r => r.dev[focus] == null ? '' : cellName(focus) + ' ' + sgn(r.dev[focus]) + ' mV · ' + V(r.v[focus]) + ' V'},
            recs, t0, t1, 2 * 3600 + 60, hov);
  let h = '<tr><th>Cell</th><th>Spänning</th><th>Avvikelse</th><th>Drift mV/dygn</th></tr>';
  for (const i of [...lows, ...highs]) {
    const dr = drift(recs, i);
    h += '<tr data-i="' + i + '"' + (i === cellSel ? ' class="sel"' : '') + '><td><i class="sw" style="background:' + color[i] + '"></i>' + cellName(i) +
         '</td><td>' + V(last.v[i]) + ' V</td><td>' + sgn(last.dev[i]) + ' mV</td><td' + (dr !== null && dr <= -3 ? ' class="neg"' : '') + '>' +
         (dr === null ? '–' : sgn(dr)) + '</td></tr>';
  }
  tabEl.innerHTML = h;
}

$('cellTab').addEventListener('click', e => {
  const tr = e.target.closest('tr[data-i]');
  if (!tr) return;
  const i = Number(tr.dataset.i);
  cellSel = cellSel === i ? null : i;
  drawCells();
});
$('cellSeg').addEventListener('click', e => {
  const b = e.target.closest('button');
  if (!b) return;
  cellDays = Number(b.dataset.d);
  for (const x of $('cellSeg').children) x.classList.toggle('on', x === b);
  drawCells();
});
{
  const cv = $('cC');
  const move = e => {
    const rect = cv.getBoundingClientRect(), t1 = Date.now() / 1000, t0 = t1 - cellDays * 86400;
    cellHoverT = t0 + (e.clientX - rect.left - 46) / (rect.width - 46 - 10) * (t1 - t0);
    drawCells();
  };
  cv.addEventListener('pointermove', move);
  cv.addEventListener('pointerdown', move);
  cv.addEventListener('pointerleave', () => { cellHoverT = null; drawCells(); });
}

// ---------- Uppdateringsloop ----------
async function tick() {
  try {
    const s = await getJSON('/api/state');
    lastOk = Date.now();
    renderState(s);
    if (tab === 'graf' && Date.now() - histLoadedAt > (range === '10m' ? 5000 : 60000)) loadHist();
    if (tab === 'graf' && Date.now() - cellLoadedAt > 60000) loadCells();
    if (tab === 'diag') renderDiag(await getJSON('/api/diag'));
  } catch (e) {}
  const ok = Date.now() - lastOk < 3000;
  $('conn').classList.toggle('ok', ok);
  $('conn').title = ok ? 'Ansluten' : 'Ingen kontakt med BMS:en';
  setTimeout(tick, tab === 'diag' ? 1000 : 500);
}
buildForm();
let startTab = 'ov';
try { startTab = localStorage.getItem('tab') || 'ov'; } catch (e) {}
showTab(['ov', 'graf', 'set', 'diag'].includes(startTab) ? startTab : 'ov');
tick();
fetch('/api/time', {method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({epoch: Math.floor(Date.now() / 1000)})}).catch(() => {});
</script>
</body>
</html>)BMSUI";

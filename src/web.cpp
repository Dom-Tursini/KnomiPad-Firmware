#include "web.hpp"
#include <WebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <lvgl.h>
#include "storage.hpp"
#include "macros.hpp"
#include "net.hpp"
#include "rtc_time.hpp"
#include "ble_hid.hpp"
#include <NimBLEDevice.h>

static WebServer server(80);
static File g_upFile;
static String g_upExt;
static uint8_t g_upId = 0;

static String mimeFor(const String& path){
  if(path.endsWith(".png")) return "image/png";
  if(path.endsWith(".svg")) return "image/svg+xml";
  if(path.endsWith(".jpg") || path.endsWith(".jpeg")) return "image/jpeg";
  return "application/octet-stream";
}

static void serveIconFile(){
  String u = server.uri();
  if(!LittleFS.exists(u)){ server.send(404,"text/plain","not found"); return; }
  File f = LittleFS.open(u, "r");
  server.streamFile(f, mimeFor(u));
  f.close();
}

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><meta charset="utf-8">
<title>KnomiPad – Macro Editor</title>
<meta name=viewport content="width=device-width,initial-scale=1">
<style>
:root{
  --bg:#0e0f11; --panel:#17181b; --panel-hi:#1e2024;
  --text:#e9eef4; --muted:#a8b3c7; --ok:#3ad26b; --err:#ff4d4d; --primary:#2d9bf0; --chip:#2a2d33; --border:#2c2f36;
  --radius:16px; --radius-sm:12px; --shadow:0 10px 30px rgba(0,0,0,.35);
}
*{box-sizing:border-box}
html,body{margin:0;height:100%;background:var(--bg);color:var(--text);font-family:system-ui,-apple-system,Segoe UI,Roboto,Helvetica,Arial,sans-serif}
.container{max-width:min(720px, 34vw); min-width:340px; margin:28px auto 80px; padding:0 12px}
.h1{font-size:34px;font-weight:700;text-align:center;margin:10px 0 24px}
.card{background:var(--panel);border-radius:24px;box-shadow:var(--shadow);padding:16px 18px;margin:18px 0}
.toolbar{display:grid;grid-template-columns:1fr;gap:14px}
.toolbar .row .btn{width:100%}
@media(min-width:860px){.toolbar{grid-template-columns:1fr}}
.btn{appearance:none;border:0;background:var(--primary);color:#000;padding:10px 14px;border-radius:999px;font-weight:700;cursor:pointer}
.btn:active{transform:translateY(1px);filter:brightness(.96)}
.btn.gray{background:var(--panel-hi);color:var(--text)}
.btn.red{background:#ff5b5b;color:#000}
.row{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.section{display:flex;flex-direction:column;gap:8px;margin:8px 0}
.label{font-size:13px;color:var(--muted)}
.input, select, textarea{width:100%;background:var(--panel-hi);border:1px solid var(--border);color:var(--text);padding:10px;border-radius:10px}
select{height:40px}
textarea{min-height:42px;resize:vertical}
.badge{font-weight:800;color:var(--ok);background:rgba(58,210,107,.1);border:1px solid rgba(58,210,107,.25);padding:6px 10px;border-radius:10px}
.badge.err{color:var(--err);background:rgba(255,77,77,.12);border-color:rgba(255,77,77,.28)}
.cardHead{display:flex;justify-content:space-between;align-items:center;margin-bottom:8px}
.helper{font-size:12px;color:#83f28a;opacity:.9}
.bggrid{display:grid;grid-template-columns:repeat(4,1fr);gap:12px;align-items:center;margin-top:6px}
.swatch{width:44px;height:44px;background:var(--chip);border:1px solid var(--border);border-radius:10px;cursor:pointer;display:flex;align-items:center;justify-content:center;overflow:hidden}
.swatch img{max-width:100%;max-height:100%;object-fit:contain}
.preview{width:88px;height:88px;border-radius:50%;background:#222;border:1px solid var(--border);box-shadow:inset 0 8px 24px rgba(0,0,0,.35);position:relative;display:flex;align-items:center;justify-content:center}
.preview img{width:72%;height:72%;object-fit:contain;display:block;filter:drop-shadow(0 2px 6px rgba(0,0,0,.5))}
.actions{display:flex;gap:8px;justify-content:flex-end;margin-top:8px}
hr.sep{border:none;border-top:1px solid var(--border);margin:12px 0}
.center{display:flex;justify-content:center}
.hidden{display:none}
.small{font-size:12px;color:var(--muted)}
.right{float:right}
@media(max-width:720px){.container{max-width:92vw}}
</style>

<div class="container">
  <div class="h1">KnomiPad – Macro Editor</div>

  <!-- TOOLBAR -->
  <div class="card">
    <div class="toolbar">
      <!-- Row 1: Export -->
      <div class="row">
        <button class="btn" style="width:100%" onclick="exp()">Export Macro’s</button>
      </div>

      <!-- Row 2: Import + File -->
      <div class="row" style="grid-template-columns: 1fr 1fr;">
        <input id="imp" type="file" class="input" accept=".json">
        <button class="btn" onclick="imp()">Import Macro’s</button>
      </div>

      <!-- Row 3: Reset (red), Sync, Add Macro (right) -->
      <div class="row" style="grid-template-columns: 1fr 1fr 1fr; align-items:center;">
        <button class="btn red" onclick="factoryReset()">Firmware Reset</button>
        <button class="btn" onclick="syncTime()">Sync Time & Date</button>
        <div style="display:flex; gap:10px; justify-content:flex-end; align-items:center;">
          <div id="toolbarStatus" class="badge hidden">SAVED!</div>
          <button class="btn" onclick="addMacro()">Add Macro</button>
        </div>
      </div>
    </div>
  </div>

  <div id="list"></div>
</div>

<script>
const el = (tag, cls='', html='') => { const n=document.createElement(tag); if(cls) n.className=cls; if(html) n.innerHTML=html; return n; };
function toHex(c){return '#'+('000000'+c.toString(16)).slice(-6);}
function fromHex(h){return parseInt(h.replace('#',''),16);}

async function load(){
  const r = await fetch('/api/pages'); const js = await r.json();
  const list = document.getElementById('list'); list.innerHTML = '';
  (js.slots||[]).forEach((s,i)=> list.appendChild(renderCard(s,i)));
}

function renderCard(s, idx){
  const card = el('div','card');
  const head = el('div','cardHead');
  head.appendChild(el('div','small',`Macro ${idx+1}`));
  const status = el('div','badge hidden'); status.id = `status${s.id}`; head.appendChild(status);
  card.appendChild(head);

  // Name
  const name = el('div','section'); name.appendChild(el('div','label','Name:'));
  const nameIn = el('input','input'); nameIn.value = s.title||''; name.appendChild(nameIn); card.appendChild(name);

  // Type + helper
  const type = el('div','section'); type.appendChild(el('div','label','Type:'));
  const grid = el('div','row');
  const sel = el('select'); sel.innerHTML = `
    <option value="keystroke"${s.type==='keystroke'?' selected':''}>Keystroke</option>
    <option value="typing"${s.type==='typing'?' selected':''}>Typing</option>
    <option value="holdseq"${s.type==='holdseq'?' selected':''}>Hold sequence</option>
    <option value="keybind"${s.type==='keybind'?' selected':''}>Keybind</option>`;
  const help = el('div','helper','');
  function updateHelp(){
    if(sel.value==='keystroke'){
      help.innerHTML = 'Keystroke is a combination of keys, e.g. <b>LCtrl+c, 1s, LCtrl+v</b>';
    }else if(sel.value==='typing'){
      help.innerHTML = 'Typing sends characters with optional speed, e.g. <b>Hello (10/s)</b> or <b>password</b>.';
    }else if(sel.value==='holdseq'){
      help.innerHTML = 'Hold a modifier, then send a sequence. e.g. <b>LAlt | 0,1,7,9</b> (ALT code) or <b>LCtrl | c, v</b>. Delays: <b>500ms</b>, <b>0.5s</b>, <b>1s</b>.';
    }else{
      help.innerHTML = 'Keybind sends a single key, e.g. <b>F12</b> or <b>Tab</b>.';
    }
  }
  sel.addEventListener('change', updateHelp);
  updateHelp();
  grid.appendChild(sel); grid.appendChild(help); type.appendChild(grid); card.appendChild(type);

  // Macro text
  const mac = el('div','section'); mac.appendChild(el('div','label','Macro:'));
  const ta = el('textarea'); ta.value = s.payload||''; mac.appendChild(ta); card.appendChild(mac);

  // Background = always gradient (color1/color2) + Icon + Preview
  const bg = el('div','section'); bg.appendChild(el('div','label','Background:'));
  const bggrid = el('div','bggrid');

  // Color1
  const c1 = el('div','swatch'); c1.title='Color 1';
  const c1In = el('input','hidden'); c1In.type='color'; c1In.value = toHex(s.bg||0x2d9bf0);
  c1.style.background = c1In.value;
  c1.onclick = ()=> c1In.click();
  c1In.oninput = ()=> c1.style.background = c1In.value;
  c1.appendChild(c1In);
  bggrid.appendChild(el('div','small','Color 1')); bggrid.appendChild(c1);

  // Color2
  const c2 = el('div','swatch'); c2.title='Color 2';
  const c2In = el('input','hidden'); c2In.type='color'; c2In.value = toHex((s.bg2??s.bg)||0x2d9bf0);
  c2.style.background = c2In.value;
  c2.onclick = ()=> c2In.click();
  c2In.oninput = ()=> c2.style.background = c2In.value;
  c2.appendChild(c2In);
  bggrid.appendChild(el('div','small','Color 2')); bggrid.appendChild(c2);

  // Icon picker
  const ic = el('div','swatch'); ic.title='Icon';
  const icImg = el('img'); if(s.iconPath){ icImg.src = s.iconPath + '?v=' + Date.now(); }
  ic.appendChild(icImg);
  const icIn = el('input'); icIn.type='file'; icIn.accept='.png,.svg'; icIn.className='hidden';
  ic.onclick = ()=> icIn.click();
  icIn.onchange = async ()=>{
    if(!icIn.files || !icIn.files[0]) return;
    const fd = new FormData(); fd.append('id', s.id); fd.append('icon', icIn.files[0]);
    const res = await fetch('/api/icon', { method:'POST', body:fd });
    if(res.ok){
      const js = await res.json();
      const url = js.path + '?v=' + (js.ts || Date.now());
      icImg.src = url;
      pvImg.src = url;
      s.iconPath = js.path;
      showOk(status);
    } else {
      showErr(status, 'Icon upload failed');
    }
  };

  // Preview
  const pv = el('div','preview');
  const pvImg = el('img'); if(s.iconPath){ pvImg.src = s.iconPath + '?v=' + Date.now(); } pv.appendChild(pvImg);
  const paintPreview = ()=> {
    pv.style.background = `linear-gradient(to bottom, ${c1In.value}, ${c2In.value})`;
  };
  c1In.oninput = ()=>{ c1.style.background=c1In.value; paintPreview(); };
  c2In.oninput = ()=>{ c2.style.background=c2In.value; paintPreview(); };
  paintPreview();

  bggrid.appendChild(el('div','small','Icon')); bggrid.appendChild(ic);
  bggrid.appendChild(el('div','small','Preview')); bggrid.appendChild(pv);
  card.appendChild(bggrid);

  // Actions
  const actions = el('div','actions');
  const del = el('button','btn red'); del.textContent='Delete';
  del.onclick = async ()=>{
    if(!confirm('Delete this macro?')) return;
    const r = await fetch('/api/page?id='+s.id, {method:'DELETE'});
    if(r.ok){ load(); } else showErr(status, 'Delete failed');
  };
  const save = el('button','btn'); save.textContent='Save';
  save.onclick = async ()=>{
    save.disabled = true; status.className='badge'; status.textContent='Saving…'; status.classList.remove('hidden');
    const body = {
      id:s.id,
      title:nameIn.value.trim(),
      type:sel.value,
      payload:ta.value,
      bg: fromHex(c1In.value),
      bg2: fromHex(c2In.value),
      gradient: true
    };
    const r = await fetch('/api/page', { method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(body) });
    if(r.ok){ showOk(status); } else { const t = await r.text(); showErr(status,'ERROR! '+t); }
    save.disabled = false;
  };
  actions.appendChild(del); actions.appendChild(save);
  card.appendChild(el('hr','sep'));
  card.appendChild(actions);

  // hidden file control appended at end
  card.appendChild(icIn);
  return card;
}

function showOk(node){ node.className='badge'; node.textContent='SAVED!'; node.classList.remove('hidden'); }
function showErr(node,msg){ node.className='badge err'; node.textContent=msg||'ERROR!'; node.classList.remove('hidden'); }

async function exp(){ const r=await fetch('/api/export'); const t=await r.text(); const a=document.createElement('a'); a.href='data:application/json;charset=utf-8,'+encodeURIComponent(t); a.download='knomipad_profile.json'; a.click(); }
async function imp(){ const f=document.getElementById('imp').files?.[0]; if(!f) return; const txt=await f.text(); const r=await fetch('/api/import',{method:'POST',headers:{'Content-Type':'application/json'},body:txt}); const s=document.getElementById('toolbarStatus'); if(r.ok) showOk(s); else showErr(s); load(); }
function syncTime(){
  const now = Date.now();
  const tz  = -new Date().getTimezoneOffset(); // minutes EAST of UTC
  fetch('/api/settime', {
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify({ epoch_ms: now, tz_offset_min: tz })
  }).then(r=>{
    const s=document.getElementById('toolbarStatus');
    if(r.ok) showOk(s); else showErr(s);
  }).catch(()=>{ const s=document.getElementById('toolbarStatus'); showErr(s); });
}
async function factoryReset(){ if(!confirm('Factory reset? This clears macros, icons, and Wi-Fi settings.')) return; const r=await fetch('/api/factory_reset',{method:'POST'}); const s=document.getElementById('toolbarStatus'); if(r.ok) showOk(s); else showErr(s); load(); }
async function addMacro(){ const r=await fetch('/api/add',{method:'POST'}); if(r.ok) load(); else { const s=document.getElementById('toolbarStatus'); showErr(s,'ERROR! Could not add macro'); } }

load();
</script>

)HTML";
static const char SETUP_HTML[] PROGMEM = R"HTML(
<!doctype html><meta charset="utf-8">
<title>KnomiPad • Wi-Fi Setup</title>
<meta name=viewport content="width=device-width,initial-scale=1">
<style>
body{font-family:system-ui,Segoe UI,Roboto,Helvetica,Arial,sans-serif;background:#111;color:#eee;margin:0}
main{padding:16px}
.card{background:#1c1c1e;border-radius:14px;padding:16px;margin:12px 0;box-shadow:0 8px 20px rgba(0,0,0,.3)}
input,select,button{width:100%;padding:10px;border-radius:10px;border:1px solid #333;background:#2a2a2b;color:#fff;margin:6px 0}
button{background:#2d9bf0;color:#000;font-weight:700}
small{color:#aaa}
</style>
<main>
  <div class=card>
    <h3>Connect to Wi-Fi</h3>
    <button id=scanBtn type=button>Scan Networks</button>
    <select id=ssid><option>Scanning…</option></select>
    <input id=pass type=password placeholder="Password (if any)">
    <button id=saveBtn type=button>Save & Reboot</button>
    <small>After reboot, browse to <b>http://knomipad.local/</b> on the same network.</small>
  </div>
</main>
<script>
async function scan(){
  document.getElementById('ssid').innerHTML = '<option>Scanning…</option>';
  const r = await fetch('/api/scan'); const js = await r.json();
  const sel = document.getElementById('ssid'); sel.innerHTML='';
  js.networks.forEach(n=>{
    const o = document.createElement('option'); o.value=n.ssid; o.textContent = n.ssid + ' ('+n.rssi+'dBm)'+(n.secure?' 🔒':'');
    sel.appendChild(o);
  });
}
async function save(){
  const ssid = document.getElementById('ssid').value;
  const pass = document.getElementById('pass').value;
  await fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid,pass})});
  document.body.innerHTML = '<main><div class=card><h3>Rebooting…</h3><p>Please reconnect to your Wi-Fi, then visit <b>http://knomipad.local/</b>.</p></div></main>';
  setTimeout(()=>{ fetch('/status').catch(()=>{}); }, 1500);
}
document.getElementById('scanBtn').onclick=scan;
document.getElementById('saveBtn').onclick=save;
scan();
</script>
)HTML";


static void send_json(std::function<void(JsonDocument&)> fn) {
  StaticJsonDocument<4096> doc;
  fn(doc);
  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
}

static void handle_index(){
  server.sendHeader("Cache-Control","no-store, no-cache, must-revalidate, max-age=0");
  if(!net::sta_ok()){
    server.send_P(200, "text/html; charset=utf-8", SETUP_HTML);
  } else {
    server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  }
}

static void handle_pages() {
  send_json([](JsonDocument& d){
    auto& prof = storage::profile();
    JsonArray arr = d.createNestedArray("slots");
    for (auto& s : prof.slots) {
      JsonObject o = arr.createNestedObject();
      o["id"] = s.id;
      o["title"] = s.title;
      switch (s.type) {
        case macros::Type::Keystroke: o["type"] = "keystroke"; break;
        case macros::Type::Typing:    o["type"] = "typing";    break;
        case macros::Type::HoldSeq:   o["type"] = "holdseq";  break;
        default:                      o["type"] = "keybind";   break;
      }
      o["payload"] = s.payload;
      o["bg"] = s.bg;
      o["bg2"] = s.bg2;
      o["gradient"] = s.gradient;
      o["iconPath"] = s.iconPath;
    }
  });
}

static void handle_export(){
  auto& p = storage::profile();
  StaticJsonDocument<4096> d;
  JsonArray arr = d.createNestedArray("slots");
  for (auto& s : p.slots){
    JsonObject o=arr.createNestedObject();
    o["id"]=s.id; o["title"]=s.title; o["payload"]=s.payload; o["iconPath"]=s.iconPath; o["bg"]=s.bg; o["bg2"]=s.bg2; o["gradient"]=s.gradient;
    switch(s.type){case macros::Type::Keystroke:o["type"]="keystroke";break;case macros::Type::Typing:o["type"]="typing";break;case macros::Type::HoldSeq:o["type"]="holdseq";break;default:o["type"]="keybind";}
  }
  String out; serializeJson(d,out);
  server.send(200,"application/json",out);
}

// Recursively delete a folder (icons)
static void rmrf(const char* path){
  File dir = LittleFS.open(path);
  if(!dir) return;
  if(!dir.isDirectory()){ LittleFS.remove(path); return; }
  File f;
  while((f = dir.openNextFile())){
    String p = String(path) + "/" + String(f.name()).substring(1); // ensure absolute
    if(f.isDirectory()) rmrf(p.c_str()); else LittleFS.remove(p.c_str());
  }
  LittleFS.rmdir(path);
}

static void handle_hidtest(){
  // Sends F12 once
  #include "ble_hid.hpp"
  if (!knomi::ble_is_connected()) { server.send(503,"text/plain","ble not connected"); return; }
  knomi::ble_press_release(0x45 /*F12*/, 0, 20);
  server.send(200,"text/plain","ok");
}

static void handle_import(){
  String body = server.arg("plain");
  StaticJsonDocument<4096> d;
  if (deserializeJson(d,body)){ server.send(400,"text/plain","bad json"); return; }
  storage::Profile np;
  for (JsonObject s : d["slots"].as<JsonArray>()){
    macros::Slot sl;
    sl.id = s["id"] | 0;
    sl.title = String((const char*)s["title"]);
    sl.payload = String((const char*)s["payload"]);
    sl.iconPath = String((const char*)s["iconPath"]);
    sl.bg = s["bg"] | 0x2D9BF0;
    sl.bg2 = s["bg2"] | sl.bg;
    sl.gradient = s["gradient"] | false;
    String t = String((const char*)s["type"]); t.toLowerCase();
    sl.type = (t=="keystroke")?macros::Type::Keystroke:(t=="typing")?macros::Type::Typing:(t=="holdseq")?macros::Type::HoldSeq:macros::Type::Keybind;
    np.slots.push_back(sl);
  }
  storage::save(np); storage::load(); // persist & reload
  server.send(200,"text/plain","ok");
}

// POST /api/add  -> creates a new empty slot, returns {id:<newId>}
static void handle_add(){
  auto& p = storage::profile();
  macros::Slot s;
  s.id = p.slots.empty()? 0 : (uint8_t)(p.slots.back().id + 1);
  s.title = String("Macro ") + (int)(p.slots.size()+1);
  s.type = macros::Type::Keystroke;
  s.payload = "";
  s.bg = 0x2d2f38;
  s.bg2 = s.bg;
  s.gradient = true;
  p.slots.push_back(s);
  storage::save();
  String out; out.reserve(32);
  out = "{\"id\":" + String((int)s.id) + "}";
  server.send(200,"application/json",out);
}

static void handle_settime(){
  String body = server.arg("plain");
  StaticJsonDocument<192> d;
  if (deserializeJson(d, body)) { server.send(400,"text/plain","bad json"); return; }

  // Accept epoch_ms as number or string; fallback to epoch_s
  uint64_t ms = 0;
  if (d.containsKey("epoch_ms")) {
    if (d["epoch_ms"].is<const char*>()) {
      const char* s = d["epoch_ms"];
      ms = strtoull(s, nullptr, 10);
    } else {
      ms = (uint64_t) d["epoch_ms"].as<unsigned long long>();
    }
  } else if (d.containsKey("epoch_s")) {
    unsigned long long secs = d["epoch_s"].as<unsigned long long>();
    ms = secs * 1000ULL;
  } else {
    server.send(400,"text/plain","missing epoch"); return;
  }

  int tzmin = 0;
  if (d.containsKey("tz_offset_min")) tzmin = (int)d["tz_offset_min"].as<long>(); // minutes EAST of UTC

  rtime::set_from_epoch_ms(ms, tzmin);
  server.send(200,"text/plain","ok");
}

static void handle_save_page() {
  String body = server.arg("plain");
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, body)) { server.send(400, "text/plain", "bad json"); return; }
  uint8_t id = doc["id"] | 0;
  auto s = storage::get_slot(id);
  if (!s) { macros::Slot ns; ns.id=id; storage::set_slot(id, ns); s = storage::get_slot(id); }
  s->title = String((const char*)doc["title"]);
  s->payload = String((const char*)doc["payload"]);
  if (doc.containsKey("bg"))       s->bg = (uint32_t) doc["bg"].as<unsigned long>();
  if (doc.containsKey("bg2"))      s->bg2 = (uint32_t) doc["bg2"].as<unsigned long>();
  if (doc.containsKey("gradient")) s->gradient = (bool) doc["gradient"];
  String t = String((const char*)doc["type"]); t.toLowerCase();
  if (t=="keystroke") s->type = macros::Type::Keystroke;
  else if (t=="typing") s->type = macros::Type::Typing;
  else if (t=="holdseq") s->type = macros::Type::HoldSeq;
  else s->type = macros::Type::Keybind;
  storage::save();
  server.send(200, "text/plain", "ok");
}

// DELETE /api/page?id=<id>
static void handle_delete_page(){
  if(!server.hasArg("id")){ server.send(400,"text/plain","missing id"); return; }
  uint8_t id = (uint8_t)server.arg("id").toInt();
  auto& p = storage::profile();
  for(size_t i=0;i<p.slots.size();++i){
    if(p.slots[i].id == id){
      if(p.slots[i].iconPath.length()){
        String path = p.slots[i].iconPath;
        if(!path.startsWith("/")) path = "/" + path;
        LittleFS.remove(path);
      }
      p.slots.erase(p.slots.begin()+i);
      storage::save();
      server.send(200,"text/plain","ok");
      return;
    }
  }
  server.send(404,"text/plain","not found");
}

static void handle_test() {
  String body = server.arg("plain");
  StaticJsonDocument<128> doc; if (deserializeJson(doc, body)) { server.send(400,"text/plain","bad json"); return; }
  uint8_t id = doc["id"] | 0;
  auto s = storage::get_slot(id);
  if (!s) { server.send(404,"text/plain","no slot"); return; }
  switch (s->type) {
    case macros::Type::Keystroke: macros::run_keystroke(s->payload); break;
    case macros::Type::Typing:    macros::run_typing(s->payload); break;
    case macros::Type::HoldSeq:   macros::run_holdseq(s->payload); break;
    default:                      macros::run_keybind(s->payload); break;
  }
  server.send(200,"text/plain","ok");
}

static void handle_scan(){
  int n = WiFi.scanNetworks(/*async=*/false, /*hidden=*/true);
  StaticJsonDocument<2048> d;
  JsonArray arr = d.createNestedArray("networks");
  for (int i=0;i<n;i++){
    JsonObject o = arr.createNestedObject();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
    o["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }
  String out; serializeJson(d,out);
  server.send(200,"application/json",out);
}

static void handle_wifi() {
  String body = server.arg("plain");
  StaticJsonDocument<256> doc; if (deserializeJson(doc, body)) { server.send(400,"text/plain","bad json");return; }
  String ssid = String((const char*)doc["ssid"]);
  String pass = String((const char*)doc["pass"]);
  if (ssid.length()==0) { server.send(400,"text/plain","ssid required"); return; }
  net::save_sta(ssid, pass);
  server.send(200,"text/plain","saved, reconnecting");
  delay(250);
  ESP.restart();
  // app code can reboot after response, if desired
}

static void handle_icon_done(){
  auto s = storage::get_slot(g_upId);
  String p = s ? s->iconPath : String();
  if(!p.startsWith("/")) p = "/" + p;
  StaticJsonDocument<96> d;
  d["path"] = p;
  d["ts"] = millis();
  String out; serializeJson(d, out);
  server.send(200, "application/json", out);
}

static void handle_icon_upload(){
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    g_upId = (uint8_t) server.arg("id").toInt();
    String name = up.filename; name.trim();
    int dot = name.lastIndexOf('.');
    g_upExt = (dot>=0) ? name.substring(dot) : String(".png"); // keep .png or .svg
    LittleFS.mkdir("/icons");
    String path = String("/icons/") + g_upId + g_upExt;
    if (g_upFile) g_upFile.close();
    g_upFile = LittleFS.open(path, "w");
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (g_upFile) g_upFile.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    if (g_upFile) g_upFile.close();
    if (auto s = storage::get_slot(g_upId)) {
      s->iconPath = String("/icons/") + g_upId + g_upExt;
      storage::save();
    }
  }
}

static void handle_status(){
  StaticJsonDocument<256> d;
  d["ap"] = true;              // AP is always on in our flow
  d["sta_ok"] = net::sta_ok();
  d["ip"] = WiFi.localIP().toString();
  String out; serializeJson(d, out);
  server.send(200, "application/json", out);
}

static void handle_bleinfo(){
  StaticJsonDocument<256> d;
  d["addr"] = NimBLEDevice::getAddress().toString();
  d["connected"] = knomi::ble_is_connected();
  // no direct "adv running" API; report connected==false as "should be advertising"
  String out; serializeJson(d,out);
  server.send(200,"application/json",out);
}

static void handle_debug(){
  StaticJsonDocument<2048> d;
  d["uptime_ms"] = (uint32_t)millis();
  d["lvgl_png"] = (int)LV_USE_PNG;
  d["lvgl_svg"] = (int)LV_USE_SVG;
  d["ip"] = WiFi.localIP().toString();
  d["sta_ok"] = net::sta_ok();
  d["ble_connected"] = knomi::ble_is_connected();
  auto& p = storage::profile();
  JsonArray arr = d.createNestedArray("icons");
  for (auto& s : p.slots) {
    JsonObject o = arr.createNestedObject();
    o["id"]=s.id; o["path"]=s.iconPath; bool exists=false;
    String pth=s.iconPath; if(pth.length() && !pth.startsWith("/")) pth="/"+pth;
    if(pth.length()) exists = LittleFS.exists(pth);
    o["exists"]=exists;
  }
  String out; serializeJsonPretty(d,out);
  server.send(200,"application/json",out);
}

static void handle_factory(){
  LittleFS.begin(true);
  LittleFS.remove("/config/wifi.json");
  LittleFS.remove("/macros/slots.json");
  if (LittleFS.exists("/icons")) rmrf("/icons");
  server.send(200,"text/plain","resetting");
  delay(250);
  ESP.restart();
}

static void handle_clearbonds(){
  NimBLEDevice::deleteAllBonds();
  server.send(200,"text/plain","bonds cleared, rebooting");
  delay(250);
  ESP.restart();
}

bool web::begin() {
  server.on("/", HTTP_GET, handle_index);
  server.on("/api/pages", HTTP_GET, handle_pages);
  server.on("/api/page", HTTP_POST, handle_save_page);
  server.on("/api/page", HTTP_DELETE, handle_delete_page);
  server.on("/api/test", HTTP_POST, handle_test);
  server.on("/api/scan", HTTP_GET, handle_scan);
  server.on("/api/wifi", HTTP_POST, handle_wifi);
  server.on("/api/icon", HTTP_POST, handle_icon_done, handle_icon_upload);
  server.on("/api/export", HTTP_GET, handle_export);
  server.on("/api/import", HTTP_POST, handle_import);
  server.on("/api/add", HTTP_POST, handle_add);
  server.on("/api/settime", HTTP_POST, handle_settime);
  server.on("/api/hidtest", HTTP_POST, handle_hidtest);
  server.on("/api/bleinfo", HTTP_GET, handle_bleinfo);
  server.on("/status", HTTP_GET, handle_status);
  server.on("/api/factory_reset", HTTP_POST, handle_factory);
  server.on("/api/clearbonds", HTTP_POST, handle_clearbonds);
  server.on("/debug", HTTP_GET, handle_debug);
  server.onNotFound([](){
    if (server.method() == HTTP_OPTIONS) { server.send(204); return; }
    String u = server.uri();
    if(u.startsWith("/icons/")) { serveIconFile(); return; }
    server.sendHeader("Cache-Control","no-store, no-cache, must-revalidate, max-age=0");
    server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  });
  server.begin();
  return true;
}

void web::loop() { server.handleClient(); }

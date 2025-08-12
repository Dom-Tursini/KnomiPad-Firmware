#include "storage.hpp"
#include <LittleFS.h>
#include <ArduinoJson.h>

namespace storage {

static Profile g_prof;

static void ensure_defaults(Profile& p) {
  if (!p.slots.empty()) return;
  p.slots.resize(5);
  const uint32_t colors[5] = {0x2D9BF0,0x34C759,0xFF9F0A,0xFF375F,0x8E8E93};
  const char* titles[5] = {"Copy/Paste","Hello","F12","Tab","Enter"};
  for (uint8_t i=0;i<5;i++) {
    p.slots[i].id = i;
    p.slots[i].title = titles[i];
    p.slots[i].bg = colors[i];
    p.slots[i].bg2 = p.slots[i].bg;
    p.slots[i].gradient = false;
    if (i==0) { p.slots[i].type = macros::Type::Keystroke; p.slots[i].payload = "LCtrl+c, 500ms, LCtrl+v"; }
    else if (i==1) { p.slots[i].type = macros::Type::Typing; p.slots[i].payload = "This is KnomiPad (8/s)"; }
    else if (i==2) { p.slots[i].type = macros::Type::Keybind; p.slots[i].payload = "F12"; }
    else if (i==3) { p.slots[i].type = macros::Type::Keybind; p.slots[i].payload = "Tab"; }
    else { p.slots[i].type = macros::Type::Keybind; p.slots[i].payload = "Enter"; }
    p.slots[i].iconPath = String("/icons/") + i + ".svg"; // may not exist yet
  }
}

bool begin() {
  return LittleFS.begin(true);
}

bool load(Profile& out) {
  if (!begin()) return false;
  if (!LittleFS.exists("/macros/slots.json")) {
    ensure_defaults(out);
    save(out);
    return true;
  }
  File f = LittleFS.open("/macros/slots.json", "r");
  if (!f) { ensure_defaults(out); return false; }
  StaticJsonDocument<2048> doc;
  auto err = deserializeJson(doc, f);
  if (err) { ensure_defaults(out); return false; }
  out.slots.clear();
  for (JsonObject s : doc["slots"].as<JsonArray>()) {
    macros::Slot slot;
    slot.id = s["id"] | 0;
    slot.title = String((const char*)s["title"]);
    slot.payload = String((const char*)s["payload"]);
    slot.iconPath = String((const char*)s["iconPath"]);
    slot.bg = s["bg"] | 0x2D9BF0;
    slot.bg2 = s["bg2"] | slot.bg;
    slot.gradient = s["gradient"] | false;
    String t = String((const char*)s["type"]);
    t.toLowerCase();
    if (t=="keystroke") slot.type = macros::Type::Keystroke;
    else if (t=="typing") slot.type = macros::Type::Typing;
    else if (t=="holdseq") slot.type = macros::Type::HoldSeq;
    else slot.type = macros::Type::Keybind;
    out.slots.push_back(slot);
  }
  if (out.slots.empty()) ensure_defaults(out);
  return true;
}

bool save(const Profile& in) {
  if (!begin()) return false;
  LittleFS.mkdir("/macros");
  File f = LittleFS.open("/macros/slots.json", "w");
  if (!f) return false;
  StaticJsonDocument<2048> doc;
  JsonArray arr = doc.createNestedArray("slots");
  for (auto& s : in.slots) {
    JsonObject o = arr.createNestedObject();
    o["id"] = s.id;
    o["title"] = s.title;
    o["payload"] = s.payload;
    o["iconPath"] = s.iconPath;
    o["bg"] = s.bg;
    o["bg2"] = s.bg2;
    o["gradient"] = s.gradient;
    switch (s.type) {
      case macros::Type::Keystroke: o["type"]="keystroke"; break;
      case macros::Type::Typing:    o["type"]="typing"; break;
      case macros::Type::HoldSeq:   o["type"]="holdseq"; break;
      default:                      o["type"]="keybind"; break;
    }
  }
  serializeJson(doc, f);
  return true;
}

Profile& profile() { return g_prof; }
bool load() { return load(g_prof); }
bool save() { return save(g_prof); }

macros::Slot* get_slot(uint8_t id) {
  if (id >= g_prof.slots.size()) return nullptr;
  return &g_prof.slots[id];
}
bool set_slot(uint8_t id, const macros::Slot& s) {
  if (id >= g_prof.slots.size()) g_prof.slots.resize(id+1);
  g_prof.slots[id] = s;
  g_prof.slots[id].id = id;
  return true;
}
uint8_t count() { return g_prof.slots.size(); }

} // namespace storage

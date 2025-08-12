#include "macros.hpp"
#include "ble_hid.hpp"
#include "tusb.h"

#ifndef HID_KEYPAD_0
#define HID_KEYPAD_0 HID_KEY_KEYPAD_0
#define HID_KEYPAD_1 HID_KEY_KEYPAD_1
#define HID_KEYPAD_2 HID_KEY_KEYPAD_2
#define HID_KEYPAD_3 HID_KEY_KEYPAD_3
#define HID_KEYPAD_4 HID_KEY_KEYPAD_4
#define HID_KEYPAD_5 HID_KEY_KEYPAD_5
#define HID_KEYPAD_6 HID_KEY_KEYPAD_6
#define HID_KEYPAD_7 HID_KEY_KEYPAD_7
#define HID_KEYPAD_8 HID_KEY_KEYPAD_8
#define HID_KEYPAD_9 HID_KEY_KEYPAD_9
#endif

#ifndef HID_KEY_LEFT_ARROW
#define HID_KEY_LEFT_ARROW  HID_KEY_ARROW_LEFT
#define HID_KEY_RIGHT_ARROW HID_KEY_ARROW_RIGHT
#define HID_KEY_UP_ARROW    HID_KEY_ARROW_UP
#define HID_KEY_DOWN_ARROW  HID_KEY_ARROW_DOWN
#endif
extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "freertos/queue.h"
}

#include <vector>
#include <tuple>

namespace {
  struct Msg { macros::Type type; String payload; };
  static TaskHandle_t s_macroTask = nullptr;
  static QueueHandle_t s_macroQ   = nullptr;

  void macro_task(void*) {
    for(;;){
      Msg* m = nullptr;
      if(xQueueReceive(s_macroQ, &m, portMAX_DELAY) == pdTRUE && m){
        // BLE ready guard — keeps logs you added earlier
        switch(m->type){
          case macros::Type::Keystroke: macros::run_keystroke(m->payload); break;
          case macros::Type::Typing:    macros::run_typing(m->payload);    break;
          case macros::Type::HoldSeq:   macros::run_holdseq(m->payload);   break;
          default:                      macros::run_keybind(m->payload);   break;
        }
        delete m;
      }
    }
  }

  void ensure_worker() {
    if(!s_macroQ)   s_macroQ = xQueueCreate(8, sizeof(Msg*));
    if(!s_macroTask) xTaskCreatePinnedToCore(macro_task, "macroTask", 4096, nullptr, 1, &s_macroTask, 0);
  }
}

static uint16_t parse_delay_token(const String& t){
  String s=t; s.trim(); s.toLowerCase();
  if(s.endsWith("ms")) return (uint16_t)s.substring(0,s.length()-2).toFloat();
  if(s.endsWith("s"))  return (uint16_t)(s.substring(0,s.length()-1).toFloat()*1000.0f);
  return (uint16_t)s.toFloat();
}
static bool is_all_digits(const String& s){
  if(!s.length()) return false;
  for(char c: s) if(c<'0'||c>'9') return false;
  return true;
}
static bool digits_commas_only(const String& s){
  for(char c: s){ if(!(c==' '||c==','||(c>='0'&&c<='9'))) return false; }
  return s.length()>0;
}
static String collapse_digits(const String& s){
  String out; out.reserve(s.length());
  for(char c: s){ if(c>='0'&&c<='9') out += c; }
  return out;
}
static uint8_t kp_digit(char c){
  static const uint8_t kp[10] = { HID_KEYPAD_0,HID_KEYPAD_1,HID_KEYPAD_2,HID_KEYPAD_3,HID_KEYPAD_4,
                                  HID_KEYPAD_5,HID_KEYPAD_6,HID_KEYPAD_7,HID_KEYPAD_8,HID_KEYPAD_9 };
  return kp[c-'0'];
}
static uint8_t mod_from_token(const String& t){
  String s=t; s.toLowerCase();
  if(s=="lalt"||s=="alt")  return KEYBOARD_MODIFIER_LEFTALT;
  if(s=="ralt")            return KEYBOARD_MODIFIER_RIGHTALT;
  if(s=="lctrl"||s=="ctrl")return KEYBOARD_MODIFIER_LEFTCTRL;
  if(s=="rctrl")           return KEYBOARD_MODIFIER_RIGHTCTRL;
  if(s=="lshift"||s=="shift") return KEYBOARD_MODIFIER_LEFTSHIFT;
  if(s=="rshift")          return KEYBOARD_MODIFIER_RIGHTSHIFT;
  if(s=="lgui"||s=="win"||s=="cmd") return KEYBOARD_MODIFIER_LEFTGUI;
  if(s=="rgui")            return KEYBOARD_MODIFIER_RIGHTGUI;
  return 0;
}
static uint8_t parse_single_key(const String& tok){
  // very small mapper for letters, digits row, function keys, Tab, Enter, etc.
  String s=tok; s.trim();
  if(s.length()==1){
    char c=s[0];
    if(c>='a'&&c<='z') return HID_KEY_A + (c-'a');
    if(c>='A'&&c<='Z') return HID_KEY_A + (c-'A');
    if(c>='0'&&c<='9'){ static const uint8_t row[]={HID_KEY_0,HID_KEY_1,HID_KEY_2,HID_KEY_3,HID_KEY_4,HID_KEY_5,HID_KEY_6,HID_KEY_7,HID_KEY_8,HID_KEY_9}; return row[c-'0']; }
  }
  if(s.length()>=2 && (s[0]=='F'||s[0]=='f')){
    int n=s.substring(1).toInt(); if(n>=1&&n<=24) return HID_KEY_F1+(n-1);
  }
  String l=s; l.toLowerCase();
  if(l=="tab") return HID_KEY_TAB;
  if(l=="enter"||l=="return") return HID_KEY_ENTER;
  if(l=="esc"||l=="escape") return HID_KEY_ESCAPE;
  if(l=="space"||l=="spacebar") return HID_KEY_SPACE;
  if(l=="backspace") return HID_KEY_BACKSPACE;
  if(l=="delete"||l=="del") return HID_KEY_DELETE;
  if(l=="home") return HID_KEY_HOME;
  if(l=="end")  return HID_KEY_END;
  if(l=="pgup") return HID_KEY_PAGE_UP;
  if(l=="pgdn"||l=="pagedown") return HID_KEY_PAGE_DOWN;
  if(l=="left") return HID_KEY_LEFT_ARROW;
  if(l=="right")return HID_KEY_RIGHT_ARROW;
  if(l=="up")   return HID_KEY_UP_ARROW;
  if(l=="down") return HID_KEY_DOWN_ARROW;
  return 0;
}

namespace macros {

void begin_async(){ ensure_worker(); }
void enqueue(Type type, const String& payload){
  ensure_worker();
  Msg* m = new Msg{type, payload};
  xQueueSend(s_macroQ, &m, 0);
}

using knomi::press_release;
using knomi::send_vk;

namespace {

// Parse a wait token like "500ms" or "2s" into milliseconds.
bool parse_wait_token(const String& token, uint16_t& ms) {
  String t = token; t.trim(); t.toUpperCase();
  if (t.endsWith("MS")) {
    String num = t.substring(0, t.length() - 2);
    num.trim();
    if (num.length() == 0) return false;
    long val = 0;
    for (size_t i = 0; i < num.length(); ++i) {
      if (!isDigit(num[i])) return false;
      val = val * 10 + (num[i] - '0');
    }
    ms = static_cast<uint16_t>(val);
    return true;
  }
  if (t.endsWith("S")) {
    String num = t.substring(0, t.length() - 1);
    num.trim();
    if (num.length() == 0) return false;
    long val = 0;
    for (size_t i = 0; i < num.length(); ++i) {
      if (!isDigit(num[i])) return false;
      val = val * 10 + (num[i] - '0');
    }
    ms = static_cast<uint16_t>(val * 1000);
    return true;
  }
  return false;
}

// Parse a token like "LCTRL+LALT+TAB" into keycode and modifiers.
bool parse_combo(const String& token, uint8_t& key, uint8_t& mods) {
  key = 0; mods = 0;
  int start = 0;
  while (true) {
    int plus = token.indexOf('+', start);
    String part = (plus == -1) ? token.substring(start) : token.substring(start, plus);
    part.trim();
    if (part.length() == 0) return false;
    auto km = knomi::ble_map_token(part);
    if (km.first == 0 && km.second == 0) return false;
    if (km.first != 0) {
      if (key != 0) return false; // multiple keys
      key = km.first;
    }
    mods |= km.second;
    if (plus == -1) break;
    start = plus + 1;
  }
  return key != 0;
}

// Helper for typing parsing that returns cleaned text.
bool parse_typing_impl(const String& text, String& clean, uint8_t& cps) {
  String s = text; s.trim();
  cps = 10;
  int open = s.lastIndexOf('(');
  if (open != -1) {
    int close = s.indexOf(')', open);
    if (close == -1) return false;
    String inside = s.substring(open + 1, close);
    inside.trim();
    inside.toUpperCase();
    if (!inside.endsWith("/S")) return false;
    String num = inside.substring(0, inside.length() - 2);
    num.trim();
    if (num.length() == 0) return false;
    long val = 0;
    for (size_t i = 0; i < num.length(); ++i) {
      if (!isDigit(num[i])) return false;
      val = val * 10 + (num[i] - '0');
    }
    if (val < 1 || val > 20) return false;
    cps = static_cast<uint8_t>(val);
    String tail = s.substring(close + 1);
    tail.trim();
    if (tail.length() != 0) return false;
    clean = s.substring(0, open);
    clean.trim();
    return true;
  }
  clean = s;
  return true;
}

// Internal keystroke parser optionally building steps.
bool parse_keystroke_impl(const String& text, std::vector<knomi::KeyStep>* steps) {
  int start = 0;
  while (true) {
    int comma = text.indexOf(',', start);
    String tok = (comma == -1) ? text.substring(start) : text.substring(start, comma);
    tok.trim();
    if (tok.length() == 0) return false;
    uint16_t waitms = 0; uint8_t key = 0, mods = 0;
    if (parse_wait_token(tok, waitms)) {
      if (steps) {
        if (steps->empty()) {
          steps->push_back({0,0,waitms});
        } else {
          auto& last = steps->back();
          std::get<2>(last) += waitms;
        }
      }
    } else {
      if (!parse_combo(tok, key, mods)) return false;
      if (steps) steps->push_back({key, mods, 0});
    }
    if (comma == -1) break;
    start = comma + 1;
  }
  return true;
}

} // anon

bool parse_keystroke(const String& text) {
  return parse_keystroke_impl(text, nullptr);
}

bool parse_typing(const String& text, uint8_t& cps) {
  String clean; return parse_typing_impl(text, clean, cps);
}

bool parse_keybind(const String& text) {
  uint8_t key, mods; return parse_combo(text, key, mods);
}

void run_keystroke(const String& text) {
  if(!knomi::ble_ready()){
    Serial.println("[MACRO] BLE not ready (not connected or not subscribed) — ignoring tap");
    return;
  }
  std::vector<knomi::KeyStep> steps;
  if (!parse_keystroke_impl(text, &steps)) {
    Serial.println("Invalid keystroke");
    return;
  }
  knomi::ble_run_sequence(steps);
}

void run_typing(const String& text) {
  if(!knomi::ble_ready()){
    Serial.println("[MACRO] BLE not ready (not connected or not subscribed) — ignoring tap");
    return;
  }
  String clean; uint8_t cps;
  if (!parse_typing_impl(text, clean, cps)) {
    Serial.println("Invalid typing");
    return;
  }
  knomi::ble_type_text(clean, cps);
}

void run_holdseq(const String& payload){
  if(!knomi::ble_ready()){ Serial.println("[MACRO] BLE not ready"); return; }

  int bar = payload.indexOf('|');
  if(bar<0){ Serial.println("[MACRO] holdseq missing '|'"); return; }

  String hold = payload.substring(0,bar);
  String seq  = payload.substring(bar+1);

  uint8_t holdMods = 0;
  {
    int start=0;
    while(true){
      int plus = hold.indexOf('+', start);
      String tok = hold.substring(start, plus<0?hold.length():plus); tok.trim();
      uint8_t m = mod_from_token(tok);
      if(!m){ Serial.printf("[MACRO] hold '%s' not a modifier; ignoring.\n", tok.c_str()); }
      else holdMods |= m;
      if(plus<0) break; start = plus+1;
    }
    if(!holdMods){ Serial.println("[MACRO] holdseq has no valid modifier"); return; }
  }

  bool altLike = (holdMods & (KEYBOARD_MODIFIER_LEFTALT|KEYBOARD_MODIFIER_RIGHTALT)) != 0;
  // ALT-CODE FAST PATH: whole right side is a number (digits/commas/spaces only)
  if (altLike && digits_commas_only(seq)) {
    String digits = collapse_digits(seq);    // e.g., "0,1,7,9" -> "0179"
    if (digits.length()) {
      // Ensure NumLock during entry; restore afterward
      bool restoreNum = false;
      if (!knomi::numlock_on()) {
        press_release(HID_KEY_NUM_LOCK, 0, 12);
        restoreNum = true;
        delay(20);
      }

      // Build keypad digit array
      std::vector<uint8_t> kp; kp.reserve(digits.length());
      for (char d : digits) kp.push_back(kp_digit(d));

      // Hold Alt the whole time and emit digits RAW (no stray reports)
      uint8_t altMods = holdMods; // already LAlt/RAlt (or chord)
      bool ok = knomi::emit_altcode_digits(kp.data(), kp.size(), altMods);

      // Restore NumLock if we toggled it
      if (restoreNum) { delay(20); press_release(HID_KEY_NUM_LOCK, 0, 12); }

      if (ok) return;  // done
      // if not ok, fall through to generic path (unlikely)
    }
  }

  bool restoreNum = false;
  if(altLike && !knomi::numlock_on()){
    press_release(HID_KEY_NUM_LOCK, 0, 12); restoreNum = true; delay(20);
  }

  send_vk(0, true, holdMods); delay(6);

  int start=0;
  while(true){
    int comma = seq.indexOf(',', start);
    String t = seq.substring(start, comma<0?seq.length():comma); t.trim();
    if(t.length()){
      if(t.endsWith("ms") || t.endsWith("s")){
        delay(parse_delay_token(t));
      } else if(altLike && is_all_digits(t)){
        for(char c: t){ press_release(kp_digit(c), holdMods, 8); }
      } else {
        int plus = t.indexOf('+');
        uint8_t extraMods = 0; uint8_t key = 0;
        if(plus>0){
          String lm = t.substring(0, plus); lm.trim();
          extraMods = mod_from_token(lm);
          key = parse_single_key(t.substring(plus+1));
        }else{
          key = parse_single_key(t);
        }
        if(key) press_release(key, holdMods | extraMods, 8);
        else Serial.printf("[MACRO] holdseq unknown token '%s'\n", t.c_str());
      }
    }
    if(comma<0) break; start = comma+1;
  }

  send_vk(0, false, holdMods); delay(6);

  if(restoreNum){ delay(20); press_release(HID_KEY_NUM_LOCK, 0, 12); }
}

void run_keybind(const String& text) {
  if(!knomi::ble_ready()){
    Serial.println("[MACRO] BLE not ready (not connected or not subscribed) — ignoring tap");
    return;
  }
  uint8_t key, mods;
  if (!parse_combo(text, key, mods)) {
    Serial.println("Invalid keybind");
    return;
  }
  knomi::ble_press_release(key, mods, 15);
}

} // namespace macros

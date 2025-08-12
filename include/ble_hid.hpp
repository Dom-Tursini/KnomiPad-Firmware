#pragma once
#include <Arduino.h>
#include <vector>
#include <utility>

namespace knomi {

bool ble_begin_keyboard(const char* deviceName = "KnomiPad");
bool ble_is_connected();
bool ble_ready();   // returns true only if connected + input CCCD subscribed
bool numlock_on();
void set_mods(uint8_t mods);
void release_all();
bool emit_altcode_digits(const uint8_t* keys, size_t n, uint8_t mods);
void ble_disconnect();

// Press a combo like LCtrl + 'c' (modifiers are a bitmask)
// HID usage codes follow standard USB HID (Keyboard/Keypad Page).
void ble_press_release(uint8_t keycode, uint8_t modifiers = 0, uint16_t hold_ms = 10);

// Type ASCII text at a given cps (chars per second). Limit to 7-bit ASCII for now.
void ble_type_text(const String& text, uint8_t cps = 10);

// Run a sequence of (keycode, modifiers, wait_ms_after) triples.
using KeyStep = std::tuple<uint8_t,uint8_t,uint16_t>;
void ble_run_sequence(const std::vector<KeyStep>& steps);

// Convenience mapping: returns (keycode, modifier_mask).
// Supports: A-Z, a-z, 0-9, F1-F12, Enter, Esc, Tab, Space, Minus, Equal, LCtrl,LShift,LAlt,LGUI.
// Returns {0,0} if unknown.
std::pair<uint8_t,uint8_t> ble_map_token(const String& token);

void send_vk(uint8_t key, bool down, uint8_t mods = 0);
void press_release(uint8_t key, uint8_t mods = 0, uint16_t d_ms = 10);

} // namespace knomi

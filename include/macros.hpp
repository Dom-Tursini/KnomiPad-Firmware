#pragma once
#include <Arduino.h>

namespace macros {

enum class Type { Keystroke, Typing, Keybind, HoldSeq };

struct Slot {
  uint8_t id;
  String title;
  Type type;
  String payload;   // raw string as user provided
  uint32_t bg;      // ARGB or RGB hex
  uint32_t bg2 = 0; // secondary color for gradient
  bool gradient = false;
  String iconPath;  // e.g., /icons/1.svg
};

bool parse_keystroke(const String& text); // validates only
bool parse_typing(const String& text, uint8_t& cps);
bool parse_keybind(const String& text);

void run_keystroke(const String& text);
void run_typing(const String& text);
void run_keybind(const String& text);
void run_holdseq(const String& payload);

void begin_async();
void enqueue(Type type, const String& payload);

inline const char* type_to_string(Type t){
  switch(t){
    case Type::Keystroke: return "keystroke";
    case Type::Typing:    return "typing";
    case Type::Keybind:   return "keybind";
    case Type::HoldSeq:   return "holdseq";
  }
  return "keystroke";
}

inline Type type_from_string(const String& s){
  String a=s; a.toLowerCase();
  if(a=="typing") return Type::Typing;
  if(a=="keybind") return Type::Keybind;
  if(a=="holdseq") return Type::HoldSeq;
  return Type::Keystroke;
}

} // namespace macros

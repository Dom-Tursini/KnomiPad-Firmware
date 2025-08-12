#pragma once
#include <Arduino.h>
#include <vector>
#include "macros.hpp"

namespace storage {

struct Profile {
  std::vector<macros::Slot> slots;
};

bool begin();             // Mount FS
bool load(Profile& out);  // Load from /macros/slots.json (create defaults if missing)
bool save(const Profile& in);

Profile& profile();       // global profile in RAM
bool load();              // loads into global
bool save();              // saves global

// Helpers
macros::Slot* get_slot(uint8_t id);
bool set_slot(uint8_t id, const macros::Slot& s);
uint8_t count();

} // namespace storage

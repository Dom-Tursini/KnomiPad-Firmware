#pragma once
#include <Arduino.h>
namespace rtime {
  void begin();                       // keep NTP as fallback (optional)
  void loop();                        // no-op
  bool valid();                       // true if time is set
  void set_from_epoch_ms(uint64_t ms, int tz_offset_min); // host sync
  void format(char* out, size_t n, const char* timeFmt, const char* dateFmt);
}

#include "rtc_time.hpp"
#include <time.h>
#include <sys/time.h>

namespace {
  // Minutes EAST of UTC (e.g., South Africa = +120)
  int g_tz_min = 0;
}

void rtime::begin() {
  // Optional SNTP fallback (will update if network allows)
  configTime(0, 0, "pool.ntp.org", "time.windows.com", "time.nist.gov");
}

void rtime::loop(){}

bool rtime::valid() {
  time_t t; time(&t);
  return t > 100000; // ~1970-01-02
}

void rtime::set_from_epoch_ms(uint64_t ms, int tz_offset_min) {
  timeval tv;
  tv.tv_sec  = (time_t)(ms / 1000ULL);
  tv.tv_usec = (suseconds_t)((ms % 1000ULL) * 1000ULL);
  settimeofday(&tv, nullptr);
  g_tz_min = tz_offset_min;
}

void rtime::format(char* out, size_t n, const char* timeFmt, const char* dateFmt) {
  time_t t = time(nullptr);
  // Apply browser-provided offset without messing with global TZ
  t += g_tz_min * 60;
  struct tm tm; gmtime_r(&t, &tm);
  char tbuf[32], dbuf[48];
  strftime(tbuf, sizeof(tbuf), timeFmt, &tm);
  strftime(dbuf, sizeof(dbuf), dateFmt, &tm);
  snprintf(out, n, "%s\n%s", tbuf, dbuf);
}

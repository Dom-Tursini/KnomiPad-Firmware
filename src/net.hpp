#pragma once
#include <Arduino.h>
#include <functional>

namespace net {

enum class StaState { OFF, CONNECTING, GOT_IP, FAIL };

struct WifiCallbacks {
  std::function<void()> onApStarted;          // Captive portal up
  std::function<void()> onStaConnecting;      // Trying STA
  std::function<void()> onStaGotIp;           // STA OK
  std::function<void()> onStaFailed;          // STA failed (timeout)
};

bool begin(const char* apSsid, const char* apPass, uint32_t staTimeoutMs,
           const WifiCallbacks& cb);

// periodic processing (DNS captive portal, HTTP, etc.)
void loop();

StaState sta_state();
bool sta_ok();
bool ap_running();

// Save/clear creds (LittleFS paths: /config/wifi.json)
bool save_sta(const String& ssid, const String& pass);
bool clear_sta();

} // namespace net

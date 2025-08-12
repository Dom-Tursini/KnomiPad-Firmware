#include "net.hpp"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

namespace {
const byte DNS_PORT = 53;
DNSServer dnsServer;

String g_ssid, g_pass;
net::WifiCallbacks g_cb;
uint32_t g_deadline = 0;
net::StaState g_state = net::StaState::OFF;
bool g_ap = false;

bool load_sta() {
  if (!LittleFS.begin(true)) return false;
  if (!LittleFS.exists("/config/wifi.json")) return false;
  File f = LittleFS.open("/config/wifi.json", "r");
  if (!f) return false;
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, f)) return false;
  g_ssid = String((const char*)doc["ssid"]);
  g_pass = String((const char*)doc["pass"]);
  return g_ssid.length() > 0;
}

void start_ap(const char* ssid, const char* pass) {
  WiFi.softAP(ssid, pass && strlen(pass)? pass : nullptr);
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  g_ap = true;
  if (g_cb.onApStarted) g_cb.onApStarted();
}

void start_sta() {
  if (g_ssid.isEmpty()) return;
  WiFi.begin(g_ssid.c_str(), g_pass.c_str());
  g_state = net::StaState::CONNECTING;
  if (g_cb.onStaConnecting) g_cb.onStaConnecting();
}

} // anon

namespace net {

bool save_sta(const String& ssid, const String& pass) {
  if (!LittleFS.begin(true)) return false;
  LittleFS.mkdir("/config");
  File f = LittleFS.open("/config/wifi.json", "w");
  if (!f) return false;
  StaticJsonDocument<256> doc;
  doc["ssid"] = ssid;
  doc["pass"] = pass;
  serializeJson(doc, f);
  return true;
}

bool clear_sta() {
  if (!LittleFS.begin(true)) return false;
  if (LittleFS.exists("/config/wifi.json")) LittleFS.remove("/config/wifi.json");
  return true;
}

bool begin(const char* apSsid, const char* apPass, uint32_t staTimeoutMs,
           const WifiCallbacks& cb) {
  g_cb = cb;
  WiFi.mode(WIFI_AP_STA);
  start_ap(apSsid, apPass);

  // Load STA creds; try to connect if present.
  if (load_sta()) {
    start_sta();
    g_deadline = millis() + staTimeoutMs;
  }
  // Always start mDNS for knomipad.local
  MDNS.begin("knomipad");
  return true;
}

void loop() {
  dnsServer.processNextRequest();

  if (g_state == StaState::CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      g_state = StaState::GOT_IP;
      if (g_cb.onStaGotIp) g_cb.onStaGotIp();
      // keep AP for a short grace so clients don't drop; you can stop it later from app code if desired
    } else if ((int32_t)(millis() - g_deadline) > 0) {
      g_state = StaState::FAIL;
      if (g_cb.onStaFailed) g_cb.onStaFailed();
      // leave AP running for recovery
    }
  }
}

StaState sta_state() { return g_state; }
bool sta_ok() { return g_state == StaState::GOT_IP; }
bool ap_running() { return g_ap; }

} // namespace net

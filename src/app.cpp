#include "app.hpp"
#include <Arduino.h>
#include <lvgl.h>
#include <LittleFS.h>
#include <WiFi.h>
extern "C" {
  #include "esp_system.h"
  #include "esp_wifi.h"
  #if __has_include("esp_coexist.h")
  #include "esp_coexist.h"
  #endif
}
#include "storage.hpp"
#include "ui.hpp"
#include "net.hpp"
#include "web.hpp"
#include "ble_hid.hpp"
#include "macros.hpp"
#include "rtc_time.hpp"

static uint32_t t_ble = 0;
static uint32_t _hb_last = 0;

static void onAp()      { /* could show toast later */ }
static void onStaTry()  { /* optional */ }
static void onStaOK()   { ui::wifi_ok(); }
static void onStaFail() { ui::wifi_failed(); }

void app_begin() {
  Serial.begin(115200);
  delay(50);
  Serial.printf("[BOOT] reset_reason=%d\n", (int)esp_reset_reason());
  // REQUIRED by ESP-IDF when Wi-Fi + BLE coexist: enable modem sleep
  WiFi.persistent(false);
  WiFi.setSleep(true);
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  Serial.println("[BOOT] WiFi modem sleep ENABLED (MIN_MODEM) for BLE coexist");

#if defined(ESP_COEX_PREFER_BT) || defined(CONFIG_BT_COEXIST)
  esp_coex_preference_set(ESP_COEX_PREFER_BT);
  Serial.println("[BOOT] Coexistence preference: BT");
#endif

  Serial.printf("[BOOT] LV_USE_PNG=%d LV_USE_SVG=%d\n",(int)LV_USE_PNG,(int)LV_USE_SVG);
  LittleFS.begin(true);
  storage::load();
  macros::begin_async();

  knomi::ble_begin_keyboard("KnomiPad");

  net::WifiCallbacks cb{ onAp, onStaTry, onStaOK, onStaFail };
  net::begin("Basilisk KnomiPad", "", 15000, cb);

  web::begin();    // http api/ui
  rtime::begin();
  ui::begin();     // lvgl screens
}

void app_loop() {
  net::loop();
  web::loop();
  rtime::loop();
  // BLE pill refresh every 500ms
  if (millis() - t_ble > 500) {
    ui::notify_ble(knomi::ble_is_connected());
    t_ble = millis();
  }
  if (millis() - _hb_last > 1000) {
    _hb_last = millis();
    Serial.printf("[HB] up=%lu ms, BLE=%d, STA_OK=%d, IP=%s\n",
                  (unsigned long)millis(),
                  (int)knomi::ble_is_connected(),
                  (int)net::sta_ok(),
                  WiFi.localIP().toString().c_str());
  }
}

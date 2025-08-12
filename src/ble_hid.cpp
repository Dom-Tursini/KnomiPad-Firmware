#include "ble_hid.hpp"
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <NimBLEServer.h>

static int gapHandler(struct ble_gap_event *event, void *arg);

extern "C" void delayMicroseconds(uint32_t us);  // Arduino core

namespace {
NimBLEServer* g_server = nullptr;
NimBLEHIDDevice* g_hid = nullptr;
NimBLECharacteristic* g_input = nullptr;
static NimBLECharacteristic* g_output = nullptr;
bool g_connected = false;
static uint8_t g_ledState = 0;

static inline bool can_notify() {
  return g_connected && g_input && (g_input->getSubscribedCount() > 0);
}

static inline void send_report_raw(uint8_t mods, uint8_t key) {
  if (!can_notify()) return;
  uint8_t r[8] = {mods, 0, key, 0, 0, 0, 0, 0};
  g_input->setValue(r, sizeof(r));
  g_input->notify();
}

const uint8_t REPORT_ID = 1;

// Very small US keyboard report map (keyboard only).
static const uint8_t REPORT_MAP[] = {
  0x05, 0x01,       // USAGE_PAGE (Generic Desktop)
  0x09, 0x06,       // USAGE (Keyboard)
  0xA1, 0x01,       // COLLECTION (Application)
  0x85, REPORT_ID,  //   REPORT_ID (1)
  0x05, 0x07,       //   USAGE_PAGE (Keyboard/Keypad)
  0x19, 0xE0,       //   USAGE_MINIMUM (Keyboard LeftControl)
  0x29, 0xE7,       //   USAGE_MAXIMUM (Keyboard Right GUI)
  0x15, 0x00,       //   LOGICAL_MINIMUM (0)
  0x25, 0x01,       //   LOGICAL_MAXIMUM (1)
  0x75, 0x01,       //   REPORT_SIZE (1)
  0x95, 0x08,       //   REPORT_COUNT (8)
  0x81, 0x02,       //   INPUT (Data,Var,Abs) ; Modifiers
  0x95, 0x01,       //   REPORT_COUNT (1)
  0x75, 0x08,       //   REPORT_SIZE (8)
  0x81, 0x03,       //   INPUT (Cnst,Var,Abs) ; Reserved
  0x95, 0x05,       //   REPORT_COUNT (5)
  0x75, 0x01,       //   REPORT_SIZE (1)
  0x05, 0x08,       //   USAGE_PAGE (LEDs)
  0x19, 0x01,       //   USAGE_MINIMUM (Num Lock)
  0x29, 0x05,       //   USAGE_MAXIMUM (Kana)
  0x91, 0x02,       //   OUTPUT (Data,Var,Abs)
  0x95, 0x01,       //   REPORT_COUNT (1)
  0x75, 0x03,       //   REPORT_SIZE (3)
  0x91, 0x03,       //   OUTPUT (Cnst,Var,Abs)
  0x95, 0x06,       //   REPORT_COUNT (6)
  0x75, 0x08,       //   REPORT_SIZE (8)
  0x15, 0x00,       //   LOGICAL_MINIMUM (0)
  0x25, 0x73,       //   LOGICAL_MAXIMUM (115)
  0x05, 0x07,       //   USAGE_PAGE (Keyboard/Keypad)
  0x19, 0x00,       //   USAGE_MINIMUM (Reserved (no event) 0)
  0x29, 0x73,       //   USAGE_MAXIMUM (Keyboard Application)
  0x81, 0x00,       //   INPUT (Data,Ary,Abs) ; 6-key rollover
  0xC0              // END_COLLECTION
};

struct ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*) override { g_connected = true; }
  void onDisconnect(NimBLEServer*) override {
    g_connected = false;
    Serial.println("[BLE] disconnect; re-adv in 50ms");
    delay(50);
    if (g_server) g_server->startAdvertising();
  }
};

} // anon

namespace knomi {
void set_mods(uint8_t mods) {
  send_report_raw(mods, 0);
  delayMicroseconds(800);  // ~0.8ms pacing
}

void release_all() {
  send_report_raw(0, 0);
  delayMicroseconds(800);
}

static inline void tap_kp_digit_raw(uint8_t mods, uint8_t key) {
  send_report_raw(mods, key);
  delayMicroseconds(900);
  send_report_raw(mods, 0);
  delayMicroseconds(900);
}

bool emit_altcode_digits(const uint8_t* keys, size_t n, uint8_t mods) {
  if (!can_notify() || !keys || n==0) return false;
  set_mods(mods);
  for (size_t i=0;i<n;++i) tap_kp_digit_raw(mods, keys[i]);
  release_all();
  return true;
}

bool ble_ready() {
  return can_notify();
}

enum Mod {
  MOD_LCTRL = 0x01,
  MOD_LSHFT = 0x02,
  MOD_LALT  = 0x04,
  MOD_LGUI  = 0x08,
};

static void send_report(uint8_t mods, uint8_t k0) {
  if (!can_notify()) { Serial.println("[BLE] skip notify (not subscribed)"); return; }
  uint8_t report[8] = {mods,0x00,k0,0x00,0x00,0x00,0x00,0x00};
  g_input->setValue(report, sizeof(report));
  g_input->notify();
  delay(6);
}

bool ble_begin_keyboard(const char* deviceName) {
  NimBLEDevice::init(deviceName);
  // Use PUBLIC address (works across Windows/Android/Linux)
  NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_PUBLIC);

  // Bonded "Just Works" (no PIN); secure connections OK
  NimBLEDevice::setSecurityAuth(true /*bond*/, false /*mitm*/, true /*sc*/);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);

  // Conservative MTU + sane power
  NimBLEDevice::setMTU(69);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  NimBLEDevice::setCustomGapHandler(gapHandler);
  g_server = NimBLEDevice::createServer();
  g_server->setCallbacks(new ServerCallbacks());

  g_hid = new NimBLEHIDDevice(g_server);
  g_input  = g_hid->inputReport(REPORT_ID);
  g_output = g_hid->outputReport(REPORT_ID);
  g_hid->setBatteryLevel(100);

  // Log CCCD (subscribe) changes on input report
  struct InCb : public NimBLECharacteristicCallbacks {
    void onSubscribe(NimBLECharacteristic* c, ble_gap_conn_desc* desc, uint16_t subValue) override {
      Serial.printf("[BLE] input CCCD=0x%04X subscribed=%u conn=%u\n", subValue, (unsigned)(subValue!=0), desc? desc->conn_handle: 0xFFFF);
    }
  };
  if (g_input) g_input->setCallbacks(new InCb());

  // Accept LED writes (optional, but improves compatibility)
  if (g_output) {
    struct OutCb : public NimBLECharacteristicCallbacks {
      void onWrite(NimBLECharacteristic* c) override {
        auto v = c->getValue();
        if(v.length()) g_ledState = v[0];
      }
    };
    g_output->setCallbacks(new OutCb());
  }

  g_hid->manufacturer()->setValue("Basilisk");
  // PnP: 0x01 = Bluetooth SIG assigned IDs, use Espressif VID 0x303A and a made-up PID 0x4002, version 0x0110
  g_hid->pnp(0x01, 0x303A, 0x4002, 0x0110);
  g_hid->hidInfo(0x00, 0x01);

  g_hid->reportMap((uint8_t*)REPORT_MAP, sizeof(REPORT_MAP));
  g_hid->startServices();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setAppearance(HID_KEYBOARD);
  adv->addServiceUUID(g_hid->hidService()->getUUID());
  adv->setScanResponse(true);
  adv->setName(deviceName);
  bool ok = adv->start(0);  // 0 = advertise indefinitely
  Serial.printf("[BLE] addr=%s adv_start=%d\n",
                NimBLEDevice::getAddress().toString().c_str(), (int)ok);
  return ok;
}

bool ble_is_connected() { return g_connected; }
void ble_disconnect() { if (g_server) g_server->disconnect(NimBLEAddress()); }

bool numlock_on(){ return (g_ledState & 0x01) != 0; }

void send_vk(uint8_t key, bool down, uint8_t mods){
  send_report(down ? mods : 0, down ? key : 0);
}

void press_release(uint8_t key, uint8_t mods, uint16_t d_ms){
  send_vk(key, true, mods);
  delay(d_ms);
  send_vk(key, false, mods);
  delay(8);
}

void ble_press_release(uint8_t keycode, uint8_t mods, uint16_t hold_ms) {
  send_report(mods, keycode);
  delay(hold_ms);
  send_report(0x00, 0x00);
  delay(8);
}

static std::pair<uint8_t,uint8_t> map_char(char c) {
  // Basic US mapping
  if (c >= 'a' && c <= 'z') return { uint8_t(0x04 + (c-'a')), 0 };
  if (c >= 'A' && c <= 'Z') return { uint8_t(0x04 + (c-'A')), MOD_LSHFT };
  if (c >= '1' && c <= '9') return { uint8_t(0x1E + (c-'1')), 0 };
  if (c == '0') return { 0x27, 0 };
  if (c == ' ') return { 0x2C, 0 };
  if (c == '\n') return { 0x28, 0 };
  if (c == '-') return { 0x2D, 0 };
  if (c == '=') return { 0x2E, 0 };
  if (c == '\t') return { 0x2B, 0 };
  return {0,0};
}

void ble_type_text(const String& text, uint8_t cps) {
  if (!can_notify()) { Serial.println("[BLE] type ignored (not subscribed)"); return; }
  if (cps == 0) cps = 1;
  uint16_t d = 1000 / cps;
  for (size_t i=0;i<text.length();++i) {
    auto kc = map_char(text[i]);
    if (kc.first == 0) continue;
    ble_press_release(kc.first, kc.second, 5);
    delay(d);
  }
}

void ble_run_sequence(const std::vector<KeyStep>& steps) {
  if (!can_notify()) { Serial.println("[BLE] run ignored (not subscribed)"); return; }
  for (auto& s : steps) {
    uint8_t key, mod; uint16_t waitms;
    std::tie(key,mod,waitms) = s;
    ble_press_release(key, mod, 15);
    if (waitms) delay(waitms);
  }
}

std::pair<uint8_t,uint8_t> ble_map_token(const String& t) {
  String token = t; token.trim();
  token.toUpperCase();
  if (token == "ENTER") return {0x28,0};
  if (token == "ESC" || token=="ESCAPE") return {0x29,0};
  if (token == "TAB") return {0x2B,0};
  if (token == "SPACE") return {0x2C,0};

  if (token == "LCTRL") return {0x00, MOD_LCTRL};
  if (token == "LSHIFT") return {0x00, MOD_LSHFT};
  if (token == "LALT") return {0x00, MOD_LALT};
  if (token == "LGUI" || token=="LWIN") return {0x00, MOD_LGUI};

  if (token.startsWith("F")) {
    int n = token.substring(1).toInt();
    if (n>=1 && n<=12) return {uint8_t(0x3A + (n-1)), 0};
  }
  if (token.length()==1) {
    char c = token[0];
    if (c>='A' && c<='Z') return { uint8_t(0x04 + (c-'A')), 0 };
    if (c>='0' && c<='9') {
      if (c=='0') return {0x27,0};
      return { uint8_t(0x1E + (c-'1')), 0 };
    }
  }
  return {0,0};
}

} // namespace knomi

static int gapHandler(struct ble_gap_event *event, void *arg) {
  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
      Serial.printf("[GAP] CONNECT status=%d handle=%d\n",
                    event->connect.status, event->connect.conn_handle);
      break;
    case BLE_GAP_EVENT_DISCONNECT:
      Serial.printf("[GAP] DISCONNECT reason=0x%02X handle=%d\n",
                    event->disconnect.reason, event->disconnect.conn.conn_handle);
      break;
    case BLE_GAP_EVENT_CONN_UPDATE: {
      struct ble_gap_conn_desc desc;
      if (ble_gap_conn_find(event->conn_update.conn_handle, &desc) == 0) {
        Serial.printf("[GAP] CONN_UPDATE itvl=%u latency=%u timeout=%u status=%d\n",
                      desc.conn_itvl, desc.conn_latency,
                      desc.supervision_timeout, event->conn_update.status);
      } else {
        Serial.printf("[GAP] CONN_UPDATE status=%d\n", event->conn_update.status);
      }
      break;
    }
    case BLE_GAP_EVENT_SUBSCRIBE:
      Serial.printf("[GAP] SUBSCRIBE attr=%u reason=%u prev=%u cur=%u\n",
                    event->subscribe.attr_handle, event->subscribe.reason,
                    event->subscribe.prev_notify, event->subscribe.cur_notify);
      break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
      Serial.printf("[GAP] ADV_COMPLETE reason=%d\n", event->adv_complete.reason);
      break;
    default: break;
  }
  return 0;
}

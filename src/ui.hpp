#pragma once
#include <Arduino.h>

namespace ui {
void begin();                // build screens
void show(uint8_t index);    // show slot index
void notify_ble(bool on);    // status pill
void wifi_failed();         // show STA failure dialog
void wifi_ok();             // hide dialog if shown
uint8_t current_index();    // expose current index
} // namespace ui

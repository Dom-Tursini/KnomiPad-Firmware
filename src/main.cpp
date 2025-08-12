#include <Arduino.h>
#include "lvgl_hal.h"
#include "app.hpp"

void setup() {
  Serial.begin(115200);
  delay(25);
  Serial.println(">>> MODULAR MAIN ACTIVE: lvgl_hal_init + app_begin <<<");
  lvgl_hal_init();
  app_begin();
}

void loop() {
  lv_timer_handler();
  app_loop();
  delay(0); // was 5
}

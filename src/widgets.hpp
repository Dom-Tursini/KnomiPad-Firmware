#pragma once
#include <lvgl.h>
#include "storage.hpp"
#include "rtc_time.hpp"
#include "macros.hpp"

namespace widgets {

struct Widget {
  virtual ~Widget() {}
  virtual lv_obj_t* root() = 0;         // container owned by the widget (child of UI content)
  virtual void show() = 0;              // unhide + refresh
  virtual void hide() = 0;              // hide
  virtual void tick(uint32_t /*ms*/) {} // called each second
  virtual void onTap() {}               // tap on page
};

Widget* createClock(lv_obj_t* parent);
Widget* createMacro(lv_obj_t* parent, macros::Slot* slot);

} // namespace widgets

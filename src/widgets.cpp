#include "widgets.hpp"
#include "ble_hid.hpp"
#include <time.h>

namespace {

struct ClockWidget : public widgets::Widget {
  lv_obj_t* cont{nullptr};
  lv_obj_t* timeLbl{nullptr};
  lv_obj_t* dateLbl{nullptr};
  bool blinkOn{true};
  time_t lastShown{0};

  ClockWidget(lv_obj_t* parent){
    cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(cont, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_border_width(cont, 0, 0);

    timeLbl = lv_label_create(cont);
    lv_label_set_text(timeLbl, "--:--");
#if LV_FONT_MONTSERRAT_48
    lv_obj_set_style_text_font(timeLbl, &lv_font_montserrat_48, 0);
#endif
    lv_obj_set_style_text_color(timeLbl, lv_color_white(), 0);
    lv_obj_set_style_text_letter_space(timeLbl, 1, 0);
    lv_obj_align(timeLbl, LV_ALIGN_CENTER, 0, -6);

    dateLbl = lv_label_create(cont);
    lv_label_set_text(dateLbl, "Sun, 01 Jan 1970");
#if LV_FONT_MONTSERRAT_22
    lv_obj_set_style_text_font(dateLbl, &lv_font_montserrat_22, 0);
#endif
    lv_obj_set_style_text_color(dateLbl, lv_color_white(), 0);
    lv_obj_align(dateLbl, LV_ALIGN_CENTER, 0, 36);
  }

  void refresh(){
    time_t now = time(nullptr);
    if(now != lastShown){
      lastShown = now;
      char buf[64];
      rtime::format(buf, sizeof(buf), "%H:%M", "%a, %d %b %Y");
      const char* nl = strchr(buf, '\n');
      String timeStr, dateStr;
      if(nl){ timeStr = String(buf, nl - buf); dateStr = String(nl + 1); }
      else  { timeStr = String(buf); dateStr = ""; }

      if(!blinkOn){
        int c = timeStr.indexOf(':');
        if(c >= 0) timeStr.setCharAt(c, ' ');
      }

      lv_label_set_text(timeLbl, timeStr.c_str());
      if(dateStr.length()) lv_label_set_text(dateLbl, dateStr.c_str());
    }else{
      const char* cur = lv_label_get_text(timeLbl);
      if(!cur) return;
      String timeStr(cur);
      if(!blinkOn){
        int c = timeStr.indexOf(':');
        if(c >= 0) timeStr.setCharAt(c, ' ');
      }else{
        int c = timeStr.indexOf(' ');
        if(c >= 0) timeStr.setCharAt(c, ':');
      }
      lv_label_set_text(timeLbl, timeStr.c_str());
    }
  }

  lv_obj_t* root() override { return cont; }
  void show() override { lv_obj_clear_flag(cont, LV_OBJ_FLAG_HIDDEN); refresh(); }
  void hide() override { lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN); }
  void tick(uint32_t) override {
    // Toggle colon each tick (we’re called every 500 ms)
    blinkOn = !blinkOn;
    refresh();
  }
};

struct MacroWidget : public widgets::Widget {
  lv_obj_t* cont{nullptr};
  lv_obj_t* img{nullptr};
  lv_obj_t* tapDot { nullptr };
  macros::Slot* slot{nullptr};
  bool pressed = false;
  bool moved   = false;
  lv_point_t press_pt{};
  uint32_t press_tick = 0;
  uint32_t swipe_lock_until = 0;
  uint16_t baseZoom = 256;
  uint16_t curZoom  = 256;

  MacroWidget(lv_obj_t* parent, macros::Slot* s): slot(s){
    cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_border_width(cont, 0, 0);
    tapDot = lv_obj_create(cont);
    lv_obj_set_size(tapDot, 14, 14);
    lv_obj_set_style_radius(tapDot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(tapDot, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(tapDot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tapDot, 0, 0);
    lv_obj_add_flag(tapDot, LV_OBJ_FLAG_HIDDEN);
    // position at top-center, 10px down
    lv_obj_align(tapDot, LV_ALIGN_TOP_MID, 0, 10);

    img = lv_img_create(cont);

    lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(cont, [](lv_event_t* e){
      auto self = (MacroWidget*)lv_event_get_user_data(e);
      auto code = lv_event_get_code(e);

      // Helper lambdas
      auto showDot = [&](){
        lv_obj_clear_flag(self->tapDot, LV_OBJ_FLAG_HIDDEN);
      };
      auto hideDot = [&](){
        lv_obj_add_flag(self->tapDot, LV_OBJ_FLAG_HIDDEN);
      };

      // Treat any gesture as a swipe; lock out taps briefly
      if(code == LV_EVENT_GESTURE){
        self->moved = true;
        self->swipe_lock_until = lv_tick_get() + 250; // 250ms lockout after swipe
        hideDot();
        return;
      }

      if(code == LV_EVENT_PRESSED){
        self->pressed = true; self->moved = false;
        self->press_tick = lv_tick_get();
        lv_indev_get_point(lv_indev_get_act(), &self->press_pt);
        showDot();               // instant visual on press
        return;
      }

      if(code == LV_EVENT_PRESSING){
        lv_point_t p; lv_indev_get_point(lv_indev_get_act(), &p);
        const int dx = p.x - self->press_pt.x;
        const int dy = p.y - self->press_pt.y;
        // Early, strict swipe detection:
        // if finger moves >10px OR horizontal bias (|dx| > |dy|+4) with >8px, cancel tap
        if (LV_ABS(dx) > 10 || LV_ABS(dy) > 10 || (LV_ABS(dx) > LV_ABS(dy)+4 && LV_ABS(dx) > 8)) {
          self->moved = true;
          self->swipe_lock_until = lv_tick_get() + 250;
          hideDot();
        }
        return;
      }

      if(code == LV_EVENT_RELEASED){
        uint32_t now = lv_tick_get();
        // If we just swiped or moved too much, or within lockout, ignore tap
        if(now < self->swipe_lock_until || self->moved){
          self->pressed=false; self->moved=false; hideDot(); return;
        }
        // Valid quick tap (avoid long holds >700ms)
        if(self->pressed && (now - self->press_tick) < 700){
          hideDot();
          self->onTap();   // enqueue macro (async)
        } else {
          hideDot();
        }
        self->pressed=false; self->moved=false;
        return;
      }
    }, LV_EVENT_ALL, this);

    // Start with sane zoom and center
    lv_img_set_zoom(img, baseZoom);
    lv_obj_center(img);

    applyStyle();
    applyIcon();
  }

  void applyStyle(){
    // Always gradient; if bg==bg2, looks solid
    lv_obj_set_style_bg_color(cont, lv_color_make((slot->bg>>16)&255,(slot->bg>>8)&255,slot->bg&255), 0);
    if(slot->bg2 != slot->bg){
      lv_obj_set_style_bg_grad_dir(cont, LV_GRAD_DIR_VER, 0);
      lv_color_t c2 = lv_color_make((slot->bg2>>16)&255,(slot->bg2>>8)&255,slot->bg2&255);
      lv_obj_set_style_bg_grad_color(cont, c2, 0);
    }else{
      lv_obj_set_style_bg_grad_dir(cont, LV_GRAD_DIR_NONE, 0);
    }
  }

  void applyIcon(){
    if(slot->iconPath.length()){
      String p = slot->iconPath; if (p.startsWith("/")) p.remove(0,1);
      String fsPath = String("L:") + p;

      // Get real image size from decoder (so pivot centers correctly)
      lv_img_header_t hdr;
      if(lv_img_decoder_get_info(fsPath.c_str(), &hdr) == LV_RES_OK){
        lv_img_set_pivot(img, hdr.w / 2, hdr.h / 2);
      } else {
        lv_img_set_pivot(img, 0, 0);
      }

      lv_img_set_src(img, fsPath.c_str());

      // Auto-scale to fit inside the round screen with margin
      const int pw = lv_obj_get_width(cont);
      const int ph = lv_obj_get_height(cont);
      const int diameter = (pw < ph ? pw : ph);
      const int target   = LV_MAX(60, diameter - 40); // 20px margin all around

      int iw = (int)lv_obj_get_width(img);
      int ih = (int)lv_obj_get_height(img);
      if (iw <= 0 || ih <= 0) { iw = hdr.w; ih = hdr.h; }
      if (iw <= 0 || ih <= 0) { iw = ih = 200; } // fallback

      const float factor = (float)target / (float)LV_MAX(iw, ih);
      uint16_t z = (uint16_t)LV_CLAMP(64, (int)(256.0f * factor), 512);
      lv_img_set_zoom(img, z);
      baseZoom = curZoom = z;

      lv_obj_center(img);
    } else {
      lv_img_set_src(img, NULL);
    }
  }


  lv_obj_t* root() override { return cont; }
  void show() override { lv_obj_clear_flag(cont, LV_OBJ_FLAG_HIDDEN); applyStyle(); applyIcon(); lv_obj_center(img); }
  void hide() override { lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN); }
  void onTap() override {
    Serial.println("[UI] tap on MacroWidget");
    if(!slot) return;
    macros::enqueue(slot->type, slot->payload);   // run in background task
  }
};

} // anon

namespace widgets {

Widget* createClock(lv_obj_t* parent){ return new ClockWidget(parent); }
Widget* createMacro(lv_obj_t* parent, macros::Slot* slot){ return new MacroWidget(parent, slot); }

} // namespace widgets

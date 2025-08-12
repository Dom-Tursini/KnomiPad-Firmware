#include "ui.hpp"
#include <lvgl.h>
#include "widgets.hpp"
#include "fs_lvgl.hpp"
#include <LittleFS.h>
#include <vector>

namespace {
static lv_obj_t* scr;
static lv_obj_t* content;
static std::vector<widgets::Widget*> g_widgets;
static uint8_t cur = 0;
static lv_point_t g_press_pt; static bool g_moved=false;
static lv_timer_t* g_tickTimer = nullptr;
static lv_obj_t* wifiDlg = nullptr;

static uint8_t total_pages(){ return storage::count() + 1; } // 0=Clock, 1..N=Macros
}

namespace ui {

void begin() {
  lv_fs_littlefs_init();

  // Make display background pure black (covers any uncovered pixels)
  lv_disp_t* disp = lv_disp_get_default();
  if (disp) {
    lv_disp_set_bg_color(disp, lv_color_black());
    lv_disp_set_bg_opa(disp, LV_OPA_COVER);
  }

  scr = lv_obj_create(NULL);
  lv_obj_set_size(scr, LV_HOR_RES, LV_VER_RES);
  lv_obj_set_style_pad_all(scr, 0, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(scr, 0, 0);

  content = lv_obj_create(scr);
  lv_obj_set_size(content, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_all(content, 0, 0);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(content, 0, 0);

  // Build widgets: index 0 = Clock
  g_widgets.clear();
  g_widgets.push_back(widgets::createClock(content));
  // then each macro slot
  for (uint8_t i=0; i<storage::count(); ++i) {
    g_widgets.push_back(widgets::createMacro(content, storage::get_slot(i)));
  }
  // Hide all except current
  for (size_t i=0;i<g_widgets.size();++i) {
    if(i==cur) g_widgets[i]->show(); else g_widgets[i]->hide();
  }

  // Tap vs swipe handling (like before, but delegate tap)
  lv_obj_add_event_cb(scr, [](lv_event_t* e){
    auto code = lv_event_get_code(e);
    if(code==LV_EVENT_PRESSED){ lv_indev_get_point(lv_indev_get_act(), &g_press_pt); g_moved=false; }
    else if(code==LV_EVENT_PRESSING){
      lv_point_t p; lv_indev_get_point(lv_indev_get_act(), &p);
      if (LV_ABS(p.x-g_press_pt.x)>12 || LV_ABS(p.y-g_press_pt.y)>12) g_moved = true;
    } else if(code==LV_EVENT_RELEASED){
      if(!g_moved && cur < g_widgets.size()) g_widgets[cur]->onTap();
    }
  }, LV_EVENT_ALL, NULL);

  // Swipe gestures switch pages
  lv_obj_add_event_cb(scr, [](lv_event_t*){
    lv_dir_t d = lv_indev_get_gesture_dir(lv_indev_get_act());
    if(d==LV_DIR_LEFT){ if(total_pages()>0){ uint8_t n=total_pages(); cur=(cur+1)%n; } }
    else if(d==LV_DIR_RIGHT){ if(total_pages()>0){ uint8_t n=total_pages(); cur=(cur==0)?(n-1):(cur-1);} }
    // Show only the current widget
    for (size_t i=0;i<g_widgets.size();++i) (i==cur)? g_widgets[i]->show() : g_widgets[i]->hide();
  }, LV_EVENT_GESTURE, NULL);

  lv_scr_load(scr);

  if(!g_tickTimer){
    g_tickTimer = lv_timer_create([](lv_timer_t*){
      if(cur < g_widgets.size()) g_widgets[cur]->tick(500);
    }, 500, nullptr);
  }
}

void show(uint8_t index){
  cur = index;
  for (size_t i=0;i<g_widgets.size();++i) (i==cur)? g_widgets[i]->show() : g_widgets[i]->hide();
}

void notify_ble(bool){ /* no BLE pill in minimal UI */ }

void wifi_failed(){
  if (wifiDlg) return;
  wifiDlg = lv_obj_create(lv_layer_top());
  lv_obj_set_size(wifiDlg, LV_PCT(80), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(wifiDlg, lv_palette_main(LV_PALETTE_RED), 0);
  lv_obj_set_style_bg_opa(wifiDlg, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(wifiDlg, 14, 0);
  lv_obj_center(wifiDlg);
  lv_obj_t* lbl = lv_label_create(wifiDlg);
  lv_label_set_text(lbl, "Wi-Fi failed.\nReset credentials?");
  lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 8);
  lv_obj_t* row = lv_obj_create(wifiDlg);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_t* btnR = lv_btn_create(row);
  lv_label_set_text(lv_label_create(btnR), "Reset");
  lv_obj_add_event_cb(btnR, [](lv_event_t*){
    LittleFS.remove("/config/wifi.json");
    delay(100);
    ESP.restart();
  }, LV_EVENT_CLICKED, NULL);
  lv_obj_t* btnI = lv_btn_create(row);
  lv_label_set_text(lv_label_create(btnI), "Ignore");
  lv_obj_add_event_cb(btnI, [](lv_event_t*){ lv_obj_del(wifiDlg); wifiDlg=nullptr; }, LV_EVENT_CLICKED, NULL);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
}

void wifi_ok(){ if (wifiDlg) { lv_obj_del(wifiDlg); wifiDlg=nullptr; } }

uint8_t current_index(){ return cur; }

} // namespace ui

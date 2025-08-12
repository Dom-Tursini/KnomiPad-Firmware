#include "lvgl_hal.h"
#include "pinout.h"

extern "C" {
  #include "esp_timer.h"
}
static esp_timer_handle_t lvgl_tick_timer = nullptr;

static bool isTouched = false;

TFT_eSPI tft_gc9a01 = TFT_eSPI();
#ifdef CST816S_SUPPORT
CST816S ts_cst816s = CST816S(CST816S_RST_PIN, CST816S_IRQ_PIN, &Wire);
#endif

/* Display flushing */
void usr_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft_gc9a01.startWrite();
  tft_gc9a01.setAddrWindow(area->x1, area->y1, w, h);
  tft_gc9a01.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft_gc9a01.endWrite();

  lv_disp_flush_ready(disp);
}

#ifdef CST816S_SUPPORT
extern "C" void touch_idle_time_clear() { /* no-op stub */ }
void usr_touchpad_read(struct _lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
  static touch_event_t event;
  static bool lastTouch = false;

  if (ts_cst816s.ready())
  {
    ts_cst816s.getTouch(&event);
    Serial.printf("Touch event: finger=%d x=%d y=%d\n", event.finger, event.x, event.y);
  }
  if (event.finger)
  {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = event.x;
    data->point.y = event.y;
    touch_idle_time_clear();
    isTouched = true;
  }
  else
  {
    data->state = LV_INDEV_STATE_REL;
    isTouched = false;
  }
}
#endif

static int8_t aw9346_from_light = -1;

void tft_backlight_init(void)
{
  pinMode(LCD_BL_PIN, OUTPUT);
  digitalWrite(LCD_BL_PIN, LOW);
  delay(3); // > 2.5ms for shutdown
  aw9346_from_light = 0;
}
void tft_set_backlight(int8_t aw9346_to_light)
{
  if (aw9346_to_light > 16)
    aw9346_to_light = 16;
  if (aw9346_to_light < 0)
    aw9346_to_light = 0;
  if (aw9346_from_light == aw9346_to_light)
    return;

  if (aw9346_to_light == 0)
  {
    digitalWrite(LCD_BL_PIN, LOW);
    delay(3); // > 2.5ms for shutdown
    aw9346_from_light = 0;
    return;
  }
  if (aw9346_from_light <= 0)
  {
    digitalWrite(LCD_BL_PIN, HIGH);
    delayMicroseconds(25); // > 20us for poweron
    aw9346_from_light = 16;
  }

  if (aw9346_from_light < aw9346_to_light)
    aw9346_from_light += 16;

  int8_t num = aw9346_from_light - aw9346_to_light;

  for (int8_t i = 0; i < num; i++)
  {
    digitalWrite(LCD_BL_PIN, LOW);
    delayMicroseconds(1); // 0.5us < T_low < 500us
    digitalWrite(LCD_BL_PIN, HIGH);
    delayMicroseconds(1); // 0.5us < T_high
  }

  aw9346_from_light = aw9346_to_light;
}

lv_indev_t *ts_cst816s_indev;
void lvgl_hal_init(void)
{
  // ----------- ADD I2C STARTUP AND TOUCH RESET AT THE VERY BEGINNING: -------------
  Wire.begin(I2C0_SDA_PIN, I2C0_SCL_PIN, I2C0_SPEED);
  delay(50);

  pinMode(CST816S_RST_PIN, OUTPUT);
  digitalWrite(CST816S_RST_PIN, LOW);
  delay(10);
  digitalWrite(CST816S_RST_PIN, HIGH);
  delay(10);
  // --------------------------------------------------------------------------------

#ifdef CST816S_SUPPORT
  // touch screen
  ts_cst816s.begin();
  Serial.println("Touch begin called"); // Debug!
  ts_cst816s.setReportRate(2);          // 20ms
  ts_cst816s.setReportMode(0x60);       // touch + gesture generated interrupt
  ts_cst816s.setMotionMask(0);          // disable motion
  ts_cst816s.setAutoRst(0);             // disable auto reset
  ts_cst816s.setLongRst(0);             // disable long press reset
  ts_cst816s.setDisAutoSleep(1);        // disable auto sleep
#endif

  // display
  tft_gc9a01.begin();
  tft_gc9a01.invertDisplay(1);
  // tft_gc9a01.setRotation(2);

  tft_gc9a01.fillScreen(TFT_BLACK);
  tft_backlight_init();
  delay(50);
  tft_set_backlight(16);

  // tft_fps_test();

  // must static
  static lv_disp_draw_buf_t draw_buf;
  static lv_color_t *color_buf = (lv_color_t *)LV_MEM_CUSTOM_ALLOC(TFT_WIDTH * TFT_HEIGHT * sizeof(lv_color_t));
  lv_init();
  if (!lvgl_tick_timer) {
    const esp_timer_create_args_t args = {
      .callback = [](void*){
        lv_tick_inc(1);
      },
      .arg = nullptr,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "lvgl_tick"
    };
    esp_timer_create(&args, &lvgl_tick_timer);
    esp_timer_start_periodic(lvgl_tick_timer, 1000); // 1 ms
  }
  lv_disp_draw_buf_init(&draw_buf, color_buf, NULL, TFT_WIDTH * TFT_HEIGHT);

  /*Initialize the display*/
  // must static
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = TFT_WIDTH;
  disp_drv.ver_res = TFT_HEIGHT;
  disp_drv.flush_cb = usr_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);
  // lv_disp_set_rotation(NULL, LV_DISP_ROT_180);

#ifdef CST816S_SUPPORT
  /* touch screen */
  // must static
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv); /*Basic initialization*/
  indev_drv.gesture_limit = 1;
  indev_drv.gesture_min_velocity = 1;
  indev_drv.type = LV_INDEV_TYPE_POINTER; /*See below.*/
  indev_drv.read_cb = usr_touchpad_read;  /*See below.*/
  /*Register the driver in LVGL and save the created input device object*/
  ts_cst816s_indev = lv_indev_drv_register(&indev_drv);
#endif

  /* set background color to black (default white) */
  lv_obj_set_style_bg_color(lv_scr_act(), LV_COLOR_MAKE(0, 0, 0), LV_STATE_DEFAULT);
}
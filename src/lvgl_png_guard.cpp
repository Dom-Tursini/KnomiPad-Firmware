#include <lvgl.h>
#if !defined(LV_USE_PNG) || (LV_USE_PNG==0)
#error "LV_USE_PNG is not enabled. Check platformio.ini build_flags and lv_conf.h include path."
#endif

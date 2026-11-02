#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE "lvgl_memory.h"
#define LV_MEM_CUSTOM_ALLOC   powerwall_lvgl_alloc
#define LV_MEM_CUSTOM_FREE    free
#define LV_MEM_CUSTOM_REALLOC powerwall_lvgl_realloc

#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_36 1
#define LV_FONT_MONTSERRAT_40 1
#define LV_FONT_MONTSERRAT_48 1

#define LV_FONT_CUSTOM_DECLARE \
  LV_FONT_DECLARE(font_cjk_12) \
  LV_FONT_DECLARE(font_cjk_14) \
  LV_FONT_DECLARE(font_cjk_18) \
  LV_FONT_DECLARE(font_cjk_20)

#if defined(POWERWALL_WAVESHARE_S3_LCD_4B) || defined(POWERWALL_NATIVE480)
#ifdef __cplusplus
extern "C" {
#endif
struct _lv_font_t;
const struct _lv_font_t* powerwall_default_font(void);
#ifdef __cplusplus
}
#endif
#define LV_FONT_DEFAULT powerwall_default_font()
#else
#define LV_FONT_DEFAULT &font_cjk_14
#endif

#define LV_USE_LOG 0

#endif /*LV_CONF_H*/

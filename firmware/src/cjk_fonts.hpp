#pragma once
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_font_t font_cjk_12;
extern const lv_font_t font_cjk_14;
extern const lv_font_t font_cjk_18;
extern const lv_font_t font_cjk_20;
extern const lv_font_t font_dashboard_cjk_10;
extern const lv_font_t font_dashboard_cjk_12;
extern const lv_font_t font_dashboard_cjk_14;
extern const lv_font_t font_dashboard_cjk_18;
extern const lv_font_t font_dashboard_cjk_20;
extern const lv_font_t font_dashboard_cjk_28;
extern const lv_font_t font_dashboard_cjk_48;
extern const lv_font_t lv_font_montserrat_48;
extern const lv_font_t font_universal_sans_12;
extern const lv_font_t font_universal_sans_14;
extern const lv_font_t font_universal_sans_18;
extern const lv_font_t font_universal_sans_20;
extern const lv_font_t font_home_cjk_24;
extern const lv_font_t font_home_cjk_30;
extern const lv_font_t font_home_cjk_33;
extern const lv_font_t font_home_cjk_40;
extern const lv_font_t font_s3_latin_48;

#ifdef __cplusplus
}
#endif

// S3 keeps the original Latin raster but omits unused 48px icon glyphs.
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B) || defined(POWERWALL_S3_LATIN_FONT)
#define lv_font_montserrat_48 font_s3_latin_48
#endif

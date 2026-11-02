#pragma once
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B) || (defined(POWERWALL_SIM_WINDOW_SIZE) && POWERWALL_SIM_WINDOW_SIZE == 480)
#include "font_assets.hpp"
#undef font_cjk_12
#define font_cjk_12 (*powerwallFont(0))
#undef font_cjk_14
#define font_cjk_14 (*powerwallFont(1))
#undef font_cjk_18
#define font_cjk_18 (*powerwallFont(2))
#undef font_cjk_20
#define font_cjk_20 (*powerwallFont(3))
#undef font_dashboard_cjk_10
#define font_dashboard_cjk_10 (*powerwallFont(4))
#undef font_dashboard_cjk_12
#define font_dashboard_cjk_12 (*powerwallFont(5))
#undef font_dashboard_cjk_14
#define font_dashboard_cjk_14 (*powerwallFont(6))
#undef font_dashboard_cjk_18
#define font_dashboard_cjk_18 (*powerwallFont(7))
#undef font_dashboard_cjk_20
#define font_dashboard_cjk_20 (*powerwallFont(8))
#undef font_dashboard_cjk_28
#define font_dashboard_cjk_28 (*powerwallFont(9))
#undef font_dashboard_cjk_48
#define font_dashboard_cjk_48 (*powerwallFont(10))
#undef font_universal_sans_12
#define font_universal_sans_12 (*powerwallFont(11))
#undef font_universal_sans_14
#define font_universal_sans_14 (*powerwallFont(12))
#undef font_universal_sans_18
#define font_universal_sans_18 (*powerwallFont(13))
#undef font_universal_sans_20
#define font_universal_sans_20 (*powerwallFont(14))
#undef font_home_cjk_24
#define font_home_cjk_24 (*powerwallFont(15))
#undef font_home_cjk_30
#define font_home_cjk_30 (*powerwallFont(16))
#undef font_home_cjk_33
#define font_home_cjk_33 (*powerwallFont(17))
#undef font_home_cjk_40
#define font_home_cjk_40 (*powerwallFont(18))
#undef lv_font_montserrat_10
#define lv_font_montserrat_10 (*powerwallFont(19))
#undef lv_font_montserrat_12
#define lv_font_montserrat_12 (*powerwallFont(20))
#undef lv_font_montserrat_14
#define lv_font_montserrat_14 (*powerwallFont(21))
#undef lv_font_montserrat_18
#define lv_font_montserrat_18 (*powerwallFont(22))
#undef lv_font_montserrat_20
#define lv_font_montserrat_20 (*powerwallFont(23))
#undef lv_font_montserrat_24
#define lv_font_montserrat_24 (*powerwallFont(24))
#undef lv_font_montserrat_28
#define lv_font_montserrat_28 (*powerwallFont(25))
#undef lv_font_montserrat_32
#define lv_font_montserrat_32 (*powerwallFont(26))
#undef lv_font_montserrat_36
#define lv_font_montserrat_36 (*powerwallFont(27))
#undef lv_font_montserrat_40
#define lv_font_montserrat_40 (*powerwallFont(28))
#undef lv_font_montserrat_48
#define lv_font_montserrat_48 (*powerwallFont(29))
#endif

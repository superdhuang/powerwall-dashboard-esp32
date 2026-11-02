#pragma once

#include "font_assets.hpp"

// The names are the physical raster sizes stored in native480/fonts.pwf.
// Lookups stay name-based so asset ordering is not part of the UI contract.
namespace native480_fonts {
inline const lv_font_t* cjk_8() { return powerwallFontByName("native480_cjk_8"); }
inline const lv_font_t* cjk_9() { return powerwallFontByName("native480_cjk_9"); }
inline const lv_font_t* cjk_12() { return powerwallFontByName("native480_cjk_12"); }
inline const lv_font_t* cjk_13() { return powerwallFontByName("native480_cjk_13"); }
inline const lv_font_t* cjk_14() { return powerwallFontByName("native480_cjk_14"); }
inline const lv_font_t* dashboard_cjk_7() { return powerwallFontByName("native480_dashboard_cjk_7"); }
inline const lv_font_t* dashboard_cjk_8() { return powerwallFontByName("native480_dashboard_cjk_8"); }
inline const lv_font_t* dashboard_cjk_9() { return powerwallFontByName("native480_dashboard_cjk_9"); }
inline const lv_font_t* dashboard_cjk_12() { return powerwallFontByName("native480_dashboard_cjk_12"); }
inline const lv_font_t* dashboard_cjk_13() { return powerwallFontByName("native480_dashboard_cjk_13"); }
inline const lv_font_t* dashboard_cjk_19() { return powerwallFontByName("native480_dashboard_cjk_19"); }
inline const lv_font_t* dashboard_cjk_32() { return powerwallFontByName("native480_dashboard_cjk_32"); }
inline const lv_font_t* universal_sans_8() { return powerwallFontByName("native480_universal_sans_8"); }
inline const lv_font_t* universal_sans_9() { return powerwallFontByName("native480_universal_sans_9"); }
inline const lv_font_t* universal_sans_12() { return powerwallFontByName("native480_universal_sans_12"); }
inline const lv_font_t* universal_sans_13() { return powerwallFontByName("native480_universal_sans_13"); }
inline const lv_font_t* home_cjk_16() { return powerwallFontByName("native480_home_cjk_16"); }
inline const lv_font_t* home_cjk_20() { return powerwallFontByName("native480_home_cjk_20"); }
inline const lv_font_t* home_cjk_22() { return powerwallFontByName("native480_home_cjk_22"); }
inline const lv_font_t* home_cjk_27() { return powerwallFontByName("native480_home_cjk_27"); }
inline const lv_font_t* montserrat_7() { return powerwallFontByName("native480_montserrat_7"); }
inline const lv_font_t* montserrat_8() { return powerwallFontByName("native480_montserrat_8"); }
inline const lv_font_t* montserrat_9() { return powerwallFontByName("native480_montserrat_9"); }
inline const lv_font_t* montserrat_12() { return powerwallFontByName("native480_montserrat_12"); }
inline const lv_font_t* montserrat_13() { return powerwallFontByName("native480_montserrat_13"); }
inline const lv_font_t* montserrat_16() { return powerwallFontByName("native480_montserrat_16"); }
inline const lv_font_t* montserrat_19() { return powerwallFontByName("native480_montserrat_19"); }
inline const lv_font_t* montserrat_21() { return powerwallFontByName("native480_montserrat_21"); }
inline const lv_font_t* montserrat_24() { return powerwallFontByName("native480_montserrat_24"); }
inline const lv_font_t* montserrat_27() { return powerwallFontByName("native480_montserrat_27"); }
inline const lv_font_t* montserrat_32() { return powerwallFontByName("native480_montserrat_32"); }
inline const lv_font_t* montserrat_60() { return powerwallFontByName("native480_montserrat_60"); }
inline const lv_font_t* grid_cjk_16() { return powerwallFontByName("native480_grid_cjk_16"); }
inline const lv_font_t* grid_cjk_18() { return powerwallFontByName("native480_grid_cjk_18"); }
inline const lv_font_t* grid_cjk_20() { return powerwallFontByName("native480_grid_cjk_20"); }
}  // namespace native480_fonts

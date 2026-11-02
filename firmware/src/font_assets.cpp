#if defined(POWERWALL_WAVESHARE_S3_LCD_4B) || defined(POWERWALL_NATIVE480)
#include "font_assets.hpp"
extern "C" const lv_font_t* powerwall_default_font(void) {
#if defined(POWERWALL_NATIVE480)
  return powerwallFontByName("native480_cjk_9");
#else
  return powerwallFont(1);
#endif
}
#endif

#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include <cmath>
#include <ctime>
#if !defined(ESP32)
#include <chrono>
#endif
#if defined(ESP32)
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <WiFi.h>
#endif
#include "native480_fonts.hpp"
#include "native480_i18n.hpp"
#include "stage_3d_assets.hpp"
#include "powerflow_animation.hpp"
#include "powerwall_client.hpp"
#include "weather_client.hpp"
#include "phase_console.hpp"
#include "usable_eta.hpp"
#include "weekly_history.hpp"
#include "daily_trend_history.hpp"

#include "header_clock.hpp"

// Native pixel masks for the 9x8 on-panel charge arrows.
static const uint8_t kChargeArrowUpMap480[9 * 8] = {
  0x00, 0x01, 0x0d, 0xa5, 0xa4, 0x0d, 0x01, 0x00, 0x00,
  0x00, 0x02, 0x4a, 0xeb, 0xf1, 0x42, 0x02, 0x00, 0x00,
  0x01, 0x10, 0xc1, 0xfd, 0xfd, 0xbc, 0x12, 0x01, 0x00,
  0x02, 0x51, 0xf9, 0xfe, 0xfe, 0xf0, 0x57, 0x02, 0x00,
  0x1d, 0xc6, 0xfd, 0xff, 0xff, 0xfd, 0xcf, 0x14, 0x01,
  0x6a, 0xf4, 0xfe, 0xff, 0xff, 0xfe, 0xfc, 0x60, 0x01,
  0xe2, 0xfc, 0xfc, 0xfc, 0xfc, 0xfc, 0xfb, 0xc7, 0x18,
  0x7d, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x6f, 0x0b,
};
static const uint8_t kChargeArrowDownMap480[9 * 8] = {
  0x7d, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x6f, 0x0b,
  0xe2, 0xfc, 0xfc, 0xfc, 0xfc, 0xfc, 0xfb, 0xc7, 0x18,
  0x6a, 0xf4, 0xfe, 0xff, 0xff, 0xfe, 0xfc, 0x60, 0x01,
  0x1d, 0xc6, 0xfd, 0xff, 0xff, 0xfd, 0xcf, 0x14, 0x01,
  0x02, 0x51, 0xf9, 0xfe, 0xfe, 0xf0, 0x57, 0x02, 0x00,
  0x01, 0x10, 0xc1, 0xfd, 0xfe, 0xbc, 0x12, 0x01, 0x00,
  0x00, 0x02, 0x4a, 0xeb, 0xf1, 0x42, 0x02, 0x00, 0x00,
  0x00, 0x01, 0x0d, 0xa5, 0xa4, 0x0d, 0x01, 0x00, 0x00,
};
static const lv_img_dsc_t kChargeArrowUpDsc = {
  { LV_IMG_CF_ALPHA_8BIT, 0, 0, 9, 8 },
  sizeof(kChargeArrowUpMap480),
  kChargeArrowUpMap480,
};
static const lv_img_dsc_t kChargeArrowDownDsc = {
  { LV_IMG_CF_ALPHA_8BIT, 0, 0, 9, 8 },
  sizeof(kChargeArrowDownMap480),
  kChargeArrowDownMap480,
};
static bool isTaiwanTouSummerMonth(int month) {
  return month >= 6 && month <= 9;
}

static bool isTaiwanTouWeekend(const struct tm& localTime) {
  return localTime.tm_wday == 0 || localTime.tm_wday == 6;
}

static bool isTaiwanTouWeekdayPeak(int month, float hourOfDay) {
  return isTaiwanTouSummerMonth(month)
    ? (hourOfDay >= 9.0f && hourOfDay < 24.0f)
    : ((hourOfDay >= 6.0f && hourOfDay < 11.0f) ||
       (hourOfDay >= 14.0f && hourOfDay < 24.0f));
}

static void formatSettingsUptime(uint64_t totalSeconds, char* output, size_t outputSize) {
  if (!output || outputSize == 0) return;
  const uint64_t days = totalSeconds / 86400ULL;
  const uint64_t hours = (totalSeconds / 3600ULL) % 24ULL;
  const uint64_t minutes = (totalSeconds / 60ULL) % 60ULL;
  const uint64_t seconds = totalSeconds % 60ULL;
  snprintf(output, outputSize, "%s: %llud %02llu:%02llu:%02llu",
           native480_i18n::text("開機時間"),
           static_cast<unsigned long long>(days),
           static_cast<unsigned long long>(hours),
           static_cast<unsigned long long>(minutes),
           static_cast<unsigned long long>(seconds));
}

// Waveshare ESP32-S3-Touch-LCD-4B 480x480 dashboard UI (firmware + firmware_sim_native480)
class Ui480 {
public:
  static void setLabelText480(lv_obj_t* label, const char* text) {
    lv_label_set_text(label, native480_i18n::text(text ? text : ""));
  }

  static void setLabelTextRaw480(lv_obj_t* label, const char* text) {
    lv_label_set_text(label, text ? text : "");
  }

  // Native framebuffer dimensions and positions.
  static constexpr int kStageW = 469;
  static constexpr int kStageH = 293;
  static constexpr int kChartW = 416;
  static constexpr int kHomeChartW = 424;
  static constexpr int kHomeChartPlotX = 28;
  static constexpr int kHomeChartPlotY = 19;
  static constexpr int kHomeChartPlotH = 105;
  static constexpr int kHomeChartTouY = 127;
  static constexpr int kHomeChartTouH = 5;
  static constexpr int kHomeChartTimeTicksY = 136;
  static constexpr int kHomeTouCardY = 43;
  static constexpr int kHomeTouCardH = 74;
  static constexpr int kHomeTouPillY = 1;
  static constexpr int kHomeTouPillH = 22;
  static constexpr int kHomeTouIntervalTextY = 4;
  static constexpr int kHomeTouIntervalDashY = 12;
  static constexpr int kHomeTouIntervalPillGap = 10;
  static constexpr int kHomeTouIntervalDashGap = 6;
  static constexpr int kHomeTouIntervalDashW = 9;
  static constexpr int kHomeTouPriceY = 24;
  static constexpr int kHomeTouCountdownY = 26;
  static constexpr int kHomeTouSegmentY = 44;
  static constexpr int kHomeTouNowLineY = 39;
  static constexpr int kHomeTouNowDotY = 40;
  static constexpr int kHomeTouTicksY = 58;
  static constexpr int kHomeGridCardY = 121;
  static constexpr int kHomeGridCardH = 101;
  static constexpr int kHomeGridTotalsSeparatorY = 67;
  static constexpr int kHomeMetricsCardY = 226;
  static constexpr int kHomeMetricsCardH = 91;
  static constexpr int kHomeChartCardY = 321;
  static constexpr int kHomeChartCardH = 155;
  static constexpr int kChart1H = 55;
  static constexpr int kChart2W = 428;
  static constexpr int kChart2H = 60;
  static constexpr int kDashboardChartH = 140;
  static constexpr int kPwChartPlotX = 36;
  static constexpr int kPwChartPlotY = 7;
  static constexpr int kPwChartBandY = 148;
  static constexpr int kPwChartTimeY = 154;
  static constexpr int kPwChartCursorH = 147;
  static constexpr int kPwChartCardY = 303;
  static constexpr int kPwChartCardH = 171;
  static constexpr int kPowerwallTrendLoadH = kDashboardChartH / 2;
  static constexpr int kPowerwallTrendSocH = kDashboardChartH - kPowerwallTrendLoadH;
  static constexpr int kPowerwallTrendLoadY = kPwChartPlotY;
  static constexpr int kPowerwallTrendSocY = kPwChartPlotY + kPowerwallTrendLoadH;

  void setWifiStartupCardMode(uint8_t mode, const char* ssid = nullptr,
                              const char* setupUrl = nullptr) {
    if (active_dashboard_page_ != 0) {
      setHome480WifiCardMode(3, nullptr, nullptr);
      return;
    }
    setHome480WifiCardMode(mode, ssid, setupUrl);
  }
#if !defined(ESP32)
  void setSimulatorWifiCardMode(uint8_t mode, const char* ssid = nullptr,
                                const char* setupUrl = nullptr) {
    setWifiStartupCardMode(mode, ssid, setupUrl);
  }
#endif

  void init() {
    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_size(scr, 480, 480);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0b0c0f), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // Native 480 hero card with a 469x293 stage canvas.
    hero_card_ = lv_obj_create(scr);
    lv_obj_set_pos(hero_card_, 5, 5);
    lv_obj_set_size(hero_card_, 469, 293);
    lv_obj_set_style_bg_color(hero_card_, lv_color_hex(0x101820), 0);
    lv_obj_set_style_border_color(hero_card_, lv_color_hex(0x27272a), 0);
    lv_obj_set_style_border_width(hero_card_, 1, 0);
    lv_obj_set_style_radius(hero_card_, 11, 0);
    lv_obj_set_style_border_width(hero_card_, 0, 0);
    lv_obj_set_style_radius(hero_card_, 0, 0);
    lv_obj_set_style_pad_all(hero_card_, 0, 0);
    lv_obj_set_style_clip_corner(hero_card_, true, 0);
    lv_obj_clear_flag(hero_card_, LV_OBJ_FLAG_SCROLLABLE);

    // Full-bleed native 3D stage canvas.
    stage_base_buf_   = allocPixelBuf(kStageW * kStageH);
    stage_canvas_buf_ = allocPixelBuf(kStageW * kStageH);
    if (stage_canvas_buf_) {
      stage_canvas_ = lv_canvas_create(hero_card_);
      lv_canvas_set_buffer(stage_canvas_, stage_canvas_buf_, kStageW, kStageH, LV_IMG_CF_TRUE_COLOR);
      lv_obj_set_pos(stage_canvas_, 0, 0);
      lv_obj_set_size(stage_canvas_, 469, 293);
    }

    // Floating header overlay inside the 3D scene.
    lv_obj_t* hdr_bar = lv_obj_create(hero_card_);
    lv_obj_set_pos(hdr_bar, 0, 1);
    lv_obj_set_size(hdr_bar, 469, 27);
    lv_obj_set_style_bg_opa(hdr_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hdr_bar, 0, 0);
    lv_obj_set_style_radius(hdr_bar, 0, 0);
    lv_obj_set_style_pad_all(hdr_bar, 0, 0);
    lv_obj_clear_flag(hdr_bar, LV_OBJ_FLAG_SCROLLABLE);

    lbl_site_title_ = lv_label_create(hdr_bar);
    lv_obj_set_style_text_font(lbl_site_title_, native480_fonts::cjk_12(), 0);
    lv_obj_set_style_text_color(lbl_site_title_, lv_color_hex(0xffffff), 0);
    setLabelTextRaw480(lbl_site_title_, native480_i18n::kDefaultSiteTitle);
    lv_obj_align(lbl_site_title_, LV_ALIGN_LEFT_MID, 11, 0);

    pill_status_ = lv_obj_create(hdr_bar);
    lv_obj_set_size(pill_status_, LV_SIZE_CONTENT, 16);
    lv_obj_align(pill_status_, LV_ALIGN_LEFT_MID, 67, 0);
    lv_obj_set_style_bg_color(pill_status_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(pill_status_, LV_OPA_40, 0);
    lv_obj_set_style_border_color(pill_status_, lv_color_hex(0x059669), 0);
    lv_obj_set_style_border_width(pill_status_, 1, 0);
    lv_obj_set_style_radius(pill_status_, 8, 0);
    flattenDashboardPill(pill_status_, 0x059669);
    lv_obj_set_style_pad_hor(pill_status_, 7, 0);
    lv_obj_set_style_pad_top(pill_status_, 0, 0);
    lv_obj_set_style_pad_bottom(pill_status_, 0, 0);
    lv_obj_set_flex_flow(pill_status_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pill_status_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(pill_status_, 4, 0);
    lv_obj_clear_flag(pill_status_, LV_OBJ_FLAG_SCROLLABLE);
#if !defined(POWERWALL_NATIVE480_RELEASE)
    lv_obj_add_flag(pill_status_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(pill_status_, [](lv_event_t* e) {
      Ui480* self = static_cast<Ui480*>(lv_event_get_user_data(e));
      if (self) {
        self->state_mode_idx_ = (self->state_mode_idx_ + 1) % 7;
        self->refreshAllFromCache();
      }
    }, LV_EVENT_CLICKED, this);
#endif

    dot_status_ = lv_obj_create(pill_status_);
    lv_obj_set_size(dot_status_, 5, 5);
    lv_obj_set_style_radius(dot_status_, 3, 0);
    lv_obj_set_style_bg_color(dot_status_, lv_color_hex(0x34d399), 0);
    lv_obj_set_style_bg_opa(dot_status_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot_status_, 0, 0);
    lv_obj_clear_flag(dot_status_, LV_OBJ_FLAG_SCROLLABLE);

    lbl_live_status_ = lv_label_create(pill_status_);
    lv_obj_set_style_text_font(lbl_live_status_, native480_fonts::cjk_8(), 0);
    // Center the visible glyphs, accounting for the font's baseline space.
    lv_obj_set_style_translate_y(lbl_live_status_, -1, 0);
    lv_obj_set_style_text_color(lbl_live_status_, lv_color_hex(0x34d399), 0);
    setLabelText480(lbl_live_status_, "連線中");

    pill_weather_ = lv_obj_create(hdr_bar);
    lv_obj_set_size(pill_weather_, LV_SIZE_CONTENT, 16);
    lv_obj_align(pill_weather_, LV_ALIGN_RIGHT_MID, -9, 0);
    lv_obj_set_style_bg_color(pill_weather_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(pill_weather_, LV_OPA_40, 0);
    lv_obj_set_style_border_color(pill_weather_, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(pill_weather_, 1, 0);
    lv_obj_set_style_radius(pill_weather_, 8, 0);
    flattenDashboardPill(pill_weather_, 0x334155);
    lv_obj_set_style_pad_hor(pill_weather_, 7, 0);
    lv_obj_set_style_pad_top(pill_weather_, 1, 0);
    lv_obj_set_style_pad_bottom(pill_weather_, 0, 0);
    lv_obj_set_flex_flow(pill_weather_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pill_weather_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(pill_weather_, LV_OBJ_FLAG_SCROLLABLE);
#if !defined(POWERWALL_NATIVE480_RELEASE)
    lv_obj_add_flag(pill_weather_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(pill_weather_, [](lv_event_t* e) {
      Ui480* self = static_cast<Ui480*>(lv_event_get_user_data(e));
      if (self) {
        self->weather_mode_idx_ = (self->weather_mode_idx_ + 1) % 11;
        self->refreshAllFromCache();
      }
    }, LV_EVENT_CLICKED, this);
#endif

    lbl_weather_ = lv_label_create(pill_weather_);
    lv_obj_set_style_text_font(lbl_weather_, native480_fonts::cjk_8(), 0);
    lv_obj_set_style_text_color(lbl_weather_, lv_color_hex(0xbae6fd), 0);
    setLabelText480(lbl_weather_, "台北：天氣載入中...");

    auto makeLeaderLine = [&](int x, int y, int h_px) {
      lv_obj_t* ln = lv_obj_create(hero_card_);
      lv_obj_set_pos(ln, x, y);
      lv_obj_set_size(ln, 1, h_px);
      lv_obj_set_style_bg_color(ln, lv_color_hex(0xa1a1aa), 0);
      lv_obj_set_style_bg_opa(ln, LV_OPA_60, 0);
      lv_obj_set_style_border_width(ln, 0, 0);
      lv_obj_set_style_radius(ln, 0, 0);
      lv_obj_clear_flag(ln, LV_OBJ_FLAG_SCROLLABLE);
    };

    // Native stage callouts.
    makeLeaderLine(242, 67, 37);
    lv_obj_t* lbl_s_title = lv_label_create(hero_card_);
    lv_obj_set_style_text_font(lbl_s_title, native480_i18n::kEnglish
      ? native480_fonts::montserrat_16() : native480_fonts::grid_cjk_16(), 0);
    lv_obj_set_style_text_color(lbl_s_title, lv_color_hex(0xa1a1aa), 0);
    setLabelText480(lbl_s_title, "太陽能");
    lv_obj_set_width(lbl_s_title, 67);
    lv_obj_set_style_text_align(lbl_s_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_s_title, 209, 37);

    lbl_solar_val_ = lv_label_create(hero_card_);
    lv_obj_set_style_text_font(lbl_solar_val_, native480_fonts::cjk_13(), 0);
    lv_obj_set_style_text_color(lbl_solar_val_, lv_color_hex(0xa1a1aa), 0);
    setLabelText480(lbl_solar_val_, "0 kW");
    lv_obj_set_width(lbl_solar_val_, 80);
    lv_obj_set_style_text_align(lbl_solar_val_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_solar_val_, 202, 48);

    makeLeaderLine(323, 117, 53);
    lv_obj_t* lbl_h_title = lv_label_create(hero_card_);
    lv_obj_set_style_text_font(lbl_h_title, native480_i18n::kEnglish
      ? native480_fonts::montserrat_16() : native480_fonts::grid_cjk_16(), 0);
    lv_obj_set_style_text_color(lbl_h_title, lv_color_hex(0xa1a1aa), 0);
    setLabelText480(lbl_h_title, "住家");
    lv_obj_set_width(lbl_h_title, 67);
    lv_obj_set_style_text_align(lbl_h_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_h_title, 289, 88);

    lbl_home_val_ = lv_label_create(hero_card_);
    lv_obj_set_style_text_font(lbl_home_val_, native480_fonts::cjk_13(), 0);
    lv_obj_set_style_text_color(lbl_home_val_, lv_color_hex(0xffffff), 0);
    setLabelText480(lbl_home_val_, "-- kW");
    lv_obj_set_width(lbl_home_val_, 80);
    lv_obj_set_style_text_align(lbl_home_val_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_home_val_, 283, 99);

    makeLeaderLine(186, 237, 9);
    lbl_pw_title_ = lv_label_create(hero_card_);
    lv_obj_set_style_text_font(lbl_pw_title_, native480_fonts::cjk_8(), 0);
    lv_obj_set_style_text_color(lbl_pw_title_, lv_color_hex(0xa1a1aa), 0);
    setLabelText480(lbl_pw_title_, "POWERWALL");
    lv_obj_set_width(lbl_pw_title_, 93);
    lv_obj_set_style_text_align(lbl_pw_title_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_pw_title_, 139, 249);

    pw_val_row_ = lv_obj_create(hero_card_);
    lv_obj_set_size(pw_val_row_, 120, 16);
    lv_obj_set_pos(pw_val_row_, 126, 260);
    lv_obj_set_style_bg_opa(pw_val_row_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pw_val_row_, 0, 0);
    lv_obj_set_style_pad_all(pw_val_row_, 0, 0);
    lv_obj_set_style_pad_column(pw_val_row_, 3, 0);
    lv_obj_set_flex_flow(pw_val_row_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pw_val_row_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(pw_val_row_, LV_OBJ_FLAG_SCROLLABLE);

    lbl_pw_kw_ = lv_label_create(pw_val_row_);
    lv_obj_set_style_text_font(lbl_pw_kw_, native480_fonts::cjk_13(), 0);
    setLabelText480(lbl_pw_kw_, "-- kW");
    lv_obj_set_style_text_color(lbl_pw_kw_, lv_color_hex(0xffffff), 0);

    lbl_pw_dot_ = lv_obj_create(pw_val_row_);
    lv_obj_set_size(lbl_pw_dot_, 3, 3);
    lv_obj_set_style_radius(lbl_pw_dot_, 1, 0);
    lv_obj_set_style_bg_color(lbl_pw_dot_, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(lbl_pw_dot_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(lbl_pw_dot_, 0, 0);
    lv_obj_set_style_pad_all(lbl_pw_dot_, 0, 0);
    lv_obj_clear_flag(lbl_pw_dot_, LV_OBJ_FLAG_SCROLLABLE);

    img_pw_arrow_ = lv_img_create(pw_val_row_);
    lv_img_set_src(img_pw_arrow_, &kChargeArrowUpDsc);
    lv_obj_set_style_img_recolor(img_pw_arrow_, lv_color_hex(0x11E58E), 0);
    lv_obj_set_style_img_recolor_opa(img_pw_arrow_, LV_OPA_COVER, 0);
    lv_obj_add_flag(img_pw_arrow_, LV_OBJ_FLAG_HIDDEN);

    lbl_pw_soc_ = lv_label_create(pw_val_row_);
    lv_obj_set_style_text_font(lbl_pw_soc_, native480_fonts::cjk_13(), 0);
    setLabelText480(lbl_pw_soc_, "--%");
    lv_obj_set_style_text_color(lbl_pw_soc_, lv_color_hex(0xffffff), 0);

    lbl_stat_pw_eta_ = lv_label_create(hero_card_);
    lv_obj_set_style_text_font(lbl_stat_pw_eta_, native480_fonts::cjk_8(), 0);
    lv_obj_set_style_text_color(lbl_stat_pw_eta_, lv_color_hex(0xd4d4d8), 0);
    setLabelText480(lbl_stat_pw_eta_, "--");
    lv_obj_set_width(lbl_stat_pw_eta_, 120);
    lv_obj_set_style_text_align(lbl_stat_pw_eta_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_stat_pw_eta_, 126, 276);

    makeLeaderLine(305, 249, 7);
    lbl_grid_title_ = lv_label_create(hero_card_);
    lv_obj_set_style_text_font(lbl_grid_title_, native480_i18n::kEnglish
      ? native480_fonts::montserrat_16() : native480_fonts::grid_cjk_16(), 0);
    lv_obj_set_style_text_color(lbl_grid_title_, lv_color_hex(0xa1a1aa), 0);
    setLabelText480(lbl_grid_title_, "GRID");
    lv_obj_set_width(lbl_grid_title_, 80);
    lv_obj_set_style_text_align(lbl_grid_title_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_grid_title_, 265, 258);

    lbl_grid_val_ = lv_label_create(hero_card_);
    lv_obj_set_style_text_font(lbl_grid_val_, native480_fonts::cjk_13(), 0);
    lv_obj_set_style_text_color(lbl_grid_val_, lv_color_hex(0xffffff), 0);
    setLabelText480(lbl_grid_val_, "-- kW");
    lv_obj_set_width(lbl_grid_val_, 93);
    lv_obj_set_style_text_align(lbl_grid_val_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_grid_val_, 258, 269);

    // Native 24-hour timeline card.
    chart_card_ = lv_obj_create(scr);
    lv_obj_set_pos(chart_card_, 5, 304);
    lv_obj_set_size(chart_card_, 469, 171);
    lv_obj_set_style_bg_color(chart_card_, lv_color_hex(0x13151a), 0);
    lv_obj_set_style_bg_opa(chart_card_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(chart_card_, lv_color_hex(0x27272a), 0);
    lv_obj_set_style_border_width(chart_card_, 1, 0);
    lv_obj_set_style_radius(chart_card_, 11, 0);
    lv_obj_set_style_border_width(chart_card_, 0, 0);
    lv_obj_set_style_radius(chart_card_, 0, 0);
    lv_obj_t* chart_rule = lv_obj_create(chart_card_);
    lv_obj_set_pos(chart_rule, 0, 170); lv_obj_set_size(chart_rule, 469, 1);
    lv_obj_set_style_bg_color(chart_rule, lv_color_hex(0x263746), 0);
    lv_obj_set_style_bg_opa(chart_rule, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(chart_rule, 0, 0);
    lv_obj_clear_flag(chart_rule, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(chart_card_, 0, 0);
    lv_obj_clear_flag(chart_card_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl_h_stat_t = lv_label_create(chart_card_);
    lv_obj_set_style_text_font(lbl_h_stat_t, native480_fonts::cjk_8(), 0);
    lv_obj_set_style_text_color(lbl_h_stat_t, lv_color_hex(0xa1a1aa), 0);
    setLabelText480(lbl_h_stat_t, "住家用量");
    lv_obj_set_pos(lbl_h_stat_t, 9, 5);

    lbl_stat_home_kw_ = lv_label_create(chart_card_);
    lv_obj_set_style_text_font(lbl_stat_home_kw_, native480_fonts::cjk_12(), 0);
    lv_obj_set_style_text_color(lbl_stat_home_kw_, lv_color_hex(0x38bdf8), 0);
    setLabelText480(lbl_stat_home_kw_, "-- kW");
    lv_obj_set_pos(lbl_stat_home_kw_, 9, 15);

    lbl_stat_home_va_ = lv_label_create(chart_card_);
    lv_obj_set_style_text_font(lbl_stat_home_va_, native480_fonts::cjk_8(), 0);
    lv_obj_set_style_text_color(lbl_stat_home_va_, lv_color_hex(0xa1a1aa), 0);
    setLabelText480(lbl_stat_home_va_, "-- V · -- A");
    lv_obj_set_pos(lbl_stat_home_va_, 56, 18);

    lv_obj_t* vdiv = lv_obj_create(chart_card_);
    lv_obj_set_pos(vdiv, 132, 8);
    lv_obj_set_size(vdiv, 1, 17);
    lv_obj_set_style_bg_color(vdiv, lv_color_hex(0x27272a), 0);
    lv_obj_set_style_border_width(vdiv, 0, 0);
    lv_obj_clear_flag(vdiv, LV_OBJ_FLAG_SCROLLABLE);

    lbl_stat_reserve_ = lv_label_create(chart_card_);
    lv_obj_set_style_text_font(lbl_stat_reserve_, native480_fonts::cjk_8(), 0);
    lv_obj_set_style_text_color(lbl_stat_reserve_, lv_color_hex(0xa1a1aa), 0);
    setLabelText480(lbl_stat_reserve_, "Powerwall (備援 24%)");
    lv_obj_set_pos(lbl_stat_reserve_, 140, 5);

    lbl_stat_pw_soc_ = lv_label_create(chart_card_);
    lv_obj_set_style_text_font(lbl_stat_pw_soc_, native480_fonts::cjk_12(), 0);
    lv_obj_set_style_text_color(lbl_stat_pw_soc_, lv_color_hex(0x00e676), 0);
    setLabelText480(lbl_stat_pw_soc_, "--%");
    lv_obj_set_pos(lbl_stat_pw_soc_, 140, 15);

    lbl_stat_pw_kwh_ = lv_label_create(chart_card_);
    lv_obj_set_style_text_font(lbl_stat_pw_kwh_, native480_fonts::cjk_8(), 0);
    lv_obj_set_style_text_color(lbl_stat_pw_kwh_, lv_color_hex(0xa1a1aa), 0);
    setLabelText480(lbl_stat_pw_kwh_, "-- / -- kWh");
    lv_obj_set_pos(lbl_stat_pw_kwh_, 177, 18);

    pill_tou_ = lv_obj_create(chart_card_);
    lv_obj_set_size(pill_tou_, LV_SIZE_CONTENT, 16);
    lv_obj_align(pill_tou_, LV_ALIGN_TOP_RIGHT, -9, 8);
    lv_obj_set_style_bg_color(pill_tou_, lv_color_hex(0x064e3b), 0);
    lv_obj_set_style_border_color(pill_tou_, lv_color_hex(0x10b981), 0);
    lv_obj_set_style_border_width(pill_tou_, 1, 0);
    lv_obj_set_style_radius(pill_tou_, 8, 0);
    flattenDashboardPill(pill_tou_, 0x10b981);
    lv_obj_set_style_pad_hor(pill_tou_, 7, 0);
    lv_obj_set_style_pad_top(pill_tou_, 1, 0);
    lv_obj_set_style_pad_bottom(pill_tou_, 0, 0);
    lv_obj_set_flex_flow(pill_tou_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pill_tou_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(pill_tou_, LV_OBJ_FLAG_SCROLLABLE);

    lbl_tou_pill_ = lv_label_create(pill_tou_);
    lv_obj_set_style_text_font(lbl_tou_pill_, native480_fonts::cjk_8(), 0);
    lv_obj_set_style_text_color(lbl_tou_pill_, lv_color_hex(0x6ee7b7), 0);
    setLabelText480(lbl_tou_pill_, "時間電價載入中");

    lv_obj_t* hdiv = lv_obj_create(chart_card_);
    lv_obj_set_pos(hdiv, 9, 31);
    lv_obj_set_size(hdiv, 451, 1);
    lv_obj_set_style_bg_color(hdiv, lv_color_hex(0x27272a), 0);
    lv_obj_set_style_border_width(hdiv, 0, 0);

    auto addYLabel = [&](const char* txt, int y, uint32_t col) {
      lv_obj_t* l = lv_label_create(chart_card_);
      lv_obj_set_style_text_font(l, native480_fonts::cjk_8(), 0);
      lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
      setLabelText480(l, txt);
      lv_obj_set_width(l, 37);
      lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_RIGHT, 0);
      lv_obj_set_pos(l, 3, y);
    };
    addYLabel("住家 kW", 37, 0x38bdf8);
    addYLabel("3 kW",    61, 0x71717a);
    addYLabel("0 kW",   83, 0x71717a);

    addYLabel("PW 100%", 93, 0x00e676);
    addYLabel("50%",     112, 0x71717a);
    addYLabel("24%",     123, 0x38bdf8);
    addYLabel("0%",      132, 0x71717a);

    chart1_buf_ = allocPixelBuf(kChartW * kChart1H);
    chart2_buf_ = allocPixelBuf(kChart2W * kChart2H);

    if (chart1_buf_) {
      chart1_canvas_ = lv_canvas_create(chart_card_);
      lv_canvas_set_buffer(chart1_canvas_, chart1_buf_, kChartW, kChart1H, LV_IMG_CF_TRUE_COLOR);
      lv_obj_set_pos(chart1_canvas_, 44, 37);
    }
    if (chart2_buf_) {
      chart2_canvas_ = lv_canvas_create(chart_card_);
      lv_canvas_set_buffer(chart2_canvas_, chart2_buf_, kChart2W, kChart2H, LV_IMG_CF_TRUE_COLOR);
      lv_obj_set_pos(chart2_canvas_, 44, 93);
    }

    lbl_tou_track_ = lv_label_create(chart_card_);
    lv_obj_set_style_text_font(lbl_tou_track_, native480_fonts::cjk_8(), 0);
    lv_obj_set_style_text_color(lbl_tou_track_, lv_color_hex(0x34d399), 0);
    setLabelText480(lbl_tou_track_, "週間費率");
    lv_obj_set_width(lbl_tou_track_, 37);
    lv_obj_set_style_text_align(lbl_tou_track_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(lbl_tou_track_, 3, 144);

    tou_bar_we_ = lv_obj_create(chart_card_);
    lv_obj_set_pos(tou_bar_we_, 44, 145);
    lv_obj_set_size(tou_bar_we_, 416, 4);
    lv_obj_set_style_bg_color(tou_bar_we_, lv_color_hex(0x26a64c), 0);
    lv_obj_set_style_border_width(tou_bar_we_, 0, 0);
    lv_obj_set_style_radius(tou_bar_we_, 3, 0);
    lv_obj_add_flag(tou_bar_we_, LV_OBJ_FLAG_HIDDEN);

    tou_bar_wk_ = lv_obj_create(chart_card_);
    lv_obj_set_pos(tou_bar_wk_, 44, 143);
    lv_obj_set_size(tou_bar_wk_, 416, 8);
    lv_obj_set_style_bg_opa(tou_bar_wk_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tou_bar_wk_, 0, 0);
    lv_obj_set_style_pad_all(tou_bar_wk_, 0, 0);
    lv_obj_clear_flag(tou_bar_wk_, LV_OBJ_FLAG_SCROLLABLE);

    tou_bar_wk_summer_ = lv_obj_create(chart_card_);
    lv_obj_set_pos(tou_bar_wk_summer_, 44, 143);
    lv_obj_set_size(tou_bar_wk_summer_, 416, 8);
    lv_obj_set_style_bg_opa(tou_bar_wk_summer_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tou_bar_wk_summer_, 0, 0);
    lv_obj_set_style_pad_all(tou_bar_wk_summer_, 0, 0);
    lv_obj_clear_flag(tou_bar_wk_summer_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(tou_bar_wk_summer_, LV_OBJ_FLAG_HIDDEN);

    auto makeSegOn = [&](lv_obj_t* parent, int x, int w, int h_seg, uint32_t col) {
      lv_obj_t* s = lv_obj_create(parent);
      lv_obj_set_pos(s, x, h_seg == 4 ? 2 : 0);
      lv_obj_set_size(s, w, h_seg);
      lv_obj_set_style_bg_color(s, lv_color_hex(col), 0);
      lv_obj_set_style_border_width(s, 0, 0);
      lv_obj_set_style_radius(s, 2, 0);
    };
    const int w0_6   = static_cast<int>(roundf(kChartW * 0.25f));
    const int w6_11  = static_cast<int>(roundf(kChartW * 0.2083f));
    const int w11_14 = static_cast<int>(roundf(kChartW * 0.125f));
    const int w14_24 = kChartW - w0_6 - w6_11 - w11_14;
    makeSegOn(tou_bar_wk_, 0, w0_6, 4, 0x26a64c);
    makeSegOn(tou_bar_wk_, w0_6, w6_11, 7, 0xb9312f);
    makeSegOn(tou_bar_wk_, w0_6 + w6_11, w11_14, 4, 0x26a64c);
    makeSegOn(tou_bar_wk_, w0_6 + w6_11 + w11_14, w14_24, 7, 0xb9312f);

    const int w0_9  = static_cast<int>(roundf(kChartW * (9.0f / 24.0f)));
    const int w9_24 = kChartW - w0_9;
    makeSegOn(tou_bar_wk_summer_, 0, w0_9, 4, 0x26a64c);
    makeSegOn(tou_bar_wk_summer_, w0_9, w9_24, 7, 0xb9312f);

    static const char* kTicks[6] = {"00:00", "06:00", "11:00", "14:00", "18:00", "24:00"};
    static const int kTickX[6] = {44, 143, 225, 274, 340, 439};
    for (int i = 0; i < 6; ++i) {
      lv_obj_t* tl = lv_label_create(chart_card_);
      lv_obj_set_style_text_font(tl, native480_fonts::cjk_8(), 0);
      lv_obj_set_style_text_color(tl, lv_color_hex(0xa1a1aa), 0);
      setLabelText480(tl, kTicks[i]);
      lv_obj_set_pos(tl, kTickX[i], 103);
    }

    cursor_line_ = lv_obj_create(chart_card_);
    lv_obj_set_size(cursor_line_, 1, 115);
    lv_obj_set_pos(cursor_line_, 44, 37);
    lv_obj_set_style_bg_color(cursor_line_, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(cursor_line_, LV_OPA_90, 0);
    lv_obj_set_style_border_width(cursor_line_, 0, 0);
    lv_obj_set_style_radius(cursor_line_, 0, 0);
    lv_obj_clear_flag(cursor_line_, LV_OBJ_FLAG_SCROLLABLE);

    auto createCurrentPoint = [&](uint32_t color) {
      lv_obj_t* dot = lv_obj_create(chart_card_);
      lv_obj_set_size(dot, 6, 6);
      lv_obj_set_style_radius(dot, 3, 0);
      lv_obj_set_style_bg_color(dot, lv_color_hex(color), 0);
      lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
      lv_obj_set_style_border_color(dot, lv_color_hex(0xffffff), 0);
      lv_obj_set_style_border_width(dot, 1, 0);
      lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
      return dot;
    };
    chart1_now_dot_ = createCurrentPoint(0x38bdf8);
    chart2_now_dot_ = createCurrentPoint(0x11e58e);

    initializeHome480();
    initializeAddedPages(scr);

    renderAreaChartToRgb565(chart1_buf_, kChartW, kChart1H, nullptr, 0, false, 6.0f, 56, 189, 248, 0.0f);
    renderAreaChartToRgb565(chart2_buf_, kChart2W, kChart2H, nullptr, 0, true, 100.0f, 0, 230, 118, 24.0f);

    render3DStageFrame();
    registerPageTapRouting(scr);
    createPageTransitionOverlay(scr);
    stage_timer_ = lv_timer_create([](lv_timer_t* t) {
      Ui480* self = static_cast<Ui480*>(t->user_data);
      if (self) {
        self->tickPeriodicUi();
        self->render3DStageFrame();
      }
    }, 33, this);
  }

  void render(const PowerwallTelemetry& t, const WeatherSnapshot& w, bool configured) {
    last_telemetry_  = t;
    last_weather_    = w;
    last_configured_ = configured;
    refreshAllFromCache();
  }

  // Simulator-only capture hook. The normal firmware always boots on the original page.
  void setDashboardPageForSimulator(uint8_t page) {
    cancelDashboardPageTransition();
    active_dashboard_page_ = page % dashboardPageCount();
    if (active_dashboard_page_ < kNormalDashboardPageCount) previous_normal_page_ = active_dashboard_page_;
    applyDashboardPageVisibility();
    refreshAddedPages();
  }

  // Simulator-only read hook for the Powerwall gauge intro animation checks.
  struct PowerwallGaugeSimState {
    int page; bool intro_active; int32_t arc_value; const char* percent_text;
    lv_point_t marker_outer;
  };
  PowerwallGaugeSimState powerwallGaugeStateForSimulator() const {
    return {active_dashboard_page_, pw_gauge_intro_active_,
            pw_arc_ ? lv_arc_get_value(pw_arc_) : -1,
            pw_soc_text_ ? lv_label_get_text(pw_soc_text_) : "",
            pw_reserve_marker_points_[1]};
  }

  // Touch and PWRKEY share this route; settings returns to the page that opened it.
  void nextDashboardPage() {
    if (active_dashboard_page_ >= kNormalDashboardPageCount) {
      restart_armed_ = false;
      if (restart_button_label_) setLabelText480(restart_button_label_, "重新啟動");
    }
    const uint8_t target = active_dashboard_page_ >= kNormalDashboardPageCount
      ? previous_normal_page_
      : static_cast<uint8_t>((active_dashboard_page_ + 1) % kNormalDashboardPageCount);
    navigateDashboardPage(target);
  }

  void openSettingsPage() {
    if (dashboardPageCount() <= kNormalDashboardPageCount ||
        active_dashboard_page_ >= kNormalDashboardPageCount) return;
    if (page_transition_phase_ != PageTransitionPhase::Idle) return;
    previous_normal_page_ = static_cast<uint8_t>(active_dashboard_page_);
    navigateDashboardPage(kNormalDashboardPageCount);
  }

  void setWeeklyHistory(const WeeklyHistory& history) { weekly_history_ = history; refreshWeeklyPage(); }
  void setDailyTrendHistory(const DailyTrendHistory& history) { daily_trend_history_ = history; }
  using SettingChangedFn = void (*)(void*, uint8_t);
  using TestToneFn = void (*)(void*);
  using RestartFn = void (*)(void*);
  void setDeviceSettingsCallbacks(void* context, SettingChangedFn brightness,
                                  SettingChangedFn volume, TestToneFn test_tone,
                                  void (*voice_changed)(void*, bool) = nullptr) {
    settings_context_ = context;
    brightness_changed_ = brightness;
    volume_changed_ = volume;
    test_tone_ = test_tone;
    voice_changed_ = voice_changed;
  }
  void setRestartCallback(RestartFn restart) { restart_ = restart; }
  void setDeviceSettings(uint8_t brightness, uint8_t volume, bool voice_enabled = true) {
    settings_brightness_ = std::min<uint8_t>(brightness, 100);
    settings_volume_ = std::min<uint8_t>(volume, 100);
    settings_voice_enabled_ = voice_enabled;
    settings_updating_ = true;
    if (settings_brightness_slider_) lv_slider_set_value(settings_brightness_slider_, settings_brightness_, LV_ANIM_OFF);
    if (settings_volume_slider_) lv_slider_set_value(settings_volume_slider_, settings_volume_, LV_ANIM_OFF);
    refreshSettingsLabels();
    settings_updating_ = false;
  }
#if defined(ESP32)
  void setGatewayConfigIp(const char* ip) {
    const String next_ip = ip && ip[0] ? ip : "--";
    if (settings_gateway_config_ip_ == next_ip) return;
    settings_gateway_config_ip_ = next_ip;
    refreshSettingsLabels();
  }
#endif
#if !defined(ESP32)
  uint8_t simulatorDashboardPage() const { return static_cast<uint8_t>(active_dashboard_page_); }
#if !defined(ESP32)
  void setSettingsNetworkPageForSimulator() { showSettingsSubpage(true); }
#endif
  bool simulatorPageTransitionActive() const {
    return page_transition_phase_ != PageTransitionPhase::Idle;
  }
  uint32_t simulatorPageTransitionConsumedClicks() const {
    return page_transition_consumed_clicks_;
  }
  uint8_t simulatorPageTransitionPhase() const {
    return static_cast<uint8_t>(page_transition_phase_);
  }
  uint8_t simulatorPageTransitionOpacity() const { return page_transition_opa_; }
  bool simulatorPageTransitionBlocksInput() const {
    return page_transition_overlay_ &&
      !lv_obj_has_flag(page_transition_overlay_, LV_OBJ_FLAG_HIDDEN) &&
      lv_obj_has_flag(page_transition_overlay_, LV_OBJ_FLAG_CLICKABLE);
  }
  bool simulatorPageTransitionOverlayIsForeground() const {
    return page_transition_overlay_ && dashboard_screen_ &&
      lv_obj_get_parent(page_transition_overlay_) == dashboard_screen_ &&
      lv_obj_get_index(page_transition_overlay_) >
        (page_header_ ? lv_obj_get_index(page_header_) : 0);
  }
  bool simulatorPageTransitionIsIdle() const {
    return page_transition_phase_ == PageTransitionPhase::Idle &&
      (!page_transition_overlay_ ||
       (lv_obj_has_flag(page_transition_overlay_, LV_OBJ_FLAG_HIDDEN) &&
        !lv_obj_has_flag(page_transition_overlay_, LV_OBJ_FLAG_CLICKABLE) &&
        page_transition_opa_ == LV_OPA_TRANSP));
  }
  int simulatorStatusMode() const { return state_mode_idx_; }
  int simulatorWeatherMode() const { return weather_mode_idx_; }
  bool simulatorWeeklyPageIsVisible() const {
    if (active_dashboard_page_ != 4 || !page_week_ || !page_header_ ||
        lv_obj_has_flag(page_week_, LV_OBJ_FLAG_HIDDEN) ||
        !lv_obj_has_flag(page_header_, LV_OBJ_FLAG_HIDDEN) ||
        strcmp(lv_label_get_text(page_title_), native480_i18n::text("7 日用電與電量總表"))) return false;
    for (int d = 0; d < 7; ++d) {
      if (!week_rows_[d] || lv_obj_has_flag(week_rows_[d], LV_OBJ_FLAG_HIDDEN) ||
          !lv_label_get_text(week_dates_[d])[0] ||
          lv_obj_get_y(week_rows_[d]) != d * 480 / 7 ||
          lv_obj_get_height(week_rows_[d]) != (d + 1) * 480 / 7 - d * 480 / 7) return false;
    }
    return true;
  }
  bool simulatorDailyTrendPlotRulesAreValid() const {
    const int jitterBucketA = dailyTrendBucketIndex(6.011f);
    const int jitterBucketB = dailyTrendBucketIndex(6.080f);
    if (jitterBucketA != 72 || jitterBucketB != jitterBucketA ||
        dailyTrendBucketStartHour(jitterBucketA) != 6.0f) return false;
    if (!dailyTrendPlotBucketsConnect(72, 74) || !dailyTrendPlotBucketsConnect(72, 75) ||
        dailyTrendPlotBucketsConnect(72, 76) || dailyTrendPlotBucketsConnect(72, 72)) return false;

    DailyTrendPoint partial{};
    partial.valid_mask = kDailyTrendHomeValid;
    partial.home_kw = 2.5f;
    float value = 0.0f;
    for (int series = 0; series < 4; ++series)
      if (dailyTrendValue(partial, series, value) != (series == 1)) return false;
    DailyTrendPoint empty{};
    for (int series = 0; series < 4; ++series)
      if (dailyTrendValue(empty, series, value)) return false;
    return true;
  }
  bool simulatorDailyTrendMetricTextRulesAreValid() const {
    char text[48];
    formatDailyTrendGridEnergySubtitle(true, 953300.0f, true, 1900.0f, text, sizeof(text));
    if (strcmp(text, native480_i18n::kEnglish ? "In 953.3/Out 1.9kWh" : "購953.3／回1.9kWh") ||
        dailyTrendGridEnergySubtitleFont(text) != native480_fonts::cjk_12()) return false;
    formatDailyTrendGridEnergySubtitle(false, 953300.0f, true, 1900.0f, text, sizeof(text));
    if (strcmp(text, native480_i18n::kEnglish ? "In —/Out 1.9kWh" : "購—／回1.9kWh")) return false;
    formatDailyTrendGridEnergySubtitle(false, 0.0f, false, 0.0f, text, sizeof(text));
    if (strcmp(text, native480_i18n::kEnglish ? "In —/Out —kWh" : "購—／回—kWh")) return false;
    formatDailyTrendGridEnergySubtitle(true, 12345678.0f, true, 4567890.0f, text, sizeof(text));
    const lv_font_t* font = dailyTrendGridEnergySubtitleFont(text);
    lv_point_t extent{};
    lv_txt_get_size(&extent, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    const int letterSpace = dailyTrendGridEnergySubtitleLetterSpace(text, font);
    const int totalWidth = extent.x + letterSpace * std::max(0, dailyTrendUtf8GlyphCount(text) - 1);
    if (font != native480_fonts::cjk_9() || totalWidth > kDailyTrendMetricValueWidth) return false;
    return true;
  }
  bool simulatorTodayTrendPageIsValid() const {
    if (active_dashboard_page_ != 3 || kDailyTrendPagePlotX != 44 ||
        kDailyTrendPlotW != kChart2W || kDailyTrendPlotH != 80 ||
        kDailyTrendCursorY != kDailyTrendRowY[0] || !page_today_trend_ ||
        lv_obj_has_flag(page_today_trend_, LV_OBJ_FLAG_HIDDEN) || !page_header_ ||
        !lv_obj_has_flag(page_header_, LV_OBJ_FLAG_HIDDEN) ||
        !simulatorDailyTrendPlotRulesAreValid() || !simulatorDailyTrendMetricTextRulesAreValid() ||
        !daily_trend_metric_title_[3] || !daily_trend_metric_icon_[0] || !daily_trend_metric_icon_[3] ||
        !daily_trend_metric_value_[0] || !daily_trend_metric_value_[1] ||
        !daily_trend_metric_value_[2] || !daily_trend_metric_value_[3] ||
        strcmp(lv_label_get_text(daily_trend_metric_title_[3]), "Powerwall")) return false;
    for (int i = 0; i < 4; ++i) {
      const int plotHeight = dailyTrendPlotHeight(i);
      const int plotOffsetY = dailyTrendPlotOffsetY(i);
      const int axisFontHeight = native480_fonts::montserrat_12()->line_height;
      const int expectedAxisY[3] = {plotOffsetY, plotOffsetY + (plotHeight - axisFontHeight) / 2,
                                    plotOffsetY + plotHeight - axisFontHeight};
      if (!daily_trend_rows_[i] || !daily_trend_canvas_[i] ||
          lv_obj_has_flag(daily_trend_rows_[i], LV_OBJ_FLAG_HIDDEN) ||
          !daily_trend_latest_dot_[i] || !daily_trend_title_[i] ||
          !simulatorDailyTrendMetricIconIsValid(i) ||
          lv_obj_get_x(daily_trend_metric_title_[i]) != i * kDailyTrendMetricWidth + kDailyTrendMetricTitleX ||
          lv_obj_get_y(daily_trend_metric_title_[i]) != (i < 3 ? -1 : 2) ||
          lv_obj_get_style_text_font(daily_trend_metric_title_[i], LV_PART_MAIN) !=
            (i == 3 ? native480_fonts::montserrat_16() : native480_fonts::grid_cjk_20()) ||
          lv_obj_get_y(daily_trend_rows_[i]) != kDailyTrendRowY[i] ||
          lv_obj_get_height(daily_trend_rows_[i]) != kDailyTrendRowH ||
          lv_obj_get_x(daily_trend_canvas_[i]) != kDailyTrendPlotX ||
          lv_obj_get_y(daily_trend_canvas_[i]) != plotOffsetY ||
          lv_obj_get_height(daily_trend_canvas_[i]) != plotHeight ||
          lv_obj_get_x(daily_trend_rows_[i]) + lv_obj_get_x(daily_trend_canvas_[i]) != kDailyTrendPagePlotX ||
          lv_obj_get_x(daily_trend_title_[i]) != kDailyTrendPagePlotX + kDailyTrendPlotW - 124 ||
          lv_obj_get_y(daily_trend_title_[i]) != (i == 3 ? kDailyTrendRowY[i] - 3 : kDailyTrendRowY[i] + 1)) return false;
      if ((i == 3 && lv_obj_get_y(daily_trend_rows_[i]) + lv_obj_get_y(daily_trend_canvas_[i]) <=
            lv_obj_get_y(daily_trend_title_[i]) + lv_obj_get_height(daily_trend_title_[i])) ||
          (i < 3 && (lv_obj_get_y(daily_trend_canvas_[i]) != 0 ||
                     lv_obj_get_height(daily_trend_canvas_[i]) != kDailyTrendPlotH))) return false;
      for (int axis = 0; axis < 3; ++axis)
        if (!daily_trend_axis_[i][axis]) return false;
        else {
          auto* label = daily_trend_axis_[i][axis];
          const int globalRight = lv_obj_get_x(daily_trend_rows_[i]) + lv_obj_get_x(label) + lv_obj_get_width(label);
          if (lv_obj_get_y(label) != expectedAxisY[axis]) return false;
          const lv_font_t* font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
          lv_point_t textSize{};
          lv_txt_get_size(&textSize, lv_label_get_text(label), font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
          if (globalRight > 40 || textSize.x > lv_obj_get_width(label)) return false;
        }
    }
    const char* solarSubtitle = lv_label_get_text(daily_trend_metric_subtitle_[0]);
    if (strchr(solarSubtitle, '\n') || strstr(solarSubtitle, "估算")) return false;
    const char* homeSubtitle = lv_label_get_text(daily_trend_metric_subtitle_[1]);
    lv_point_t homeSubtitleSize{};
    lv_txt_get_size(&homeSubtitleSize, homeSubtitle, native480_fonts::cjk_12(), 0, 0,
      LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    if (strchr(homeSubtitle, '\n') || homeSubtitleSize.x > kDailyTrendMetricValueWidth) return false;
    const char* gridSubtitle = lv_label_get_text(daily_trend_metric_subtitle_[2]);
    const lv_font_t* gridSubtitleFont = lv_obj_get_style_text_font(daily_trend_metric_subtitle_[2], LV_PART_MAIN);
    lv_point_t gridSubtitleSize{};
    lv_txt_get_size(&gridSubtitleSize, gridSubtitle, gridSubtitleFont, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    const int gridSubtitleSpacing = lv_obj_get_style_text_letter_space(daily_trend_metric_subtitle_[2], LV_PART_MAIN);
    const int gridSubtitleWidth = gridSubtitleSize.x +
      gridSubtitleSpacing * std::max(0, dailyTrendUtf8GlyphCount(gridSubtitle) - 1);
    if (strchr(gridSubtitle, '\n') || gridSubtitleWidth > kDailyTrendMetricValueWidth) return false;
    struct tm localTime{};
    if (!hasValidLocalTime(localTime) || !daily_trend_now_glow_ || !daily_trend_now_line_ ||
        !daily_trend_tou_segments_[0] || !daily_trend_tou_ticks_[0] || !daily_trend_tou_ticks_[4]) return false;
    const int expectedX = kDailyTrendPagePlotX + static_cast<int>(roundf(
      (localTime.tm_hour * 60 + localTime.tm_min) / 1440.0f * (kDailyTrendPlotW - 1)));
    for (int i = 0; i < 3; ++i)
      if (!daily_trend_metric_unit_[i] ||
          (i < 2 && lv_obj_has_flag(daily_trend_metric_unit_[i], LV_OBJ_FLAG_HIDDEN)) ||
          (i < 2 && strcmp(lv_label_get_text(daily_trend_metric_unit_[i]), "kW"))) return false;
    for (int i = 0; i < 5; ++i)
      if (!daily_trend_tou_ticks_[i] ||
          lv_obj_get_x(daily_trend_tou_ticks_[i]) != dailyTrendAxisTickLabelX(i) ||
          strcmp(lv_label_get_text(daily_trend_tou_ticks_[i]), kDailyTrendTimeTicks[i])) return false;
    return lv_obj_get_x(daily_trend_tou_segments_[0]) == kDailyTrendPagePlotX &&
      lv_obj_get_y(daily_trend_tou_segments_[0]) == kDailyTrendTouBarY &&
      lv_obj_get_height(daily_trend_tou_segments_[0]) == kDailyTrendTouBarH &&
      lv_obj_get_x(daily_trend_now_line_) == expectedX &&
      lv_obj_get_x(daily_trend_now_glow_) == expectedX - 2 &&
      lv_obj_get_y(daily_trend_now_line_) == kDailyTrendCursorY &&
      lv_obj_get_y(daily_trend_now_glow_) == kDailyTrendCursorY &&
      lv_obj_get_height(daily_trend_now_line_) == kDailyTrendCursorHeight &&
      lv_obj_get_height(daily_trend_now_glow_) == kDailyTrendCursorHeight &&
      kDailyTrendCursorY + kDailyTrendCursorHeight == kDailyTrendTouBarY + kDailyTrendTouBarH + 4 &&
      lv_obj_get_y(daily_trend_tou_ticks_[0]) == kDailyTrendTimeTicksY;
  }

  bool simulatorSettingsNetworkPageIsVisible() const {
    return active_dashboard_page_ == 5 && settings_network_page_ &&
      !lv_obj_has_flag(settings_network_page_, LV_OBJ_FLAG_HIDDEN);
  }
#if !defined(ESP32)
  bool simulatorVoiceEnabled() const { return settings_voice_enabled_; }
  bool simulatorSettingsUptimeLabelIsVisible() const {
    return active_dashboard_page_ == 5 && settings_uptime_label_ &&
      !lv_obj_has_flag(settings_uptime_label_, LV_OBJ_FLAG_HIDDEN) &&
      strncmp(lv_label_get_text(settings_uptime_label_), native480_i18n::text("開機時間: "),
              strlen(native480_i18n::text("開機時間: "))) == 0;
  }
  bool simulatorSettingsVersionLabelIsVisible() const {
    return active_dashboard_page_ == 5 && settings_version_label_ &&
      !lv_obj_has_flag(settings_version_label_, LV_OBJ_FLAG_HIDDEN) &&
      strcmp(lv_label_get_text(settings_version_label_), native480_i18n::kManualVersion) == 0;
  }
  bool simulatorSettingsTouchTargetsMeetMinimum() const {
    return settings_brightness_slider_ && settings_volume_slider_ && settings_voice_switch_ &&
      settings_voice_test_button_ && restart_button_ &&
      lv_obj_get_height(settings_brightness_slider_) + 20 >= 48 &&
      lv_obj_get_height(settings_volume_slider_) + 20 >= 48 &&
      lv_obj_get_height(settings_voice_switch_) + 20 >= 48 &&
      lv_obj_get_height(settings_voice_test_button_) >= 52 &&
      lv_obj_get_height(restart_button_) >= 52;
  }
#endif
  void setSimulatorNetworkAvailable(bool available) {
    simulator_network_available_ = available;
    refreshSettingsLabels();
  }
  void setSimulatorWifiSsid(const char* ssid) {
    const char* value = (ssid && ssid[0]) ? ssid : "Home-WiFi";
    std::snprintf(simulator_wifi_ssid_, sizeof(simulator_wifi_ssid_), "%s", value);
    refreshSettingsLabels();
  }
  bool simulatorNetworkPreviewMatches(bool available) const {
    return settings_wifi_title_ && settings_wifi_ssid_value_ && settings_http_title_ &&
      settings_gateway_url_label_ &&
      strcmp(lv_label_get_text(settings_wifi_title_), native480_i18n::text("Wi-Fi 網路")) == 0 &&
      strcmp(lv_label_get_text(settings_wifi_ssid_value_),
             available ? simulator_wifi_ssid_ : "powerwall-dashboard") == 0 &&
      strcmp(lv_label_get_text(settings_http_title_), native480_i18n::text(
             "設定頁網址")) == 0 &&
      strcmp(lv_label_get_text(settings_gateway_url_label_),
             available ? "http://192.0.2.15/" : "http://192.168.4.1/") == 0;
  }
  bool simulatorAddedHeaderIsVisible() const {
    if (active_dashboard_page_ == 1) {
      const bool live = (state_mode_idx_ == 0 || state_mode_idx_ == 1) &&
                        last_configured_ && last_telemetry_.ok &&
                        !last_telemetry_.stale && !last_telemetry_.demo;
      const lv_color_t expectedStatus = lv_color_hex(live ? 0x34d399 : 0xff5252);
      const bool trendChartsValid =
        pw_chart_canvas_ && lv_obj_get_x(pw_chart_canvas_) == kPwChartPlotX &&
        lv_obj_get_y(pw_chart_canvas_) == kPowerwallTrendSocY &&
        lv_obj_get_width(pw_chart_canvas_) == kChartW &&
        lv_obj_get_height(pw_chart_canvas_) == kPowerwallTrendSocH &&
        pw_load_chart_canvas_ && lv_obj_get_x(pw_load_chart_canvas_) == kPwChartPlotX &&
        lv_obj_get_y(pw_load_chart_canvas_) == kPowerwallTrendLoadY &&
        lv_obj_get_width(pw_load_chart_canvas_) == kChartW &&
        lv_obj_get_height(pw_load_chart_canvas_) == kPowerwallTrendLoadH;
      return headerClockDotLayoutIsValid(pw_header_clock_, pw_header_status_dot_,
          native480_i18n::kEnglish ? native480_fonts::montserrat_16() : native480_fonts::cjk_13(), 201) &&
        page_header_ && pw_header_title_ && pw_header_clock_ && pw_header_date_ &&
        pw_header_status_dot_ && page_site_ && page_status_pill_ && page_clock_ && page_date_ &&
        !lv_obj_has_flag(page_header_, LV_OBJ_FLAG_HIDDEN) &&
        !lv_obj_has_flag(pw_header_title_, LV_OBJ_FLAG_HIDDEN) &&
        !lv_obj_has_flag(pw_header_clock_, LV_OBJ_FLAG_HIDDEN) &&
        lv_obj_has_flag(pw_header_date_, LV_OBJ_FLAG_HIDDEN) &&
        !lv_obj_has_flag(pw_header_status_dot_, LV_OBJ_FLAG_HIDDEN) &&
        strcmp(lv_label_get_text(pw_header_title_), "POWERWALL") == 0 &&
        lv_obj_has_flag(page_site_, LV_OBJ_FLAG_HIDDEN) &&
        lv_obj_has_flag(page_status_pill_, LV_OBJ_FLAG_HIDDEN) &&
        lv_obj_has_flag(page_clock_, LV_OBJ_FLAG_HIDDEN) &&
        lv_obj_has_flag(page_date_, LV_OBJ_FLAG_HIDDEN) &&
        lv_obj_get_style_bg_color(pw_header_status_dot_, 0).full == expectedStatus.full &&
        pw_chart_card_ && (lv_obj_get_x(pw_chart_card_) == 9) && (lv_obj_get_y(pw_chart_card_) == 303) &&
        (lv_obj_get_height(pw_chart_card_) == 171) &&
        trendChartsValid &&
        pw_now_line_ && (lv_obj_get_y(pw_now_line_) == 7) &&
        (lv_obj_get_height(pw_now_line_) == 147) &&
        pw_tou_segments_[0] && (lv_obj_get_y(pw_tou_segments_[0]) == 148) &&
        (lv_obj_get_height(pw_tou_segments_[0]) == 5) &&
        pw_time_ticks_[0] && strcmp(lv_label_get_text(pw_time_ticks_[0]), "00") == 0 &&
        pw_time_ticks_[1] && strcmp(lv_label_get_text(pw_time_ticks_[1]), "06") == 0 &&
        pw_time_ticks_[2] && strcmp(lv_label_get_text(pw_time_ticks_[2]), "12") == 0 &&
        pw_time_ticks_[3] && strcmp(lv_label_get_text(pw_time_ticks_[3]), "18") == 0 &&
        pw_time_ticks_[4] && strcmp(lv_label_get_text(pw_time_ticks_[4]), "24") == 0 &&
        (lv_obj_get_y(pw_time_ticks_[0]) == 154) && !pw_data_stamp_;
    }
    const bool live = (state_mode_idx_ == 0 || state_mode_idx_ == 1) &&
                      last_configured_ && last_telemetry_.ok &&
                      !last_telemetry_.stale && !last_telemetry_.demo;
    lv_area_t plot{};
    lv_obj_get_coords(home_chart_canvas_, &plot);
    lv_point_t titleSize{};
    const lv_font_t* const titleFont = native480_i18n::kEnglish
      ? native480_fonts::home_cjk_27() : native480_fonts::grid_cjk_20();
    const char* const titleText = native480_i18n::text("住家 / 電網");
    lv_txt_get_size(&titleSize, titleText, titleFont,
                    0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return active_dashboard_page_ == 2 && page_header_ && page_home_grid_ &&
      !lv_obj_has_flag(page_header_, LV_OBJ_FLAG_HIDDEN) &&
      lv_obj_has_flag(page_title_, LV_OBJ_FLAG_HIDDEN) &&
      lv_obj_has_flag(page_site_, LV_OBJ_FLAG_HIDDEN) &&
      lv_obj_has_flag(page_status_pill_, LV_OBJ_FLAG_HIDDEN) &&
      lv_obj_has_flag(page_clock_, LV_OBJ_FLAG_HIDDEN) &&
      lv_obj_has_flag(page_date_, LV_OBJ_FLAG_HIDDEN) &&
      !lv_obj_has_flag(pw_header_title_, LV_OBJ_FLAG_HIDDEN) &&
      !lv_obj_has_flag(pw_header_clock_, LV_OBJ_FLAG_HIDDEN) &&
      lv_obj_has_flag(pw_header_date_, LV_OBJ_FLAG_HIDDEN) &&
      !lv_obj_has_flag(pw_header_status_dot_, LV_OBJ_FLAG_HIDDEN) &&
      strcmp(lv_label_get_text(pw_header_title_), titleText) == 0 &&
      lv_obj_get_style_text_font(pw_header_title_, 0) == titleFont &&
      titleSize.x <= lv_obj_get_width(pw_header_title_) &&
      lv_obj_get_x(pw_header_title_) + titleSize.x < lv_obj_get_x(pw_header_status_dot_) &&
      headerClockDotLayoutIsValid(pw_header_clock_, pw_header_status_dot_,
          native480_i18n::kEnglish ? native480_fonts::montserrat_16() : native480_fonts::cjk_13(), 201) &&
      lv_obj_get_style_bg_color(pw_header_status_dot_, 0).full ==
        lv_color_hex(live ? 0x34d399 : 0xff5252).full &&
      plot.x1 == 37 && plot.x2 == 460 && plot.y1 == kHomeChartCardY + kHomeChartPlotY &&
      plot.y2 == kHomeChartCardY + kHomeChartPlotY + kHomeChartPlotH - 1 &&
      lv_obj_get_width(home_chart_canvas_) == kHomeChartW &&
      lv_obj_get_height(home_chart_canvas_) == kHomeChartPlotH &&
      lv_obj_get_y(home_now_line_) == kHomeChartPlotY &&
      lv_obj_get_height(home_now_line_) == kHomeChartPlotH &&
      home_chart_tou_segments_[0] &&
      lv_obj_get_y(home_chart_tou_segments_[0]) == kHomeChartTouY &&
      lv_obj_get_height(home_chart_tou_segments_[0]) == kHomeChartTouH &&
      home_chart_ticks_[0] && lv_obj_get_y(home_chart_ticks_[0]) == kHomeChartTimeTicksY &&
      lv_obj_get_index(page_header_) > lv_obj_get_index(page_home_grid_);
  }

  // Exercise the actual label fonts/widths with missing and long meter values.
  bool simulatorHomeGridTextFits() {
    lv_obj_t* touCard = lv_obj_get_parent(home_tou_pill_);
    lv_obj_t* gridCard = lv_obj_get_parent(home_grid_heading_);
    lv_obj_t* homeCard = lv_obj_get_parent(home_grid_home_heading_);
    lv_obj_t* chartCard = lv_obj_get_parent(home_chart_canvas_);
    const int intervalStartX = lv_obj_get_x(home_tou_pill_) + lv_obj_get_width(home_tou_pill_) +
                               kHomeTouIntervalPillGap;
    const int intervalDashX = intervalStartX + lv_obj_get_width(home_interval_start_) +
                              kHomeTouIntervalDashGap;
    const int intervalEndX = intervalDashX + kHomeTouIntervalDashW + kHomeTouIntervalDashGap;
    if (!touCard || !gridCard || !homeCard || !chartCard ||
        lv_obj_get_y(touCard) != kHomeTouCardY || lv_obj_get_height(touCard) != kHomeTouCardH ||
        lv_obj_get_y(home_tou_pill_) != kHomeTouPillY || lv_obj_get_height(home_tou_pill_) != kHomeTouPillH ||
        lv_obj_get_x(home_tou_pill_text_) != 0 || lv_obj_get_width(home_tou_pill_text_) != 108 ||
        !dashboardLabelInkIsVerticallyCentered(home_tou_pill_, home_tou_pill_text_) ||
        lv_obj_get_x(home_interval_start_) != intervalStartX ||
        lv_obj_get_y(home_interval_start_) != kHomeTouIntervalTextY ||
        lv_obj_get_width(home_interval_start_) <= 0 ||
        lv_obj_get_x(home_interval_rule_) != intervalDashX ||
        lv_obj_get_y(home_interval_rule_) != kHomeTouIntervalDashY ||
        lv_obj_get_width(home_interval_rule_) != kHomeTouIntervalDashW ||
        lv_obj_get_height(home_interval_rule_) != 1 ||
        lv_obj_get_x(home_interval_end_) != intervalEndX ||
        lv_obj_get_y(home_interval_end_) != kHomeTouIntervalTextY ||
        lv_obj_get_width(home_interval_end_) <= 0 ||
        intervalEndX + lv_obj_get_width(home_interval_end_) >= lv_obj_get_content_width(touCard) ||
        lv_obj_get_y(home_price_) != kHomeTouPriceY ||
        lv_obj_get_y(home_countdown_) != kHomeTouCountdownY ||
        lv_obj_get_y(home_tou_segments_[0]) != kHomeTouSegmentY ||
        lv_obj_get_height(home_tou_segments_[0]) != 12 ||
        lv_obj_get_y(home_tou_now_line_) != kHomeTouNowLineY ||
        lv_obj_get_height(home_tou_now_line_) != 21 ||
        lv_obj_get_y(home_tou_now_dot_) != kHomeTouNowDotY ||
        lv_obj_get_y(home_tou_ticks_[0]) != kHomeTouTicksY ||
        lv_obj_get_y(gridCard) != kHomeGridCardY || lv_obj_get_height(gridCard) != kHomeGridCardH ||
        lv_obj_get_y(homeCard) != kHomeMetricsCardY || lv_obj_get_height(homeCard) != kHomeMetricsCardH ||
        lv_obj_get_y(chartCard) != kHomeChartCardY || lv_obj_get_height(chartCard) != kHomeChartCardH ||
        lv_obj_get_y(home_chart_canvas_) != kHomeChartPlotY ||
        lv_obj_get_height(home_chart_canvas_) != kHomeChartPlotH) {
      printf("[HomeGridText] FAIL: compact card or plot geometry\n");
      return false;
    }
    const char* expectedPeak = native480_i18n::kEnglish ? "Peak" : "目前：尖峰";
    const char* expectedOffPeak = native480_i18n::kEnglish ? "Off-peak" : "目前：離峰";
    if (strcmp(native480_i18n::text("目前：尖峰"), expectedPeak) != 0 ||
        strcmp(native480_i18n::text("目前：離峰"), expectedOffPeak) != 0) {
      printf("[HomeGridText] FAIL: peak/off-peak pill translations\n");
      return false;
    }
    const char* currentPillText = lv_label_get_text(home_tou_pill_text_);
    const char* syncPillText = native480_i18n::kEnglish ? "Syncing time" : "時間同步中";
    if (strcmp(currentPillText, expectedPeak) != 0 &&
        strcmp(currentPillText, expectedOffPeak) != 0 &&
        strcmp(currentPillText, syncPillText) != 0) {
      printf("[HomeGridText] FAIL: active TOU pill text '%s'\n", currentPillText);
      return false;
    }
    const int axisFontH = native480_fonts::montserrat_12()->line_height;
    if (lv_obj_get_y(home_chart_y_labels_[0]) != kHomeChartPlotY - axisFontH / 2 ||
        lv_obj_get_y(home_chart_y_labels_[1]) != kHomeChartPlotY + (kHomeChartPlotH - axisFontH) / 2 ||
        lv_obj_get_y(home_chart_y_labels_[2]) != kHomeChartPlotY + kHomeChartPlotH - 1 - axisFontH / 2) {
      printf("[HomeGridText] FAIL: plot y-axis alignment\n");
      return false;
    }
    struct CenteredMetric { lv_obj_t* label; int x; int width; const char* sample; };
    const CenteredMetric centered[] = {
      {home_grid_power_value_, 8, 151, "12.99 kW"},
      {home_grid_current_value_, 160, 141, "128.1 A"},
      {home_grid_voltage_value_, 302, 151, "240.0 V"},
      {home_load_value_, 8, 151, "12.99 kW"},
      {home_load_current_value_, 160, 141, "128.1 A"},
      {home_load_voltage_value_, 302, 151, "240.0 V"},
    };
    for (const auto& metric : centered) {
      if (!metric.label || lv_obj_get_x(metric.label) != metric.x ||
          lv_obj_get_width(metric.label) != metric.width ||
          lv_obj_get_style_text_align(metric.label, LV_PART_MAIN) != LV_TEXT_ALIGN_CENTER) {
        printf("[HomeGridText] FAIL: centered metric geometry/alignment at x=%d width=%d\n",
               metric.x, metric.width);
        return false;
      }
      const auto* font = lv_obj_get_style_text_font(metric.label, LV_PART_MAIN);
      lv_point_t size{};
      lv_txt_get_size(&size, metric.sample, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
      if (size.x > metric.width) {
        printf("[HomeGridText] FAIL: %s needs %dpx, available %dpx\n",
               metric.sample, size.x, metric.width);
        return false;
      }
    }
    const int centeredTotalsValueY = homeGridCumulativeValueY();
    if (lv_obj_get_y(home_grid_power_value_) != 42 ||
        lv_obj_get_y(home_grid_current_value_) != 42 ||
        lv_obj_get_y(home_grid_voltage_value_) != 42 ||
        lv_obj_get_y(home_load_value_) != 41 ||
        lv_obj_get_y(home_load_current_value_) != 41 ||
        lv_obj_get_y(home_load_voltage_value_) != 41 ||
        lv_obj_get_y(home_grid_import_value_) != centeredTotalsValueY ||
        lv_obj_get_y(home_grid_export_value_) != centeredTotalsValueY ||
        lv_obj_get_y(home_power_factor_) != 71 || lv_obj_get_y(home_energy_value_) != 71 ||
        lv_obj_get_y(home_metrics_h_rules_[1]) != 65 ||
        lv_obj_get_y(home_section_bottom_rules_[2]) != kHomeMetricsCardH - 1 ||
        lv_obj_get_y(home_power_factor_) -
          (lv_obj_get_y(home_metrics_h_rules_[1]) + lv_obj_get_height(home_metrics_h_rules_[1])) < 5 ||
        lv_obj_get_y(home_section_bottom_rules_[2]) -
          (lv_obj_get_y(home_power_factor_) + lv_obj_get_height(home_power_factor_)) < 4) {
      printf("[HomeGridText] FAIL: compact metric row geometry\n");
      return false;
    }
    const int gridRuleY[] = {27, kHomeGridTotalsSeparatorY};
    const int metricsRuleY[] = {27, 65};
    for (int i = 0; i < 2; ++i) {
      if (!home_grid_h_rules_[i] || lv_obj_get_x(home_grid_h_rules_[i]) != 8 ||
          lv_obj_get_y(home_grid_h_rules_[i]) != gridRuleY[i] ||
          lv_obj_get_width(home_grid_h_rules_[i]) != 445 || lv_obj_get_height(home_grid_h_rules_[i]) != 1 ||
          !home_metrics_h_rules_[i] || lv_obj_get_x(home_metrics_h_rules_[i]) != 8 ||
          lv_obj_get_y(home_metrics_h_rules_[i]) != metricsRuleY[i] ||
          lv_obj_get_width(home_metrics_h_rules_[i]) != 445 || lv_obj_get_height(home_metrics_h_rules_[i]) != 1) {
        printf("[HomeGridText] FAIL: compact horizontal separators\n");
        return false;
      }
    }
    lv_obj_t* sectionCards[] = {touCard, gridCard, homeCard};
    const int sectionHeights[] = {kHomeTouCardH, kHomeGridCardH, kHomeMetricsCardH};
    for (int i = 0; i < 3; ++i) {
      if (!home_section_bottom_rules_[i] || lv_obj_get_x(home_section_bottom_rules_[i]) != 0 ||
          lv_obj_get_y(home_section_bottom_rules_[i]) != sectionHeights[i] - 1 ||
          lv_obj_get_width(home_section_bottom_rules_[i]) != 461 ||
          lv_obj_get_height(home_section_bottom_rules_[i]) != 1 ||
          lv_obj_get_style_bg_color(home_section_bottom_rules_[i], 0).full != lv_color_hex(0x36deda).full ||
          lv_obj_get_parent(home_section_bottom_rules_[i]) != sectionCards[i]) {
        printf("[HomeGridText] FAIL: cyan section bottom rule geometry\n");
        return false;
      }
    }
    const lv_font_t* const font16 = native480_fonts::grid_cjk_16();
    const lv_font_t* const font14 = native480_fonts::cjk_14();
    if (!home_grid_heading_ || !home_grid_home_heading_ || !home_grid_status_ ||
        !home_solar_value_ || !home_tou_pill_text_ || !home_grid_import_title_ ||
        !home_grid_export_title_ ||
        lv_obj_get_style_text_font(home_grid_status_, 0) != font16 ||
        lv_obj_get_style_text_font(home_solar_value_, 0) != font16 ||
        lv_obj_get_style_text_font(home_tou_pill_text_, 0) != font16 ||
        lv_obj_get_style_text_font(home_countdown_, 0) != font14 ||
        lv_obj_get_style_text_font(home_grid_import_title_, 0) != font14 ||
        lv_obj_get_style_text_font(home_grid_export_title_, 0) != font14 ||
        lv_obj_get_style_text_font(home_grid_import_value_, 0) != font16 ||
        lv_obj_get_style_text_font(home_grid_export_value_, 0) != font16 ||
        lv_obj_get_x(home_grid_heading_) != 34 || lv_obj_get_x(home_grid_home_heading_) != 34) {
      printf("[HomeGridText] FAIL: heading or requested font size\n");
      return false;
    }
    static const char* fixedHomeTicks[] = {"00", "06", "12", "18", "24"};
    for (int i = 0; i < 5; ++i) {
      if (!home_chart_ticks_[i] || strcmp(lv_label_get_text(home_chart_ticks_[i]), fixedHomeTicks[i]) != 0 ||
          lv_obj_get_style_text_font(home_chart_ticks_[i], 0) != native480_fonts::montserrat_13()) {
        printf("[HomeGridText] FAIL: fixed home chart tick %d\n", i);
        return false;
      }
    }
    static const char* fixedYAxis[] = {"6kW", "3kW", "0"};
    for (int i = 0; i < 3; ++i) {
      lv_point_t size{};
      if (!home_chart_y_labels_[i] ||
          lv_obj_get_style_text_align(home_chart_y_labels_[i], 0) != LV_TEXT_ALIGN_RIGHT ||
          strcmp(lv_label_get_text(home_chart_y_labels_[i]), fixedYAxis[i]) != 0) return false;
      lv_txt_get_size(&size, fixedYAxis[i], native480_fonts::montserrat_12(), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
      if (size.x > lv_obj_get_width(home_chart_y_labels_[i])) {
        printf("[HomeGridText] FAIL: %s y-axis label needs %dpx, available %dpx\n", fixedYAxis[i], size.x, lv_obj_get_width(home_chart_y_labels_[i]));
        return false;
      }
    }
    for (const auto& points : home_grid_icon_points_) for (const auto& point : points) {
      if (point.x < 0 || point.x > 19 || point.y < 0 || point.y > 19) return false;
    }
    for (const auto& points : home_house_icon_points_) for (const auto& point : points) {
      if (point.x < 0 || point.x > 19 || point.y < 0 || point.y > 19) return false;
    }
    struct Sample { lv_obj_t* label; const char* text; };
    const Sample samples[] = {
      {home_grid_power_value_, "12.99 kW"}, {home_grid_power_value_, "— kW"},
      {home_grid_power_title_, "回送功率"},
      {home_grid_current_value_, "128.1 A"}, {home_grid_voltage_value_, "240.0 V"},
      {home_grid_import_value_, "985.75 kWh"},
      {home_grid_export_value_, "2.15 kWh"},
      {home_grid_import_value_, "99999.99 kWh"},
      {home_grid_export_value_, "99999.99 kWh"},
      {home_grid_status_, "已併網"}, {home_tou_pill_text_, lv_label_get_text(home_tou_pill_text_)},
      {home_energy_value_, "累計用電 99999.99 kWh"},
      {home_solar_value_, "太陽能 12.99 kW"},
      {home_countdown_, "下一次離峰 12/31 23:00 · 還有 23小時59分"},
      {home_price_, native480_i18n::text("NT$5.01 / 度")}, {home_chart_ticks_[4], "24"},
    };
    for (const auto& sample : samples) {
      const auto* font = lv_obj_get_style_text_font(sample.label, 0);
      lv_point_t size{};
      lv_txt_get_size(&size, sample.text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
      if (size.x > lv_obj_get_width(sample.label)) {
        printf("[HomeGridText] FAIL: %s needs %dpx, available %dpx\n",
               sample.text, size.x, lv_obj_get_width(sample.label));
        return false;
      }
    }
    return true;
  }
  lv_opa_t simulatorPowerwallStatusDotOpacity() const {
    return pw_header_status_dot_ ? lv_obj_get_style_bg_opa(pw_header_status_dot_, 0) : LV_OPA_TRANSP;
  }
  void simulatorPowerwallStatusDotBounds(lv_area_t& bounds) const {
    if (pw_header_status_dot_) lv_obj_get_coords(pw_header_status_dot_, &bounds);
  }
  uint32_t simulatorPowerflowStartedAt() const { return route_.route_start_ms; }
  uint8_t simulatorPowerflowMask() const {
    uint8_t mask = 0;
    for (int wire = 0; wire < 4; ++wire) if (route_.branches[wire].enabled) mask |= 1 << wire;
    return mask;
  }
  uint64_t simulatorWireChecksum(uint8_t wire) const {
    uint64_t hash = 1469598103934665603ULL;
    for (size_t i = 0; i < kStageWirePixelCount; ++i) {
      const auto& pixel = kStageWirePixels[i];
      if (pixel.wid != wire) continue;
      hash = (hash ^ stage_canvas_buf_[pixel.y * kStageW + pixel.x].full) * 1099511628211ULL;
    }
    return hash;
  }
  bool simulatorHome480Valid(bool live) const {
    const lv_font_t* const clockFont = native480_i18n::kEnglish
      ? native480_fonts::montserrat_16() : native480_fonts::cjk_13();
    if (!headerClockDotLayoutIsValid(home480_clock_, dot_status_, clockFont, 201) || !home480_reserve_ ||
        lv_obj_get_style_bg_color(dot_status_, 0).full != lv_color_hex(live ? 0x34d399 : 0xff5252).full ||
        lv_obj_is_visible(pill_status_) ||
        lv_obj_is_visible(pill_weather_) ||
        !lv_obj_has_flag(chart1_canvas_, LV_OBJ_FLAG_HIDDEN)) {
      fprintf(stderr, "[Home480] indicator/visibility: color=%u expected=%u status=%d weather=%d chart=%d\n",
        unsigned(lv_obj_get_style_bg_color(dot_status_, 0).full), unsigned(lv_color_hex(live ? 0x34d399 : 0xff5252).full),
        lv_obj_is_visible(pill_status_), lv_obj_is_visible(pill_weather_), lv_obj_is_visible(chart1_canvas_));
      return false;
    }
    for (auto* obj : {lbl_site_title_, home480_clock_, dot_status_, lbl_solar_val_, lbl_home_val_,
                      pw_val_row_, lbl_stat_pw_eta_, lbl_grid_val_}) {
      lv_area_t bounds; lv_obj_get_coords(obj, &bounds);
      if (bounds.x1 < 0 || bounds.x2 >= 480 || bounds.y1 < 0 || bounds.y2 >= 379) {
        fprintf(stderr, "[Home480] bounds: %d,%d..%d,%d\n", bounds.x1, bounds.y1, bounds.x2, bounds.y2); return false;
      }
    }
    lv_area_t reserve_bounds, chart_bounds;
    lv_obj_get_coords(home480_reserve_, &reserve_bounds);
    lv_obj_get_coords(chart2_canvas_, &chart_bounds);
    if (last_telemetry_.history_count > 0) {
      lv_area_t dot_bounds; lv_obj_get_coords(chart2_now_dot_, &dot_bounds);
      if (lv_obj_has_flag(chart2_now_dot_, LV_OBJ_FLAG_HIDDEN) ||
          dot_bounds.y1 < chart_bounds.y1 - 1 || dot_bounds.y2 > chart_bounds.y2 + 3) {
        fprintf(stderr, "[Home480] current point outside plot: %d..%d (plot %d..%d)\n",
                dot_bounds.y1, dot_bounds.y2, chart_bounds.y1, chart_bounds.y2);
        return false;
      }
    }
    if (!(lv_obj_get_x(chart_card_) == 0) || !(lv_obj_get_width(chart_card_) == 480) ||
        !(chart_bounds.x1 == 44) || !(chart_bounds.x2 == 471)) return false;
    for (auto* band : {tou_bar_we_, tou_bar_wk_, tou_bar_wk_summer_}) {
      if (!lv_obj_is_visible(band)) continue;
      lv_area_t bounds; lv_obj_get_coords(band, &bounds);
      if (bounds.x1 != chart_bounds.x1 || !(bounds.x2 == 471)) return false;
    }
    for (auto* marker : {cursor_line_, chart2_now_dot_}) {
      if (!lv_obj_is_visible(marker)) continue;
      lv_area_t bounds; lv_obj_get_coords(marker, &bounds);
      if (bounds.x1 < 0 || bounds.x2 > chart_bounds.x2) return false;
    }
    if (reserve_bounds.x1 < chart_bounds.x1 || reserve_bounds.x2 > chart_bounds.x2 ||
        reserve_bounds.y1 < chart_bounds.y1 || reserve_bounds.y2 > chart_bounds.y2 ||
        strncmp(lv_label_get_text(home480_reserve_), native480_i18n::text("備援 "),
                strlen(native480_i18n::text("備援 "))) != 0) return false;
    if (!(lv_obj_get_y(pw_val_row_) == 323) || !(lv_obj_get_y(lbl_grid_val_) == 342) ||
        !(lv_obj_get_y(lbl_stat_pw_eta_) == 353) || !(lv_obj_get_y(stage_canvas_) == 35) ||
        !(lv_obj_get_x(stage_canvas_) == 21) ||
        !(lv_obj_get_y(lbl_solar_val_) == 47) || !(lv_obj_get_y(lbl_home_val_) == 127)) return false;
    for (auto* axis : {home480_axis_top_, home480_axis_bottom_}) {
      lv_area_t bounds; lv_obj_get_coords(axis, &bounds);
      if (bounds.x2 >= chart_bounds.x1) return false;
      const int center_y = (bounds.y1 + bounds.y2 + 1) / 2;
      const int expected_y = chart_bounds.y1 + (axis == home480_axis_top_ ? 3 : kChart2H - 2);
      if (abs(center_y - expected_y) > 1) return false;
    }
    for (auto* tick : home480_ticks_) {
      lv_area_t bounds; lv_obj_get_coords(tick, &bounds);
      if (bounds.x1 < 0 || bounds.x2 >= 480 || bounds.y2 >= 480) return false;
    }
    const auto* clock = lv_label_get_text(home480_clock_);
    const bool clockFormatValid = native480_i18n::kEnglish
      ? strlen(clock) == 15 && clock[2] == '/' && clock[12] == ':'
      : strlen(clock) == 16 && strstr(clock, "月") && strstr(clock, "日") && clock[13] == ':';
    return clockFormatValid && lv_obj_get_style_bg_opa(dot_status_, 0) != LV_OPA_TRANSP;
  }
  void simulatorHome480PlotBounds(lv_area_t& bounds) const {
    lv_obj_get_coords(chart2_canvas_, &bounds);
  }
  lv_opa_t simulatorHome480PointOpacity() const {
    return lv_obj_get_style_opa(chart2_now_dot_, 0);
  }
  uint64_t simulatorStageChecksum() const {
    uint64_t hash = 1469598103934665603ULL;
    for (int i = 0; i < kStageW * kStageH; ++i) {
      hash ^= stage_canvas_buf_[i].full; hash *= 1099511628211ULL;
    }
    return hash;
  }
  bool simulatorTapPoint(uint8_t target, int16_t& x, int16_t& y) const {
    if (target == 0) {
      x = 240;
      y = 240;
      return true;
    }
    lv_obj_t* control = target == 1 ? pill_status_ : target == 2 ? pill_weather_ : nullptr;
    if (!control) return false;
    lv_area_t coords;
    lv_obj_get_coords(control, &coords);
    x = static_cast<int16_t>((coords.x1 + coords.x2) / 2);
    y = static_cast<int16_t>((coords.y1 + coords.y2) / 2);
    return true;
  }
#endif

  void setAnimationActive(bool active) {
    if (!stage_timer_) return;
    if (active) { lv_timer_resume(stage_timer_); lv_timer_ready(stage_timer_); }
    else lv_timer_pause(stage_timer_);
  }

#if !defined(POWERWALL_NATIVE480_RELEASE)
  bool handlePhaseConsoleLine(const char* line, char* response, size_t response_capacity) {
    PhaseConsoleCommand command;
    if (!parsePhaseConsoleCommand(line, command)) {
      if (response && response_capacity) {
        snprintf(response, response_capacity, "[phase] invalid key; use s, m, w, z, or h");
      }
      return false;
    }

    if (command.kind == PhaseCommandKind::Help) {
      if (response && response_capacity) snprintf(response, response_capacity, "%s", phaseConsoleHelpText());
      return true;
    }

    switch (command.kind) {
      case PhaseCommandKind::Live:
        phase_sun_override_active_ = false;
        phase_moon_override_active_ = false;
        phase_sun_next_index_ = 0;
        phase_moon_next_index_ = 0;
        weather_mode_idx_ = 0;
        state_mode_idx_ = 0;
        refreshAllFromCache();
        render3DStageFrame();
        break;
      case PhaseCommandKind::SunNext:
        phase_sun_override_active_ = true;
        phase_sun_override_pct_ = phaseConsoleSunPercent(phase_sun_next_index_);
        phase_sun_next_index_ = (phase_sun_next_index_ + 1) % 9;
        phase_moon_override_active_ = false;
        refreshAllFromCache();
        render3DStageFrame();
        break;
      case PhaseCommandKind::MoonNext:
        phase_moon_override_active_ = true;
        phase_moon_override_pct_ = phaseConsoleMoonPercent(phase_moon_next_index_);
        phase_moon_next_index_ = (phase_moon_next_index_ + 1) % 8;
        phase_sun_override_active_ = false;
        refreshAllFromCache();
        render3DStageFrame();
        break;
      case PhaseCommandKind::WeatherNext:
        phase_sun_override_active_ = false;
        phase_moon_override_active_ = false;
        weather_mode_idx_ = weather_mode_idx_ % 10 + 1;
        refreshAllFromCache();
        render3DStageFrame();
        break;
      case PhaseCommandKind::Help:
      case PhaseCommandKind::Invalid:
        break;
    }

    if (response && response_capacity) {
      const char* mode = active_is_night_ ? "night" : "day";
      const char* source = phase_moon_override_active_ ? "moon-preview" :
                           phase_sun_override_active_ ? "sun-preview" :
                           weather_mode_idx_ != 0 ? "weather-preview" : "auto";
      const char active_preset_key = phase_moon_override_active_ ? 'm' :
                                     phase_sun_override_active_ ? 's' :
                                     weather_mode_idx_ != 0 ? 'w' :
                                     (state_mode_idx_ == 0 && weather_mode_idx_ == 0 ? 'z' : '\0');
      const char* preset_name = phase_moon_override_active_ ? phaseConsoleMoonName(phase_moon_override_pct_) :
                                phaseConsolePresetName(active_preset_key);
      const float moon_pct = phase_moon_override_active_
        ? phase_moon_override_pct_
        : active_moon_phase_ * 100.0f;
      const char* sun_mode = phase_sun_override_active_ ? "preview" : "auto";
      const char* moon_mode = phase_moon_override_active_ ? "preview" : "auto";
      const GodotMoonPhaseParams godot_phase = mapGodotMoonPhase(active_moon_phase_);
      snprintf(response, response_capacity,
               "[phase] key=%c preset=%s mode=%s scene=%s source=%s sun=%d(%s) moon=%.2f(%s,%s) godot_phase=%.2f crescent=%d cloud=%d",
               active_preset_key ? active_preset_key : '-', preset_name,
               mode, active_stage_key_.c_str(), source,
               active_sun_pct_, sun_mode, moon_pct,
               phaseConsoleMoonName(moon_pct), moon_mode,
               godot_phase.phase, godot_phase.is_crescent ? 1 : 0,
               active_cloud_pct_);
    }
    return true;
  }
#endif

private:
  // 0="live", 1="live_ip", 2="solar", 3="charge", 4="peak", 5="low", 6="offgrid"
  int state_mode_idx_ = 0;
  // 0="auto", 1="clear", 2="partly", 3="overcast", 4="rain", 5="storm", 6="snow", 7="wind", 8="sunrise", 9="sunset", 10="night"
  int weather_mode_idx_ = 0;

  enum class PageTransitionPhase : uint8_t { Idle = 0, Covering = 1, Uncovering = 2 };
  static constexpr uint16_t kPageTransitionCoverMs = 80;
  static constexpr uint16_t kPageTransitionUncoverMs = 120;
  // Powerwall gauge intro: arc, percent text, and reserve marker sweep up from 0.
  static constexpr uint16_t kPwGaugeIntroMs = 900;
  static constexpr int32_t kPwGaugeIntroSteps = 1000;
  bool pw_gauge_intro_active_ = false;
  PageTransitionPhase page_transition_phase_ = PageTransitionPhase::Idle;
  lv_obj_t* page_transition_overlay_ = nullptr;
  lv_opa_t page_transition_opa_ = LV_OPA_TRANSP;
  uint8_t page_transition_target_ = 0;
  uint32_t page_transition_consumed_clicks_ = 0;
  lv_obj_t* page_settings_ = nullptr;
  lv_obj_t* settings_controls_page_ = nullptr;
  lv_obj_t* settings_network_page_ = nullptr;
  lv_obj_t* settings_controls_tab_label_ = nullptr;
  lv_obj_t* settings_info_tab_label_ = nullptr;
  lv_obj_t* settings_controls_tab_indicator_ = nullptr;
  lv_obj_t* settings_info_tab_indicator_ = nullptr;
  lv_obj_t* settings_voice_test_button_ = nullptr;
  lv_obj_t* settings_voice_switch_ = nullptr;
  lv_obj_t* settings_wifi_title_ = nullptr;
  lv_obj_t* settings_wifi_ssid_value_ = nullptr;
  lv_obj_t* settings_http_title_ = nullptr;
  lv_obj_t* settings_gateway_ip_label_ = nullptr;
  lv_obj_t* settings_gateway_url_label_ = nullptr;
  lv_obj_t* settings_uptime_label_ = nullptr;
  lv_obj_t* settings_version_label_ = nullptr;
  bool settings_uptime_initialized_ = false;
  uint64_t settings_uptime_last_seconds_ = 0;
#if !defined(ESP32)
  const std::chrono::steady_clock::time_point simulator_started_at_ =
    std::chrono::steady_clock::now();
#endif
#if defined(ESP32)
  String settings_gateway_config_ip_ = "--";
#endif
  lv_obj_t* settings_brightness_slider_ = nullptr;
  lv_obj_t* settings_volume_slider_ = nullptr;
  lv_obj_t* settings_brightness_value_ = nullptr;
  lv_obj_t* settings_volume_value_ = nullptr;
  void* settings_context_ = nullptr;
  SettingChangedFn brightness_changed_ = nullptr;
  SettingChangedFn volume_changed_ = nullptr;
  void (*voice_changed_)(void*, bool) = nullptr;
  TestToneFn test_tone_ = nullptr;
  RestartFn restart_ = nullptr;
  lv_obj_t* restart_button_ = nullptr;
  lv_obj_t* restart_button_label_ = nullptr;
  bool restart_armed_ = false;
  uint32_t restart_armed_at_ms_ = 0;
  uint8_t settings_brightness_ = 80;
  uint8_t settings_volume_ = 85;
  bool settings_voice_enabled_ = true;
  bool settings_network_page_active_ = false;
  bool simulator_network_available_ = true;
  char simulator_wifi_ssid_[33] = "Home-WiFi";
  bool settings_updating_ = false;

  bool last_configured_ = false;
  PowerwallTelemetry last_telemetry_;
  WeatherSnapshot last_weather_;
  unsigned long last_periodic_check_ms_ = 0;
  bool last_rendered_weather_stale_ = false;
  int  last_rendered_min_ = -1;

  bool phase_sun_override_active_ = false;
  int phase_sun_override_pct_ = 50;
  uint8_t phase_sun_next_index_ = 0;
  bool phase_moon_override_active_ = false;
  float phase_moon_override_pct_ = 39.0f;
  uint8_t phase_moon_next_index_ = 0;
  int active_sun_pct_ = 0;
  int active_cloud_pct_ = 0;
  float active_moon_phase_ = 0.39f;
  bool active_is_night_ = true;
  bool phase_effects_cache_valid_ = false;
  int cached_phase_index_ = 0;
  int cached_next_phase_index_ = 1;
  float cached_phase_blend_ = 0.0f;
  float cached_solar_cos_ = 1.0f;
  float cached_solar_sin_ = 0.0f;
  float cached_solar_strength_ = 0.0f;
  int cached_sun_pct_ = -1;
  int cached_cloud_pct_ = -1;
  float cached_moon_phase_ = -1.0f;
  bool cached_is_night_ = false;

  String active_stage_key_ = "night";
  String decoded_base_key_ = "";
  float active_solar_w_ = 0.0f;
  float active_pw_w_    = 0.0f;
  float active_grid_w_  = 0.0f;
  float active_load_w_  = 0.0f;
  float active_soe_pct_ = 100.0f;
  bool  active_offgrid_ = false;

  struct WireBranch {
    bool enabled = false;
    float scale  = 1.0f;
    float offset = 0.0f;
  };
  struct ActiveRoute {
    WireBranch branches[4]; // 0=grid, 1=load, 2=battery, 3=solar
    float speed = 1.0f;
    uint32_t route_start_ms = 0;
    String signature = "";
  } route_;

  lv_obj_t* hero_card_ = nullptr;
  lv_obj_t* stage_canvas_ = nullptr;
  lv_color_t* stage_base_buf_ = nullptr;
  lv_color_t* stage_canvas_buf_ = nullptr;
  lv_timer_t* stage_timer_ = nullptr;

  lv_obj_t* chart_card_ = nullptr;
  lv_obj_t* lbl_site_title_ = nullptr;
  lv_obj_t* pill_status_ = nullptr;
  lv_obj_t* dot_status_ = nullptr;
  lv_obj_t* lbl_live_status_ = nullptr;
  lv_obj_t* pill_weather_ = nullptr;
  lv_obj_t* lbl_weather_ = nullptr;
  lv_obj_t* lbl_solar_val_ = nullptr;
  lv_obj_t* lbl_home_val_ = nullptr;
  lv_obj_t* lbl_pw_title_ = nullptr;
  lv_obj_t* pw_val_row_ = nullptr;
  lv_obj_t* lbl_pw_kw_ = nullptr;
  lv_obj_t* lbl_pw_dot_ = nullptr;
  lv_obj_t* img_pw_arrow_ = nullptr;
  lv_obj_t* lbl_pw_soc_ = nullptr;
  lv_obj_t* lbl_grid_title_ = nullptr;
  lv_obj_t* lbl_grid_val_ = nullptr;

  lv_obj_t* lbl_stat_home_kw_ = nullptr;
  lv_obj_t* lbl_stat_home_va_ = nullptr;
  lv_obj_t* lbl_stat_reserve_ = nullptr;
  lv_obj_t* lbl_stat_pw_soc_ = nullptr;
  lv_obj_t* lbl_stat_pw_kwh_ = nullptr;
  lv_obj_t* lbl_stat_pw_eta_ = nullptr;
  lv_obj_t* pill_tou_ = nullptr;
  lv_obj_t* lbl_tou_pill_ = nullptr;
  lv_obj_t* lbl_tou_track_ = nullptr;
  lv_obj_t* tou_bar_we_ = nullptr;
  lv_obj_t* tou_bar_wk_ = nullptr;
  lv_obj_t* tou_bar_wk_summer_ = nullptr;
  lv_obj_t* cursor_line_ = nullptr;
  lv_obj_t* chart1_canvas_ = nullptr;
  lv_obj_t* chart2_canvas_ = nullptr;
  lv_obj_t* chart1_now_dot_ = nullptr;
  lv_obj_t* chart2_now_dot_ = nullptr;
  lv_color_t* chart1_buf_ = nullptr;
  lv_color_t* chart2_buf_ = nullptr;

  // The original hero and chart stay intact and are hidden only while an added page is active.
  int active_dashboard_page_ = 0; // 0=original, 1=Powerwall, 2=home/grid, 3=today trend, 4=week
  static constexpr uint8_t kNormalDashboardPageCount = 5;
  uint8_t previous_normal_page_ = 0;
  lv_obj_t* dashboard_screen_ = nullptr;
  lv_obj_t* page_powerwall_ = nullptr;
  lv_obj_t* page_home_grid_ = nullptr;
  lv_obj_t* page_today_trend_ = nullptr;
  lv_obj_t* page_header_ = nullptr;
  lv_obj_t* page_title_ = nullptr;
  lv_obj_t* page_site_ = nullptr;
  lv_obj_t* pw_header_title_ = nullptr;
  lv_obj_t* pw_header_clock_ = nullptr;
  lv_obj_t* pw_header_date_ = nullptr;
  lv_obj_t* pw_header_status_dot_ = nullptr;
  lv_color_t* dashboard_chart_buf_ = nullptr; // shared by the two hidden/visible page canvases
  lv_obj_t* pw_chart_card_ = nullptr;
  lv_obj_t* pw_chart_canvas_ = nullptr;
  lv_obj_t* home_chart_canvas_ = nullptr;
  lv_obj_t* page_status_pill_ = nullptr;
  lv_obj_t* page_status_text_ = nullptr;
  lv_obj_t* page_clock_ = nullptr;
  lv_obj_t* page_date_ = nullptr;

  lv_obj_t* pw_arc_ = nullptr;
  lv_obj_t* pw_reserve_marker_ = nullptr;
  lv_point_t pw_reserve_marker_points_[2] = {};
  lv_obj_t* pw_soc_text_ = nullptr;
  lv_obj_t* pw_soc_bold_pass_ = nullptr;
  lv_obj_t* pw_state_text_ = nullptr;
  lv_obj_t* pw_eta_text_ = nullptr;
  lv_obj_t* pw_backup_eta_text_ = nullptr;
  lv_obj_t* pw_voltage_value_ = nullptr;
  lv_obj_t* pw_capacity_current_value_ = nullptr;
  lv_obj_t* pw_capacity_total_value_ = nullptr;
  lv_obj_t* pw_now_line_ = nullptr;
  lv_obj_t* pw_now_dot_ = nullptr;
  lv_obj_t* pw_load_chart_canvas_ = nullptr;
  lv_color_t* pw_load_chart_buf_ = nullptr;
  lv_obj_t* pw_load_now_dot_ = nullptr;
  lv_obj_t* pw_tou_segments_[4] = {};
  lv_obj_t* pw_time_ticks_[5] = {};
  lv_obj_t* pw_reserve_chart_label_ = nullptr;
  lv_obj_t* pw_data_stamp_ = nullptr;

  lv_obj_t* home_tou_pill_ = nullptr;
  lv_obj_t* home_tou_pill_text_ = nullptr;
  lv_obj_t* home_grid_heading_ = nullptr;
  lv_obj_t* home_grid_home_heading_ = nullptr;
  lv_obj_t* home_grid_import_title_ = nullptr;
  lv_obj_t* home_grid_export_title_ = nullptr;
  lv_obj_t* home_interval_start_ = nullptr;
  lv_obj_t* home_interval_rule_ = nullptr;
  lv_obj_t* home_interval_end_ = nullptr;
  lv_obj_t* home_price_ = nullptr;
  lv_obj_t* home_countdown_ = nullptr;
  lv_obj_t* home_tou_segments_[4] = {};
  lv_obj_t* home_tou_segment_labels_[4] = {};
  lv_obj_t* home_tou_ticks_[5] = {};
  lv_obj_t* home_tou_now_line_ = nullptr;
  lv_obj_t* home_tou_now_dot_ = nullptr;
  lv_obj_t* home_grid_status_ = nullptr;
  lv_obj_t* home_grid_power_title_ = nullptr;
  lv_obj_t* home_grid_power_value_ = nullptr;
  lv_obj_t* home_grid_h_rules_[2] = {};
  lv_obj_t* home_metrics_h_rules_[2] = {};
  lv_obj_t* home_section_bottom_rules_[3] = {};
  lv_obj_t* home_grid_current_value_ = nullptr;
  lv_obj_t* home_grid_voltage_value_ = nullptr;
  lv_obj_t* home_grid_import_value_ = nullptr;
  lv_obj_t* home_grid_export_value_ = nullptr;
  lv_obj_t* home_load_value_ = nullptr;
  lv_obj_t* home_load_current_value_ = nullptr;
  lv_obj_t* home_load_voltage_value_ = nullptr;
  lv_obj_t* home_solar_value_ = nullptr;
  lv_obj_t* home_power_factor_ = nullptr;
  lv_obj_t* home_energy_value_ = nullptr;
  lv_obj_t* home_now_line_ = nullptr;
  lv_obj_t* home_now_dot_ = nullptr;
  lv_obj_t* home_chart_ticks_[5] = {};
  lv_obj_t* home_chart_y_labels_[3] = {};
  lv_obj_t* home_data_stamp_ = nullptr;
  lv_obj_t* home_chart_tou_segments_[4] = {};
  lv_point_t home_grid_icon_points_[6][2] = {};
  lv_point_t home_house_icon_points_[5][2] = {};

  unsigned long last_added_page_clock_ms_ = 0;
  int cached_tou_year_ = -1;
  int cached_tou_yday_ = -1;

#include "home_480.inc"

  static lv_obj_t* makeDashboardCard(lv_obj_t* parent, int x, int y, int w, int h) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 0, 0);
    lv_obj_t* rule = lv_obj_create(card);
    lv_obj_set_pos(rule, 0, h - 1);
    lv_obj_set_size(rule, w, 1);
    lv_obj_set_style_bg_color(rule, lv_color_hex(0x263746), 0);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(rule, 0, 0);
    lv_obj_set_style_radius(rule, 0, 0);
    lv_obj_clear_flag(rule, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
  }

  static void flattenDashboardPill(lv_obj_t* pill, uint32_t rule_color) {
    if (!pill) return;
    lv_obj_set_style_bg_opa(pill, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(pill, lv_color_hex(rule_color), 0);
    lv_obj_set_style_border_width(pill, 1, 0);
    lv_obj_set_style_border_side(pill, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_radius(pill, 0, 0);
  }

  static lv_obj_t* makeDashboardLabel(lv_obj_t* parent, const char* text,
                                      int x, int y, int width,
                                      const lv_font_t* font, uint32_t color,
                                      lv_text_align_t align = LV_TEXT_ALIGN_LEFT) {
    lv_obj_t* label = lv_label_create(parent);
    setLabelText480(label, text ? text : "—");
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, align, 0);
    lv_obj_set_style_text_decor(label, LV_TEXT_DECOR_NONE, 0);
    if (width > 0) lv_obj_set_width(label, width);
    lv_obj_set_pos(label, x, y);
    return label;
  }

  static lv_obj_t* makeDashboardRule(lv_obj_t* parent, int x, int y, int w, int h = 1) {
    lv_obj_t* rule = lv_obj_create(parent);
    lv_obj_set_pos(rule, x, y);
    lv_obj_set_size(rule, w, h);
    lv_obj_set_style_bg_color(rule, lv_color_hex(0x263746), 0);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(rule, 0, 0);
    lv_obj_set_style_radius(rule, 0, 0);
    lv_obj_clear_flag(rule, LV_OBJ_FLAG_SCROLLABLE);
    return rule;
  }

  static int homeGridCumulativeValueY() {
    // Center the cumulative values in the row between its horizontal separator and cyan bottom rule.
    const int rowTop = kHomeGridTotalsSeparatorY + 1;
    const int rowBottom = kHomeGridCardH - 1;
    const int fontHeight = native480_fonts::grid_cjk_16()->line_height;
    return rowTop + (rowBottom - rowTop - fontHeight) / 2;
  }

  static void positionHeaderDotBeforeClock(lv_obj_t* clock, lv_obj_t* dot) {
    if (!clock || !dot) return;
    const auto* font = lv_obj_get_style_text_font(clock, LV_PART_MAIN);
    lv_point_t textSize{};
    lv_txt_get_size(&textSize, lv_label_get_text(clock), font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    const int x = lv_obj_get_x(clock) + lv_obj_get_width(clock) - textSize.x - 8 - lv_obj_get_width(dot);
    const int y = lv_obj_get_y(clock) + (textSize.y - lv_obj_get_height(dot)) / 2;
    lv_obj_set_pos(dot, x, y);
  }

  static constexpr int kHeaderScreenRightExclusive = 480 - 8;

  static int rightAlignedHeaderClockX(lv_obj_t* parent, int width) {
    if (!parent) return 0;
    lv_obj_update_layout(parent);
    lv_area_t parent_bounds{};
    lv_obj_get_coords(parent, &parent_bounds);
    return kHeaderScreenRightExclusive - parent_bounds.x1 - width;
  }

  static bool headerClockDotLayoutIsValid(const lv_obj_t* clock, const lv_obj_t* dot,
                                          const lv_font_t* font, int clockWidth) {
    if (!clock || !dot || lv_obj_get_width(clock) != clockWidth ||
        lv_obj_get_style_text_font(clock, LV_PART_MAIN) != font ||
        lv_obj_get_style_text_align(clock, LV_PART_MAIN) != LV_TEXT_ALIGN_RIGHT) return false;
    lv_point_t textSize{};
    lv_txt_get_size(&textSize, lv_label_get_text(clock),
                    lv_obj_get_style_text_font(clock, LV_PART_MAIN), 0, 0,
                    LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_area_t clock_bounds{}, dot_bounds{};
    lv_obj_get_coords(clock, &clock_bounds);
    lv_obj_get_coords(dot, &dot_bounds);
    const int clock_right = clock_bounds.x2 + 1;
    const int text_left = clock_right - textSize.x;
    const int dot_right = dot_bounds.x2 + 1;
    const int text_top = clock_bounds.y1 + (textSize.y - lv_obj_get_height(dot)) / 2;
    return abs(clock_right - kHeaderScreenRightExclusive) <= 1 &&
           dot_right + 8 == text_left && dot_bounds.y1 == text_top;
  }

  // Reused by both daily charts and the host seven-day overview.
  struct DashboardTimeAxis {
    lv_obj_t* labels[8] = {};
    lv_obj_t* marks[8] = {};
    const lv_font_t* font = nullptr;
    int left = 0, width = 0, y = 0, markY = 0;
  };

  static void addTouAxisBoundaries(const struct tm& date, bool (&boundaries)[25]) {
    if (isTaiwanTouWeekend(date)) return;
    for (int hour = 1; hour < 24; ++hour) {
      if (isTaiwanTouWeekdayPeak(date.tm_mon + 1, hour - 0.5f) !=
          isTaiwanTouWeekdayPeak(date.tm_mon + 1, hour)) boundaries[hour] = true;
    }
  }

  static void initializeTimeAxis(DashboardTimeAxis& axis, lv_obj_t* parent,
                                 int left, int width, int y, int markY, const lv_font_t* font) {
    axis.left = left; axis.width = width; axis.y = y; axis.markY = markY; axis.font = font;
    for (int i = 0; i < 8; ++i) {
      axis.labels[i] = makeDashboardLabel(parent, "", left, y, 1, font, 0x94a3b8);
      lv_label_set_long_mode(axis.labels[i], LV_LABEL_LONG_CLIP);
      axis.marks[i] = makeDashboardRule(parent, left, markY, 1, 1);
      lv_obj_set_style_bg_color(axis.marks[i], lv_color_hex(0xcbd5e1), 0);
      lv_obj_add_flag(axis.labels[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(axis.marks[i], LV_OBJ_FLAG_HIDDEN);
    }
  }

  static void updateTimeAxis(DashboardTimeAxis& axis, const bool (&boundaries)[25]) {
    bool selected[25] = {};
    int left[25] = {}, width[25] = {};
    // Place endpoints and transitions first; only then fill in six-hour references.
    for (int pass = 0; pass < 2; ++pass) {
      for (int hour = 0; hour <= 24; ++hour) {
        const bool required = hour == 0 || hour == 24 || boundaries[hour];
        if ((pass == 0) != required || (!required && hour % 6 != 0)) continue;
        char text[8]; snprintf(text, sizeof(text), "%02d:00", hour);
        lv_point_t size;
        lv_txt_get_size(&size, text, axis.font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        width[hour] = size.x;
        const int x = axis.left + static_cast<int>(roundf(hour * axis.width / 24.0f));
        left[hour] = hour == 0 ? x : hour == 24 ? x - width[hour] : x - width[hour] / 2;
        bool fits = true;
        for (int other = 0; other <= 24; ++other) {
          if (selected[other] && left[hour] < left[other] + width[other] + 4 &&
              left[other] < left[hour] + width[hour] + 4) fits = false;
        }
        if (required || fits) selected[hour] = true;
      }
    }
    int slot = 0;
    for (int hour = 0; hour <= 24 && slot < 8; ++hour) {
      if (!selected[hour]) continue;
      char text[8]; snprintf(text, sizeof(text), "%02d:00", hour);
      auto* label = axis.labels[slot];
      setLabelText480(label, text);
      lv_obj_set_width(label, width[hour]);
      lv_obj_set_pos(label, left[hour], axis.y);
      lv_obj_set_style_text_align(label, hour == 24 ? LV_TEXT_ALIGN_RIGHT :
          hour == 0 ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_style_text_color(label, lv_color_hex(boundaries[hour] ? 0xcbd5e1 : 0x94a3b8), 0);
      lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
      if (boundaries[hour]) {
        const int x = axis.left + static_cast<int>(roundf(hour * axis.width / 24.0f));
        lv_obj_set_pos(axis.marks[slot], x, axis.markY);
        lv_obj_clear_flag(axis.marks[slot], LV_OBJ_FLAG_HIDDEN);
      } else lv_obj_add_flag(axis.marks[slot], LV_OBJ_FLAG_HIDDEN);
      ++slot;
    }
    for (; slot < 8; ++slot) {
      lv_obj_add_flag(axis.labels[slot], LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(axis.marks[slot], LV_OBJ_FLAG_HIDDEN);
    }
  }

  static constexpr int dashboardPageCount() {
    return 6;
  }
#include "weekly_480.inc"
#include "daily_trend_480.inc"
  void refreshSettingsLabels() {
    char value[8];
    if (settings_brightness_value_) {
      snprintf(value, sizeof(value), "%u%%", static_cast<unsigned>(settings_brightness_));
      setLabelText480(settings_brightness_value_, value);
    }
    if (settings_volume_value_) {
      snprintf(value, sizeof(value), "%u%%", static_cast<unsigned>(settings_volume_));
      setLabelText480(settings_volume_value_, value);
    }
    if (settings_voice_switch_) {
      if (settings_voice_enabled_) lv_obj_add_state(settings_voice_switch_, LV_STATE_CHECKED);
      else lv_obj_clear_state(settings_voice_switch_, LV_STATE_CHECKED);
    }
    if (settings_gateway_ip_label_ || settings_gateway_url_label_) {
      const char* ip = last_telemetry_.gateway_ip.length() ? last_telemetry_.gateway_ip.c_str() : "--";
      char url[96];
#if !defined(ESP32)
      snprintf(url, sizeof(url), "http://%s/", simulator_network_available_ ? "192.0.2.15" : "192.168.4.1");
#elif defined(ESP32)
      const bool connected = WiFi.status() == WL_CONNECTED;
      const String localIp = connected ? WiFi.localIP().toString() : String();
      snprintf(url, sizeof(url), "http://%s/", connected ? localIp.c_str() : "192.168.4.1");
      ip = settings_gateway_config_ip_.c_str();
#else
      snprintf(url, sizeof(url), "http://%s/", ip);
#endif
      if (settings_gateway_ip_label_) setLabelText480(settings_gateway_ip_label_, ip);
      if (settings_gateway_url_label_) setLabelText480(settings_gateway_url_label_, url);
    }
#if !defined(ESP32)
    if (settings_wifi_ssid_value_)
      setLabelText480(settings_wifi_ssid_value_, simulator_network_available_
        ? simulator_wifi_ssid_ : "powerwall-dashboard");
    if (settings_http_title_) setLabelText480(settings_http_title_, "設定頁網址");
#elif defined(ESP32)
    const bool connected = WiFi.status() == WL_CONNECTED;
    const String localIp = connected ? WiFi.localIP().toString() : String();
    const String ssid = connected ? WiFi.SSID() : String("powerwall-dashboard");
    if (settings_wifi_ssid_value_) setLabelText480(settings_wifi_ssid_value_, ssid.c_str());
    if (settings_http_title_) setLabelText480(settings_http_title_, "設定頁網址");
#endif
  }

  uint64_t uptimeSeconds() const {
#if defined(ESP32)
    return static_cast<uint64_t>(esp_timer_get_time() / 1000000LL);
#else
    const auto elapsed = std::chrono::steady_clock::now() - simulator_started_at_;
    return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::seconds>(elapsed).count());
#endif
  }

  void refreshSettingsUptime() {
    if (!settings_uptime_label_) return;
    const uint64_t seconds = uptimeSeconds();
    if (settings_uptime_initialized_ && seconds == settings_uptime_last_seconds_) return;
    settings_uptime_initialized_ = true;
    settings_uptime_last_seconds_ = seconds;
    char text[48];
    formatSettingsUptime(seconds, text, sizeof(text));
    setLabelText480(settings_uptime_label_, text);
  }

  static uint32_t nextSettingsUtf8Codepoint(const char*& p) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(p);
    if (!bytes || bytes[0] == 0) return 0;
    uint32_t cp;
    size_t length;
    if ((bytes[0] & 0x80) == 0) { cp = bytes[0]; length = 1; }
    else if ((bytes[0] & 0xe0) == 0xc0) { cp = bytes[0] & 0x1f; length = 2; }
    else if ((bytes[0] & 0xf0) == 0xe0) { cp = bytes[0] & 0x0f; length = 3; }
    else if ((bytes[0] & 0xf8) == 0xf0) { cp = bytes[0] & 0x07; length = 4; }
    else { ++p; return 0xfffd; }
    for (size_t i = 1; i < length && bytes[i]; ++i) cp = (cp << 6) | (bytes[i] & 0x3f);
    p += length;
    return cp;
  }

  static void centerSettingsButtonLabel(lv_obj_t* button, lv_obj_t* label) {
    if (!button || !label) return;
    lv_obj_set_style_pad_all(button, 0, 0);
    lv_obj_set_style_pad_all(label, 0, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_size(label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_update_layout(label);

    const lv_font_t* font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    const char* text = lv_label_get_text(label);
    if (!font || !text) return;
    const int letter_space = lv_obj_get_style_text_letter_space(label, LV_PART_MAIN);
    const int baseline_top = font->line_height - font->base_line;
    int pen_x = 0, min_x = 0, max_x = 0, min_y = 0, max_y = 0;
    bool has_ink = false;
    const char* p = text;
    uint32_t cp = nextSettingsUtf8Codepoint(p);
    while (cp) {
      const char* next_p = p;
      const uint32_t next_cp = nextSettingsUtf8Codepoint(next_p);
      lv_font_glyph_dsc_t glyph{};
      if (lv_font_get_glyph_dsc(font, &glyph, cp, next_cp) && glyph.box_w && glyph.box_h) {
        const int left = pen_x + glyph.ofs_x;
        const int right = left + glyph.box_w;
        const int top = baseline_top - glyph.box_h - glyph.ofs_y;
        const int bottom = top + glyph.box_h;
        if (!has_ink) {
          min_x = left; max_x = right; min_y = top; max_y = bottom;
          has_ink = true;
        } else {
          min_x = std::min(min_x, left); max_x = std::max(max_x, right);
          min_y = std::min(min_y, top); max_y = std::max(max_y, bottom);
        }
      }
      const int advance = lv_font_get_glyph_width(font, cp, next_cp);
      pen_x += advance;
      if (advance > 0) pen_x += letter_space;
      cp = nextSettingsUtf8Codepoint(p);
    }
    if (!has_ink) return;
    const int ink_width = max_x - min_x;
    const int ink_height = max_y - min_y;
    const int x = (lv_obj_get_width(button) - ink_width) / 2 - min_x;
    const int y = (lv_obj_get_height(button) - ink_height) / 2 - min_y;
    lv_obj_set_pos(label, x, y);  // measured native pixels
  }

  static bool dashboardLabelInkBoundsY(lv_obj_t* label, int& minY, int& maxY) {
    if (!label) return false;
    const lv_font_t* font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    const char* text = lv_label_get_text(label);
    if (!font || !text) return false;
    const int baselineTop = font->line_height - font->base_line;
    bool hasInk = false;
    const char* p = text;
    uint32_t cp = nextSettingsUtf8Codepoint(p);
    while (cp) {
      const char* nextP = p;
      const uint32_t nextCp = nextSettingsUtf8Codepoint(nextP);
      lv_font_glyph_dsc_t glyph{};
      if (lv_font_get_glyph_dsc(font, &glyph, cp, nextCp) && glyph.box_w && glyph.box_h) {
        const int top = baselineTop - glyph.box_h - glyph.ofs_y;
        const int bottom = top + glyph.box_h;
        if (!hasInk) {
          minY = top;
          maxY = bottom;
          hasInk = true;
        } else {
          minY = std::min(minY, top);
          maxY = std::max(maxY, bottom);
        }
      }
      cp = nextSettingsUtf8Codepoint(p);
    }
    return hasInk;
  }

  static void centerDashboardLabelInkVertically(lv_obj_t* parent, lv_obj_t* label) {
    if (!parent || !label) return;
    lv_obj_set_style_pad_all(label, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(label, 0, LV_PART_MAIN);
    lv_obj_set_height(label, LV_SIZE_CONTENT);
    lv_obj_update_layout(parent);
    lv_obj_update_layout(label);

    // Center LVGL's measured label box, then account for its font baseline and
    // this translated string's visible glyph bounds inside the pill content box.
    const int parentContentHeight = lv_obj_get_content_height(parent);
    const int labelContentTop = lv_obj_get_style_pad_top(label, LV_PART_MAIN);
    const int labelBoxY = (parentContentHeight - lv_obj_get_height(label)) / 2;
    int minY = 0;
    int maxY = 0;
    if (!dashboardLabelInkBoundsY(label, minY, maxY)) {
      lv_obj_set_y(label, labelBoxY);
      return;
    }
    const int currentInkCenter2 = 2 * (labelBoxY + labelContentTop) + minY + maxY;
    const int delta2 = parentContentHeight - currentInkCenter2;
    const int correction = delta2 >= 0 ? (delta2 + 1) / 2 : (delta2 - 1) / 2;
    lv_obj_set_y(label, labelBoxY + correction);
  }

  static bool dashboardLabelInkIsVerticallyCentered(lv_obj_t* parent, lv_obj_t* label) {
    if (!parent || !label) return false;
    int minY = 0;
    int maxY = 0;
    if (!dashboardLabelInkBoundsY(label, minY, maxY)) return false;
    const int labelContentTop = lv_obj_get_style_pad_top(label, LV_PART_MAIN);
    const int inkCenter2 = 2 * (lv_obj_get_y(label) + labelContentTop) + minY + maxY;
    const int parentCenter2 = lv_obj_get_content_height(parent);
    const int delta2 = inkCenter2 - parentCenter2;
    return delta2 >= -1 && delta2 <= 1;
  }

  void layoutHomeTouIntervalLabels() {
    if (!home_tou_pill_ || !home_interval_start_ || !home_interval_rule_ || !home_interval_end_) return;
    lv_obj_set_width(home_interval_start_, LV_SIZE_CONTENT);
    lv_obj_set_width(home_interval_end_, LV_SIZE_CONTENT);
    lv_obj_update_layout(home_interval_start_);
    lv_obj_update_layout(home_interval_end_);

    const int startX = lv_obj_get_x(home_tou_pill_) + lv_obj_get_width(home_tou_pill_) +
                       kHomeTouIntervalPillGap;
    const int dashX = startX + lv_obj_get_width(home_interval_start_) + kHomeTouIntervalDashGap;
    const int endX = dashX + kHomeTouIntervalDashW + kHomeTouIntervalDashGap;
    lv_obj_set_pos(home_interval_start_, startX, kHomeTouIntervalTextY);
    lv_obj_set_pos(home_interval_rule_, dashX, kHomeTouIntervalDashY);
    lv_obj_set_pos(home_interval_end_, endX, kHomeTouIntervalTextY);
  }

  static lv_obj_t* makeCenteredSettingsButtonLabel(lv_obj_t* button, const char* text,
                                                   const lv_font_t* font, uint32_t color) {
    lv_obj_t* label = makeDashboardLabel(button, text, 0, 0, 0, font, color, LV_TEXT_ALIGN_LEFT);
    centerSettingsButtonLabel(button, label);
    return label;
  }

  void showSettingsSubpage(bool info) {
    settings_network_page_active_ = info;
    if (settings_controls_page_) {
      if (info) lv_obj_add_flag(settings_controls_page_, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_clear_flag(settings_controls_page_, LV_OBJ_FLAG_HIDDEN);
    }
    if (settings_network_page_) {
      if (info) lv_obj_clear_flag(settings_network_page_, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(settings_network_page_, LV_OBJ_FLAG_HIDDEN);
    }
    if (settings_controls_tab_label_)
      lv_obj_set_style_text_color(settings_controls_tab_label_,
        lv_color_hex(info ? 0x7d879a : 0x25a9e0), 0);
    if (settings_info_tab_label_)
      lv_obj_set_style_text_color(settings_info_tab_label_,
        lv_color_hex(info ? 0x25a9e0 : 0x7d879a), 0);
    if (settings_controls_tab_indicator_) {
      if (info) lv_obj_add_flag(settings_controls_tab_indicator_, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_clear_flag(settings_controls_tab_indicator_, LV_OBJ_FLAG_HIDDEN);
    }
    if (settings_info_tab_indicator_) {
      if (info) lv_obj_clear_flag(settings_info_tab_indicator_, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(settings_info_tab_indicator_, LV_OBJ_FLAG_HIDDEN);
    }
  }

  void initializeSettingsPage480() {
    lv_obj_set_style_bg_color(page_settings_, lv_color_hex(0x03080b), 0);
    makeDashboardLabel(page_settings_, "裝置設定", 24, 12, 432,
      native480_i18n::kEnglish ? native480_fonts::montserrat_32() : native480_fonts::dashboard_cjk_32(),
      0xe7e9ed);
    const lv_font_t* tabFont = native480_i18n::kEnglish
      ? native480_fonts::montserrat_21() : native480_fonts::home_cjk_20();
    auto makeTab = [&](const char* text, int x, bool info) {
      lv_obj_t* button = lv_btn_create(page_settings_);
      lv_obj_set_pos(button, x, 52); lv_obj_set_size(button, 204, 48);
      lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_width(button, 0, 0); lv_obj_set_style_radius(button, 0, 0);
      lv_obj_set_style_shadow_width(button, 0, 0); lv_obj_set_style_pad_all(button, 0, 0);
      lv_obj_t* label = makeCenteredSettingsButtonLabel(button, text, tabFont, 0x7d879a);
      if (info) settings_info_tab_label_ = label;
      else settings_controls_tab_label_ = label;
      lv_obj_add_event_cb(button, [](lv_event_t* event) {
        auto* self = static_cast<Ui480*>(lv_event_get_user_data(event));
        const bool showInfo = lv_obj_get_user_data(lv_event_get_target(event)) != nullptr;
        if (self && lv_event_get_code(event) == LV_EVENT_CLICKED)
          self->showSettingsSubpage(showInfo);
        lv_event_stop_bubbling(event);
      }, LV_EVENT_CLICKED, this);
      lv_obj_set_user_data(button, reinterpret_cast<void*>(static_cast<uintptr_t>(info ? 1 : 0)));
      return button;
    };
    (void)makeTab("控制", 24, false);
    (void)makeTab("資訊", 252, true);
    auto makeIndicator = [&](int x, int width, bool hidden) {
      lv_obj_t* indicator = lv_obj_create(page_settings_);
      lv_obj_set_pos(indicator, x, 100); lv_obj_set_size(indicator, width, 2);
      lv_obj_set_style_bg_color(indicator, lv_color_hex(0x25a9e0), 0);
      lv_obj_set_style_bg_opa(indicator, LV_OPA_COVER, 0);
      lv_obj_set_style_border_width(indicator, 0, 0);
      lv_obj_set_style_radius(indicator, 1, 0);
      lv_obj_set_style_pad_all(indicator, 0, 0);
      lv_obj_clear_flag(indicator, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
      if (hidden) lv_obj_add_flag(indicator, LV_OBJ_FLAG_HIDDEN);
      return indicator;
    };
    const int controlsLineWidth = native480_i18n::kEnglish ? 82 : 44;
    const int infoLineWidth = native480_i18n::kEnglish ? 44 : 44;
    settings_controls_tab_indicator_ = makeIndicator(126 - controlsLineWidth / 2, controlsLineWidth, false);
    settings_info_tab_indicator_ = makeIndicator(354 - infoLineWidth / 2, infoLineWidth, true);

    settings_controls_page_ = lv_obj_create(page_settings_);
    settings_network_page_ = lv_obj_create(page_settings_);
    lv_obj_t* subpages[] = {settings_controls_page_, settings_network_page_};
    for (auto* page : subpages) {
      lv_obj_set_pos(page, 0, 108); lv_obj_set_size(page, 480, 340);
      lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_width(page, 0, 0); lv_obj_set_style_radius(page, 0, 0);
      lv_obj_set_style_pad_all(page, 0, 0);
      lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_add_flag(settings_network_page_, LV_OBJ_FLAG_HIDDEN);

    const lv_font_t* controlLabelFont = native480_i18n::kEnglish
      ? native480_fonts::montserrat_16() : native480_fonts::home_cjk_16();
    const lv_font_t* actionFont = native480_i18n::kEnglish
      ? native480_fonts::montserrat_19() : native480_fonts::home_cjk_16();
    makeDashboardLabel(settings_controls_page_, "顯示亮度", 24, 4, 290,
      controlLabelFont, 0xe7e9ed);
    settings_brightness_value_ = makeDashboardLabel(settings_controls_page_, "80%", 356, 0, 100,
      native480_fonts::montserrat_24(), 0xe7e9ed, LV_TEXT_ALIGN_RIGHT);
    settings_brightness_slider_ = lv_slider_create(settings_controls_page_);
    lv_obj_set_pos(settings_brightness_slider_, 24, 32);
    lv_obj_set_size(settings_brightness_slider_, 432, 28);
    lv_obj_set_ext_click_area(settings_brightness_slider_, 10);
    lv_slider_set_range(settings_brightness_slider_, 0, 100);
    lv_obj_set_style_bg_color(settings_brightness_slider_, lv_color_hex(0x0d151b), LV_PART_MAIN);
    lv_obj_set_style_bg_color(settings_brightness_slider_, lv_color_hex(0x25a9e0), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(settings_brightness_slider_, lv_color_hex(0xe7e9ed), LV_PART_KNOB);
    lv_obj_set_style_pad_ver(settings_brightness_slider_, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(settings_brightness_slider_, 0, LV_PART_KNOB);
    lv_obj_set_style_radius(settings_brightness_slider_, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(settings_brightness_slider_, 4, LV_PART_INDICATOR);
    lv_obj_set_style_radius(settings_brightness_slider_, 14, LV_PART_KNOB);
    lv_obj_set_style_border_width(settings_brightness_slider_, 0, LV_PART_KNOB);
    lv_obj_add_event_cb(settings_brightness_slider_, [](lv_event_t* event) {
      auto* self = static_cast<Ui480*>(lv_event_get_user_data(event));
      if (!self) return;
      if (lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED && !self->settings_updating_) {
        self->settings_brightness_ = static_cast<uint8_t>(lv_slider_get_value(self->settings_brightness_slider_));
        self->refreshSettingsLabels();
        if (self->brightness_changed_) self->brightness_changed_(self->settings_context_, self->settings_brightness_);
      }
      lv_event_stop_bubbling(event);
    }, LV_EVENT_ALL, this);

    makeDashboardLabel(settings_controls_page_, "音量", 24, 80, 290,
      controlLabelFont, 0xe7e9ed);
    settings_volume_value_ = makeDashboardLabel(settings_controls_page_, "85%", 356, 76, 100,
      native480_fonts::montserrat_24(), 0xe7e9ed, LV_TEXT_ALIGN_RIGHT);
    settings_volume_slider_ = lv_slider_create(settings_controls_page_);
    lv_obj_set_pos(settings_volume_slider_, 24, 108);
    lv_obj_set_size(settings_volume_slider_, 432, 28);
    lv_obj_set_ext_click_area(settings_volume_slider_, 10);
    lv_slider_set_range(settings_volume_slider_, 0, 100);
    lv_obj_set_style_bg_color(settings_volume_slider_, lv_color_hex(0x0d151b), LV_PART_MAIN);
    lv_obj_set_style_bg_color(settings_volume_slider_, lv_color_hex(0x25a9e0), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(settings_volume_slider_, lv_color_hex(0xe7e9ed), LV_PART_KNOB);
    lv_obj_set_style_pad_ver(settings_volume_slider_, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(settings_volume_slider_, 0, LV_PART_KNOB);
    lv_obj_set_style_radius(settings_volume_slider_, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(settings_volume_slider_, 4, LV_PART_INDICATOR);
    lv_obj_set_style_radius(settings_volume_slider_, 14, LV_PART_KNOB);
    lv_obj_set_style_border_width(settings_volume_slider_, 0, LV_PART_KNOB);
    lv_obj_add_event_cb(settings_volume_slider_, [](lv_event_t* event) {
      auto* self = static_cast<Ui480*>(lv_event_get_user_data(event));
      if (!self) return;
      if (lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED && !self->settings_updating_) {
        self->settings_volume_ = static_cast<uint8_t>(lv_slider_get_value(self->settings_volume_slider_));
        self->refreshSettingsLabels();
        if (self->volume_changed_) self->volume_changed_(self->settings_context_, self->settings_volume_);
      }
      lv_event_stop_bubbling(event);
    }, LV_EVENT_ALL, this);

    makeDashboardLabel(settings_controls_page_, "語音播報", 24, 164, 330,
      controlLabelFont, 0xe7e9ed);
    lv_obj_t* voice_switch = lv_switch_create(settings_controls_page_);
    lv_obj_set_pos(voice_switch, 384, 174);
    lv_obj_set_size(voice_switch, 72, 28);
    lv_obj_set_ext_click_area(voice_switch, 10);
    lv_obj_set_style_bg_color(voice_switch, lv_color_hex(0x0d151b), LV_PART_MAIN);
    const lv_style_selector_t checkedIndicatorSelector = static_cast<lv_style_selector_t>(
      static_cast<uint32_t>(LV_PART_INDICATOR) | static_cast<uint32_t>(LV_STATE_CHECKED));
    lv_obj_set_style_bg_color(voice_switch, lv_color_hex(0x16b88a), checkedIndicatorSelector);
    lv_obj_set_style_bg_color(voice_switch, lv_color_hex(0xe7e9ed), LV_PART_KNOB);
    lv_obj_set_style_pad_ver(voice_switch, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(voice_switch, 0, LV_PART_KNOB);
    lv_obj_set_style_radius(voice_switch, 14, LV_PART_MAIN);
    lv_obj_set_style_radius(voice_switch, 14, LV_PART_INDICATOR);
    lv_obj_set_style_radius(voice_switch, 14, LV_PART_KNOB);
    lv_obj_set_style_border_width(voice_switch, 0, LV_PART_KNOB);
    lv_obj_add_state(voice_switch, LV_STATE_CHECKED);
    lv_obj_add_event_cb(voice_switch, [](lv_event_t* event) {
      auto* self = static_cast<Ui480*>(lv_event_get_user_data(event));
      if (!self || lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) return;
      self->settings_voice_enabled_ = lv_obj_has_state(lv_event_get_target(event), LV_STATE_CHECKED);
      self->refreshSettingsLabels();
      if (self->voice_changed_) self->voice_changed_(self->settings_context_, self->settings_voice_enabled_);
      lv_event_stop_bubbling(event);
    }, LV_EVENT_VALUE_CHANGED, this);
    settings_voice_switch_ = voice_switch;

    auto makeActionButton = [&](const char* text, int y, bool primary) {
      lv_obj_t* button = lv_btn_create(settings_controls_page_);
      lv_obj_set_pos(button, 24, y); lv_obj_set_size(button, 432, 52);
      lv_obj_set_style_bg_color(button, lv_color_hex(primary ? 0x25a9e0 : 0xd94a64), 0);
      lv_obj_set_style_bg_opa(button, primary ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_color(button, lv_color_hex(0xd94a64), 0);
      lv_obj_set_style_border_width(button, primary ? 0 : 1, 0);
      lv_obj_set_style_radius(button, 10, 0);
      lv_obj_set_style_bg_color(button, lv_color_hex(primary ? 0x208dbd : 0x30141b), LV_STATE_PRESSED);
      lv_obj_set_style_pad_all(button, 0, 0);
      makeCenteredSettingsButtonLabel(button, text, actionFont, primary ? 0x03080b : 0xd94a64);
      return button;
    };
    settings_voice_test_button_ = makeActionButton("播放語音測試", 222, true);
    lv_obj_add_event_cb(settings_voice_test_button_, [](lv_event_t* event) {
      auto* self = static_cast<Ui480*>(lv_event_get_user_data(event));
      if (self && lv_event_get_code(event) == LV_EVENT_CLICKED && self->test_tone_)
        self->test_tone_(self->settings_context_);
      lv_event_stop_bubbling(event);
    }, LV_EVENT_ALL, this);
    restart_button_ = makeActionButton("重新啟動", 282, false);
    restart_button_label_ = lv_obj_get_child(restart_button_, 0);
    lv_obj_add_event_cb(restart_button_, [](lv_event_t* event) {
      auto* self = static_cast<Ui480*>(lv_event_get_user_data(event));
      if (!self || lv_event_get_code(event) != LV_EVENT_CLICKED) return;
      const uint32_t now = millis();
      if (self->restart_armed_ && static_cast<uint32_t>(now - self->restart_armed_at_ms_) <= 3000) {
        self->restart_armed_ = false;
        setLabelText480(self->restart_button_label_, "重新啟動");
        self->centerSettingsButtonLabel(lv_obj_get_parent(self->restart_button_label_), self->restart_button_label_);
        if (self->restart_) self->restart_(self->settings_context_);
      } else {
        self->restart_armed_ = true; self->restart_armed_at_ms_ = now;
        setLabelText480(self->restart_button_label_, "再點");
        self->centerSettingsButtonLabel(lv_obj_get_parent(self->restart_button_label_), self->restart_button_label_);
      }
      lv_event_stop_bubbling(event);
    }, LV_EVENT_ALL, this);

    makeDashboardLabel(settings_network_page_, "網路", 24, 4, 432,
      native480_i18n::kEnglish ? native480_fonts::montserrat_21() : native480_fonts::home_cjk_20(),
      0xe7e9ed);
    settings_wifi_title_ = makeDashboardLabel(settings_network_page_, "Wi-Fi 網路", 24, 40, 432,
      native480_i18n::kEnglish ? native480_fonts::montserrat_13() : native480_fonts::cjk_13(),
      0x7d879a);
    settings_wifi_ssid_value_ = makeDashboardLabel(settings_network_page_, "Home-WiFi",
      24, 60, 432, native480_fonts::montserrat_24(), 0xe7e9ed);
    lv_label_set_long_mode(settings_wifi_ssid_value_, LV_LABEL_LONG_WRAP);
    lv_obj_t* network_rule = makeDashboardRule(settings_network_page_, 24, 116, 432, 1);
    lv_obj_set_style_bg_color(network_rule, lv_color_hex(0x0d151b), 0);
    settings_http_title_ = makeDashboardLabel(settings_network_page_, "設定頁網址", 24, 136, 432,
      native480_i18n::kEnglish ? native480_fonts::montserrat_13() : native480_fonts::cjk_13(),
      0x7d879a);
    settings_gateway_url_label_ = makeDashboardLabel(settings_network_page_, "http://192.0.2.15/",
      24, 156, 432, native480_fonts::montserrat_24(), 0xe7e9ed);
    lv_label_set_long_mode(settings_gateway_url_label_, LV_LABEL_LONG_WRAP);
    network_rule = makeDashboardRule(settings_network_page_, 24, 204, 432, 1);
    lv_obj_set_style_bg_color(network_rule, lv_color_hex(0x0d151b), 0);
    makeDashboardLabel(settings_network_page_, "Powerwall Gateway IP 位址", 24, 224, 432,
      native480_i18n::kEnglish ? native480_fonts::montserrat_13() : native480_fonts::cjk_13(),
      0x7d879a);
    settings_gateway_ip_label_ = makeDashboardLabel(settings_network_page_, "--",
      24, 244, 432, native480_fonts::montserrat_24(), 0xe7e9ed);

    const lv_font_t* footerFont = native480_i18n::kEnglish
      ? native480_fonts::montserrat_13() : native480_fonts::cjk_13();
    settings_uptime_label_ = makeDashboardLabel(page_settings_, "開機時間: 0d 00:00:00",
      24, 456, 300, footerFont, 0x7d879a);
    settings_version_label_ = makeDashboardLabel(page_settings_, native480_i18n::kManualVersion,
      365, 456, 91, native480_fonts::montserrat_13(), 0x7d879a, LV_TEXT_ALIGN_RIGHT);
    refreshSettingsUptime();
    refreshSettingsLabels();
    lv_slider_set_value(settings_brightness_slider_, settings_brightness_, LV_ANIM_OFF);
    lv_slider_set_value(settings_volume_slider_, settings_volume_, LV_ANIM_OFF);
    showSettingsSubpage(settings_network_page_active_);
  }

  void initializeSettingsPage() { initializeSettingsPage480(); }

  void initializeAddedPages(lv_obj_t* screen) {
    dashboard_screen_ = screen;
    page_powerwall_ = lv_obj_create(screen);
    page_home_grid_ = lv_obj_create(screen);
    page_today_trend_ = lv_obj_create(screen);
    page_settings_ = lv_obj_create(screen);
    lv_obj_t* pages[] = {page_powerwall_, page_home_grid_, page_today_trend_, page_settings_};
    for (lv_obj_t* page : pages) {
      lv_obj_set_pos(page, 0, 0);
      lv_obj_set_size(page, 480, 480);
      lv_obj_set_style_bg_color(page, lv_color_hex(0x0b0f14), 0);
      lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
      lv_obj_set_style_border_width(page, 0, 0);
      lv_obj_set_style_radius(page, 0, 0);
      lv_obj_set_style_pad_all(page, 0, 0);
      lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
    }

    initializeWeeklyPage(screen);
    initializeDailyTrendPage();
    initializeSettingsPage();
    // One shared header keeps the two added pages aligned and leaves the original page untouched.
    page_header_ = lv_obj_create(screen);
    lv_obj_set_pos(page_header_, 0, 0);
    lv_obj_set_size(page_header_, 480, 59);
    lv_obj_set_style_bg_opa(page_header_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(page_header_, 0, 0);
    lv_obj_set_style_radius(page_header_, 0, 0);
    lv_obj_set_style_pad_all(page_header_, 0, 0);
    lv_obj_clear_flag(page_header_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(page_header_, LV_OBJ_FLAG_HIDDEN);
    page_title_ = makeDashboardLabel(page_header_, "POWERWALL", 13, 5, 167, native480_fonts::cjk_13(), 0xf1f5f9);
    page_site_ = makeDashboardLabel(page_header_, native480_i18n::kDefaultSiteTitle, 13, 23, 147, native480_fonts::cjk_8(), 0x94a3b8);
    pw_header_title_ = makeDashboardLabel(page_header_, "POWERWALL", 11, 7, 213, native480_fonts::home_cjk_27(), 0xf1f5f9);
    // Keep the Powerwall clock at the same 8 px screen inset as Home.
    pw_header_clock_ = makeDashboardLabel(page_header_, "--/-- --- --:--", rightAlignedHeaderClockX(page_header_, 201), 17, 201,
      native480_i18n::kEnglish ? native480_fonts::montserrat_16() : native480_fonts::cjk_13(),
      0xf1f5f9, LV_TEXT_ALIGN_RIGHT);
    pw_header_date_ = makeDashboardLabel(page_header_, "", 365, 37, 75, native480_fonts::montserrat_16(), 0x94a3b8, LV_TEXT_ALIGN_RIGHT);
    pw_header_status_dot_ = lv_obj_create(page_header_);
    lv_obj_set_pos(pw_header_status_dot_, 455, 22);
    lv_obj_set_size(pw_header_status_dot_, 8, 8);
    lv_obj_set_style_radius(pw_header_status_dot_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(pw_header_status_dot_, lv_color_hex(0x34d399), 0);
    lv_obj_set_style_bg_opa(pw_header_status_dot_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pw_header_status_dot_, 0, 0);
    lv_obj_clear_flag(pw_header_status_dot_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    positionHeaderDotBeforeClock(pw_header_clock_, pw_header_status_dot_);

    page_status_pill_ = lv_obj_create(page_header_);
    lv_obj_set_pos(page_status_pill_, 301, 11);
    lv_obj_set_size(page_status_pill_, 69, 17);
    lv_obj_set_style_bg_color(page_status_pill_, lv_color_hex(0x064e3b), 0);
    lv_obj_set_style_bg_opa(page_status_pill_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(page_status_pill_, lv_color_hex(0x10b981), 0);
    lv_obj_set_style_border_width(page_status_pill_, 1, 0);
    lv_obj_set_style_radius(page_status_pill_, 9, 0);
    flattenDashboardPill(page_status_pill_, 0x10b981);
    lv_obj_set_style_pad_all(page_status_pill_, 0, 0);
    lv_obj_clear_flag(page_status_pill_, LV_OBJ_FLAG_SCROLLABLE);
    page_status_text_ = makeDashboardLabel(page_status_pill_, "即時資料", 0, 3, 69, native480_fonts::cjk_8(), 0x6ee7b7, LV_TEXT_ALIGN_CENTER);
    page_clock_ = makeDashboardLabel(page_header_, "--:--", 379, 5, 88, native480_fonts::cjk_12(), 0xf1f5f9, LV_TEXT_ALIGN_RIGHT);
    page_date_ = makeDashboardLabel(page_header_, native480_i18n::kEnglish ? "--/--" : "--月--日",
      379, 23, 88, native480_i18n::kEnglish ? native480_fonts::montserrat_12() : native480_fonts::cjk_8(),
      0x94a3b8, LV_TEXT_ALIGN_RIGHT);

    // Native 480 Powerwall summary: gauge left, three stacked metrics right.
    lv_obj_t* pwHero = makeDashboardCard(page_powerwall_, 9, 52, 461, 243);
    pw_arc_ = lv_arc_create(pwHero);
    lv_obj_set_pos(pw_arc_, 8, 12);
    lv_obj_set_size(pw_arc_, 218, 218);
    lv_arc_set_range(pw_arc_, 0, 1000);  // tenths of a percent for a smooth intro sweep
    lv_arc_set_bg_angles(pw_arc_, 135, 45);
    lv_arc_set_rotation(pw_arc_, 0);
    lv_arc_set_value(pw_arc_, 0);
    for (auto part : {LV_PART_MAIN, LV_PART_INDICATOR}) {
      lv_obj_set_style_arc_width(pw_arc_, 16, part);
      lv_obj_set_style_arc_opa(pw_arc_, LV_OPA_COVER, part);
      lv_obj_set_style_arc_rounded(pw_arc_, true, part);
    }
    lv_obj_set_style_arc_color(pw_arc_, lv_color_hex(0x273b45), LV_PART_MAIN);
    lv_obj_set_style_arc_color(pw_arc_, lv_color_hex(0x39ff14), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(pw_arc_, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_opa(pw_arc_, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(pw_arc_, 0, LV_PART_KNOB);
    lv_obj_clear_flag(pw_arc_, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    pw_reserve_marker_ = lv_line_create(pwHero);
    lv_line_set_points(pw_reserve_marker_, pw_reserve_marker_points_, 2);
    lv_obj_set_style_line_color(pw_reserve_marker_, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_line_width(pw_reserve_marker_, 4, 0);
    lv_obj_set_style_line_rounded(pw_reserve_marker_, true, 0);
    lv_obj_clear_flag(pw_reserve_marker_, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    pw_soc_text_ = makeDashboardLabel(pwHero, "--%", 20, 59, 194, native480_fonts::montserrat_60(), 0xffffff, LV_TEXT_ALIGN_CENTER);
    pw_soc_bold_pass_ = makeDashboardLabel(pwHero, "--%", 21, 59, 194, native480_fonts::montserrat_60(), 0xffffff, LV_TEXT_ALIGN_CENTER);
    makeDashboardLabel(pwHero, "電池電量", 20, 123, 194, native480_fonts::home_cjk_16(), 0xffffff, LV_TEXT_ALIGN_CENTER);
    makeDashboardRule(pwHero, 49, 149, 136, 1);
    pw_state_text_ = makeDashboardLabel(pwHero, "待機中  0 kW", 21, 161, 192, native480_fonts::home_cjk_16(), 0x69ff00, LV_TEXT_ALIGN_CENTER);
    makeDashboardRule(pwHero, 240, 7, 1, 229);
    makeDashboardRule(pwHero, 250, 84, 205, 1);
    makeDashboardRule(pwHero, 250, 165, 205, 1);
    for (int y : {14, 96, 179}) {
      lv_obj_t* accent = lv_obj_create(pwHero);
      lv_obj_remove_style_all(accent);
      lv_obj_set_pos(accent, 255, y);
      lv_obj_set_size(accent, 5, 52);
      lv_obj_set_style_bg_color(accent, lv_color_hex(0x39ff14), 0);
      lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
      lv_obj_set_style_radius(accent, 3, 0);
    }
    makeDashboardLabel(pwHero, "預估可用時間 / 備援可用時間", 270, 9, 185, native480_fonts::cjk_13(), 0xffffff);
    pw_eta_text_ = makeDashboardLabel(pwHero, "--", 268, 30, 185, native480_fonts::montserrat_27(), 0x38bdf8, LV_TEXT_ALIGN_RIGHT);
    pw_backup_eta_text_ = makeDashboardLabel(pwHero, "/ --", 268, 63, 185, native480_fonts::montserrat_16(), 0xb8c1d0, LV_TEXT_ALIGN_RIGHT);
    lv_label_set_long_mode(pw_eta_text_, LV_LABEL_LONG_CLIP);
    lv_label_set_long_mode(pw_backup_eta_text_, LV_LABEL_LONG_CLIP);
    makeDashboardLabel(pwHero, "剩餘電量 / 目前容量", 270, 91, 185, native480_fonts::home_cjk_16(), 0xffffff);
    pw_capacity_current_value_ = makeDashboardLabel(pwHero, "-- kWh", 268, 113, 185, native480_fonts::montserrat_24(), 0xffffff, LV_TEXT_ALIGN_RIGHT);
    pw_capacity_total_value_ = makeDashboardLabel(pwHero, "/ -- kWh", 268, 141, 185, native480_fonts::montserrat_16(), 0xb8c1d0, LV_TEXT_ALIGN_RIGHT);
    makeDashboardLabel(pwHero, "電池電壓", 270, 174, 185, native480_fonts::home_cjk_16(), 0xffffff);
    pw_voltage_value_ = makeDashboardLabel(pwHero, "-- V", 268, 201, 185, native480_fonts::montserrat_27(), 0xffffff, LV_TEXT_ALIGN_RIGHT);

    pw_chart_card_ = makeDashboardCard(page_powerwall_, 9, 303, 461, 171);
    dashboard_chart_buf_ = allocPixelBuf(kChartW * kDashboardChartH);
    if (dashboard_chart_buf_) {
      pw_chart_canvas_ = lv_canvas_create(pw_chart_card_);
      lv_canvas_set_buffer(pw_chart_canvas_, dashboard_chart_buf_, kChartW, kPowerwallTrendSocH, LV_IMG_CF_TRUE_COLOR);
      lv_obj_set_pos(pw_chart_canvas_, kPwChartPlotX, kPowerwallTrendSocY);
      lv_obj_clear_flag(pw_chart_canvas_, LV_OBJ_FLAG_SCROLLABLE);
    }
    pw_load_chart_buf_ = allocPixelBuf(kChartW * kPowerwallTrendLoadH);
    if (pw_load_chart_buf_) {
      pw_load_chart_canvas_ = lv_canvas_create(pw_chart_card_);
      lv_canvas_set_buffer(pw_load_chart_canvas_, pw_load_chart_buf_, kChartW, kPowerwallTrendLoadH, LV_IMG_CF_TRUE_COLOR);
      lv_obj_set_pos(pw_load_chart_canvas_, kPwChartPlotX, kPowerwallTrendLoadY);
      lv_obj_clear_flag(pw_load_chart_canvas_, LV_OBJ_FLAG_SCROLLABLE);
    }
    const lv_font_t* const trendAxisFont = native480_fonts::montserrat_12();
    const lv_font_t* const trendTitleFont = native480_i18n::kEnglish
      ? native480_fonts::montserrat_12() : native480_fonts::cjk_12();
    makeDashboardLabel(pw_chart_card_, "10 kW", 0, 1, 36,
      trendAxisFont, 0x94a3b8, LV_TEXT_ALIGN_RIGHT);
    makeDashboardLabel(pw_chart_card_, "5 kW", 0, 32, 36,
      trendAxisFont, 0x94a3b8, LV_TEXT_ALIGN_RIGHT);
    makeDashboardLabel(pw_chart_card_, "0 kW", 0, 56, 36, trendAxisFont, 0x94a3b8, LV_TEXT_ALIGN_RIGHT);
    makeDashboardLabel(pw_chart_card_, "100%", 0, 78, 36, trendAxisFont, 0x94a3b8, LV_TEXT_ALIGN_RIGHT);
    makeDashboardLabel(pw_chart_card_, "0%", 0, 134, 36, trendAxisFont, 0x94a3b8, LV_TEXT_ALIGN_RIGHT);
    makeDashboardLabel(pw_chart_card_, native480_i18n::kEnglish ? "Home" : "住家",
      kPwChartPlotX + 3, kPowerwallTrendLoadY + 2,
      42, trendTitleFont, 0x38bdf8);
    makeDashboardLabel(pw_chart_card_, "PW", kPwChartPlotX + 3, kPowerwallTrendSocY + 2,
      28, trendAxisFont, 0x11e58e);
    static const char* pwFixedTicks[] = {"00", "06", "12", "18", "24"};
    for (int i = 0; i < 5; ++i) {
      constexpr int tickWidth = 40;
      const int x = i == 0 ? kPwChartPlotX :
                    i == 4 ? kPwChartPlotX + kChartW - tickWidth :
                    kPwChartPlotX + static_cast<int>(roundf(i * kChartW / 4.0f)) - tickWidth / 2;
      const lv_text_align_t align = i == 0 ? LV_TEXT_ALIGN_LEFT :
                                    i == 4 ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_CENTER;
      pw_time_ticks_[i] = makeDashboardLabel(pw_chart_card_, pwFixedTicks[i], x, 154, 27, native480_fonts::montserrat_13(), 0xcbd5e1, align);
    }
    for (int i = 0; i < 4; ++i) {
      pw_tou_segments_[i] = lv_obj_create(pw_chart_card_);
      lv_obj_set_pos(pw_tou_segments_[i], 36, 148);
      lv_obj_set_size(pw_tou_segments_[i], 1, 5);
      lv_obj_set_style_border_width(pw_tou_segments_[i], 0, 0);
      lv_obj_set_style_radius(pw_tou_segments_[i], 0, 0);
      lv_obj_clear_flag(pw_tou_segments_[i], LV_OBJ_FLAG_SCROLLABLE);
    }
    pw_now_line_ = lv_obj_create(pw_chart_card_);
    lv_obj_set_size(pw_now_line_, 1, 147);
    lv_obj_set_pos(pw_now_line_, 36, 7);
    lv_obj_set_style_bg_color(pw_now_line_, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(pw_now_line_, LV_OPA_60, 0);
    lv_obj_set_style_border_width(pw_now_line_, 0, 0);
    pw_now_dot_ = lv_obj_create(pw_chart_card_);
    lv_obj_set_size(pw_now_dot_, 7, 7);
    lv_obj_set_style_radius(pw_now_dot_, 3, 0);
    lv_obj_set_style_bg_color(pw_now_dot_, lv_color_hex(0x11e58e), 0);
    lv_obj_set_style_bg_opa(pw_now_dot_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(pw_now_dot_, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_border_width(pw_now_dot_, 1, 0);
    lv_obj_add_flag(pw_now_dot_, LV_OBJ_FLAG_HIDDEN);
    pw_load_now_dot_ = lv_obj_create(pw_chart_card_);
    lv_obj_set_size(pw_load_now_dot_, 7, 7);
    lv_obj_set_style_radius(pw_load_now_dot_, 3, 0);
    lv_obj_set_style_bg_color(pw_load_now_dot_, lv_color_hex(0x38bdf8), 0);
    lv_obj_set_style_bg_opa(pw_load_now_dot_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(pw_load_now_dot_, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_border_width(pw_load_now_dot_, 1, 0);
    lv_obj_add_flag(pw_load_now_dot_, LV_OBJ_FLAG_HIDDEN);
    pw_reserve_chart_label_ = makeDashboardLabel(pw_chart_card_, "", 0, 0, 0, native480_fonts::home_cjk_20(), 0x7dd3fc, LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_width(pw_reserve_chart_label_, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(pw_reserve_chart_label_, lv_color_make(9, 10, 15), 0);
    lv_obj_set_style_bg_opa(pw_reserve_chart_label_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(pw_reserve_chart_label_, 2, 0);

    // Home/grid page: one TOU schedule band, meter rows, and source-colored load history.
    lv_obj_t* touCard = makeDashboardCard(page_home_grid_, 9, kHomeTouCardY, 461, kHomeTouCardH);
    home_tou_pill_ = lv_obj_create(touCard);
    lv_obj_set_pos(home_tou_pill_, 8, kHomeTouPillY);
    lv_obj_set_size(home_tou_pill_, 108, kHomeTouPillH);
    lv_obj_set_style_bg_color(home_tou_pill_, lv_color_hex(0x064e3b), 0);
    lv_obj_set_style_bg_opa(home_tou_pill_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(home_tou_pill_, lv_color_hex(0x10b981), 0);
    lv_obj_set_style_border_width(home_tou_pill_, 1, 0);
    lv_obj_set_style_radius(home_tou_pill_, 11, 0);
    lv_obj_set_style_border_width(home_tou_pill_, 1, 0);
    lv_obj_set_style_pad_all(home_tou_pill_, 0, 0);
    lv_obj_clear_flag(home_tou_pill_, LV_OBJ_FLAG_SCROLLABLE);
    home_tou_pill_text_ = makeDashboardLabel(home_tou_pill_, "目前：離峰", 0, 0, 108, native480_fonts::grid_cjk_16(), 0x6ee7b7, LV_TEXT_ALIGN_CENTER);
    centerDashboardLabelInkVertically(home_tou_pill_, home_tou_pill_text_);
    home_interval_start_ = makeDashboardLabel(touCard, "00:00", 0, kHomeTouIntervalTextY, 0, native480_fonts::cjk_12(), 0xf1f5f9);
    home_interval_rule_ = lv_obj_create(touCard);
    lv_obj_set_pos(home_interval_rule_, 0, kHomeTouIntervalDashY);
    lv_obj_set_size(home_interval_rule_, kHomeTouIntervalDashW, 1);
    lv_obj_set_style_bg_color(home_interval_rule_, lv_color_hex(0xf1f5f9), 0);
    lv_obj_set_style_bg_opa(home_interval_rule_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(home_interval_rule_, 0, 0);
    lv_obj_set_style_radius(home_interval_rule_, 1, 0);
    lv_obj_clear_flag(home_interval_rule_, LV_OBJ_FLAG_SCROLLABLE);
    home_interval_end_ = makeDashboardLabel(touCard, "06:00", 0, kHomeTouIntervalTextY, 0, native480_fonts::cjk_12(), 0xf1f5f9);
    layoutHomeTouIntervalLabels();
    home_price_ = makeDashboardLabel(touCard, "NT$1.99 / 度", 8, kHomeTouPriceY, 133, native480_fonts::grid_cjk_16(), 0xf1f5f9);
    home_countdown_ = makeDashboardLabel(touCard, "下一次尖峰 06:00 · 還有 --", 145, kHomeTouCountdownY, 307, native480_fonts::cjk_14(), 0xcbd5e1);
    for (int i = 0; i < 4; ++i) {
      home_tou_segments_[i] = lv_obj_create(touCard);
      lv_obj_set_pos(home_tou_segments_[i], 8, kHomeTouSegmentY);
      lv_obj_set_size(home_tou_segments_[i], 1, 12);
      lv_obj_set_style_border_width(home_tou_segments_[i], 0, 0);
      lv_obj_set_style_radius(home_tou_segments_[i], 0, 0);
      lv_obj_clear_flag(home_tou_segments_[i], LV_OBJ_FLAG_SCROLLABLE);
      home_tou_segment_labels_[i] = makeDashboardLabel(home_tou_segments_[i], "", 0, 1, 1, native480_fonts::cjk_12(), 0xffffff, LV_TEXT_ALIGN_CENTER);
    }
    for (int i = 0; i < 5; ++i) {
      home_tou_ticks_[i] = makeDashboardLabel(touCard, "", 0, kHomeTouTicksY, 36, native480_fonts::cjk_12(), 0xcbd5e1, i == 0 ? LV_TEXT_ALIGN_LEFT : (i == 4 ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_CENTER));
    }
    home_tou_now_line_ = lv_obj_create(touCard);
    lv_obj_set_size(home_tou_now_line_, 1, 21);
    lv_obj_set_style_bg_color(home_tou_now_line_, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(home_tou_now_line_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(home_tou_now_line_, 0, 0);
    lv_obj_set_style_radius(home_tou_now_line_, 0, 0);
    lv_obj_clear_flag(home_tou_now_line_, LV_OBJ_FLAG_SCROLLABLE);
    home_tou_now_dot_ = lv_obj_create(touCard);
    lv_obj_set_size(home_tou_now_dot_, 5, 5);
    lv_obj_set_style_radius(home_tou_now_dot_, 3, 0);
    lv_obj_set_style_bg_color(home_tou_now_dot_, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(home_tou_now_dot_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(home_tou_now_dot_, 0, 0);

    lv_obj_t* gridCard = makeDashboardCard(page_home_grid_, 9, kHomeGridCardY, 461, kHomeGridCardH);
    memcpy(home_grid_icon_points_, kHomeGridIconGridLines, sizeof(home_grid_icon_points_));
    for (auto& points : home_grid_icon_points_)
      makeHomeGridIconLine(gridCard, points, 8, 6, 0xf1f5f9);
    home_grid_heading_ = makeDashboardLabel(gridCard, "電網", 34, 3, 180, native480_fonts::grid_cjk_20(), 0xf1f5f9);
    home_grid_status_ = makeDashboardLabel(gridCard, "—", 277, 6, 173, native480_fonts::grid_cjk_16(), 0x38bdf8, LV_TEXT_ALIGN_RIGHT);
    home_grid_h_rules_[0] = makeDashboardRule(gridCard, 8, 27, 445);
    home_grid_power_title_ = makeDashboardLabel(gridCard, "輸入功率", 13, 29, 137, native480_fonts::cjk_12(), 0xcbd5e1);
    home_grid_power_value_ = makeDashboardLabel(gridCard, "— kW", 8, 42, 151, native480_fonts::grid_cjk_20(), 0x38bdf8, LV_TEXT_ALIGN_CENTER);
    makeDashboardRule(gridCard, 159, 31, 1, 34);
    makeDashboardLabel(gridCard, "電流", 169, 29, 113, native480_fonts::cjk_12(), 0xcbd5e1);
    home_grid_current_value_ = makeDashboardLabel(gridCard, "— A", 160, 42, 141, native480_fonts::grid_cjk_18(), 0xf1f5f9, LV_TEXT_ALIGN_CENTER);
    makeDashboardRule(gridCard, 301, 31, 1, 34);
    makeDashboardLabel(gridCard, "電壓", 312, 29, 127, native480_fonts::cjk_12(), 0xcbd5e1);
    home_grid_voltage_value_ = makeDashboardLabel(gridCard, "— V", 302, 42, 151, native480_fonts::grid_cjk_18(), 0xf1f5f9, LV_TEXT_ALIGN_CENTER);
    home_grid_h_rules_[1] = makeDashboardRule(gridCard, 8, kHomeGridTotalsSeparatorY, 445);
    home_grid_import_title_ = makeDashboardLabel(gridCard, "累計購電", 13, 76, 59, native480_fonts::cjk_14(), 0xcbd5e1);
    const int centeredTotalsValueY = homeGridCumulativeValueY();
    home_grid_import_value_ = makeDashboardLabel(gridCard, "— kWh", 75, centeredTotalsValueY, 145, native480_fonts::grid_cjk_16(), 0xf1f5f9);
    makeDashboardRule(gridCard, 225, 71, 1, 24);
    home_grid_export_title_ = makeDashboardLabel(gridCard, "累計回送", 237, 76, 59, native480_fonts::cjk_14(), 0xcbd5e1);
    home_grid_export_value_ = makeDashboardLabel(gridCard, "— kWh", 299, centeredTotalsValueY, 148, native480_fonts::grid_cjk_16(), 0xf1f5f9);

    lv_obj_t* homeCard = makeDashboardCard(page_home_grid_, 9, kHomeMetricsCardY, 461, kHomeMetricsCardH);
    memcpy(home_house_icon_points_, kHomeGridIconHouseLines, sizeof(home_house_icon_points_));
    for (auto& points : home_house_icon_points_)
      makeHomeGridIconLine(homeCard, points, 8, 4, 0xf1f5f9);
    home_grid_home_heading_ = makeDashboardLabel(homeCard, "住家", 34, 3, 180, native480_fonts::grid_cjk_20(), 0xf1f5f9);
    home_solar_value_ = makeDashboardLabel(homeCard, "太陽能 — kW", 270, 6, 181, native480_fonts::grid_cjk_16(), 0xfbbf24, LV_TEXT_ALIGN_RIGHT);
    home_metrics_h_rules_[0] = makeDashboardRule(homeCard, 8, 27, 445);
    makeDashboardLabel(homeCard, "用電功率", 13, 28, 137, native480_fonts::cjk_12(), 0xcbd5e1);
    home_load_value_ = makeDashboardLabel(homeCard, "— kW", 8, 41, 151, native480_fonts::grid_cjk_20(), 0x38bdf8, LV_TEXT_ALIGN_CENTER);
    makeDashboardRule(homeCard, 159, 29, 1, 34);
    makeDashboardLabel(homeCard, "電流", 169, 28, 113, native480_fonts::cjk_12(), 0xcbd5e1);
    home_load_current_value_ = makeDashboardLabel(homeCard, "— A", 160, 41, 141, native480_fonts::grid_cjk_18(), 0xf1f5f9, LV_TEXT_ALIGN_CENTER);
    makeDashboardRule(homeCard, 301, 29, 1, 34);
    makeDashboardLabel(homeCard, "電壓", 312, 28, 127, native480_fonts::cjk_12(), 0xcbd5e1);
    home_load_voltage_value_ = makeDashboardLabel(homeCard, "— V", 302, 41, 151, native480_fonts::grid_cjk_18(), 0xf1f5f9, LV_TEXT_ALIGN_CENTER);
    home_metrics_h_rules_[1] = makeDashboardRule(homeCard, 8, 65, 445);
    home_power_factor_ = makeDashboardLabel(homeCard, "功率因數 —", 13, 71, 185, native480_fonts::cjk_12(), 0xcbd5e1);
    home_energy_value_ = makeDashboardLabel(homeCard, "累計用電 — kWh", 199, 71, 252, native480_fonts::cjk_12(), 0xcbd5e1, LV_TEXT_ALIGN_RIGHT);

    lv_obj_t* homeChartCard = makeDashboardCard(page_home_grid_, 9, kHomeChartCardY, 461, kHomeChartCardH);
    makeDashboardLabel(homeChartCard, "住家用電", 8, 0, 70, native480_fonts::cjk_12(), 0xcbd5e1);
    makeDashboardLabel(homeChartCard, "未知", 224, 0, 35, native480_fonts::cjk_12(), 0xcbd5e1);
    lv_obj_t* unknownSwatch = lv_obj_create(homeChartCard);
    lv_obj_set_pos(unknownSwatch, 211, 6); lv_obj_set_size(unknownSwatch, 7, 2);
    lv_obj_set_style_bg_color(unknownSwatch, lv_color_hex(0xcbd5e1), 0); lv_obj_set_style_border_width(unknownSwatch, 0, 0);
    makeDashboardLabel(homeChartCard, "電網", 291, 0, 35, native480_fonts::cjk_12(), 0x38bdf8);
    lv_obj_t* gridSwatch = lv_obj_create(homeChartCard);
    lv_obj_set_pos(gridSwatch, 278, 6); lv_obj_set_size(gridSwatch, 7, 2);
    lv_obj_set_style_bg_color(gridSwatch, lv_color_hex(0x38bdf8), 0); lv_obj_set_style_border_width(gridSwatch, 0, 0);
    makeDashboardLabel(homeChartCard, "Powerwall", 360, 0, 92, native480_fonts::cjk_12(), 0x11e58e);
    lv_obj_t* pwSwatch = lv_obj_create(homeChartCard);
    lv_obj_set_pos(pwSwatch, 347, 6); lv_obj_set_size(pwSwatch, 7, 2);
    lv_obj_set_style_bg_color(pwSwatch, lv_color_hex(0x11e58e), 0); lv_obj_set_style_border_width(pwSwatch, 0, 0);
    if (dashboard_chart_buf_) {
      home_chart_canvas_ = lv_canvas_create(homeChartCard);
      lv_canvas_set_buffer(home_chart_canvas_, dashboard_chart_buf_, kHomeChartW, kHomeChartPlotH, LV_IMG_CF_TRUE_COLOR);
      lv_obj_set_pos(home_chart_canvas_, kHomeChartPlotX, kHomeChartPlotY);
      lv_obj_clear_flag(home_chart_canvas_, LV_OBJ_FLAG_SCROLLABLE);
    }
    const int homeAxisFontH = native480_fonts::montserrat_12()->line_height;
    home_chart_y_labels_[0] = makeDashboardLabel(homeChartCard, "6kW", 0, kHomeChartPlotY - homeAxisFontH / 2, 28, native480_fonts::montserrat_12(), 0x64748b, LV_TEXT_ALIGN_RIGHT);
    home_chart_y_labels_[1] = makeDashboardLabel(homeChartCard, "3kW", 0, kHomeChartPlotY + (kHomeChartPlotH - homeAxisFontH) / 2, 28, native480_fonts::montserrat_12(), 0x64748b, LV_TEXT_ALIGN_RIGHT);
    home_chart_y_labels_[2] = makeDashboardLabel(homeChartCard, "0", 0, kHomeChartPlotY + kHomeChartPlotH - 1 - homeAxisFontH / 2, 28, native480_fonts::montserrat_12(), 0x64748b, LV_TEXT_ALIGN_RIGHT);
    home_now_line_ = lv_obj_create(homeChartCard);
    lv_obj_set_size(home_now_line_, 1, kHomeChartPlotH);
    lv_obj_set_pos(home_now_line_, kHomeChartPlotX, kHomeChartPlotY);
    lv_obj_set_style_bg_color(home_now_line_, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(home_now_line_, LV_OPA_60, 0);
    lv_obj_set_style_border_width(home_now_line_, 0, 0);
    for (int i = 0; i < 4; ++i) {
      home_chart_tou_segments_[i] = lv_obj_create(homeChartCard);
      lv_obj_set_pos(home_chart_tou_segments_[i], kHomeChartPlotX, kHomeChartTouY);
      lv_obj_set_size(home_chart_tou_segments_[i], 1, kHomeChartTouH);
      lv_obj_set_style_border_width(home_chart_tou_segments_[i], 0, 0);
      lv_obj_set_style_radius(home_chart_tou_segments_[i], 0, 0);
      lv_obj_clear_flag(home_chart_tou_segments_[i], LV_OBJ_FLAG_SCROLLABLE);
    }
    lv_obj_move_foreground(home_now_line_);
    home_now_dot_ = lv_obj_create(homeChartCard);
    lv_obj_set_size(home_now_dot_, 6, 6);
    lv_obj_set_style_radius(home_now_dot_, 3, 0);
    lv_obj_set_style_bg_color(home_now_dot_, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(home_now_dot_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(home_now_dot_, lv_color_hex(0x0b0f14), 0);
    lv_obj_set_style_border_width(home_now_dot_, 1, 0);
    lv_obj_add_flag(home_now_dot_, LV_OBJ_FLAG_HIDDEN);
    static const char* homeTicks[] = {"00", "06", "12", "18", "24"};
    for (int i = 0; i < 5; ++i) {
      const int x = i == 0 ? kHomeChartPlotX : i == 4 ? kHomeChartPlotX + kHomeChartW - 36 :
        kHomeChartPlotX + static_cast<int>(roundf(i * kHomeChartW / 4.0f)) - 18;
      const lv_text_align_t align = i == 0 ? LV_TEXT_ALIGN_LEFT : i == 4 ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_CENTER;
      home_chart_ticks_[i] = makeDashboardLabel(homeChartCard, homeTicks[i], x, kHomeChartTimeTicksY, 36, native480_fonts::montserrat_13(), 0xcbd5e1, align);
    }
    home_data_stamp_ = makeDashboardLabel(homeChartCard, "", 247, 89, 200, native480_fonts::cjk_12(), 0x64748b, LV_TEXT_ALIGN_RIGHT);
    lv_obj_add_flag(home_data_stamp_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t* homeSections[] = {touCard, gridCard, homeCard};
    const int homeSectionBottoms[] = {
      kHomeTouCardH - 1, kHomeGridCardH - 1, kHomeMetricsCardH - 1,
    };
    for (int i = 0; i < 3; ++i) {
      home_section_bottom_rules_[i] = makeDashboardRule(homeSections[i], 0, homeSectionBottoms[i], 461);
      lv_obj_set_style_bg_color(home_section_bottom_rules_[i], lv_color_hex(0x36deda), 0);
    }
  }

  void registerPageTapRouting(lv_obj_t* root) {
    lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(root, [](lv_event_t* event) {
      if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
      Ui480* self = static_cast<Ui480*>(lv_event_get_user_data(event));
      if (!self) return;
      // Settings controls and tabs bubble clicks to the screen. Keep the 480
      // settings page open on background/text taps; PWRKEY still uses the
      // shared nextDashboardPage() route directly.
      if (self->active_dashboard_page_ == 5) return;
      self->nextDashboardPage();
    }, LV_EVENT_CLICKED, this);
    markPageTapBubble(root, pill_status_, pill_weather_);
  }

  static void markPageTapBubble(lv_obj_t* parent, lv_obj_t* excludedA, lv_obj_t* excludedB) {
    const uint32_t childCount = lv_obj_get_child_cnt(parent);
    for (uint32_t i = 0; i < childCount; ++i) {
      lv_obj_t* child = lv_obj_get_child(parent, i);
      if (child == excludedA || child == excludedB) continue;
      lv_obj_add_flag(child, LV_OBJ_FLAG_EVENT_BUBBLE);
      markPageTapBubble(child, excludedA, excludedB);
    }
  }

  void navigateDashboardPage(uint8_t target) {
    if (page_transition_phase_ != PageTransitionPhase::Idle) return;
    if (target >= dashboardPageCount() || target == active_dashboard_page_) return;
    page_transition_target_ = target;
    if (!page_transition_overlay_) {
      switchToPageTransitionTarget();
      return;
    }

    page_transition_phase_ = PageTransitionPhase::Covering;
    page_transition_opa_ = LV_OPA_TRANSP;
    lv_obj_set_style_bg_opa(page_transition_overlay_, page_transition_opa_, 0);
    lv_obj_clear_flag(page_transition_overlay_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(page_transition_overlay_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(page_transition_overlay_);
    if (!startPageTransitionAnimation(LV_OPA_TRANSP, LV_OPA_COVER, kPageTransitionCoverMs)) {
      // If LVGL cannot allocate its animation descriptor, keep navigation usable.
      switchToPageTransitionTarget();
      finishPageTransition();
    }
  }

  void createPageTransitionOverlay(lv_obj_t* screen) {
    page_transition_overlay_ = lv_obj_create(screen);
    if (!page_transition_overlay_) return;
    lv_obj_set_pos(page_transition_overlay_, 0, 0);
    lv_obj_set_size(page_transition_overlay_, 480, 480);
    lv_obj_set_style_bg_color(page_transition_overlay_, lv_color_hex(0x0b0c0f), 0);
    lv_obj_set_style_bg_opa(page_transition_overlay_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(page_transition_overlay_, 0, 0);
    lv_obj_set_style_radius(page_transition_overlay_, 0, 0);
    lv_obj_set_style_pad_all(page_transition_overlay_, 0, 0);
    lv_obj_clear_flag(page_transition_overlay_,
      LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(page_transition_overlay_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(page_transition_overlay_, [](lv_event_t* event) {
      // Consume taps on the full-screen cover so they never reach the page router.
      Ui480* self = static_cast<Ui480*>(lv_event_get_user_data(event));
      if (self && lv_event_get_code(event) == LV_EVENT_CLICKED) {
        ++self->page_transition_consumed_clicks_;
      }
      lv_event_stop_bubbling(event);
    }, LV_EVENT_ALL, this);
  }


  static void pageTransitionAnimationExec(void* var, int32_t value) {
    Ui480* self = static_cast<Ui480*>(var);
    if (!self || !self->page_transition_overlay_) return;
    const int32_t bounded = std::max<int32_t>(LV_OPA_TRANSP,
      std::min<int32_t>(LV_OPA_COVER, value));
    self->page_transition_opa_ = static_cast<lv_opa_t>(bounded);
    // Animate only the background opacity. Group opacity would allocate an LVGL layer.
    lv_obj_set_style_bg_opa(self->page_transition_overlay_, self->page_transition_opa_, 0);
  }

  static void pageTransitionAnimationReady(lv_anim_t* animation) {
    if (!animation) return;
    Ui480* self = static_cast<Ui480*>(animation->var);
    if (self) self->onPageTransitionAnimationReady();
  }

  bool startPageTransitionAnimation(lv_opa_t from, lv_opa_t to, uint16_t duration_ms) {
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, this);
    lv_anim_set_values(&animation, from, to);
    lv_anim_set_time(&animation, duration_ms);
    lv_anim_set_path_cb(&animation, lv_anim_path_linear);
    lv_anim_set_exec_cb(&animation, pageTransitionAnimationExec);
    lv_anim_set_ready_cb(&animation, pageTransitionAnimationReady);
    return lv_anim_start(&animation) != nullptr;
  }

  void onPageTransitionAnimationReady() {
    if (page_transition_phase_ == PageTransitionPhase::Covering) {
      // The source is fully covered before the page changes or its contents refresh.
      switchToPageTransitionTarget();
      if (!page_transition_overlay_) {
        finishPageTransition();
        return;
      }
      page_transition_phase_ = PageTransitionPhase::Uncovering;
      if (!startPageTransitionAnimation(LV_OPA_COVER, LV_OPA_TRANSP, kPageTransitionUncoverMs)) {
        finishPageTransition();
      }
    } else if (page_transition_phase_ == PageTransitionPhase::Uncovering) {
      finishPageTransition();
    }
  }

  void switchToPageTransitionTarget() {
    const int previousPage = active_dashboard_page_;
    active_dashboard_page_ = page_transition_target_;
    if (active_dashboard_page_ == 1 && previousPage != 1) startPowerwallGaugeIntro();
    else cancelPowerwallGaugeIntro();
    applyDashboardPageVisibility();
    refreshAddedPages();
  }

  void finishPageTransition() {
    page_transition_phase_ = PageTransitionPhase::Idle;
    page_transition_opa_ = LV_OPA_TRANSP;
    if (!page_transition_overlay_) return;
    lv_obj_set_style_bg_opa(page_transition_overlay_, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(page_transition_overlay_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(page_transition_overlay_, LV_OBJ_FLAG_HIDDEN);
  }

  void cancelDashboardPageTransition() {
    lv_anim_del(this, pageTransitionAnimationExec);
    cancelPowerwallGaugeIntro();
    finishPageTransition();
  }

  void applyDashboardPageVisibility() {
    if (active_dashboard_page_ != 0) hideHome480WifiCard();
    if (active_dashboard_page_ == 3) lv_obj_clear_flag(page_today_trend_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(page_today_trend_, LV_OBJ_FLAG_HIDDEN);
    if (active_dashboard_page_ == 4) lv_obj_clear_flag(page_week_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(page_week_, LV_OBJ_FLAG_HIDDEN);
    if (active_dashboard_page_ == 5) lv_obj_clear_flag(page_settings_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(page_settings_, LV_OBJ_FLAG_HIDDEN);
    const bool onOriginal = active_dashboard_page_ == 0;
    if (onOriginal) {
      lv_obj_clear_flag(hero_card_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(chart_card_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(page_powerwall_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(page_home_grid_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(page_today_trend_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(page_header_, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(hero_card_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(chart_card_, LV_OBJ_FLAG_HIDDEN);
      if (active_dashboard_page_ == 1) {
        lv_obj_clear_flag(page_powerwall_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(page_home_grid_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(page_today_trend_, LV_OBJ_FLAG_HIDDEN);
        setLabelText480(page_title_, "POWERWALL");
      } else {
        lv_obj_add_flag(page_powerwall_, LV_OBJ_FLAG_HIDDEN);
        if (active_dashboard_page_ == 2) lv_obj_clear_flag(page_home_grid_, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(page_home_grid_, LV_OBJ_FLAG_HIDDEN);
        if (active_dashboard_page_ == 3) lv_obj_clear_flag(page_today_trend_, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(page_today_trend_, LV_OBJ_FLAG_HIDDEN);
        setLabelText480(page_title_, active_dashboard_page_ == 3 ? "今日趨勢" :
                          active_dashboard_page_ == 4 ? "7 日用電與電量總表" :
                          active_dashboard_page_ == 5 ? "設定" : "住家與電網");
      }
      const bool onPowerwallPage = active_dashboard_page_ == 1;
      lv_obj_set_height(page_header_, onPowerwallPage ? 59 : 37);
      if (onPowerwallPage || active_dashboard_page_ == 2) {
        setLabelText480(pw_header_title_, onPowerwallPage ? "POWERWALL" : "住家 / 電網");
        lv_obj_set_style_text_font(pw_header_title_, onPowerwallPage || native480_i18n::kEnglish
          ? native480_fonts::home_cjk_27() : native480_fonts::grid_cjk_20(), 0);
        lv_obj_set_width(pw_header_title_, onPowerwallPage ? 213 : 250);
        lv_obj_add_flag(page_title_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(page_site_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(page_status_pill_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(page_clock_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(page_date_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(pw_header_title_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(pw_header_clock_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pw_header_date_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(pw_header_status_dot_, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_clear_flag(page_title_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(page_site_, LV_OBJ_FLAG_HIDDEN);
        if (active_dashboard_page_ >= 4) lv_obj_add_flag(page_status_pill_, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(page_status_pill_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(page_clock_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(page_date_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pw_header_title_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pw_header_clock_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pw_header_date_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pw_header_status_dot_, LV_OBJ_FLAG_HIDDEN);
      }
      lv_obj_clear_flag(page_header_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(page_header_);
    }
    // Full-height Today trend and weekly chart pages replace the shared title row.
    if (active_dashboard_page_ == 3 || active_dashboard_page_ == 4)
      lv_obj_add_flag(page_header_, LV_OBJ_FLAG_HIDDEN);
    if (active_dashboard_page_ == 5) lv_obj_add_flag(page_header_, LV_OBJ_FLAG_HIDDEN);
    if (page_transition_overlay_ && page_transition_phase_ != PageTransitionPhase::Idle &&
        !lv_obj_has_flag(page_transition_overlay_, LV_OBJ_FLAG_HIDDEN)) {
      lv_obj_move_foreground(page_transition_overlay_);
    }
    lv_obj_invalidate(dashboard_screen_);
  }

  static bool hasValidLocalTime(struct tm& localTime) {
    return getLocalTime(&localTime, 10) && localTime.tm_year >= 120;
  }

  static bool isWeekend(const struct tm& localTime) {
    return isTaiwanTouWeekend(localTime);
  }

  static bool isSummerTouMonth(const struct tm& localTime) {
    return isTaiwanTouSummerMonth(localTime.tm_mon + 1);
  }

  static bool isPeakAtLocalTime(const struct tm& localTime) {
    if (isWeekend(localTime)) return false;
    const float hourOfDay = localTime.tm_hour + localTime.tm_min / 60.0f;
    return isTaiwanTouWeekdayPeak(localTime.tm_mon + 1, hourOfDay);
  }

  static void getTouInterval(const struct tm& localTime, int& startMinute, int& endMinute) {
    if (isWeekend(localTime)) {
      startMinute = 0;
      endMinute = 24 * 60;
      return;
    }
    const int minuteOfDay = localTime.tm_hour * 60 + localTime.tm_min;
    if (isSummerTouMonth(localTime)) {
      startMinute = minuteOfDay < 9 * 60 ? 0 : 9 * 60;
      endMinute = minuteOfDay < 9 * 60 ? 9 * 60 : 24 * 60;
      return;
    }
    if (minuteOfDay < 6 * 60) {
      startMinute = 0; endMinute = 6 * 60;
    } else if (minuteOfDay < 11 * 60) {
      startMinute = 6 * 60; endMinute = 11 * 60;
    } else if (minuteOfDay < 14 * 60) {
      startMinute = 11 * 60; endMinute = 14 * 60;
    } else {
      startMinute = 14 * 60; endMinute = 24 * 60;
    }
  }

  static void putDashboardPixel(lv_color_t* buffer, int width, int height, int x, int y,
                                uint8_t r, uint8_t g, uint8_t b) {
    if (!buffer || x < 0 || y < 0 || x >= width || y >= height) return;
    buffer[y * width + x] = lv_color_make(r, g, b);
  }

  static void blendDashboardPixel(lv_color_t* buffer, int width, int height, int x, int y,
                                  uint8_t r, uint8_t g, uint8_t b, float alpha) {
    if (!buffer || x < 0 || y < 0 || x >= width || y >= height) return;
    lv_color_t current = buffer[y * width + x];
    const uint8_t cr = static_cast<uint8_t>((current.ch.red << 3) | (current.ch.red >> 2));
    const uint8_t cg = static_cast<uint8_t>((current.ch.green << 2) | (current.ch.green >> 4));
    const uint8_t cb = static_cast<uint8_t>((current.ch.blue << 3) | (current.ch.blue >> 2));
    buffer[y * width + x] = lv_color_make(
      static_cast<uint8_t>(cr * (1.0f - alpha) + r * alpha),
      static_cast<uint8_t>(cg * (1.0f - alpha) + g * alpha),
      static_cast<uint8_t>(cb * (1.0f - alpha) + b * alpha));
  }

  static void drawDashboardLine(lv_color_t* buffer, int width, int height,
                                int x0, int y0, int x1, int y1,
                                uint8_t r, uint8_t g, uint8_t b) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    while (true) {
      putDashboardPixel(buffer, width, height, x0, y0, r, g, b);
      if (x0 == x1 && y0 == y1) break;
      const int twiceError = 2 * error;
      if (twiceError >= dy) { error += dy; x0 += sx; }
      if (twiceError <= dx) { error += dx; y0 += sy; }
    }
  }

  static void renderHomeLoadChartToRgb565(lv_color_t* buffer, int width, int height,
                                           const HistoryPoint* points, size_t count,
                                           bool preserveBackground = false) {
    if (!buffer || width <= 0 || height <= 0) return;
    if (!preserveBackground) {
      const lv_color_t bg = lv_color_make(9, 10, 15);
      for (int i = 0; i < width * height; ++i) buffer[i] = bg;
      for (int division = 1; division <= 2; ++division) {
        const int y = static_cast<int>(roundf((division / 3.0f) * (height - 1)));
        for (int x = 0; x < width; ++x) blendDashboardPixel(buffer, width, height, x, y, 148, 163, 184, 0.12f);
      }
      for (int x = 0; x < width; ++x) blendDashboardPixel(buffer, width, height, x, height - 1, 148, 163, 184, 0.16f);
    }
    if (!points || count < 2) return;

    auto pointX = [width](float hour) {
      return std::max(0, std::min(width - 1, static_cast<int>(roundf((hour / 24.0f) * (width - 1)))));
    };
    auto pointY = [height](float kw) {
      const float normalized = fminf(1.0f, fmaxf(0.0f, kw / 6.0f));
      constexpr int inset = 1, span = 3;
      return std::max(inset, std::min(height - inset,
        static_cast<int>(roundf((1.0f - normalized) * (height - span))) + inset));
    };
    for (size_t i = 0; i + 1 < count; ++i) {
      const HistoryPoint& from = points[i];
      const HistoryPoint& to = points[i + 1];
      if (to.hour < from.hour) continue;
      uint8_t r = 148, g = 163, b = 184;
      if (to.power_source == HistoryPowerSource::Grid) { r = 56; g = 189; b = 248; }
      else if (to.power_source == HistoryPowerSource::Powerwall) { r = 17; g = 229; b = 142; }
      drawDashboardLine(buffer, width, height,
        pointX(from.hour), pointY(from.load_kw), pointX(to.hour), pointY(to.load_kw), r, g, b);
    }
  }

  static uint32_t powerwallTrendLoadColor(HistoryPowerSource source) {
    if (source == HistoryPowerSource::Grid) return 0x38bdf8;
    if (source == HistoryPowerSource::Powerwall) return 0x11e58e;
    return 0x94a3b8;
  }

  // Match the Week page's source-colored load trace, using the same gap
  // threshold to avoid connecting missing history.
  static void renderPowerwallTrendLoadToRgb565(lv_color_t* buffer, int width, int height,
                                                const HistoryPoint* points, size_t count,
                                                float maxKw) {
    if (!buffer || width <= 0 || height <= 0) return;
    const lv_color_t bg = lv_color_hex(0x090e13);
    for (int i = 0; i < width * height; ++i) buffer[i] = bg;
    for (const int y : {1, height / 2, height - 2})
      for (int x = 0; x < width; ++x)
        blendDashboardPixel(buffer, width, height, x, y, 148, 163, 184, 0.16f);
    for (int division = 1; division <= 3; ++division) {
      const int x = static_cast<int>(roundf(division * (width - 1) / 4.0f));
      for (int y = 1; y < height - 1; ++y)
        blendDashboardPixel(buffer, width, height, x, y, 148, 163, 184, 0.10f);
    }
    if (!points || count == 0) return;

    auto pointX = [width](float hour) {
      return std::max(0, std::min(width - 1,
        static_cast<int>(roundf((hour / 24.0f) * (width - 1)))));
    };
    auto pointY = [height, maxKw](float kw) {
      const float normalized = fminf(1.0f, fmaxf(0.0f, kw / maxKw));
      return std::max(1, std::min(height - 2,
        height - 2 - static_cast<int>(roundf(normalized * (height - 3)))));
    };
    for (size_t i = 0; i < count; ++i) {
      const HistoryPoint& point = points[i];
      if (!std::isfinite(point.hour) || !std::isfinite(point.load_kw)) continue;
      const int x = pointX(point.hour);
      const int y = pointY(point.load_kw);
      const uint32_t color = powerwallTrendLoadColor(point.power_source);
      if (i > 0) {
        const HistoryPoint& previous = points[i - 1];
        if (std::isfinite(previous.hour) && std::isfinite(previous.load_kw) &&
            point.hour >= previous.hour && point.hour - previous.hour <= 0.17f) {
          drawDashboardLine(buffer, width, height,
            pointX(previous.hour), pointY(previous.load_kw), x, y,
            static_cast<uint8_t>(color >> 16), static_cast<uint8_t>(color >> 8),
            static_cast<uint8_t>(color));
        }
      }
      putDashboardPixel(buffer, width, height, x, y,
        static_cast<uint8_t>(color >> 16), static_cast<uint8_t>(color >> 8),
        static_cast<uint8_t>(color));
    }
  }

  // Draws the Powerwall gauge at an intro progress of 0..1. Arc, percent text,
  // and reserve marker share one progress value, so the marker and the arc
  // arrive at their targets (configured backup reserve / current SOC) together.
  void applyPowerwallGauge(float progress) {
    if (!pw_arc_ || !pw_reserve_marker_) return;
    const PowerwallTelemetry& t = last_telemetry_;
    const float soc = std::isfinite(t.soe_percent) ? fminf(100.0f, fmaxf(0.0f, t.soe_percent)) : 0.0f;
    const float reserve = std::isfinite(t.backup_reserve_percent)
      ? fminf(100.0f, fmaxf(0.0f, t.backup_reserve_percent)) : 0.0f;
    const float p = fminf(1.0f, fmaxf(0.0f, progress));
    const float shownSoc = soc * p;
    const float shownReserve = reserve * p;

    lv_arc_set_value(pw_arc_, static_cast<int32_t>(roundf(shownSoc * 10.0f)));
    char text[8];
    snprintf(text, sizeof(text), "%d%%", static_cast<int>(roundf(shownSoc)));
    setLabelText480(pw_soc_text_, text);
    setLabelText480(pw_soc_bold_pass_, text);

    // The arc runs clockwise from 135 to 405 degrees. Mark the reserve at the
    // same angle as its percentage on that arc.
    const float reserveAngle = (135.0f + 270.0f * shownReserve / 100.0f) * 0.01745329252f;
    const float reserveCos = cosf(reserveAngle);
    const float reserveSin = sinf(reserveAngle);
    pw_reserve_marker_points_[0] = {
      static_cast<lv_coord_t>(roundf(117.0f + 94.0f * reserveCos)),
      static_cast<lv_coord_t>(roundf(121.0f + 94.0f * reserveSin))
    };
    pw_reserve_marker_points_[1] = {
      static_cast<lv_coord_t>(roundf(117.0f + 116.0f * reserveCos)),
      static_cast<lv_coord_t>(roundf(121.0f + 116.0f * reserveSin))
    };
    lv_line_set_points(pw_reserve_marker_, pw_reserve_marker_points_, 2);
  }

  static void pwGaugeIntroExec(void* var, int32_t value) {
    Ui480* self = static_cast<Ui480*>(var);
    if (self) self->applyPowerwallGauge(static_cast<float>(value) / kPwGaugeIntroSteps);
  }

  static void pwGaugeIntroReady(lv_anim_t* animation) {
    Ui480* self = animation ? static_cast<Ui480*>(animation->var) : nullptr;
    if (!self) return;
    self->pw_gauge_intro_active_ = false;
    self->applyPowerwallGauge(1.0f);
  }

  void startPowerwallGaugeIntro() {
    lv_anim_del(this, pwGaugeIntroExec);
    pw_gauge_intro_active_ = true;
    applyPowerwallGauge(0.0f);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, this);
    lv_anim_set_values(&animation, 0, kPwGaugeIntroSteps);
    lv_anim_set_time(&animation, kPwGaugeIntroMs);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&animation, pwGaugeIntroExec);
    lv_anim_set_ready_cb(&animation, pwGaugeIntroReady);
    if (lv_anim_start(&animation) == nullptr) {
      pw_gauge_intro_active_ = false;
      applyPowerwallGauge(1.0f);
    }
  }

  void cancelPowerwallGaugeIntro() {
    lv_anim_del(this, pwGaugeIntroExec);
    if (!pw_gauge_intro_active_) return;
    pw_gauge_intro_active_ = false;
    applyPowerwallGauge(1.0f);
  }

  void refreshAddedPages() {
    if (!page_powerwall_ || !page_home_grid_) return;
    refreshSettingsLabels();
    const PowerwallTelemetry& t = last_telemetry_;
    updateAddedPageClock();
    refreshWeeklyPage();
    if (active_dashboard_page_ == 3) refreshDailyTrendPage();

    char text[96];
    if (!pw_gauge_intro_active_) applyPowerwallGauge(1.0f);

    const float batteryPower = t.has_battery_power ? t.battery_power_w : NAN;
    const char* state = !std::isfinite(batteryPower) ? "狀態未知" :
                        batteryPower > 60.0f ? "放電中" :
                        batteryPower < -60.0f ? "充電中" : "待機中";
    const uint32_t stateColor = std::isfinite(batteryPower) ? 0x69ff00 : 0x94a3b8;
    if (std::isfinite(batteryPower)) {
      if (fabsf(batteryPower) < 5.0f) snprintf(text, sizeof(text), "%s - 0 kW", state);
      else snprintf(text, sizeof(text), "%s - %.2f kW", state, fabsf(batteryPower) / 1000.0f);
    } else {
      snprintf(text, sizeof(text), "%s - ? kW", state);
    }
    setLabelText480(pw_state_text_, text);
    lv_obj_set_style_text_color(pw_state_text_, lv_color_hex(stateColor), 0);
    char backupEta[32];
    if (t.has_battery_capacity && t.has_load_power) {
      formatUsableEta(text, sizeof(text), t.nominal_remaining_kwh, t.nominal_full_kwh,
                      t.backup_reserve_percent, t.load_power_w / 1000.0f, false);
      const double full = std::isfinite(t.nominal_full_kwh) && t.nominal_full_kwh > 0.0
        ? t.nominal_full_kwh : 14.6;
      const double reserve = std::isfinite(t.backup_reserve_percent)
        ? std::fmin(100.0, std::fmax(0.0, t.backup_reserve_percent)) : 24.0;
      const double remaining = std::isfinite(t.nominal_remaining_kwh)
        ? std::fmax(0.0, t.nominal_remaining_kwh) : 0.0;
      // Above the reserve, show its full allocation; once consumed, count down
      // from the actual remaining energy. The usable ETA is then already 0m.
      const double backupKwh = std::fmin(remaining, full * reserve / 100.0);
      formatUsableEta(backupEta, sizeof(backupEta), backupKwh, full, 0.0,
                      t.load_power_w / 1000.0f, false);
    } else {
      snprintf(text, sizeof(text), "--");
      snprintf(backupEta, sizeof(backupEta), "--");
    }
    // Fixed two-line layout: both labels share the capacity/voltage right edge.
    setLabelText480(pw_eta_text_, text);
    snprintf(text, sizeof(text), "/ %s", backupEta);
    setLabelText480(pw_backup_eta_text_, text);

    snprintf(text, sizeof(text), "備援 %d%%", static_cast<int>(roundf(t.backup_reserve_percent)));
    setLabelText480(pw_reserve_chart_label_, text);
    const float reservePct = fminf(100.0f, fmaxf(0.0f, t.backup_reserve_percent));
    constexpr int reservePlotY = kPowerwallTrendSocY;
    constexpr int reservePlotH = kPowerwallTrendSocH;
    const int reserveY = static_cast<int>(roundf((1.0f - reservePct / 100.0f) * (reservePlotH - 4))) + 2;
    lv_obj_update_layout(pw_reserve_chart_label_);
    const int reserveLabelH = lv_obj_get_height(pw_reserve_chart_label_);
    const int reserveLabelW = lv_obj_get_width(pw_reserve_chart_label_);
    lv_obj_set_pos(pw_reserve_chart_label_, kPwChartPlotX + kChartW - reserveLabelW - 3,
                   reservePlotY + std::max(2, reserveY - reserveLabelH - 3));

    if (t.has_battery_voltage && t.battery_voltage_v > 0.0f) {
      snprintf(text, sizeof(text), "%.1f V", t.battery_voltage_v);
      setLabelText480(pw_voltage_value_, text);
    } else {
      setLabelText480(pw_voltage_value_, "-- V");
    }
    if (t.has_battery_capacity) {
      snprintf(text, sizeof(text), "%.2f kWh", t.nominal_remaining_kwh);
      setLabelText480(pw_capacity_current_value_, text);
      snprintf(text, sizeof(text), "/ %.2f kWh", t.nominal_full_kwh);
      setLabelText480(pw_capacity_total_value_, text);
    } else {
      setLabelText480(pw_capacity_current_value_, "-- kWh");
      setLabelText480(pw_capacity_total_value_, "/ -- kWh");
    }
    if (t.has_grid_status) {
      const bool connected = t.grid_status == "SystemGridConnected";
      setLabelText480(home_grid_status_, connected ? "已併網" : t.grid_status.c_str());
      lv_obj_set_style_text_color(home_grid_status_, lv_color_hex(connected ? 0x38bdf8 : 0xfbbf24), 0);
    } else {
      setLabelText480(home_grid_status_, "—");
    }

    if (t.has_site_power) {
      setLabelText480(home_grid_power_title_, t.site_power_w < -60.0f ? "回送功率" : "輸入功率");
      snprintf(text, sizeof(text), "%.2f kW", fabsf(t.site_power_w) / 1000.0f);
      setLabelText480(home_grid_power_value_, text);
      lv_obj_set_style_text_color(home_grid_power_value_, lv_color_hex(t.site_power_w < -60.0f ? 0x11e58e : 0x38bdf8), 0);
    } else {
      setLabelText480(home_grid_power_value_, "— kW");
      lv_obj_set_style_text_color(home_grid_power_value_, lv_color_hex(0x38bdf8), 0);
    }
    if (t.has_site_current) snprintf(text, sizeof(text), "%.1f A", t.site_current_a);
    else snprintf(text, sizeof(text), "— A");
    setLabelText480(home_grid_current_value_, text);
    if (t.has_site_voltage) snprintf(text, sizeof(text), "%.1f V", t.site_voltage_v);
    else snprintf(text, sizeof(text), "— V");
    setLabelText480(home_grid_voltage_value_, text);
    if (t.has_site_import_energy) snprintf(text, sizeof(text), "%.2f kWh", t.site_energy_imported_wh / 1000.0f);
    else snprintf(text, sizeof(text), "— kWh");
    setLabelText480(home_grid_import_value_, text);
    if (t.has_site_export_energy) snprintf(text, sizeof(text), "%.2f kWh", t.site_energy_exported_wh / 1000.0f);
    else snprintf(text, sizeof(text), "— kWh");
    setLabelText480(home_grid_export_value_, text);

    if (t.has_load_power) snprintf(text, sizeof(text), "%.2f kW", t.load_power_w / 1000.0f);
    else snprintf(text, sizeof(text), "— kW");
    setLabelText480(home_load_value_, text);
    if (t.has_load_current) snprintf(text, sizeof(text), "%.1f A", t.load_current_a);
    else snprintf(text, sizeof(text), "— A");
    setLabelText480(home_load_current_value_, text);
    if (t.has_load_voltage) snprintf(text, sizeof(text), "%.1f V", t.load_voltage_v);
    else snprintf(text, sizeof(text), "— V");
    setLabelText480(home_load_voltage_value_, text);
    if (t.has_solar_power) snprintf(text, sizeof(text), "太陽能 %.2f kW", t.solar_power_w / 1000.0f);
    else snprintf(text, sizeof(text), "太陽能 — kW");
    setLabelText480(home_solar_value_, text);
    if (t.has_load_power && t.has_load_voltage && t.has_load_current &&
        t.load_voltage_v > 0.0f && t.load_current_a > 0.0f) {
      const float pf = fabsf(t.load_power_w) / (t.load_voltage_v * t.load_current_a);
      snprintf(text, sizeof(text), "功率因數 %.2f", fminf(1.0f, pf));
    } else snprintf(text, sizeof(text), "功率因數 —");
    setLabelText480(home_power_factor_, text);
    if (t.has_load_energy) snprintf(text, sizeof(text), "累計用電 %.2f kWh", t.load_energy_imported_wh / 1000.0f);
    else snprintf(text, sizeof(text), "累計用電 — kWh");
    setLabelText480(home_energy_value_, text);

    String dataSource = t.demo ? "SIM" : "--";
#if defined(ESP32)
    if (!t.demo) dataSource = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "--";
#endif
    const String stamp = t.timestamp + " · " + dataSource;
    if (pw_data_stamp_) setLabelText480(pw_data_stamp_, stamp.c_str());
    setLabelText480(home_data_stamp_, stamp.c_str());

    struct tm localTime = {};
    const int minuteOfDay = hasValidLocalTime(localTime)
      ? localTime.tm_hour * 60 + localTime.tm_min : -1;
    if (minuteOfDay >= 0) {
      const int currentX = static_cast<int>(roundf((minuteOfDay / 1440.0f) * (kChartW - 1)));
      lv_obj_set_pos(pw_now_line_, kPwChartPlotX + currentX - 1, 7);
      const int homeCurrentX = static_cast<int>(roundf((minuteOfDay / 1440.0f) * (kHomeChartW - 1)));
      lv_obj_set_pos(home_now_line_, kHomeChartPlotX + homeCurrentX - 1, kHomeChartPlotY);
    }

    if (dashboard_chart_buf_ && active_dashboard_page_ == 1) {
      constexpr float maxKw = 10.0f;
      if (pw_load_chart_buf_ && pw_load_chart_canvas_) {
        renderPowerwallTrendLoadToRgb565(pw_load_chart_buf_, kChartW, kPowerwallTrendLoadH,
          t.history, t.history_count, maxKw);
        lv_obj_invalidate(pw_load_chart_canvas_);
      }
      renderAreaChartToRgb565(dashboard_chart_buf_, kChartW, kPowerwallTrendSocH,
        t.history, t.history_count, true, 100.0f, 17, 229, 142,
        t.backup_reserve_percent, false, 1.0f);
      for (int x = 0; x < kChartW; ++x)
        blendDashboardPixel(dashboard_chart_buf_, kChartW, kPowerwallTrendSocH,
          x, 1, 148, 163, 184, 0.12f);
      if (t.history_count > 0) {
        const HistoryPoint& latest = t.history[t.history_count - 1];
        const int x = std::max(0, std::min(kChartW - 1,
          static_cast<int>(roundf((latest.hour / 24.0f) * (kChartW - 1)))));
        const float loadNorm = fminf(1.0f, fmaxf(0.0f, latest.load_kw / maxKw));
        const int loadY = std::max(1, std::min(kPowerwallTrendLoadH - 2,
          kPowerwallTrendLoadH - 2 - static_cast<int>(roundf(loadNorm * (kPowerwallTrendLoadH - 3)))));
        const float soeNorm = fminf(1.0f, fmaxf(0.0f, latest.soe / 100.0f));
        const int soeY = std::max(2, std::min(kPowerwallTrendSocH - 1,
          static_cast<int>(roundf((1.0f - soeNorm) * (kPowerwallTrendSocH - 3)) + 2)));
        lv_obj_set_style_bg_color(pw_load_now_dot_,
          lv_color_hex(powerwallTrendLoadColor(latest.power_source)), 0);
        lv_obj_set_pos(pw_load_now_dot_, kPwChartPlotX + x - 3,
          kPowerwallTrendLoadY + loadY - 3);
        lv_obj_set_pos(pw_now_dot_, kPwChartPlotX + x - 3,
          kPowerwallTrendSocY + soeY - 3);
        lv_obj_clear_flag(pw_load_now_dot_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(pw_now_dot_, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(pw_load_now_dot_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pw_now_dot_, LV_OBJ_FLAG_HIDDEN);
      }
      lv_obj_invalidate(pw_chart_canvas_);
    }

    if (dashboard_chart_buf_ && active_dashboard_page_ == 2) {
      renderHomeLoadChartToRgb565(dashboard_chart_buf_, kHomeChartW, kHomeChartPlotH, t.history, t.history_count);
      if (t.history_count > 0) {
        const HistoryPoint& latest = t.history[t.history_count - 1];
        const int x = std::max(0, std::min(kHomeChartW - 1, static_cast<int>(roundf((latest.hour / 24.0f) * (kHomeChartW - 1)))));
        const int y = std::max(1, std::min(kHomeChartPlotH - 2,
          static_cast<int>(roundf((1.0f - fminf(1.0f, fmaxf(0.0f, latest.load_kw / 6.0f))) *
            (kHomeChartPlotH - 3))) + 1));
        uint32_t color = latest.power_source == HistoryPowerSource::Grid ? 0x38bdf8 :
                         latest.power_source == HistoryPowerSource::Powerwall ? 0x11e58e : 0x94a3b8;
        lv_obj_set_pos(home_now_dot_, kHomeChartPlotX + x - 3, kHomeChartPlotY + y - 3);
        lv_obj_set_style_bg_color(home_now_dot_, lv_color_hex(color), 0);
        lv_obj_clear_flag(home_now_dot_, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(home_now_dot_, LV_OBJ_FLAG_HIDDEN);
      }
      lv_obj_invalidate(home_chart_canvas_);
    }
  }

  static time_t nextTouTransition(const struct tm& nowLocal, bool currentPeak) {
    struct tm nowCopy = nowLocal;
    const time_t nowEpoch = mktime(&nowCopy);
    if (nowEpoch <= 0) return 0;
    static const int candidateHours[] = {0, 6, 9, 11, 14};
    time_t nearest = 0;
    for (int dayOffset = 0; dayOffset <= 8; ++dayOffset) {
      for (int hour : candidateHours) {
        struct tm candidateLocal = nowLocal;
        candidateLocal.tm_mday += dayOffset;
        candidateLocal.tm_hour = hour;
        candidateLocal.tm_min = 0;
        candidateLocal.tm_sec = 0;
        const time_t candidateEpoch = mktime(&candidateLocal);
        if (candidateEpoch <= nowEpoch || (nearest > 0 && candidateEpoch >= nearest)) continue;
        struct tm before = {}, after = {};
        const time_t beforeEpoch = candidateEpoch - 60;
        localtime_r(&beforeEpoch, &before);
        localtime_r(&candidateEpoch, &after);
        if (isPeakAtLocalTime(before) != isPeakAtLocalTime(after)) nearest = candidateEpoch;
      }
    }
    (void)currentPeak;
    return nearest;
  }

  void updateTouBandLayout(const struct tm& localTime) {
    if (cached_tou_year_ == localTime.tm_year && cached_tou_yday_ == localTime.tm_yday) return;
    cached_tou_year_ = localTime.tm_year;
    cached_tou_yday_ = localTime.tm_yday;

    bool boundaries[25] = {};
    addTouAxisBoundaries(localTime, boundaries);

    struct TouSegment { int start; int end; bool peak; } segments[4] = {};
    int ticks[5] = {0, 360, 660, 840, 1440};
    int segmentCount = 0;
    if (isWeekend(localTime)) {
      segments[0] = {0, 1440, false};
      ticks[0] = 0; ticks[1] = 360; ticks[2] = 720; ticks[3] = 1080; ticks[4] = 1440;
      segmentCount = 1;
    } else if (isSummerTouMonth(localTime)) {
      segments[0] = {0, 540, false};
      segments[1] = {540, 1440, true};
      ticks[0] = 0; ticks[1] = 540; ticks[2] = 1440; ticks[3] = -1; ticks[4] = -1;
      segmentCount = 2;
    } else {
      segments[0] = {0, 360, false};
      segments[1] = {360, 660, true};
      segments[2] = {660, 840, false};
      segments[3] = {840, 1440, true};
      ticks[0] = 0; ticks[1] = 360; ticks[2] = 660; ticks[3] = 840; ticks[4] = 1440;
      segmentCount = 4;
    }

    for (int i = 0; i < 4; ++i) {
      if (i >= segmentCount) {
        lv_obj_add_flag(home_tou_segments_[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(home_chart_tou_segments_[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pw_tou_segments_[i], LV_OBJ_FLAG_HIDDEN);
        continue;
      }
      const int left = segments[i].start;
      const int right = segments[i].end;
      const int bandX = 8 + static_cast<int>(roundf((left / 1440.0f) * 445.0f));
      const int bandW = static_cast<int>(roundf(((right - left) / 1440.0f) * 445.0f));
      const uint32_t color = segments[i].peak ? 0xf43f6e : 0x18b76a;
      lv_obj_clear_flag(home_tou_segments_[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_pos(home_tou_segments_[i], bandX, kHomeTouSegmentY);
      lv_obj_set_size(home_tou_segments_[i], bandW, 12);
      lv_obj_set_style_bg_color(home_tou_segments_[i], lv_color_hex(color), 0);
      lv_obj_set_style_bg_opa(home_tou_segments_[i], LV_OPA_COVER, 0);
      setLabelText480(home_tou_segment_labels_[i], segments[i].peak ? "尖峰" : "離峰");
      lv_obj_set_pos(home_tou_segment_labels_[i], 0, 1);
      lv_obj_set_width(home_tou_segment_labels_[i], bandW);

      const int homeChartX = kHomeChartPlotX + static_cast<int>(roundf((left / 1440.0f) * kHomeChartW));
      const int homeChartW = static_cast<int>(roundf(((right - left) / 1440.0f) * kHomeChartW));
      lv_obj_clear_flag(home_chart_tou_segments_[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_pos(home_chart_tou_segments_[i], homeChartX, kHomeChartTouY);
      lv_obj_set_size(home_chart_tou_segments_[i], homeChartW, kHomeChartTouH);
      lv_obj_set_style_bg_color(home_chart_tou_segments_[i], lv_color_hex(color), 0);
      lv_obj_set_style_bg_opa(home_chart_tou_segments_[i], LV_OPA_COVER, 0);
      const int chartX = kPwChartPlotX + static_cast<int>(roundf((left / 1440.0f) * kChartW));
      const int chartW = static_cast<int>(roundf(((right - left) / 1440.0f) * kChartW));
      lv_obj_clear_flag(pw_tou_segments_[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_pos(pw_tou_segments_[i], chartX, 148);
      lv_obj_set_size(pw_tou_segments_[i], chartW, 5);
      lv_obj_set_style_bg_color(pw_tou_segments_[i], lv_color_hex(color), 0);
      lv_obj_set_style_bg_opa(pw_tou_segments_[i], LV_OPA_COVER, 0);
    }
    for (int i = 0; i < 5; ++i) {
      if (ticks[i] < 0) {
        lv_obj_add_flag(home_tou_ticks_[i], LV_OBJ_FLAG_HIDDEN);
        continue;
      }
      lv_obj_clear_flag(home_tou_ticks_[i], LV_OBJ_FLAG_HIDDEN);
      char text[8];
      const int hour = ticks[i] / 60;
      snprintf(text, sizeof(text), "%02d:%02d", hour == 24 ? 24 : hour, ticks[i] % 60);
      if (isWeekend(localTime)) {
        static const char* weekendTicks[] = {"00:00", "06:00", "12:00", "18:00", "24:00"};
        setLabelText480(home_tou_ticks_[i], weekendTicks[i]);
      } else {
        setLabelText480(home_tou_ticks_[i], text);
      }
      const int x = 8 + static_cast<int>(roundf((ticks[i] / 1440.0f) * 445.0f));
      const bool firstTick = ticks[i] == 0;
      const bool lastTick = ticks[i] == 1440;
      lv_obj_set_style_text_align(home_tou_ticks_[i], lastTick ? LV_TEXT_ALIGN_RIGHT :
        (firstTick ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_CENTER), 0);
      lv_obj_set_pos(home_tou_ticks_[i], x - (lastTick ? 48 : (firstTick ? 0 : 24)), kHomeTouTicksY);
      lv_obj_set_width(home_tou_ticks_[i], 48);
    }
  }

  void updateAddedPageClock() {
    if (!page_header_) return;
    const PowerwallTelemetry& t = last_telemetry_;
    if (t.site_name.length() > 0) setLabelTextRaw480(page_site_, t.site_name.c_str());

    const bool hasData = t.ok || t.last_update_ms > 0;
    const bool stale = !last_configured_ || !hasData || t.stale || !t.ok;
    if (t.demo) {
      setLabelText480(page_status_text_, "模擬資料");
      lv_obj_set_style_bg_color(page_status_pill_, lv_color_hex(0x172554), 0);
      lv_obj_set_style_border_color(page_status_pill_, lv_color_hex(0x3b82f6), 0);
      lv_obj_set_style_text_color(page_status_text_, lv_color_hex(0xbfdbfe), 0);
    } else if (stale) {
      setLabelText480(page_status_text_, hasData ? "資料過期" : "連線中");
      lv_obj_set_style_bg_color(page_status_pill_, lv_color_hex(0x422006), 0);
      lv_obj_set_style_border_color(page_status_pill_, lv_color_hex(0xf59e0b), 0);
      lv_obj_set_style_text_color(page_status_text_, lv_color_hex(0xfcd34d), 0);
    } else {
      setLabelText480(page_status_text_, "即時資料");
      lv_obj_set_style_bg_color(page_status_pill_, lv_color_hex(0x064e3b), 0);
      lv_obj_set_style_border_color(page_status_pill_, lv_color_hex(0x10b981), 0);
      lv_obj_set_style_text_color(page_status_text_, lv_color_hex(0x6ee7b7), 0);
    }
    const bool live = (state_mode_idx_ == 0 || state_mode_idx_ == 1) &&
                      last_configured_ && t.ok && !t.stale && !t.demo;
    if (pw_header_status_dot_) {
      lv_obj_set_style_bg_color(pw_header_status_dot_, lv_color_hex(live ? 0x34d399 : 0xff5252), 0);
    }

    struct tm localTime = {};
    if (hasValidLocalTime(localTime)) {
      char timeText[8], dateText[32];
      snprintf(timeText, sizeof(timeText), "%02d:%02d", localTime.tm_hour, localTime.tm_min);
      if (native480_i18n::kEnglish) {
        snprintf(dateText, sizeof(dateText), "%02d/%02d", localTime.tm_mon + 1, localTime.tm_mday);
      } else {
        snprintf(dateText, sizeof(dateText), "%02d月%02d日", localTime.tm_mon + 1, localTime.tm_mday);
      }
      setLabelText480(page_clock_, timeText);
      setLabelText480(page_date_, dateText);
      char headerClock[20];
      ui480::formatHeaderClock(headerClock, sizeof(headerClock), &localTime);
      setLabelText480(pw_header_clock_, headerClock);
      positionHeaderDotBeforeClock(pw_header_clock_, pw_header_status_dot_);
      updateTouBandLayout(localTime);
      updateTouLabelsAndMarkers(localTime);
    } else {
      setLabelText480(page_clock_, t.timestamp.c_str());
      setLabelText480(page_date_, "時間同步中");
      setLabelText480(pw_header_clock_, "--/-- --- --:--");
      positionHeaderDotBeforeClock(pw_header_clock_, pw_header_status_dot_);
      setLabelText480(home_tou_pill_text_, "時間同步中");
      centerDashboardLabelInkVertically(home_tou_pill_, home_tou_pill_text_);
      setLabelText480(home_interval_start_, "--:--");
      setLabelText480(home_interval_end_, "--:--");
      layoutHomeTouIntervalLabels();
      setLabelText480(home_price_, "—");
      setLabelText480(home_countdown_, "下一次切換 —");
    }
    last_added_page_clock_ms_ = millis();
  }

  void updateTouLabelsAndMarkers(const struct tm& localTime) {
    const bool peak = isPeakAtLocalTime(localTime);
    const bool summer = isSummerTouMonth(localTime);
    const char* price = peak ? (summer ? "NT$5.16 / 度" : "NT$4.93 / 度")
                             : (summer ? "NT$2.06 / 度" : "NT$1.99 / 度");
    int startMinute = 0, endMinute = 1440;
    getTouInterval(localTime, startMinute, endMinute);
    char intervalStart[8], intervalEnd[8], countdown[96];
    snprintf(intervalStart, sizeof(intervalStart), "%02d:%02d", startMinute / 60, startMinute % 60);
    snprintf(intervalEnd, sizeof(intervalEnd), "%02d:%02d",
             endMinute == 1440 ? 24 : endMinute / 60, endMinute % 60);
    setLabelText480(home_tou_pill_text_, peak ? "目前：尖峰" : "目前：離峰");
    centerDashboardLabelInkVertically(home_tou_pill_, home_tou_pill_text_);
    lv_obj_set_style_text_decor(home_tou_pill_text_, LV_TEXT_DECOR_NONE, 0);
    lv_obj_set_style_bg_color(home_tou_pill_, lv_color_hex(peak ? 0x4c0519 : 0x064e3b), 0);
    lv_obj_set_style_border_color(home_tou_pill_, lv_color_hex(peak ? 0xf43f5e : 0x10b981), 0);
    lv_obj_set_style_text_color(home_tou_pill_text_, lv_color_hex(peak ? 0xfecdd3 : 0x6ee7b7), 0);
    setLabelText480(home_interval_start_, intervalStart);
    setLabelText480(home_interval_end_, intervalEnd);
    layoutHomeTouIntervalLabels();
    setLabelText480(home_price_, price);

    time_t next = nextTouTransition(localTime, peak);
    if (next > 0) {
      struct tm nowCopy = localTime;
      time_t now = mktime(&nowCopy);
      struct tm nextLocal = {};
      localtime_r(&next, &nextLocal);
      const long minutesLeft = static_cast<long>((next - now + 59) / 60);
      const long hoursLeft = minutesLeft / 60;
      const long minsLeft = minutesLeft % 60;
      char when[32];
      if (nextLocal.tm_yday != localTime.tm_yday) {
        if (nextLocal.tm_yday == localTime.tm_yday + 1 || (localTime.tm_yday >= 364 && nextLocal.tm_yday == 0)) {
          snprintf(when, sizeof(when), "%s %02d:%02d", native480_i18n::kEnglish ? "Tomorrow" : "明日", nextLocal.tm_hour, nextLocal.tm_min);
        } else {
          snprintf(when, sizeof(when), "%02d/%02d %02d:%02d", nextLocal.tm_mon + 1, nextLocal.tm_mday, nextLocal.tm_hour, nextLocal.tm_min);
        }
      } else {
        snprintf(when, sizeof(when), "%02d:%02d", nextLocal.tm_hour, nextLocal.tm_min);
      }
      if (native480_i18n::kEnglish) {
        snprintf(countdown, sizeof(countdown), "%s %s · %ldh%02ldm",
                 peak ? "Off-peak" : "Peak", when, hoursLeft, minsLeft);
      } else {
        snprintf(countdown, sizeof(countdown), "下一次%s %s · 還有 %ld小時%02ld分",
                 peak ? "離峰" : "尖峰", when, hoursLeft, minsLeft);
      }
      setLabelText480(home_countdown_, countdown);
    } else {
      setLabelText480(home_countdown_, "下一次切換 —");
    }

    const int minuteOfDay = localTime.tm_hour * 60 + localTime.tm_min;
    const int barX = 8 + static_cast<int>(roundf((minuteOfDay / 1440.0f) * 445.0f));
    lv_obj_set_pos(home_tou_now_line_, barX - 1, kHomeTouNowLineY);
    lv_obj_set_pos(home_tou_now_dot_, barX - 2, kHomeTouNowDotY);
    const int homeChartX = kHomeChartPlotX + static_cast<int>(roundf((minuteOfDay / 1440.0f) * (kHomeChartW - 1)));
    const int pwChartX = kPwChartPlotX + static_cast<int>(roundf((minuteOfDay / 1440.0f) * kChartW));
    lv_obj_set_pos(home_now_line_, homeChartX - 1, kHomeChartPlotY);
    lv_obj_set_pos(pw_now_line_, pwChartX - 1, 7);
  }

  static lv_color_t* allocPixelBuf(size_t pixels) {
    const size_t bytes = pixels * sizeof(lv_color_t);
#if defined(ESP32)
    auto* p = static_cast<lv_color_t*>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (p) return p;
#endif
    return static_cast<lv_color_t*>(malloc(bytes));
  }

  void tickPeriodicUi() {
    unsigned long now_ms = millis();
      if (restart_armed_ && static_cast<uint32_t>(now_ms - restart_armed_at_ms_) > 3000) {
      restart_armed_ = false;
      if (restart_button_label_) setLabelText480(restart_button_label_, "重新啟動");
    }
    // Change only opacity so the status pill keeps its size while blinking.
    const bool live = (state_mode_idx_ == 0 || state_mode_idx_ == 1) &&
                      last_configured_ && last_telemetry_.ok &&
                      !last_telemetry_.stale && !last_telemetry_.demo;
    updateHome480Status();
    refreshSettingsUptime();
    if (pw_now_dot_) lv_obj_set_style_opa(pw_now_dot_,
      (now_ms / 500UL) % 2 == 0 ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    if (pw_header_status_dot_) lv_obj_set_style_bg_opa(pw_header_status_dot_,
      (now_ms / 500UL) % 2 == 0 ? LV_OPA_COVER : LV_OPA_30, 0);
    if (home_now_dot_) lv_obj_set_style_opa(home_now_dot_,
      (now_ms / 500UL) % 2 == 0 ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    const lv_opa_t pointOpa = (now_ms / 500UL) % 2 == 0 ? LV_OPA_COVER : LV_OPA_TRANSP;
    if (pw_load_now_dot_) lv_obj_set_style_opa(pw_load_now_dot_, pointOpa, 0);
    if (chart1_now_dot_) lv_obj_set_style_opa(chart1_now_dot_, pointOpa, 0);
    if (chart2_now_dot_) lv_obj_set_style_opa(chart2_now_dot_, pointOpa, 0);
    if (now_ms - last_periodic_check_ms_ < 1000UL) return;
    last_periodic_check_ms_ = now_ms;

    updateAddedPageClock();

    bool w_stale = last_weather_.isStale(now_ms);
    struct tm tm_now = {};
    bool has_time = getLocalTime(&tm_now, 10) && (tm_now.tm_year >= 120);
    int cur_min = has_time ? (tm_now.tm_hour * 60 + tm_now.tm_min) : -1;

    if (w_stale != last_rendered_weather_stale_ || cur_min != last_rendered_min_) {
      refreshAllFromCache();
    }
  }

  static bool parseTimeOfDayMinutes(const String& value, int& minutes) {
    if (value.length() < 5 || value[2] != ':') return false;
    if (value[0] < '0' || value[0] > '9' || value[1] < '0' || value[1] > '9' ||
        value[3] < '0' || value[3] > '9' || value[4] < '0' || value[4] > '9') return false;
    int hour = (value[0] - '0') * 10 + (value[1] - '0');
    int minute = (value[3] - '0') * 10 + (value[4] - '0');
    if (hour > 23 || minute > 59) return false;
    minutes = hour * 60 + minute;
    return true;
  }

  static int calculateSunPhasePct(const WeatherSnapshot& weather) {
    struct tm tm_now = {};
    int sunrise_min = 0;
    int sunset_min = 0;
    if (getLocalTime(&tm_now, 10) && tm_now.tm_year >= 120 &&
        parseTimeOfDayMinutes(weather.sunrise, sunrise_min) &&
        parseTimeOfDayMinutes(weather.sunset, sunset_min) && sunset_min > sunrise_min) {
      const int now_min = tm_now.tm_hour * 60 + tm_now.tm_min;
      if (now_min <= sunrise_min) return 10;
      if (now_min >= sunset_min) return 90;
      const float fraction = static_cast<float>(now_min - sunrise_min) /
                             static_cast<float>(sunset_min - sunrise_min);
      return 10 + static_cast<int>(roundf(fraction * 80.0f));
    }
    return weather.is_day ? 50 : 0;
  }

  void refreshAllFromCache() {
    const PowerwallTelemetry& t = last_telemetry_;
    const WeatherSnapshot& w    = last_weather_;
    const bool configured       = last_configured_;

    static const char* kWeatherModes[11] = {
      "auto", "clear", "partly", "overcast", "rain", "storm", "snow", "wind", "sunrise", "sunset", "night"
    };
    static const char* kWeatherNames[11] = {
      "自動", "晴天", "多雲", "陰天", "下雨", "強雨／閃電", "下雪／結霜", "強風", "日出", "日落", "夜晚／月亮"
    };

    const bool weather_stale = w.isStale();
    last_rendered_weather_stale_ = weather_stale;

    active_sun_pct_ = phase_sun_override_active_ ? phase_sun_override_pct_ : calculateSunPhasePct(w);
    active_moon_phase_ = phase_moon_override_active_
      ? phase_moon_override_pct_ / 100.0f
      : w.astro_moon_phase;
    active_moon_phase_ -= floorf(active_moon_phase_);
    active_cloud_pct_ = w.cloud_cover;
    if (active_cloud_pct_ < 0) active_cloud_pct_ = 0;
    if (active_cloud_pct_ > 100) active_cloud_pct_ = 100;

    char buf[220];
    if (phase_moon_override_active_) {
      active_stage_key_ = "night";
      snprintf(buf, sizeof(buf), "預覽月相 %.2f%% · %s · 雲量 %d%% · 實況月相 %s",
               phase_moon_override_pct_, phaseConsoleMoonName(phase_moon_override_pct_),
               active_cloud_pct_, w.moon_label.c_str());
    } else if (phase_sun_override_active_) {
      if (active_sun_pct_ <= 20) active_stage_key_ = "sunrise";
      else if (active_sun_pct_ >= 80) active_stage_key_ = "sunset";
      else if (w.ok && w.mode.length() > 0 && w.mode != "night") {
        active_stage_key_ = w.mode;
      } else {
        active_stage_key_ = "clear";
      }
      snprintf(buf, sizeof(buf), "預覽日相 %d · 雲量 %d%% · %s",
               active_sun_pct_, active_cloud_pct_, w.moon_label.c_str());
    } else if (weather_mode_idx_ == 0) {
      if (state_mode_idx_ == 2) { // "solar" preview
        active_stage_key_ = "clear";
        snprintf(buf, sizeof(buf), "預覽晴天發電 · %s %.1f°C · %s (%d%%)",
                 w.city.c_str(), w.temp_c, w.moon_label.c_str(), w.moon_illum_pct);
      } else if (!w.ok) {
        active_stage_key_ = "night";
        snprintf(buf, sizeof(buf), "%s：天氣載入中 · %s (%d%%) · 日出 --:--",
                 w.city.c_str(), w.moon_label.c_str(), w.moon_illum_pct);
      } else {
        active_stage_key_ = w.mode.length() > 0 ? w.mode : (w.is_day ? "clear" : "night");
        const char* sun_label = w.is_day ? "日落" : "日出";
        const char* sun_time  = w.is_day ? w.sunset.c_str() : w.sunrise.c_str();
        snprintf(buf, sizeof(buf), "%s：%s %.1f°C%s · %s (%d%%) · %s %s",
                 w.city.c_str(), w.label.c_str(), w.temp_c,
                 weather_stale ? " (過期)" : "",
                 w.moon_label.c_str(), w.moon_illum_pct,
                 sun_label, sun_time);
      }
    } else {
      active_stage_key_ = kWeatherModes[weather_mode_idx_];
      snprintf(buf, sizeof(buf), "天氣預覽：%s · 實況月相 %s (%d%%)",
               kWeatherNames[weather_mode_idx_], w.moon_label.c_str(), w.moon_illum_pct);
    }
    setLabelText480(lbl_weather_, buf);

    if (phase_moon_override_active_) active_is_night_ = true;
    else if (phase_sun_override_active_) active_is_night_ = false;
    else if (weather_mode_idx_ != 0) active_is_night_ = weather_mode_idx_ == 10;
    else if (state_mode_idx_ == 2) active_is_night_ = false;
    else active_is_night_ = w.ok ? !w.is_day : active_stage_key_ == "night";

    struct tm tm_now = {};
    bool has_time = getLocalTime(&tm_now, 10) && (tm_now.tm_year >= 120);
    last_rendered_min_ = has_time ? (tm_now.tm_hour * 60 + tm_now.tm_min) : -1;
    int month = has_time ? (tm_now.tm_mon + 1) : 10;
    bool is_summer  = isTaiwanTouSummerMonth(month);
    bool is_weekend = has_time ? isTaiwanTouWeekend(tm_now) : false;
    float hour_float = has_time
      ? (tm_now.tm_hour + tm_now.tm_min / 60.0f)
      : (t.history_count > 0 ? t.history[t.history_count - 1].hour : 12.0f);
    float cursor_pct = (hour_float / 24.0f) * 100.0f;

    bool is_weekday_peak = isTaiwanTouWeekdayPeak(month, hour_float);

    char tou_txt[128];
    bool tou_is_peak = false;
    float solarW = 0.0f, pwW = 0.0f, gridW = 0.0f, loadW = 0.0f, soePct = 100.0f;
    float reservePct = t.backup_reserve_percent;
    bool isOffGrid = false;

    if (state_mode_idx_ == 0 || state_mode_idx_ == 1) { // live / live_ip
      if (!configured || !t.ok) {
        lv_obj_set_style_bg_color(dot_status_, lv_color_hex(0xf59e0b), 0);
        lv_obj_set_style_text_color(lbl_live_status_, lv_color_hex(0xfcd34d), 0);
        if (t.ok) {
          // Preserve last valid telemetry on screen while Wi-Fi reconnects
          snprintf(buf, sizeof(buf), "連線重試中 (%s)", t.timestamp.c_str());
          setLabelText480(lbl_live_status_, buf);
          solarW    = t.solar_power_w;
          pwW       = t.battery_power_w;
          gridW     = t.site_power_w;
          loadW     = t.load_power_w;
          soePct    = roundf(t.soe_percent);
          isOffGrid = (t.grid_status != "SystemGridConnected");
        } else {
          setLabelText480(lbl_live_status_, configured ? "連線重試中" : "等待配網設定...");
          solarW = 0; pwW = 0; gridW = 0; loadW = 0; soePct = 0; isOffGrid = false;
          setLabelText480(lbl_solar_val_, "-- kW");
          setLabelText480(lbl_home_val_, "-- kW");
          setLabelText480(lbl_pw_kw_, "-- kW");
          lv_obj_clear_flag(lbl_pw_dot_, LV_OBJ_FLAG_HIDDEN);
          lv_obj_add_flag(img_pw_arrow_, LV_OBJ_FLAG_HIDDEN);
          setLabelText480(lbl_pw_soc_, "--%");
          setLabelText480(lbl_grid_title_, "GRID");
          lv_obj_set_style_text_color(lbl_grid_title_, lv_color_hex(0xa1a1aa), 0);
          setLabelText480(lbl_grid_val_, "-- kW");
          lv_obj_set_style_text_color(lbl_grid_val_, lv_color_hex(0xffffff), 0);
          setLabelText480(lbl_stat_home_kw_, "-- kW");
          setLabelText480(lbl_stat_home_va_, "-- V · -- A");
          setLabelText480(lbl_stat_pw_soc_, "--%");
          setLabelText480(lbl_stat_pw_kwh_, "-- / -- kWh");
          setLabelText480(lbl_stat_pw_eta_, "--");
        }
      } else {
        if (t.stale) {
          lv_obj_set_style_bg_color(dot_status_, lv_color_hex(0xf59e0b), 0);
          lv_obj_set_style_text_color(lbl_live_status_, lv_color_hex(0xfcd34d), 0);
          if (state_mode_idx_ == 1) {
            snprintf(buf, sizeof(buf), "連線重試中 (%s) %s", t.timestamp.c_str(), t.gateway_ip.c_str());
          } else {
            snprintf(buf, sizeof(buf), "連線重試中 (%s)", t.timestamp.c_str());
          }
        } else {
          lv_obj_set_style_bg_color(dot_status_, lv_color_hex(0x34d399), 0);
          lv_obj_set_style_text_color(lbl_live_status_, lv_color_hex(0x34d399), 0);
          if (state_mode_idx_ == 1) {
            snprintf(buf, sizeof(buf), "%s (%s) %s", t.demo ? "DEMO" : "Live", t.timestamp.c_str(), t.gateway_ip.c_str());
          } else {
            snprintf(buf, sizeof(buf), "%s (%s)", t.demo ? "DEMO" : "Live", t.timestamp.c_str());
          }
        }
        setLabelText480(lbl_live_status_, buf);
        solarW    = t.solar_power_w;
        pwW       = t.battery_power_w;
        gridW     = t.site_power_w;
        loadW     = t.load_power_w;
        soePct    = roundf(t.soe_percent);
        isOffGrid = (t.grid_status != "SystemGridConnected");
      }

      tou_is_peak = !is_weekend && is_weekday_peak;
    } else if (state_mode_idx_ == 2) { // solar
      lv_obj_set_style_bg_color(dot_status_, lv_color_hex(0x34d399), 0);
      lv_obj_set_style_text_color(lbl_live_status_, lv_color_hex(0x34d399), 0);
      setLabelText480(lbl_live_status_, "預覽：太陽能充電 (點擊切換)");
      solarW = 5000; pwW = -3200; gridW = 0; loadW = 1800; soePct = 78; isOffGrid = false;
      is_weekend = false; cursor_pct = 52.0f;
    } else if (state_mode_idx_ == 3) { // charge
      lv_obj_set_style_bg_color(dot_status_, lv_color_hex(0x34d399), 0);
      lv_obj_set_style_text_color(lbl_live_status_, lv_color_hex(0x34d399), 0);
      setLabelText480(lbl_live_status_, "預覽：電網充電 住家+PW (點擊切換)");
      solarW = 0; pwW = -5000; gridW = 7200; loadW = 2200; soePct = 68; isOffGrid = false;
      is_weekend = false; cursor_pct = 10.4f;
    } else if (state_mode_idx_ == 4) { // peak
      lv_obj_set_style_bg_color(dot_status_, lv_color_hex(0x34d399), 0);
      lv_obj_set_style_text_color(lbl_live_status_, lv_color_hex(0x34d399), 0);
      setLabelText480(lbl_live_status_, "預覽：高峰放電 (點擊切換)");
      solarW = 0; pwW = 3200; gridW = 0; loadW = 3200; soePct = 55; isOffGrid = false;
      is_weekend = false; cursor_pct = 75.0f; tou_is_peak = true;
    } else if (state_mode_idx_ == 5) { // low
      lv_obj_set_style_bg_color(dot_status_, lv_color_hex(0x34d399), 0);
      lv_obj_set_style_text_color(lbl_live_status_, lv_color_hex(0x34d399), 0);
      setLabelText480(lbl_live_status_, "預覽：備援底線 24% (點擊切換)");
      solarW = 0; pwW = 0; gridW = 1800; loadW = 1800; soePct = 24; isOffGrid = false;
      is_weekend = false; cursor_pct = 92.0f; tou_is_peak = true;
    } else { // offgrid
      lv_obj_set_style_bg_color(dot_status_, lv_color_hex(0x34d399), 0);
      lv_obj_set_style_text_color(lbl_live_status_, lv_color_hex(0x34d399), 0);
      setLabelText480(lbl_live_status_, "預覽：斷網備援 地面紅叉叉 (點擊切換)");
      solarW = 0; pwW = 2500; gridW = 0; loadW = 2500; soePct = 82; isOffGrid = true;
      is_weekend = false; cursor_pct = 83.0f; tou_is_peak = true;
    }

    // The web dashboard uses the same short rate pill in live and preview modes.
    // Preview scenarios use the non-summer schedule regardless of today's date.
    const bool pill_is_summer = (state_mode_idx_ <= 1) && is_summer;
    const char* pill_price = tou_is_peak
      ? (pill_is_summer ? "NT$5.16" : "NT$4.93")
      : (pill_is_summer ? "NT$2.06" : "NT$1.99");
    snprintf(tou_txt, sizeof(tou_txt), "%s %s / kWh",
             tou_is_peak ? "高峰時段" : "離峰時段", pill_price);

    if ((state_mode_idx_ != 0 && state_mode_idx_ != 1) || t.ok) {
      if (solarW > 60.0f) {
        snprintf(buf, sizeof(buf), "%.1f kW", solarW / 1000.0f);
        lv_obj_set_style_text_color(lbl_solar_val_, lv_color_hex(0xffffff), 0);
      } else {
        snprintf(buf, sizeof(buf), "0 kW");
        lv_obj_set_style_text_color(lbl_solar_val_, lv_color_hex(0xa1a1aa), 0);
      }
      setLabelText480(lbl_solar_val_, buf);

      snprintf(buf, sizeof(buf), "%.1f kW", loadW / 1000.0f);
      setLabelText480(lbl_home_val_, buf);
      setLabelText480(lbl_stat_home_kw_, buf);

      snprintf(buf, sizeof(buf), "%.1f V · %.1f A", t.load_voltage_v, t.load_current_a);
      setLabelText480(lbl_stat_home_va_, buf);

      if (fabsf(pwW) > 60.0f) {
        snprintf(buf, sizeof(buf), "%.1f kW", fabsf(pwW) / 1000.0f);
      } else {
        snprintf(buf, sizeof(buf), "0 kW");
      }
      setLabelText480(lbl_pw_kw_, buf);

      snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(roundf(soePct)));
      setLabelText480(lbl_pw_soc_, buf);
      setLabelText480(lbl_stat_pw_soc_, buf);

      if (pwW < -60.0f) {
        lv_obj_add_flag(lbl_pw_dot_, LV_OBJ_FLAG_HIDDEN);
        lv_img_set_src(img_pw_arrow_, &kChargeArrowUpDsc);
        lv_obj_clear_flag(img_pw_arrow_, LV_OBJ_FLAG_HIDDEN);
      } else if (pwW > 60.0f) {
        lv_obj_add_flag(lbl_pw_dot_, LV_OBJ_FLAG_HIDDEN);
        lv_img_set_src(img_pw_arrow_, &kChargeArrowDownDsc);
        lv_obj_clear_flag(img_pw_arrow_, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_clear_flag(lbl_pw_dot_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(img_pw_arrow_, LV_OBJ_FLAG_HIDDEN);
      }

      const bool preview = state_mode_idx_ > 1;
      const double fullKwh = t.nominal_full_kwh != 0.0f ? t.nominal_full_kwh : 14.6;
      const double remKwh = preview ? round(fullKwh * soePct) / 100.0 : t.nominal_remaining_kwh;
      snprintf(buf, sizeof(buf), "%.2f / %.2f kWh", remKwh, fullKwh);
      setLabelText480(lbl_stat_pw_kwh_, buf);
      formatUsableEta(buf, sizeof(buf), remKwh, fullKwh, reservePct, loadW / 1000.0f, false);
      setLabelText480(lbl_stat_pw_eta_, buf);

      snprintf(buf, sizeof(buf), "Powerwall (備援 %d%%)", static_cast<int>(roundf(reservePct)));
      setLabelText480(lbl_stat_reserve_, buf);

      if (isOffGrid) {
        setLabelText480(lbl_grid_title_, "GRID");
        lv_obj_set_style_text_color(lbl_grid_title_, lv_color_hex(0xfb7185), 0);
        setLabelText480(lbl_grid_val_, "孤島供電");
        lv_obj_set_style_text_color(lbl_grid_val_, lv_color_hex(0xfb7185), 0);
      } else {
        setLabelText480(lbl_grid_title_, "GRID");
        lv_obj_set_style_text_color(lbl_grid_title_, lv_color_hex(0xa1a1aa), 0);
        if (fabsf(gridW) > 60.0f) {
          snprintf(buf, sizeof(buf), "%.1f kW", gridW / 1000.0f);
        } else {
          snprintf(buf, sizeof(buf), "0 kW");
        }
        setLabelText480(lbl_grid_val_, buf);
        lv_obj_set_style_text_color(lbl_grid_val_, lv_color_hex(0xffffff), 0);
      }
    }

    lv_obj_update_layout(chart_card_);
    lv_obj_set_x(lbl_stat_pw_kwh_, lv_obj_get_x(lbl_stat_pw_soc_) + lv_obj_get_width(lbl_stat_pw_soc_) + 5);

    if (t.site_name.length() > 0) {
      setLabelTextRaw480(lbl_site_title_, t.site_name.c_str());
    }
    setLabelText480(lbl_tou_pill_, tou_txt);
    if (tou_is_peak) {
      lv_obj_set_style_bg_color(pill_tou_, lv_color_hex(0x4c0519), 0);
      lv_obj_set_style_border_color(pill_tou_, lv_color_hex(0xf43f5e), 0);
      lv_obj_set_style_text_color(lbl_tou_pill_, lv_color_hex(0xfecdd3), 0);
    } else {
      lv_obj_set_style_bg_color(pill_tou_, lv_color_hex(0x064e3b), 0);
      lv_obj_set_style_border_color(pill_tou_, lv_color_hex(0x10b981), 0);
      lv_obj_set_style_text_color(lbl_tou_pill_, lv_color_hex(0x6ee7b7), 0);
    }

    if (is_weekend) {
      setLabelText480(lbl_tou_track_, "週末費率");
      lv_obj_set_style_text_color(lbl_tou_track_, lv_color_hex(0x34d399), 0);
      lv_obj_clear_flag(tou_bar_we_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(tou_bar_wk_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(tou_bar_wk_summer_, LV_OBJ_FLAG_HIDDEN);
    } else {
      setLabelText480(lbl_tou_track_, "週間費率");
      lv_obj_set_style_text_color(lbl_tou_track_, lv_color_hex(0xd4d4d8), 0);
      lv_obj_add_flag(tou_bar_we_, LV_OBJ_FLAG_HIDDEN);
      if (is_summer && (state_mode_idx_ == 0 || state_mode_idx_ == 1)) {
        lv_obj_add_flag(tou_bar_wk_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(tou_bar_wk_summer_, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_clear_flag(tou_bar_wk_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(tou_bar_wk_summer_, LV_OBJ_FLAG_HIDDEN);
      }
    }

    int cursor_x = 44 + static_cast<int>(roundf((cursor_pct / 100.0f) * kChartW));
    if (cursor_x < 44) cursor_x = 44;
    if (cursor_x > 44 + kChartW - 2) cursor_x = 44 + kChartW - 2;
    lv_obj_set_pos(cursor_line_, cursor_x - 1, 37);

    active_solar_w_ = solarW;
    active_pw_w_    = pwW;
    active_grid_w_  = gridW;
    active_load_w_  = loadW;
    active_soe_pct_ = soePct;
    active_offgrid_ = isOffGrid;
    update3DRoute(solarW, pwW, gridW, loadW, isOffGrid);
    render3DStageFrame();

    if (chart1_buf_ && chart1_canvas_) {
      renderAreaChartToRgb565(
        chart1_buf_, kChartW, kChart1H,
        t.history, t.history_count,
        false, 6.0f,
        56, 189, 248, 0.0f, false
      );
      lv_obj_invalidate(chart1_canvas_);
    }
    if (chart2_buf_ && chart2_canvas_) {
      renderAreaChartToRgb565(
        chart2_buf_, kChart2W, kChart2H,
        t.history, t.history_count,
        true, 100.0f,
        0, 230, 118, reservePct, false
      );
      lv_obj_invalidate(chart2_canvas_);
    }

    if (t.history_count > 0) {
      const HistoryPoint& latest = t.history[t.history_count - 1];
      const int x = std::max(0, std::min(kChartW - 1,
        static_cast<int>(roundf((latest.hour / 24.0f) * (kChartW - 1)))));
      const float loadNorm = fminf(1.0f, fmaxf(0.0f, latest.load_kw / 6.0f));
      const int loadY = std::max(2, std::min(kChart1H - 2,
        static_cast<int>(roundf((1.0f - loadNorm) * (kChart1H - 5)) + 2)));
      const float soeNorm = fminf(1.0f, fmaxf(0.0f, latest.soe / 100.0f));
      const int soeY = std::max(2, std::min(kChart2H - 1,
        static_cast<int>(roundf((1.0f - soeNorm) * (kChart2H - 3)) + 2)));
      const uint32_t loadColor = latest.power_source == HistoryPowerSource::Grid ? 0x38bdf8 :
        latest.power_source == HistoryPowerSource::Powerwall ? 0x11e58e : 0x94a3b8;
      lv_obj_set_style_bg_color(chart1_now_dot_, lv_color_hex(loadColor), 0);
      lv_obj_set_pos(chart1_now_dot_, 44 + x - 3, 37 + loadY - 3);
      const int batteryX = std::max(0, std::min(kChart2W - 1,
        static_cast<int>(roundf((latest.hour / 24.0f) * (kChart2W - 1)))));
      // LVGL coordinates are deferred until layout; use the computed point
      // directly rather than reading back the previous frame's position.
      lv_obj_set_pos(chart2_now_dot_, std::min(44 + kChart2W - 6, 44 + batteryX - 3),
                     kHome480ChartY + soeY - 3);
      lv_obj_clear_flag(chart1_now_dot_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(chart2_now_dot_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(chart1_now_dot_);
      lv_obj_move_foreground(chart2_now_dot_);
    } else {
      lv_obj_add_flag(chart1_now_dot_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(chart2_now_dot_, LV_OBJ_FLAG_HIDDEN);
    }

    const int home_cursor_x = std::max(44, std::min(44 + kChart2W - 1,
      44 + static_cast<int>(roundf((cursor_pct / 100.0f) * (kChart2W - 1)))));
    lv_obj_set_pos(cursor_line_, std::min(44 + kChart2W - 2, home_cursor_x - 1), 5);
    lv_obj_add_flag(chart1_now_dot_, LV_OBJ_FLAG_HIDDEN);
    updateHome480Status();
    refreshAddedPages();
  }

  void update3DRoute(float solarW, float pwW, float gridW, float loadW, bool isOffGrid) {
    static constexpr float kWireMaxUv[4] = {10.109375f, 3.248046875f, 3.984375f, 2.2f};
    static constexpr float kWireLen[4]   = {1.8287096f, 0.6522216f,   0.8169308f, 0.4026279f};

    int sources[4];
    int num_sources = 0;
    int sinks[4];
    int num_sinks = 0;

    if (solarW > 60.0f) sources[num_sources++] = 3;
    if (pwW > 60.0f) sources[num_sources++] = 2;
    else if (pwW < -60.0f) sinks[num_sinks++] = 2;

    if (!isOffGrid) {
      if (gridW > 60.0f) sources[num_sources++] = 0;
      else if (gridW < -60.0f) sinks[num_sinks++] = 0;
    }
    if (loadW > 60.0f) sinks[num_sinks++] = 1;

    char sig[48];
    int pos = snprintf(sig, sizeof(sig), "%d|", isOffGrid ? 1 : 0);
    for (int i = 0; i < num_sources; ++i) pos += snprintf(sig + pos, sizeof(sig) - pos, "%d,", sources[i]);
    pos += snprintf(sig + pos, sizeof(sig) - pos, "->");
    for (int i = 0; i < num_sinks; ++i) pos += snprintf(sig + pos, sizeof(sig) - pos, "%d,", sinks[i]);

    if (route_.signature != sig) {
      route_.signature = sig;
      route_.route_start_ms = millis();
    }

    for (int i = 0; i < 4; ++i) route_.branches[i].enabled = false;
    if (num_sources == 0 || num_sinks == 0) return;

    auto wscale = [&](int id) { return kWireLen[id] / kWireMaxUv[id]; };
    float refScale = wscale(2);

    if (num_sources == 1) {
      int src = sources[0];
      float srcLen = kWireLen[src];
      float srcScale = wscale(src);
      route_.branches[src] = {true, -srcScale, kWireMaxUv[src] * srcScale};
      for (int i = 0; i < num_sinks; ++i) {
        int sink = sinks[i];
        route_.branches[sink] = {true, wscale(sink), srcLen};
      }
    } else {
      float srcLen = 0.0f;
      for (int i = 0; i < num_sources; ++i) {
        if (kWireLen[sources[i]] > srcLen) srcLen = kWireLen[sources[i]];
      }
      for (int i = 0; i < num_sources; ++i) {
        int s = sources[i];
        route_.branches[s] = {true, -wscale(s), srcLen};
      }
      for (int i = 0; i < num_sinks; ++i) {
        int sink = sinks[i];
        route_.branches[sink] = {true, wscale(sink), srcLen};
      }
    }
    route_.speed = (9.0f / 2.0f) * refScale;
  }

  void render3DStageFrame() {
    if (!stage_base_buf_ || !stage_canvas_buf_) return;

    unsigned long now_ms = millis();
    float now_sec = now_ms / 1000.0f;

    String img_key = active_stage_key_;
    if (img_key == "rain" || img_key == "storm" || img_key == "snow" || img_key == "wind") {
      int frame_idx = static_cast<int>(now_sec * 5.0f) % 3;
      if (frame_idx == 1) img_key += "_1";
      else if (frame_idx == 2) img_key += "_2";
    }
    // Only stage_night has the old baked-in crescent at x=475..531. Its
    // cleanup pixels are nearly black; applying them to rainy/cloudy frames
    // creates an extra black disc beside the current moon.
    const bool needs_legacy_moon_cleanup = img_key == "night";

    const bool background_changed = decoded_base_key_ != img_key;
    if (background_changed) {
      decodeStage3DFrame(img_key.c_str(), reinterpret_cast<uint16_t*>(stage_base_buf_));
      decoded_base_key_ = img_key;
    }
    const bool phase_effects_changed = background_changed || !phase_effects_cache_valid_ ||
      cached_sun_pct_ != active_sun_pct_ || cached_cloud_pct_ != active_cloud_pct_ ||
      cached_moon_phase_ != active_moon_phase_ || cached_is_night_ != active_is_night_;

    lv_area_t dirty = {kStageW - 1, kStageH - 1, 0, 0};
    if (background_changed) {
      memcpy(stage_canvas_buf_, stage_base_buf_, kStageW * kStageH * sizeof(lv_color_t));
    } else {
      // Restore every overlay pixel, including effects that became inactive.
      auto restore = [&](uint16_t x, uint16_t y) {
        const size_t idx = static_cast<size_t>(y) * kStageW + x;
        stage_canvas_buf_[idx] = stage_base_buf_[idx];
        if (x < dirty.x1) dirty.x1 = x;
        if (x > dirty.x2) dirty.x2 = x;
        if (y < dirty.y1) dirty.y1 = y;
        if (y > dirty.y2) dirty.y2 = y;
      };
      for (size_t i=0; i<kStageSoePixelCount; ++i) restore(kStageSoePixels[i].x, kStageSoePixels[i].y);
      for (size_t i=0; i<kStageOffGridXPixelCount; ++i) restore(kStageOffGridXPixels[i].x, kStageOffGridXPixels[i].y);
      for (size_t i=0; i<kStageWirePixelCount; ++i) restore(kStageWirePixels[i].x, kStageWirePixels[i].y);
      if (phase_effects_changed) {
        for (size_t i=0; i<kStageMoonPixelCount; ++i) restore(kStageMoonPixels[i].x, kStageMoonPixels[i].y);
        for (size_t i=0; i<kStageSolarSpanCount; ++i) {
          const StageSolarSpan& span = kStageSolarSpans[i];
          for (uint16_t x = 0; x < span.count; ++x) restore(span.x + x, span.y);
        }
      }
    }
    uint16_t* dst = reinterpret_cast<uint16_t*>(stage_canvas_buf_);

    auto addRgbDelta = [](uint16_t px, int dr, int dg, int db) -> uint16_t {
      int r = ((px >> 11) & 0x1F); r = (r << 3) | (r >> 2);
      int g = ((px >> 5)  & 0x3F); g = (g << 2) | (g >> 4);
      int b = (px         & 0x1F); b = (b << 3) | (b >> 2);
      r += dr; if (r < 0) r = 0; else if (r > 255) r = 255;
      g += dg; if (g < 0) g = 0; else if (g > 255) g = 255;
      b += db; if (b < 0) b = 0; else if (b > 255) b = 255;
      return static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    };

    auto interpolateRgb565 = [](uint16_t a, uint16_t b, float t) -> uint16_t {
      int ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
      int br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
      int r = static_cast<int>(roundf(ar + (br - ar) * t));
      int g = static_cast<int>(roundf(ag + (bg - ag) * t));
      int blue = static_cast<int>(roundf(ab + (bb - ab) * t));
      return static_cast<uint16_t>((r << 11) | (g << 5) | blue);
    };

    if (phase_effects_changed) {
      float phase_position = active_moon_phase_ * 8.0f;
      cached_phase_index_ = static_cast<int>(phase_position);
      if (cached_phase_index_ > 7) cached_phase_index_ = 7;
      cached_phase_blend_ = phase_position - cached_phase_index_;
      cached_next_phase_index_ = (cached_phase_index_ + 1) & 7;

      float solar_angle;
      if (active_is_night_) {
        const float moon_illumination = 0.5f * (1.0f - cosf(active_moon_phase_ * 6.2831853f));
        solar_angle = 78.0f + 34.0f * active_moon_phase_;
        cached_solar_strength_ = (0.12f + 0.28f * moon_illumination) * (1.0f - active_cloud_pct_ / 100.0f);
        if (cached_solar_strength_ > 0.4f) cached_solar_strength_ = 0.4f;
        cached_solar_strength_ /= 0.4f; // captured night delta is at the shader's 0.4 ceiling
      } else {
        solar_angle = 35.0f + active_sun_pct_ * 0.79f;
        const float time_strength = active_sun_pct_ > 50
          ? (100.0f - active_sun_pct_) / 50.0f
          : active_sun_pct_ / 50.0f;
        float cloud_strength = 1.0f - active_cloud_pct_ / 100.0f;
        if (cloud_strength < 0.0f) cloud_strength = 0.0f;
        cached_solar_strength_ = time_strength < cloud_strength ? time_strength : cloud_strength;
        if (cached_solar_strength_ < 0.0f) cached_solar_strength_ = 0.0f;
      }
      const float angle_rad = solar_angle * 0.01745329252f;
      cached_solar_cos_ = cosf(angle_rad);
      cached_solar_sin_ = sinf(angle_rad);
      cached_sun_pct_ = active_sun_pct_;
      cached_cloud_pct_ = active_cloud_pct_;
      cached_moon_phase_ = active_moon_phase_;
      cached_is_night_ = active_is_night_;
      phase_effects_cache_valid_ = true;
    }

    auto renderMoonPixel = [&](const StageMoonPixel& moon) {
      const size_t idx = static_cast<size_t>(moon.y) * kStageW + moon.x;
      const uint16_t phaseColor = interpolateRgb565(
        moon.phase_rgb565[cached_phase_index_],
        moon.phase_rgb565[cached_next_phase_index_], cached_phase_blend_);
      if (needs_legacy_moon_cleanup) {
        dst[idx] = phaseColor;
        return;
      }
      // Phase keyframes were captured against the dark night sky. On brighter
      // weather frames, keep their dark sky and unlit moon pixels transparent.
      auto brightness = [](uint16_t color) {
        const int r = ((color >> 11) & 31) * 255 / 31;
        const int g = ((color >> 5) & 63) * 255 / 63;
        const int b = (color & 31) * 255 / 31;
        return (54 * r + 183 * g + 19 * b) / 256;
      };
      const int contrast = brightness(phaseColor) - brightness(dst[idx]);
      if (contrast <= 0) return;
      const float opacity = fminf(1.0f, contrast / 24.0f);
      dst[idx] = interpolateRgb565(dst[idx], phaseColor, opacity);
    };

    auto applyMoonPixel = [&](uint16_t x, uint16_t y) {
      if (!active_is_night_) return;
      const size_t pixel_key = static_cast<size_t>(y) * kStageW + x;
      size_t lo = 0, hi = kStageMoonPixelCount;
      while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        const size_t key = static_cast<size_t>(kStageMoonPixels[mid].y) * kStageW + kStageMoonPixels[mid].x;
        if (key < pixel_key) lo = mid + 1;
        else hi = mid;
      }
      if (lo < kStageMoonPixelCount && kStageMoonPixels[lo].y == y && kStageMoonPixels[lo].x == x) {
        renderMoonPixel(kStageMoonPixels[lo]);
      }
    };

    auto applySolarSpanPixel = [&](const StageSolarSpan& span, uint16_t px) {
      if (cached_solar_strength_ <= 0.001f) return;
      const size_t pixel_key = static_cast<size_t>(span.y) * kStageW + span.x + px;
      const float t = span.count > 1 ? static_cast<float>(px) / (span.count - 1) : 0.0f;
      const float uv_x = kStageSolarUvMinX +
                         (span.uv_x0 + (span.uv_x1 - span.uv_x0) * t) / 255.0f * kStageSolarUvRangeX;
      const float uv_y = kStageSolarUvMinY +
                         (span.uv_y0 + (span.uv_y1 - span.uv_y0) * t) / 255.0f * kStageSolarUvRangeY;
      const float shine_coord = (uv_x * cached_solar_cos_ - uv_y * cached_solar_sin_) - 0.0117f;
      float smooth_t = (shine_coord + 0.2f) / 0.4f;
      if (smooth_t < 0.0f) smooth_t = 0.0f;
      else if (smooth_t > 1.0f) smooth_t = 1.0f;
      const float strength = smooth_t * smooth_t * (3.0f - 2.0f * smooth_t) * cached_solar_strength_;
      const uint8_t* color0 = active_is_night_ ? span.night_delta0 : span.day_delta0;
      const uint8_t* color1 = active_is_night_ ? span.night_delta1 : span.day_delta1;
      const int dr = static_cast<int>(roundf((color0[0] + (color1[0] - color0[0]) * t) * strength));
      const int dg = static_cast<int>(roundf((color0[1] + (color1[1] - color0[1]) * t) * strength));
      const int db = static_cast<int>(roundf((color0[2] + (color1[2] - color0[2]) * t) * strength));
      dst[pixel_key] = addRgbDelta(dst[pixel_key], dr, dg, db);
    };

    auto applySolarPixel = [&](uint16_t x, uint16_t y) {
      if (cached_solar_strength_ <= 0.001f) return;
      const size_t pixel_key = static_cast<size_t>(y) * kStageW + x;
      size_t lo = 0, hi = kStageSolarSpanCount;
      while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        const StageSolarSpan& span = kStageSolarSpans[mid];
        const size_t end_key = static_cast<size_t>(span.y) * kStageW + span.x + span.count - 1;
        if (end_key < pixel_key) lo = mid + 1;
        else hi = mid;
      }
      if (lo >= kStageSolarSpanCount) return;
      const StageSolarSpan& span = kStageSolarSpans[lo];
      if (span.y != y || x < span.x || x >= span.x + span.count) return;
      applySolarSpanPixel(span, x - span.x);
    };

    auto applyPhaseEffectsAt = [&](uint16_t x, uint16_t y) {
      applyMoonPixel(x, y);
      applySolarPixel(x, y);
    };

    if (phase_effects_changed) {
      if (active_is_night_) {
        for (size_t i = 0; i < kStageMoonPixelCount; ++i) {
          const StageMoonPixel& moon = kStageMoonPixels[i];
          renderMoonPixel(moon);
        }
      }
      if (cached_solar_strength_ > 0.001f) {
        for (size_t i = 0; i < kStageSolarSpanCount; ++i) {
          const StageSolarSpan& span = kStageSolarSpans[i];
          for (uint16_t px = 0; px < span.count; ++px) applySolarSpanPixel(span, px);
        }
      }
    } else if (!background_changed) {
      // Ordinary LED, wire, and off-grid restore pixels can overlap either new
      // effect. Reapply only those intersections instead of rerendering the full
      // moon and solar masks on every 33 ms animation tick.
      for (size_t i = 0; i < kStageSoePixelCount; ++i) applyPhaseEffectsAt(kStageSoePixels[i].x, kStageSoePixels[i].y);
      for (size_t i = 0; i < kStageOffGridXPixelCount; ++i) applyPhaseEffectsAt(kStageOffGridXPixels[i].x, kStageOffGridXPixels[i].y);
      for (size_t i = 0; i < kStageWirePixelCount; ++i) applyPhaseEffectsAt(kStageWirePixels[i].x, kStageWirePixels[i].y);
    }

    // 1. Powerwall 2 Green LED Bar (makeSOEMaterial shader)
    float clamped_soe = fmaxf(0.0f, fminf(1.0f, active_soe_pct_ / 100.0f));
    float pulse_strength = (fabsf(active_pw_w_) > 60.0f) ? 1.0f : 0.25f;
    float raw_pulse = powf(sinf((now_sec / 2.0f + 0.65f) * 6.2831853f) * 0.5f + 0.5f, 1.5f);
    float pulse = (1.0f - pulse_strength) + pulse_strength * raw_pulse;
    float boost = 0.65f + 0.35f * pulse;

    for (size_t i = 0; i < kStageSoePixelCount; ++i) {
      const StageSoePixel& sp = kStageSoePixels[i];
      if (sp.frac <= clamped_soe) {
        size_t idx = static_cast<size_t>(sp.y) * kStageW + sp.x;
        dst[idx] = addRgbDelta(dst[idx],
                               static_cast<int>(sp.dr * boost),
                               static_cast<int>(sp.dg * boost),
                               static_cast<int>(sp.db * boost));
      }
    }

    // 2. Off-Grid Red X on the Ground
    if (active_offgrid_) {
      for (size_t i = 0; i < kStageOffGridXPixelCount; ++i) {
        const StageOffGridXPixel& xp = kStageOffGridXPixels[i];
        size_t idx = static_cast<size_t>(xp.y) * kStageW + xp.x;
        dst[idx] = static_cast<uint16_t>(((xp.r >> 3) << 11) | ((xp.g >> 2) << 5) | (xp.b >> 3));
      }
    }

    // 3. 3D Powerflow Wires (makeFlowMaterial shader)
    const uint32_t elapsed_ms = static_cast<uint32_t>(now_ms) - route_.route_start_ms;

    for (size_t i = 0; i < kStageWirePixelCount; ++i) {
      const StageWirePixel& wp = kStageWirePixels[i];
      if (wp.wid >= 4) continue;
      const WireBranch& br = route_.branches[wp.wid];
      if (!br.enabled) continue;
      float dist = wp.u_val * br.scale + br.offset;
      const float s = powerflow::brightness(elapsed_ms, dist, route_.speed);
      if (s > 0.0f) {
        size_t idx = static_cast<size_t>(wp.y) * kStageW + wp.x;
        dst[idx] = addRgbDelta(dst[idx],
                               static_cast<int>(roundf(wp.dr * s)),
                               static_cast<int>(roundf(wp.dg * s)),
                               static_cast<int>(roundf(wp.db * s)));
      }
    }

    if (background_changed || phase_effects_changed) updateHome480Backdrop();
    if (stage_canvas_) {
      if (background_changed) lv_obj_invalidate(stage_canvas_);
      else {
        lv_area_t coords;
        lv_obj_get_coords(stage_canvas_, &coords);
        dirty.x1 -= 2;
        dirty.y1 -= 2;
        dirty.x2 += 2;
        dirty.y2 += 2;
        dirty.x1 += coords.x1; dirty.x2 += coords.x1;
        dirty.y1 += coords.y1; dirty.y2 += coords.y1;
        lv_obj_invalidate_area(stage_canvas_, &dirty);
      }
    }
  }

  static void renderAreaChartToRgb565(
    lv_color_t* buf, int w, int h,
    const HistoryPoint* pts, size_t count,
    bool is_soe, float max_y,
    uint8_t r_col, uint8_t g_col, uint8_t b_col,
    float reserve_pct,
    bool draw_latest = true,
    float fill_boost = 1.0f
  ) {
    if (!buf) return;
    const lv_color_t bg = lv_color_make(9, 10, 15);
    for (int i = 0; i < w * h; ++i) {
      buf[i] = bg;
    }

    auto blendPx = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b, float a) {
      if (x < 0 || x >= w || y < 0 || y >= h) return;
      lv_color_t cur = buf[y * w + x];
      uint8_t cr = static_cast<uint8_t>((cur.ch.red << 3) | (cur.ch.red >> 2));
      uint8_t cg = static_cast<uint8_t>((cur.ch.green << 2) | (cur.ch.green >> 4));
      uint8_t cb = static_cast<uint8_t>((cur.ch.blue << 3) | (cur.ch.blue >> 2));
      uint8_t nr = static_cast<uint8_t>(cr * (1.0f - a) + r * a);
      uint8_t ng = static_cast<uint8_t>(cg * (1.0f - a) + g * a);
      uint8_t nb = static_cast<uint8_t>(cb * (1.0f - a) + b * a);
      buf[y * w + x] = lv_color_make(nr, ng, nb);
    };

    const int mid_y = h / 2;
    for (int x = 0; x < w; ++x) {
      blendPx(x, mid_y, 255, 255, 255, 0.08f);
      blendPx(x, h - 1, 255, 255, 255, 0.08f);
    }

    auto drawReserve = [&]() {
      if (!is_soe || !std::isfinite(reserve_pct) || reserve_pct < 0.0f) return;
      const float reserve = std::min(100.0f, reserve_pct);
      const int ry = static_cast<int>(roundf((1.0f - reserve / 100.0f) * (h - 3))) + 1;
      // Draw the reserve guide above the green area fill.
      for (int x = 0; x < w; ++x) if ((x / 3) % 2 == 0) {
        blendPx(x, ry,     56, 189, 248, 0.75f);
      }
    };

    if (!pts || count == 0) {
      drawReserve();
      return;
    }

    int last_px = 0, last_py = h - 1;
    for (size_t i = 0; i < count; ++i) {
      float v1 = is_soe ? pts[i].soe : pts[i].load_kw;
      float norm1 = fminf(1.0f, fmaxf(0.0f, v1 / max_y));
      int x1 = static_cast<int>(roundf((pts[i].hour / 24.0f) * (w - 1)));
      if (x1 < 0) x1 = 0;
      if (x1 >= w) x1 = w - 1;
      const int topInset = is_soe ? 2 : 1;
      int y1 = static_cast<int>(roundf((1.0f - norm1) * (h - 3))) + topInset;
      if (y1 < topInset) y1 = topInset;
      if (y1 > h - 1) y1 = h - 1;
      last_px = x1;
      last_py = y1;

      if (i + 1 < count) {
        uint8_t segment_r = r_col, segment_g = g_col, segment_b = b_col;
        if (!is_soe) {
          const HistoryPowerSource source = pts[i + 1].power_source;
          if (source == HistoryPowerSource::Powerwall) {
            segment_r = 17; segment_g = 229; segment_b = 142;
          } else if (source == HistoryPowerSource::Grid) {
            segment_r = 56; segment_g = 189; segment_b = 248;
          } else {
            segment_r = 148; segment_g = 163; segment_b = 184;
          }
        }
        float v2 = is_soe ? pts[i + 1].soe : pts[i + 1].load_kw;
        float norm2 = fminf(1.0f, fmaxf(0.0f, v2 / max_y));
        int x2 = static_cast<int>(roundf((pts[i + 1].hour / 24.0f) * (w - 1)));
        if (x2 < 0) x2 = 0;
        if (x2 >= w) x2 = w - 1;
        int y2 = static_cast<int>(roundf((1.0f - norm2) * (h - 3))) + topInset;
        if (y2 < topInset) y2 = topInset;
        if (y2 > h - 1) y2 = h - 1;
        if (x2 < x1) continue;
        // Adjacent segments share their endpoint column. Fill that boundary only
        // once, otherwise repeated alpha blending leaves visible vertical stripes.
        const int fillEnd = (i + 2 < count) ? x2 - 1 : x2;
        for (int x = x1; x <= fillEnd; ++x) {
          float t = (x2 == x1) ? 0.0f : static_cast<float>(x - x1) / static_cast<float>(x2 - x1);
          int y_top = static_cast<int>(roundf(y1 + t * (y2 - y1)));
          for (int y = y_top; y < h; ++y) {
            float vfrac = 1.0f - static_cast<float>(y) / static_cast<float>(h);
            float alpha = is_soe
              ? fminf(0.72f, (0.08f + 0.37f * vfrac) * fmaxf(1.0f, fill_boost))
              : (0.42f * vfrac);
            blendPx(x, y, segment_r, segment_g, segment_b, alpha);
          }
          if (is_soe) {
            blendPx(x, y_top - 1, segment_r, segment_g, segment_b, 0.85f);
            blendPx(x, y_top,     segment_r, segment_g, segment_b, 1.0f);
          }
        }
      }
    }

    // Draw the load outline after filling, using the same complete segment
    // rasterization as the Home/Grid page, including vertical same-column jumps.
    if (!is_soe) renderHomeLoadChartToRgb565(buf, w, h, pts, count, true);

    if (draw_latest) {
      uint8_t dot_r = r_col, dot_g = g_col, dot_b = b_col;
      if (!is_soe) {
        const HistoryPowerSource source = pts[count - 1].power_source;
        if (source == HistoryPowerSource::Powerwall) {
          dot_r = 17; dot_g = 229; dot_b = 142;
        } else if (source == HistoryPowerSource::Grid) {
          dot_r = 56; dot_g = 189; dot_b = 248;
        } else {
          dot_r = 148; dot_g = 163; dot_b = 184;
        }
      }
      constexpr int dotR = 3;
      constexpr int innerR2 = 6 * dotR * dotR / 16, outerR2 = dotR * dotR;
      for (int dy = -dotR; dy <= dotR; ++dy) {
        for (int dx = -dotR; dx <= dotR; ++dx) {
          int d2 = dx * dx + dy * dy;
          if (d2 <= innerR2) {
            blendPx(last_px + dx, last_py + dy, 255, 255, 255, 1.0f);
          } else if (d2 <= outerR2) {
            blendPx(last_px + dx, last_py + dy, dot_r, dot_g, dot_b, 1.0f);
          }
        }
      }
    }
    drawReserve();
  }
};

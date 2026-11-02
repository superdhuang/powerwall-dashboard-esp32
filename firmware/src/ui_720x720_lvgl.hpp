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
#include "cjk_fonts.hpp"
#include "font_asset_aliases.hpp"
#include "stage_3d_assets.hpp"
#include "powerflow_animation.hpp"
#include "powerwall_client.hpp"
#include "weather_client.hpp"
#include "phase_console.hpp"
#include "usable_eta.hpp"
#include "weekly_history.hpp"

#if defined(POWERWALL_WAVESHARE_S3_LCD_4B) || \
    (defined(POWERWALL_SIM_WINDOW_SIZE) && POWERWALL_SIM_WINDOW_SIZE == 480)
#define POWERWALL_UI_480 1
#endif

// 14x12 Alpha-8bit masks rasterized from Tesla ChargeIcon.svg (#11E58E)
static const uint8_t kChargeArrowUpMap[14 * 12] = {
  0x00, 0x00, 0x00, 0x02, 0x03, 0xa4, 0xe6, 0x2b, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x03, 0x00, 0x4d, 0xfe, 0xff, 0xb4, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00,
  0x00, 0x01, 0x02, 0x08, 0xd1, 0xff, 0xfb, 0xff, 0x4d, 0x00, 0x03, 0x00, 0x00, 0x00,
  0x00, 0x03, 0x00, 0x6b, 0xff, 0xfc, 0xfc, 0xff, 0xce, 0x09, 0x02, 0x01, 0x00, 0x00,
  0x01, 0x01, 0x15, 0xe3, 0xfe, 0xfd, 0xff, 0xfc, 0xff, 0x69, 0x00, 0x03, 0x00, 0x00,
  0x03, 0x00, 0x8a, 0xff, 0xfd, 0xff, 0xff, 0xfe, 0xfe, 0xe3, 0x14, 0x01, 0x02, 0x00,
  0x01, 0x27, 0xf0, 0xfd, 0xfe, 0xff, 0xff, 0xff, 0xfc, 0xff, 0x87, 0x00, 0x03, 0x00,
  0x00, 0xa2, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xfe, 0xf3, 0x24, 0x00, 0x02,
  0x40, 0xf9, 0xfd, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xa5, 0x00, 0x02,
  0xc9, 0xfc, 0xf9, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfa, 0xf9, 0xfa, 0x3b, 0x00,
  0xe5, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x51, 0x00,
  0x31, 0x69, 0x69, 0x69, 0x69, 0x69, 0x69, 0x69, 0x69, 0x68, 0x6a, 0x56, 0x05, 0x00,
};

static const uint8_t kChargeArrowDownMap[14 * 12] = {
  0x31, 0x69, 0x69, 0x69, 0x69, 0x69, 0x69, 0x69, 0x69, 0x68, 0x6a, 0x56, 0x05, 0x00,
  0xe5, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x51, 0x00,
  0xc9, 0xfc, 0xf9, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfa, 0xf9, 0xfa, 0x3b, 0x00,
  0x40, 0xf9, 0xfd, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xa5, 0x00, 0x02,
  0x00, 0xa2, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xfe, 0xf3, 0x24, 0x00, 0x02,
  0x01, 0x27, 0xf0, 0xfd, 0xfe, 0xff, 0xff, 0xff, 0xfc, 0xff, 0x87, 0x00, 0x03, 0x00,
  0x03, 0x00, 0x8a, 0xff, 0xfd, 0xff, 0xff, 0xfe, 0xfe, 0xe3, 0x14, 0x01, 0x02, 0x00,
  0x01, 0x01, 0x15, 0xe3, 0xfe, 0xfd, 0xff, 0xfc, 0xff, 0x69, 0x00, 0x03, 0x00, 0x00,
  0x00, 0x03, 0x00, 0x6b, 0xff, 0xfc, 0xfc, 0xff, 0xce, 0x09, 0x02, 0x01, 0x00, 0x00,
  0x00, 0x01, 0x02, 0x08, 0xd1, 0xff, 0xfb, 0xff, 0x4d, 0x00, 0x03, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x03, 0x00, 0x4d, 0xfe, 0xff, 0xb4, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x02, 0x03, 0xa4, 0xe6, 0x2b, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
};

static const lv_img_dsc_t kChargeArrowUpDsc = {
  { LV_IMG_CF_ALPHA_8BIT, 0, 0, 14, 12 },
  sizeof(kChargeArrowUpMap),
  kChargeArrowUpMap,
};

static const lv_img_dsc_t kChargeArrowDownDsc = {
  { LV_IMG_CF_ALPHA_8BIT, 0, 0, 14, 12 },
  sizeof(kChargeArrowDownMap),
  kChargeArrowDownMap,
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
  snprintf(output, outputSize, "Uptime: %llud %02llu:%02llu:%02llu",
           static_cast<unsigned long long>(days),
           static_cast<unsigned long long>(hours),
           static_cast<unsigned long long>(minutes),
           static_cast<unsigned long long>(seconds));
}

// Pure 720x720 Display UI for ESP32-P4 MIPI-DSI & ESP32-S3 RGB Panels
class UI720x720 {
public:
  static constexpr int kStageW  = 704;
  static constexpr int kStageH  = 440;
  static constexpr int kChartW  = 624;
  static constexpr int kChart1H = 82;
#if defined(POWERWALL_UI_480)
  static constexpr int kChart2W = 642; // Home: 66px axis column + 642px plot + 12px right inset (8px on panel).
  static constexpr int kChart2H = 90;
#else
  static constexpr int kChart2W = kChartW;
  static constexpr int kChart2H = 70;
#endif
  static constexpr int kDashboardChartH = 210;
  static constexpr int kPwChartPlotX = 54;
  static constexpr int kPwChartPlotY = 10;
  static constexpr int kPwChartTouGap = 2;
  static constexpr int kPwChartTouH = 8;
  static constexpr int kPwChartBandY = kPwChartPlotY + kDashboardChartH + kPwChartTouGap;
  static constexpr int kPwChartTimeY = kPwChartBandY + kPwChartTouH + 1;
  static constexpr int kPwChartCursorH = kDashboardChartH + kPwChartTouGap + kPwChartTouH;
  static constexpr int kPwChartCardY = 454;
  static constexpr int kPwChartCardH = 256;

#if defined(POWERWALL_UI_480)
  void setWifiStartupCardMode(uint8_t mode, const char* ssid = nullptr,
                              const char* setupUrl = nullptr) {
    if (active_dashboard_page_ != 0) {
      setHome480WifiCardMode(3, nullptr, nullptr);
      return;
    }
    setHome480WifiCardMode(mode, ssid, setupUrl);
  }
#if defined(POWERWALL_SIM_WINDOW_SIZE) && POWERWALL_SIM_WINDOW_SIZE == 480
  void setSimulatorWifiCardMode(uint8_t mode, const char* ssid = nullptr,
                                const char* setupUrl = nullptr) {
    setWifiStartupCardMode(mode, ssid, setupUrl);
  }
#endif
#endif

  void init() {
    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_size(scr, 720, 720);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0b0c0f), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // 1. TOP HERO CARD (704 x 440 at x=8, y=8)
    hero_card_ = lv_obj_create(scr);
    lv_obj_set_pos(hero_card_, 8, 8);
    lv_obj_set_size(hero_card_, kStageW, kStageH);
    lv_obj_set_style_bg_color(hero_card_, lv_color_hex(0x101820), 0);
    lv_obj_set_style_border_color(hero_card_, lv_color_hex(0x27272a), 0);
    lv_obj_set_style_border_width(hero_card_, 1, 0);
    lv_obj_set_style_radius(hero_card_, 16, 0);
#if defined(POWERWALL_UI_480)
    lv_obj_set_style_border_width(hero_card_, 0, 0);
    lv_obj_set_style_radius(hero_card_, 0, 0);
#endif
    lv_obj_set_style_pad_all(hero_card_, 0, 0);
    lv_obj_set_style_clip_corner(hero_card_, true, 0);
    lv_obj_clear_flag(hero_card_, LV_OBJ_FLAG_SCROLLABLE);

    // Full-bleed 3D Stage Canvas (704 x 440 at x=0, y=0)
    stage_base_buf_   = allocPixelBuf(kStageW * kStageH);
    stage_canvas_buf_ = allocPixelBuf(kStageW * kStageH);
    if (stage_canvas_buf_) {
      stage_canvas_ = lv_canvas_create(hero_card_);
      lv_canvas_set_buffer(stage_canvas_, stage_canvas_buf_, kStageW, kStageH, LV_IMG_CF_TRUE_COLOR);
      lv_obj_set_pos(stage_canvas_, 0, 0);
      lv_obj_set_size(stage_canvas_, kStageW, kStageH);
    }

    // Floating Header Overlay Inside the 3D Scene (704 x 40 at y=2)
    lv_obj_t* hdr_bar = lv_obj_create(hero_card_);
    lv_obj_set_pos(hdr_bar, 0, 2);
    lv_obj_set_size(hdr_bar, kStageW, 40);
    lv_obj_set_style_bg_opa(hdr_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hdr_bar, 0, 0);
    lv_obj_set_style_radius(hdr_bar, 0, 0);
    lv_obj_set_style_pad_all(hdr_bar, 0, 0);
    lv_obj_clear_flag(hdr_bar, LV_OBJ_FLAG_SCROLLABLE);

    lbl_site_title_ = lv_label_create(hdr_bar);
    lv_obj_set_style_text_font(lbl_site_title_, &font_cjk_18, 0);
    lv_obj_set_style_text_color(lbl_site_title_, lv_color_hex(0xffffff), 0);
    lv_label_set_text(lbl_site_title_, "我的家");
    lv_obj_align(lbl_site_title_, LV_ALIGN_LEFT_MID, 16, 0);

    pill_status_ = lv_obj_create(hdr_bar);
    lv_obj_set_size(pill_status_, LV_SIZE_CONTENT, 24);
    lv_obj_align(pill_status_, LV_ALIGN_LEFT_MID, 100, 0);
    lv_obj_set_style_bg_color(pill_status_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(pill_status_, LV_OPA_40, 0);
    lv_obj_set_style_border_color(pill_status_, lv_color_hex(0x059669), 0);
    lv_obj_set_style_border_width(pill_status_, 1, 0);
    lv_obj_set_style_radius(pill_status_, 12, 0);
#if defined(POWERWALL_UI_480)
    flattenDashboardPill(pill_status_, 0x059669);
#endif
    lv_obj_set_style_pad_hor(pill_status_, 10, 0);
    lv_obj_set_style_pad_top(pill_status_, 0, 0);
    lv_obj_set_style_pad_bottom(pill_status_, 0, 0);
    lv_obj_set_flex_flow(pill_status_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pill_status_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(pill_status_, 6, 0);
    lv_obj_clear_flag(pill_status_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(pill_status_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(pill_status_, [](lv_event_t* e) {
      UI720x720* self = static_cast<UI720x720*>(lv_event_get_user_data(e));
      if (self) {
        self->state_mode_idx_ = (self->state_mode_idx_ + 1) % 7;
        self->refreshAllFromCache();
      }
    }, LV_EVENT_CLICKED, this);

    dot_status_ = lv_obj_create(pill_status_);
    lv_obj_set_size(dot_status_, 7, 7);
    lv_obj_set_style_radius(dot_status_, 4, 0);
    lv_obj_set_style_bg_color(dot_status_, lv_color_hex(0x34d399), 0);
    lv_obj_set_style_bg_opa(dot_status_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot_status_, 0, 0);
    lv_obj_clear_flag(dot_status_, LV_OBJ_FLAG_SCROLLABLE);

    lbl_live_status_ = lv_label_create(pill_status_);
    lv_obj_set_style_text_font(lbl_live_status_, &font_cjk_12, 0);
    // Center the visible glyphs, accounting for the font's baseline space.
    lv_obj_set_style_translate_y(lbl_live_status_, -1, 0);
    lv_obj_set_style_text_color(lbl_live_status_, lv_color_hex(0x34d399), 0);
    lv_label_set_text(lbl_live_status_, "連線中");

    pill_weather_ = lv_obj_create(hdr_bar);
    lv_obj_set_size(pill_weather_, LV_SIZE_CONTENT, 24);
    lv_obj_align(pill_weather_, LV_ALIGN_RIGHT_MID, -14, 0);
    lv_obj_set_style_bg_color(pill_weather_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(pill_weather_, LV_OPA_40, 0);
    lv_obj_set_style_border_color(pill_weather_, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(pill_weather_, 1, 0);
    lv_obj_set_style_radius(pill_weather_, 12, 0);
#if defined(POWERWALL_UI_480)
    flattenDashboardPill(pill_weather_, 0x334155);
#endif
    lv_obj_set_style_pad_hor(pill_weather_, 10, 0);
    lv_obj_set_style_pad_top(pill_weather_, 2, 0);
    lv_obj_set_style_pad_bottom(pill_weather_, 0, 0);
    lv_obj_set_flex_flow(pill_weather_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pill_weather_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(pill_weather_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(pill_weather_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(pill_weather_, [](lv_event_t* e) {
      UI720x720* self = static_cast<UI720x720*>(lv_event_get_user_data(e));
      if (self) {
        self->weather_mode_idx_ = (self->weather_mode_idx_ + 1) % 11;
        self->refreshAllFromCache();
      }
    }, LV_EVENT_CLICKED, this);

    lbl_weather_ = lv_label_create(pill_weather_);
    lv_obj_set_style_text_font(lbl_weather_, &font_cjk_12, 0);
    lv_obj_set_style_text_color(lbl_weather_, lv_color_hex(0xbae6fd), 0);
    lv_label_set_text(lbl_weather_, "台北：天氣載入中...");

    auto makeLeaderLine = [&](int x, int y, int h_px) {
      lv_obj_t* ln = lv_obj_create(hero_card_);
      lv_obj_set_pos(ln, x, y);
      lv_obj_set_size(ln, 2, h_px);
      lv_obj_set_style_bg_color(ln, lv_color_hex(0xa1a1aa), 0);
      lv_obj_set_style_bg_opa(ln, LV_OPA_60, 0);
      lv_obj_set_style_border_width(ln, 0, 0);
      lv_obj_set_style_radius(ln, 0, 0);
      lv_obj_clear_flag(ln, LV_OBJ_FLAG_SCROLLABLE);
    };

    // Callout 1: SOLAR (3D Anchor at x=363, y=156 on 704x440 stage)
    makeLeaderLine(363, 100, 56);
    lv_obj_t* lbl_s_title = lv_label_create(hero_card_);
    lv_obj_set_style_text_font(lbl_s_title, &font_cjk_12, 0);
    lv_obj_set_style_text_color(lbl_s_title, lv_color_hex(0xa1a1aa), 0);
    lv_label_set_text(lbl_s_title, "SOLAR");
    lv_obj_set_width(lbl_s_title, 100);
    lv_obj_set_style_text_align(lbl_s_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_s_title, 363 - 50, 56);

    lbl_solar_val_ = lv_label_create(hero_card_);
    lv_obj_set_style_text_font(lbl_solar_val_, &font_cjk_20, 0);
    lv_obj_set_style_text_color(lbl_solar_val_, lv_color_hex(0xa1a1aa), 0);
    lv_label_set_text(lbl_solar_val_, "0 kW");
    lv_obj_set_width(lbl_solar_val_, 120);
    lv_obj_set_style_text_align(lbl_solar_val_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_solar_val_, 363 - 60, 72);

    // Callout 2: HOME (3D Anchor at x=484, y=256 on 704x440 stage)
    makeLeaderLine(484, 176, 80);
    lv_obj_t* lbl_h_title = lv_label_create(hero_card_);
    lv_obj_set_style_text_font(lbl_h_title, &font_cjk_12, 0);
    lv_obj_set_style_text_color(lbl_h_title, lv_color_hex(0xa1a1aa), 0);
    lv_label_set_text(lbl_h_title, "HOME");
    lv_obj_set_width(lbl_h_title, 100);
    lv_obj_set_style_text_align(lbl_h_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_h_title, 484 - 50, 132);

    lbl_home_val_ = lv_label_create(hero_card_);
    lv_obj_set_style_text_font(lbl_home_val_, &font_cjk_20, 0);
    lv_obj_set_style_text_color(lbl_home_val_, lv_color_hex(0xffffff), 0);
    lv_label_set_text(lbl_home_val_, "-- kW");
    lv_obj_set_width(lbl_home_val_, 120);
    lv_obj_set_style_text_align(lbl_home_val_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_home_val_, 484 - 60, 148);

    // Callout 3: POWERWALL (3D Anchor at x=279, y=356 on 704x440 stage)
    makeLeaderLine(279, 356, 14);
    lbl_pw_title_ = lv_label_create(hero_card_);
    lv_obj_set_style_text_font(lbl_pw_title_, &font_cjk_12, 0);
    lv_obj_set_style_text_color(lbl_pw_title_, lv_color_hex(0xa1a1aa), 0);
    lv_label_set_text(lbl_pw_title_, "POWERWALL");
    lv_obj_set_width(lbl_pw_title_, 140);
    lv_obj_set_style_text_align(lbl_pw_title_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_pw_title_, 279 - 70, 374);

    pw_val_row_ = lv_obj_create(hero_card_);
    lv_obj_set_size(pw_val_row_, 180, 24);
    lv_obj_set_pos(pw_val_row_, 279 - 90, 390);
    lv_obj_set_style_bg_opa(pw_val_row_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pw_val_row_, 0, 0);
    lv_obj_set_style_pad_all(pw_val_row_, 0, 0);
    lv_obj_set_style_pad_column(pw_val_row_, 5, 0);
    lv_obj_set_flex_flow(pw_val_row_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pw_val_row_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(pw_val_row_, LV_OBJ_FLAG_SCROLLABLE);

    lbl_pw_kw_ = lv_label_create(pw_val_row_);
    lv_obj_set_style_text_font(lbl_pw_kw_, &font_cjk_20, 0);
    lv_label_set_text(lbl_pw_kw_, "-- kW");
    lv_obj_set_style_text_color(lbl_pw_kw_, lv_color_hex(0xffffff), 0);

    lbl_pw_dot_ = lv_obj_create(pw_val_row_);
    lv_obj_set_size(lbl_pw_dot_, 4, 4);
    lv_obj_set_style_radius(lbl_pw_dot_, 2, 0);
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
    lv_obj_set_style_text_font(lbl_pw_soc_, &font_cjk_20, 0);
    lv_label_set_text(lbl_pw_soc_, "--%");
    lv_obj_set_style_text_color(lbl_pw_soc_, lv_color_hex(0xffffff), 0);

    lbl_stat_pw_eta_ = lv_label_create(hero_card_);
    lv_obj_set_style_text_font(lbl_stat_pw_eta_, &font_cjk_12, 0);
    lv_obj_set_style_text_color(lbl_stat_pw_eta_, lv_color_hex(0xd4d4d8), 0);
    lv_label_set_text(lbl_stat_pw_eta_, "--");
    lv_obj_set_width(lbl_stat_pw_eta_, 180);
    lv_obj_set_style_text_align(lbl_stat_pw_eta_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_stat_pw_eta_, 279 - 90, 414);

    // Callout 4: GRID (3D Anchor at x=457, y=373 on 704x440 stage)
    makeLeaderLine(457, 373, 10);
    lbl_grid_title_ = lv_label_create(hero_card_);
    lv_obj_set_style_text_font(lbl_grid_title_, &font_cjk_12, 0);
    lv_obj_set_style_text_color(lbl_grid_title_, lv_color_hex(0xa1a1aa), 0);
    lv_label_set_text(lbl_grid_title_, "GRID");
    lv_obj_set_width(lbl_grid_title_, 120);
    lv_obj_set_style_text_align(lbl_grid_title_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_grid_title_, 457 - 60, 387);

    lbl_grid_val_ = lv_label_create(hero_card_);
    lv_obj_set_style_text_font(lbl_grid_val_, &font_cjk_20, 0);
    lv_obj_set_style_text_color(lbl_grid_val_, lv_color_hex(0xffffff), 0);
    lv_label_set_text(lbl_grid_val_, "-- kW");
    lv_obj_set_width(lbl_grid_val_, 140);
    lv_obj_set_style_text_align(lbl_grid_val_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_grid_val_, 457 - 70, 403);

    // 2. BOTTOM UNIFIED 24H TIMELINE CARD (704 x 256 at x=8, y=456)
    chart_card_ = lv_obj_create(scr);
    lv_obj_set_pos(chart_card_, 8, 456);
    lv_obj_set_size(chart_card_, 704, 256);
    lv_obj_set_style_bg_color(chart_card_, lv_color_hex(0x13151a), 0);
    lv_obj_set_style_bg_opa(chart_card_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(chart_card_, lv_color_hex(0x27272a), 0);
    lv_obj_set_style_border_width(chart_card_, 1, 0);
    lv_obj_set_style_radius(chart_card_, 16, 0);
#if defined(POWERWALL_UI_480)
    lv_obj_set_style_border_width(chart_card_, 0, 0);
    lv_obj_set_style_radius(chart_card_, 0, 0);
    lv_obj_t* chart_rule = lv_obj_create(chart_card_);
    lv_obj_set_pos(chart_rule, 0, 255); lv_obj_set_size(chart_rule, 704, 1);
    lv_obj_set_style_bg_color(chart_rule, lv_color_hex(0x263746), 0);
    lv_obj_set_style_bg_opa(chart_rule, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(chart_rule, 0, 0);
    lv_obj_clear_flag(chart_rule, LV_OBJ_FLAG_SCROLLABLE);
#endif
    lv_obj_set_style_pad_all(chart_card_, 0, 0);
    lv_obj_clear_flag(chart_card_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl_h_stat_t = lv_label_create(chart_card_);
    lv_obj_set_style_text_font(lbl_h_stat_t, &font_cjk_12, 0);
    lv_obj_set_style_text_color(lbl_h_stat_t, lv_color_hex(0xa1a1aa), 0);
    lv_label_set_text(lbl_h_stat_t, "住家用量");
    lv_obj_set_pos(lbl_h_stat_t, 14, 8);

    lbl_stat_home_kw_ = lv_label_create(chart_card_);
    lv_obj_set_style_text_font(lbl_stat_home_kw_, &font_cjk_18, 0);
    lv_obj_set_style_text_color(lbl_stat_home_kw_, lv_color_hex(0x38bdf8), 0);
    lv_label_set_text(lbl_stat_home_kw_, "-- kW");
    lv_obj_set_pos(lbl_stat_home_kw_, 14, 23);

    lbl_stat_home_va_ = lv_label_create(chart_card_);
    lv_obj_set_style_text_font(lbl_stat_home_va_, &font_cjk_12, 0);
    lv_obj_set_style_text_color(lbl_stat_home_va_, lv_color_hex(0xa1a1aa), 0);
    lv_label_set_text(lbl_stat_home_va_, "-- V · -- A");
    lv_obj_set_pos(lbl_stat_home_va_, 84, 27);

    lv_obj_t* vdiv = lv_obj_create(chart_card_);
    lv_obj_set_pos(vdiv, 198, 12);
    lv_obj_set_size(vdiv, 1, 26);
    lv_obj_set_style_bg_color(vdiv, lv_color_hex(0x27272a), 0);
    lv_obj_set_style_border_width(vdiv, 0, 0);
    lv_obj_clear_flag(vdiv, LV_OBJ_FLAG_SCROLLABLE);

    lbl_stat_reserve_ = lv_label_create(chart_card_);
    lv_obj_set_style_text_font(lbl_stat_reserve_, &font_cjk_12, 0);
    lv_obj_set_style_text_color(lbl_stat_reserve_, lv_color_hex(0xa1a1aa), 0);
    lv_label_set_text(lbl_stat_reserve_, "Powerwall (備援 24%)");
    lv_obj_set_pos(lbl_stat_reserve_, 210, 8);

    lbl_stat_pw_soc_ = lv_label_create(chart_card_);
    lv_obj_set_style_text_font(lbl_stat_pw_soc_, &font_cjk_18, 0);
    lv_obj_set_style_text_color(lbl_stat_pw_soc_, lv_color_hex(0x00e676), 0);
    lv_label_set_text(lbl_stat_pw_soc_, "--%");
    lv_obj_set_pos(lbl_stat_pw_soc_, 210, 23);

    lbl_stat_pw_kwh_ = lv_label_create(chart_card_);
    lv_obj_set_style_text_font(lbl_stat_pw_kwh_, &font_cjk_12, 0);
    lv_obj_set_style_text_color(lbl_stat_pw_kwh_, lv_color_hex(0xa1a1aa), 0);
    lv_label_set_text(lbl_stat_pw_kwh_, "-- / -- kWh");
    lv_obj_set_pos(lbl_stat_pw_kwh_, 266, 27);

    pill_tou_ = lv_obj_create(chart_card_);
    lv_obj_set_size(pill_tou_, LV_SIZE_CONTENT, 24);
    lv_obj_align(pill_tou_, LV_ALIGN_TOP_RIGHT, -14, 12);
    lv_obj_set_style_bg_color(pill_tou_, lv_color_hex(0x064e3b), 0);
    lv_obj_set_style_border_color(pill_tou_, lv_color_hex(0x10b981), 0);
    lv_obj_set_style_border_width(pill_tou_, 1, 0);
    lv_obj_set_style_radius(pill_tou_, 12, 0);
#if defined(POWERWALL_UI_480)
    flattenDashboardPill(pill_tou_, 0x10b981);
#endif
    lv_obj_set_style_pad_hor(pill_tou_, 10, 0);
    lv_obj_set_style_pad_top(pill_tou_, 2, 0);
    lv_obj_set_style_pad_bottom(pill_tou_, 0, 0);
    lv_obj_set_flex_flow(pill_tou_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pill_tou_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(pill_tou_, LV_OBJ_FLAG_SCROLLABLE);

    lbl_tou_pill_ = lv_label_create(pill_tou_);
    lv_obj_set_style_text_font(lbl_tou_pill_, &font_cjk_12, 0);
    lv_obj_set_style_text_color(lbl_tou_pill_, lv_color_hex(0x6ee7b7), 0);
    lv_label_set_text(lbl_tou_pill_, "時間電價載入中");

    lv_obj_t* hdiv = lv_obj_create(chart_card_);
    lv_obj_set_pos(hdiv, 14, 46);
    lv_obj_set_size(hdiv, 676, 1);
    lv_obj_set_style_bg_color(hdiv, lv_color_hex(0x27272a), 0);
    lv_obj_set_style_border_width(hdiv, 0, 0);

    auto addYLabel = [&](const char* txt, int y, uint32_t col) {
      lv_obj_t* l = lv_label_create(chart_card_);
      lv_obj_set_style_text_font(l, &font_cjk_12, 0);
      lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
      lv_label_set_text(l, txt);
      lv_obj_set_width(l, 56);
      lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_RIGHT, 0);
      lv_obj_set_pos(l, 4, y);
    };
    addYLabel("住家 kW", 56, 0x38bdf8);
    addYLabel("3 kW",    92, 0x71717a);
    addYLabel("0 kW",   125, 0x71717a);

    addYLabel("PW 100%", 140, 0x00e676);
    addYLabel("50%",     168, 0x71717a);
    addYLabel("24%",     185, 0x38bdf8);
    addYLabel("0%",      198, 0x71717a);

    chart1_buf_ = allocPixelBuf(kChartW * kChart1H);
    chart2_buf_ = allocPixelBuf(kChart2W * kChart2H);

    if (chart1_buf_) {
      chart1_canvas_ = lv_canvas_create(chart_card_);
      lv_canvas_set_buffer(chart1_canvas_, chart1_buf_, kChartW, kChart1H, LV_IMG_CF_TRUE_COLOR);
      lv_obj_set_pos(chart1_canvas_, 66, 56);
    }
    if (chart2_buf_) {
      chart2_canvas_ = lv_canvas_create(chart_card_);
      lv_canvas_set_buffer(chart2_canvas_, chart2_buf_, kChart2W, kChart2H, LV_IMG_CF_TRUE_COLOR);
      lv_obj_set_pos(chart2_canvas_, 66, 140);
    }

    lbl_tou_track_ = lv_label_create(chart_card_);
    lv_obj_set_style_text_font(lbl_tou_track_, &font_cjk_12, 0);
    lv_obj_set_style_text_color(lbl_tou_track_, lv_color_hex(0x34d399), 0);
    lv_label_set_text(lbl_tou_track_, "週間費率");
    lv_obj_set_width(lbl_tou_track_, 56);
    lv_obj_set_style_text_align(lbl_tou_track_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(lbl_tou_track_, 4, 216);

    tou_bar_we_ = lv_obj_create(chart_card_);
    lv_obj_set_pos(tou_bar_we_, 66, 218);
    lv_obj_set_size(tou_bar_we_, kChartW, 6);
    lv_obj_set_style_bg_color(tou_bar_we_, lv_color_hex(0x26a64c), 0);
    lv_obj_set_style_border_width(tou_bar_we_, 0, 0);
    lv_obj_set_style_radius(tou_bar_we_, 5, 0);
    lv_obj_add_flag(tou_bar_we_, LV_OBJ_FLAG_HIDDEN);

    tou_bar_wk_ = lv_obj_create(chart_card_);
    lv_obj_set_pos(tou_bar_wk_, 66, 215);
    lv_obj_set_size(tou_bar_wk_, kChartW, 12);
    lv_obj_set_style_bg_opa(tou_bar_wk_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tou_bar_wk_, 0, 0);
    lv_obj_set_style_pad_all(tou_bar_wk_, 0, 0);
    lv_obj_clear_flag(tou_bar_wk_, LV_OBJ_FLAG_SCROLLABLE);

    tou_bar_wk_summer_ = lv_obj_create(chart_card_);
    lv_obj_set_pos(tou_bar_wk_summer_, 66, 215);
    lv_obj_set_size(tou_bar_wk_summer_, kChartW, 12);
    lv_obj_set_style_bg_opa(tou_bar_wk_summer_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tou_bar_wk_summer_, 0, 0);
    lv_obj_set_style_pad_all(tou_bar_wk_summer_, 0, 0);
    lv_obj_clear_flag(tou_bar_wk_summer_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(tou_bar_wk_summer_, LV_OBJ_FLAG_HIDDEN);

    auto makeSegOn = [&](lv_obj_t* parent, int x, int w, int h_seg, uint32_t col) {
      lv_obj_t* s = lv_obj_create(parent);
      lv_obj_set_pos(s, x, (12 - h_seg) / 2);
      lv_obj_set_size(s, w, h_seg);
      lv_obj_set_style_bg_color(s, lv_color_hex(col), 0);
      lv_obj_set_style_border_width(s, 0, 0);
      lv_obj_set_style_radius(s, 3, 0);
    };
    const int w0_6   = static_cast<int>(roundf(kChartW * 0.25f));
    const int w6_11  = static_cast<int>(roundf(kChartW * 0.2083f));
    const int w11_14 = static_cast<int>(roundf(kChartW * 0.125f));
    const int w14_24 = kChartW - w0_6 - w6_11 - w11_14;
    makeSegOn(tou_bar_wk_, 0, w0_6, 6, 0x26a64c);
    makeSegOn(tou_bar_wk_, w0_6, w6_11, 11, 0xb9312f);
    makeSegOn(tou_bar_wk_, w0_6 + w6_11, w11_14, 6, 0x26a64c);
    makeSegOn(tou_bar_wk_, w0_6 + w6_11 + w11_14, w14_24, 11, 0xb9312f);

    const int w0_9  = static_cast<int>(roundf(kChartW * (9.0f / 24.0f)));
    const int w9_24 = kChartW - w0_9;
    makeSegOn(tou_bar_wk_summer_, 0, w0_9, 6, 0x26a64c);
    makeSegOn(tou_bar_wk_summer_, w0_9, w9_24, 11, 0xb9312f);

    static const char* kTicks[6] = {"00:00", "06:00", "11:00", "14:00", "18:00", "24:00"};
    static const float kTickHrs[6] = {0.0f, 6.0f, 11.0f, 14.0f, 18.0f, 24.0f};
    for (int i = 0; i < 6; ++i) {
      lv_obj_t* tl = lv_label_create(chart_card_);
      lv_obj_set_style_text_font(tl, &font_cjk_12, 0);
      lv_obj_set_style_text_color(tl, lv_color_hex(0xa1a1aa), 0);
      lv_label_set_text(tl, kTicks[i]);
      int tx = 66 + static_cast<int>(roundf((kTickHrs[i] / 24.0f) * (kChartW - 32)));
      lv_obj_set_pos(tl, tx, 232);
    }

    cursor_line_ = lv_obj_create(chart_card_);
    lv_obj_set_size(cursor_line_, 2, 172);
    lv_obj_set_pos(cursor_line_, 66, 56);
    lv_obj_set_style_bg_color(cursor_line_, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(cursor_line_, LV_OPA_90, 0);
    lv_obj_set_style_border_width(cursor_line_, 0, 0);
    lv_obj_set_style_radius(cursor_line_, 0, 0);
    lv_obj_clear_flag(cursor_line_, LV_OBJ_FLAG_SCROLLABLE);

    auto createCurrentPoint = [&](uint32_t color) {
      lv_obj_t* dot = lv_obj_create(chart_card_);
      lv_obj_set_size(dot, 9, 9);
      lv_obj_set_style_radius(dot, 5, 0);
      lv_obj_set_style_bg_color(dot, lv_color_hex(color), 0);
      lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
      lv_obj_set_style_border_color(dot, lv_color_hex(0xffffff), 0);
      lv_obj_set_style_border_width(dot, 2, 0);
      lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
      return dot;
    };
    chart1_now_dot_ = createCurrentPoint(0x38bdf8);
    chart2_now_dot_ = createCurrentPoint(0x11e58e);

#if defined(POWERWALL_UI_480)
    initializeHome480();
#endif
    initializeAddedPages(scr);

    renderAreaChartToRgb565(chart1_buf_, kChartW, kChart1H, nullptr, 0, false, 6.0f, 56, 189, 248, 0.0f);
    renderAreaChartToRgb565(chart2_buf_, kChart2W, kChart2H, nullptr, 0, true, 100.0f, 0, 230, 118, 24.0f);

    render3DStageFrame();
    registerPageTapRouting(scr);
    createPageTransitionOverlay(scr);
    stage_timer_ = lv_timer_create([](lv_timer_t* t) {
      UI720x720* self = static_cast<UI720x720*>(t->user_data);
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

  // Touch and PWRKEY share this route; settings returns to the page that opened it.
  void nextDashboardPage() {
    if (active_dashboard_page_ >= kNormalDashboardPageCount) {
      restart_armed_ = false;
      if (restart_button_label_) lv_label_set_text(restart_button_label_, "RESTART");
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
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
  void setGatewayConfigIp(const char* ip) {
    const String next_ip = ip && ip[0] ? ip : "--";
    if (settings_gateway_config_ip_ == next_ip) return;
    settings_gateway_config_ip_ = next_ip;
    refreshSettingsLabels();
  }
#endif
#if !defined(ESP32)
  uint8_t simulatorDashboardPage() const { return static_cast<uint8_t>(active_dashboard_page_); }
#if defined(POWERWALL_SIM_WINDOW_SIZE) && POWERWALL_SIM_WINDOW_SIZE == 480
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
    if (active_dashboard_page_ != 3 || !page_week_ || !page_header_ ||
        lv_obj_has_flag(page_week_, LV_OBJ_FLAG_HIDDEN) ||
        strcmp(lv_label_get_text(page_title_), "7 日用電與電量總表")) return false;
#if defined(POWERWALL_UI_480)
    if (!lv_obj_has_flag(page_header_, LV_OBJ_FLAG_HIDDEN)) return false;
#else
    if (lv_obj_has_flag(page_header_, LV_OBJ_FLAG_HIDDEN)) return false;
#endif
    for (int d = 0; d < 7; ++d) {
      if (!week_rows_[d] || lv_obj_has_flag(week_rows_[d], LV_OBJ_FLAG_HIDDEN) ||
          !lv_label_get_text(week_dates_[d])[0]) return false;
#if defined(POWERWALL_UI_480)
      if (lv_obj_get_y(week_rows_[d]) != d * 720 / 7 ||
          lv_obj_get_height(week_rows_[d]) != (d + 1) * 720 / 7 - d * 720 / 7) return false;
#endif
    }
#if defined(POWERWALL_UI_480)
    return true;
#else
    return lv_obj_get_index(page_header_) > lv_obj_get_index(page_week_);
#endif
  }
  bool simulatorSettingsNetworkPageIsVisible() const {
    return active_dashboard_page_ == 4 && settings_network_page_ &&
      !lv_obj_has_flag(settings_network_page_, LV_OBJ_FLAG_HIDDEN);
  }
#if defined(POWERWALL_SIM_WINDOW_SIZE) && POWERWALL_SIM_WINDOW_SIZE == 480
  bool simulatorVoiceEnabled() const { return settings_voice_enabled_; }
  bool simulatorSettingsUptimeLabelIsVisible() const {
    return simulatorSettingsNetworkPageIsVisible() && settings_uptime_label_ &&
      !lv_obj_has_flag(settings_uptime_label_, LV_OBJ_FLAG_HIDDEN) &&
      strncmp(lv_label_get_text(settings_uptime_label_), "Uptime: ", 8) == 0;
  }
#endif
  void setSimulatorNetworkAvailable(bool available) {
    simulator_network_available_ = available;
    refreshSettingsLabels();
  }
  bool simulatorNetworkPreviewMatches(bool available) const {
    return settings_sim_ip_title_ && settings_sim_ip_value_ && settings_http_title_ &&
      settings_gateway_url_label_ &&
      strcmp(lv_label_get_text(settings_sim_ip_title_), available ? "模擬 ESP32 IP" : "模擬 AP SSID") == 0 &&
      strcmp(lv_label_get_text(settings_sim_ip_value_), available ? "192.0.2.15" : "powerwall-dashboard") == 0 &&
      strcmp(lv_label_get_text(settings_http_title_), available ? "ESP32 HTTP URL" : "AP HTTP URL") == 0 &&
      strcmp(lv_label_get_text(settings_gateway_url_label_),
             available ? "http://192.0.2.15/" : "http://192.168.4.1/") == 0;
  }
  bool simulatorAddedHeaderIsVisible() const {
    if (active_dashboard_page_ == 1) {
      const bool live = (state_mode_idx_ == 0 || state_mode_idx_ == 1) &&
                        last_configured_ && last_telemetry_.ok &&
                        !last_telemetry_.stale && !last_telemetry_.demo;
      const lv_color_t expectedStatus = lv_color_hex(live ? 0x34d399 : 0xff5252);
      return page_header_ && pw_header_title_ && pw_header_clock_ && pw_header_date_ &&
        pw_header_status_dot_ && page_site_ && page_status_pill_ && page_clock_ && page_date_ &&
        !lv_obj_has_flag(page_header_, LV_OBJ_FLAG_HIDDEN) &&
        !lv_obj_has_flag(pw_header_title_, LV_OBJ_FLAG_HIDDEN) &&
        !lv_obj_has_flag(pw_header_clock_, LV_OBJ_FLAG_HIDDEN) &&
        !lv_obj_has_flag(pw_header_date_, LV_OBJ_FLAG_HIDDEN) &&
        !lv_obj_has_flag(pw_header_status_dot_, LV_OBJ_FLAG_HIDDEN) &&
        strcmp(lv_label_get_text(pw_header_title_), "POWERWALL") == 0 &&
        lv_obj_has_flag(page_site_, LV_OBJ_FLAG_HIDDEN) &&
        lv_obj_has_flag(page_status_pill_, LV_OBJ_FLAG_HIDDEN) &&
        lv_obj_has_flag(page_clock_, LV_OBJ_FLAG_HIDDEN) &&
        lv_obj_has_flag(page_date_, LV_OBJ_FLAG_HIDDEN) &&
        lv_obj_get_style_bg_color(pw_header_status_dot_, 0).full == expectedStatus.full &&
        pw_chart_card_ && lv_obj_get_x(pw_chart_card_) == 14 && lv_obj_get_y(pw_chart_card_) == kPwChartCardY &&
        lv_obj_get_height(pw_chart_card_) == kPwChartCardH &&
        pw_chart_canvas_ && lv_obj_get_x(pw_chart_canvas_) == kPwChartPlotX &&
        lv_obj_get_y(pw_chart_canvas_) == kPwChartPlotY && lv_obj_get_height(pw_chart_canvas_) == kDashboardChartH &&
        pw_now_line_ && lv_obj_get_y(pw_now_line_) == kPwChartPlotY &&
        lv_obj_get_height(pw_now_line_) == kPwChartCursorH &&
        pw_tou_segments_[0] && lv_obj_get_y(pw_tou_segments_[0]) == kPwChartBandY &&
        lv_obj_get_height(pw_tou_segments_[0]) == kPwChartTouH &&
        pw_time_ticks_[0] && strcmp(lv_label_get_text(pw_time_ticks_[0]), "00") == 0 &&
        pw_time_ticks_[1] && strcmp(lv_label_get_text(pw_time_ticks_[1]), "06") == 0 &&
        pw_time_ticks_[2] && strcmp(lv_label_get_text(pw_time_ticks_[2]), "12") == 0 &&
        pw_time_ticks_[3] && strcmp(lv_label_get_text(pw_time_ticks_[3]), "18") == 0 &&
        pw_time_ticks_[4] && strcmp(lv_label_get_text(pw_time_ticks_[4]), "24") == 0 &&
        lv_obj_get_y(pw_time_ticks_[0]) == kPwChartTimeY && !pw_data_stamp_;
    }
    return active_dashboard_page_ == 2 && page_header_ && page_home_grid_ &&
      !lv_obj_has_flag(page_header_, LV_OBJ_FLAG_HIDDEN) &&
      !lv_obj_has_flag(page_site_, LV_OBJ_FLAG_HIDDEN) &&
      !lv_obj_has_flag(page_status_pill_, LV_OBJ_FLAG_HIDDEN) &&
      !lv_obj_has_flag(page_clock_, LV_OBJ_FLAG_HIDDEN) &&
      !lv_obj_has_flag(page_date_, LV_OBJ_FLAG_HIDDEN) &&
      lv_obj_has_flag(pw_header_title_, LV_OBJ_FLAG_HIDDEN) &&
      lv_obj_has_flag(pw_header_clock_, LV_OBJ_FLAG_HIDDEN) &&
      lv_obj_has_flag(pw_header_date_, LV_OBJ_FLAG_HIDDEN) &&
      lv_obj_has_flag(pw_header_status_dot_, LV_OBJ_FLAG_HIDDEN) &&
      lv_obj_get_index(page_header_) > lv_obj_get_index(page_home_grid_);
  }
  lv_opa_t simulatorPowerwallStatusDotOpacity() const {
    return pw_header_status_dot_ ? lv_obj_get_style_bg_opa(pw_header_status_dot_, 0) : LV_OPA_TRANSP;
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
#if defined(POWERWALL_UI_480)
  bool simulatorHome480Valid(bool live) const {
    if (!home480_clock_ || !home480_reserve_ ||
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
      if (bounds.x1 < 0 || bounds.x2 >= 720 || bounds.y1 < 0 || bounds.y2 >= kHome480ChartTop) {
        fprintf(stderr, "[Home480] bounds: %d,%d..%d,%d\n", bounds.x1, bounds.y1, bounds.x2, bounds.y2); return false;
      }
    }
    lv_area_t reserve_bounds, chart_bounds;
    lv_obj_get_coords(home480_reserve_, &reserve_bounds);
    lv_obj_get_coords(chart2_canvas_, &chart_bounds);
    if (last_telemetry_.history_count > 0) {
      lv_area_t dot_bounds; lv_obj_get_coords(chart2_now_dot_, &dot_bounds);
      if (lv_obj_has_flag(chart2_now_dot_, LV_OBJ_FLAG_HIDDEN) ||
          dot_bounds.y1 < chart_bounds.y1 - 1 || dot_bounds.y2 > chart_bounds.y2 + 4) {
        fprintf(stderr, "[Home480] current point outside plot: %d..%d (plot %d..%d)\n",
                dot_bounds.y1, dot_bounds.y2, chart_bounds.y1, chart_bounds.y2);
        return false;
      }
    }
    if (lv_obj_get_x(chart_card_) != 0 || lv_obj_get_width(chart_card_) != 720 ||
        chart_bounds.x1 != 66 || chart_bounds.x2 != 707) return false;
    for (auto* band : {tou_bar_we_, tou_bar_wk_, tou_bar_wk_summer_}) {
      if (!lv_obj_is_visible(band)) continue;
      lv_area_t bounds; lv_obj_get_coords(band, &bounds);
      if (bounds.x1 != chart_bounds.x1 || bounds.x2 != 707) return false;
    }
    for (auto* marker : {cursor_line_, chart2_now_dot_}) {
      if (!lv_obj_is_visible(marker)) continue;
      lv_area_t bounds; lv_obj_get_coords(marker, &bounds);
      if (bounds.x1 < 0 || bounds.x2 > chart_bounds.x2) return false;
    }
    if (reserve_bounds.x1 < chart_bounds.x1 || reserve_bounds.x2 > chart_bounds.x2 ||
        reserve_bounds.y1 < chart_bounds.y1 || reserve_bounds.y2 > chart_bounds.y2 ||
        strncmp(lv_label_get_text(home480_reserve_), "Reserve ", 8) != 0) return false;
    if (lv_obj_get_y(pw_val_row_) != 485 || lv_obj_get_y(lbl_grid_val_) != 513 ||
        lv_obj_get_y(lbl_stat_pw_eta_) != 530 || lv_obj_get_y(stage_canvas_) != 53 ||
        lv_obj_get_x(stage_canvas_) != 32 || lv_img_get_zoom(stage_canvas_) != 256 ||
        lv_obj_get_y(lbl_solar_val_) != 70 || lv_obj_get_y(lbl_home_val_) != 190) return false;
    for (auto* axis : {home480_axis_top_, home480_axis_bottom_}) {
      lv_area_t bounds; lv_obj_get_coords(axis, &bounds);
      if (bounds.x2 >= chart_bounds.x1) return false;
      const int center_y = (bounds.y1 + bounds.y2 + 1) / 2;
      const int expected_y = chart_bounds.y1 + (axis == home480_axis_top_ ? 3 : kChart2H - 2);
      if (abs(center_y - expected_y) > 1) return false;
    }
    for (auto* tick : home480_ticks_) {
      lv_area_t bounds; lv_obj_get_coords(tick, &bounds);
      if (bounds.x1 < 0 || bounds.x2 >= 720 || bounds.y2 >= 720) return false;
    }
    const auto* clock = lv_label_get_text(home480_clock_);
    return strlen(clock) == 5 && clock[2] == ':' &&
           lv_obj_get_style_bg_opa(dot_status_, 0) != LV_OPA_TRANSP;
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
#endif
  bool simulatorTapPoint(uint8_t target, int16_t& x, int16_t& y) const {
    if (target == 0) {
      x = 360;
      y = 360;
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

private:
  // 0="live", 1="live_ip", 2="solar", 3="charge", 4="peak", 5="low", 6="offgrid"
  int state_mode_idx_ = 0;
  // 0="auto", 1="clear", 2="partly", 3="overcast", 4="rain", 5="storm", 6="snow", 7="wind", 8="sunrise", 9="sunset", 10="night"
  int weather_mode_idx_ = 0;

  enum class PageTransitionPhase : uint8_t { Idle = 0, Covering = 1, Uncovering = 2 };
  static constexpr uint16_t kPageTransitionCoverMs = 80;
  static constexpr uint16_t kPageTransitionUncoverMs = 120;
  PageTransitionPhase page_transition_phase_ = PageTransitionPhase::Idle;
  lv_obj_t* page_transition_overlay_ = nullptr;
  lv_opa_t page_transition_opa_ = LV_OPA_TRANSP;
  uint8_t page_transition_target_ = 0;
  uint32_t page_transition_consumed_clicks_ = 0;
  lv_obj_t* page_settings_ = nullptr;
  lv_obj_t* settings_controls_page_ = nullptr;
  lv_obj_t* settings_network_page_ = nullptr;
  lv_obj_t* settings_voice_button_label_ = nullptr;
  lv_obj_t* settings_voice_switch_ = nullptr;
  lv_obj_t* settings_sim_ip_title_ = nullptr;
  lv_obj_t* settings_sim_ip_value_ = nullptr;
  lv_obj_t* settings_http_title_ = nullptr;
  lv_obj_t* settings_gateway_ip_label_ = nullptr;
  lv_obj_t* settings_gateway_url_label_ = nullptr;
#if defined(POWERWALL_UI_480)
  lv_obj_t* settings_uptime_label_ = nullptr;
  bool settings_uptime_initialized_ = false;
  uint64_t settings_uptime_last_seconds_ = 0;
#if !defined(ESP32)
  const std::chrono::steady_clock::time_point simulator_started_at_ =
    std::chrono::steady_clock::now();
#endif
#endif
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
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
  int active_dashboard_page_ = 0; // 0=original, 1=Powerwall, 2=home/grid
  static constexpr uint8_t kNormalDashboardPageCount = 4;
  uint8_t previous_normal_page_ = 0;
  lv_obj_t* dashboard_screen_ = nullptr;
  lv_obj_t* page_powerwall_ = nullptr;
  lv_obj_t* page_home_grid_ = nullptr;
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
  lv_obj_t* pw_state_pill_ = nullptr;
  lv_obj_t* pw_state_text_ = nullptr;
  lv_obj_t* pw_eta_text_ = nullptr;
  lv_obj_t* pw_mode_text_ = nullptr;
  lv_obj_t* pw_self_value_ = nullptr;
  lv_obj_t* pw_voltage_value_ = nullptr;
  lv_obj_t* pw_capacity_current_value_ = nullptr;
  lv_obj_t* pw_capacity_current_unit_ = nullptr;
  lv_obj_t* pw_capacity_total_value_ = nullptr;
  lv_obj_t* pw_now_line_ = nullptr;
  lv_obj_t* pw_now_dot_ = nullptr;
  lv_obj_t* pw_tou_segments_[4] = {};
  lv_obj_t* pw_time_ticks_[5] = {};
  lv_obj_t* pw_reserve_chart_label_ = nullptr;
  lv_obj_t* pw_data_stamp_ = nullptr;

  lv_obj_t* home_tou_pill_ = nullptr;
  lv_obj_t* home_tou_pill_text_ = nullptr;
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
  lv_obj_t* home_data_stamp_ = nullptr;
  lv_obj_t* home_chart_tou_segments_[4] = {};

  unsigned long last_added_page_clock_ms_ = 0;
  int cached_tou_year_ = -1;
  int cached_tou_yday_ = -1;

#if defined(POWERWALL_UI_480)
#include "home_480_ui.inc"
#endif

  static lv_obj_t* makeDashboardCard(lv_obj_t* parent, int x, int y, int w, int h) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
#if defined(POWERWALL_UI_480)
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
#else
    lv_obj_set_style_bg_color(card, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x263746), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 14, 0);
#endif
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
    lv_label_set_text(label, text ? text : "—");
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

  // Reused by both daily charts and the host seven-day overview.
  struct DashboardTimeAxis {
    lv_obj_t* labels[8] = {};
    lv_obj_t* marks[8] = {};
    const lv_font_t* font = nullptr;
    int left = 0, width = 0, y = 0;
  };
  DashboardTimeAxis home_time_axis_;

  static void addTouAxisBoundaries(const struct tm& date, bool (&boundaries)[25]) {
    if (isTaiwanTouWeekend(date)) return;
    for (int hour = 1; hour < 24; ++hour) {
      if (isTaiwanTouWeekdayPeak(date.tm_mon + 1, hour - 0.5f) !=
          isTaiwanTouWeekdayPeak(date.tm_mon + 1, hour)) boundaries[hour] = true;
    }
  }

  static void initializeTimeAxis(DashboardTimeAxis& axis, lv_obj_t* parent,
                                 int left, int width, int y, const lv_font_t* font) {
    axis.left = left; axis.width = width; axis.y = y; axis.font = font;
    for (int i = 0; i < 8; ++i) {
      axis.labels[i] = makeDashboardLabel(parent, "", left, y, 1, font, 0x94a3b8);
      lv_label_set_long_mode(axis.labels[i], LV_LABEL_LONG_CLIP);
      axis.marks[i] = makeDashboardRule(parent, left, y - 2, 1, 2);
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
        left[hour] = hour == 0 ? x : hour == 24 ? x - size.x : x - size.x / 2;
        bool fits = true;
        for (int other = 0; other <= 24; ++other) {
          if (selected[other] && left[hour] < left[other] + width[other] + 6 &&
              left[other] < left[hour] + width[hour] + 6) fits = false;
        }
        if (required || fits) selected[hour] = true;
      }
    }
    int slot = 0;
    for (int hour = 0; hour <= 24 && slot < 8; ++hour) {
      if (!selected[hour]) continue;
      char text[8]; snprintf(text, sizeof(text), "%02d:00", hour);
      auto* label = axis.labels[slot];
      lv_label_set_text(label, text);
      lv_obj_set_width(label, width[hour]);
      lv_obj_set_pos(label, left[hour], axis.y);
      lv_obj_set_style_text_align(label, hour == 24 ? LV_TEXT_ALIGN_RIGHT :
          hour == 0 ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_style_text_color(label, lv_color_hex(boundaries[hour] ? 0xcbd5e1 : 0x94a3b8), 0);
      lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
      if (boundaries[hour]) {
        const int x = axis.left + static_cast<int>(roundf(hour * axis.width / 24.0f));
        lv_obj_set_pos(axis.marks[slot], x, axis.y - 2);
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
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B) || !defined(ESP32)
    return 5;
#else
    return 4;
#endif
  }
#include "weekly_history_ui.inc"
  void refreshSettingsLabels() {
    char value[8];
    if (settings_brightness_value_) {
      snprintf(value, sizeof(value), "%u%%", static_cast<unsigned>(settings_brightness_));
      lv_label_set_text(settings_brightness_value_, value);
    }
    if (settings_volume_value_) {
      snprintf(value, sizeof(value), "%u%%", static_cast<unsigned>(settings_volume_));
      lv_label_set_text(settings_volume_value_, value);
    }
#if defined(POWERWALL_UI_480)
    if (settings_voice_button_label_) {
      lv_label_set_text(settings_voice_button_label_, settings_voice_enabled_ ? "ON" : "OFF");
      lv_obj_set_style_text_color(settings_voice_button_label_,
          lv_color_hex(settings_voice_enabled_ ? 0x34d399 : 0xf87171), 0);
    }
    if (settings_voice_switch_) {
      if (settings_voice_enabled_) lv_obj_add_state(settings_voice_switch_, LV_STATE_CHECKED);
      else lv_obj_clear_state(settings_voice_switch_, LV_STATE_CHECKED);
    }
#else
    if (settings_voice_button_label_)
      lv_label_set_text(settings_voice_button_label_, settings_voice_enabled_ ? "語音播報開啟" : "語音播報關閉");
#endif
    if (settings_gateway_ip_label_ || settings_gateway_url_label_) {
      const char* ip = last_telemetry_.gateway_ip.length() ? last_telemetry_.gateway_ip.c_str() : "--";
      char url[96];
#if defined(POWERWALL_SIM_WINDOW_SIZE) && POWERWALL_SIM_WINDOW_SIZE == 480
      snprintf(url, sizeof(url), "http://%s/", simulator_network_available_ ? "192.0.2.15" : "192.168.4.1");
#elif defined(POWERWALL_WAVESHARE_S3_LCD_4B)
      const bool connected = WiFi.status() == WL_CONNECTED;
      const String localIp = connected ? WiFi.localIP().toString() : String();
      snprintf(url, sizeof(url), "http://%s/", connected ? localIp.c_str() : "192.168.4.1");
      ip = settings_gateway_config_ip_.c_str();
#else
      snprintf(url, sizeof(url), "http://%s/", ip);
#endif
      if (settings_gateway_ip_label_) lv_label_set_text(settings_gateway_ip_label_, ip);
      if (settings_gateway_url_label_) lv_label_set_text(settings_gateway_url_label_, url);
    }
#if defined(POWERWALL_SIM_WINDOW_SIZE) && POWERWALL_SIM_WINDOW_SIZE == 480
    if (settings_sim_ip_title_)
      lv_label_set_text(settings_sim_ip_title_, simulator_network_available_ ? "模擬 ESP32 IP" : "模擬 AP SSID");
    if (settings_sim_ip_value_)
      lv_label_set_text(settings_sim_ip_value_, simulator_network_available_ ? "192.0.2.15" : "powerwall-dashboard");
    if (settings_http_title_)
      lv_label_set_text(settings_http_title_, simulator_network_available_ ? "ESP32 HTTP URL" : "AP HTTP URL");
#elif defined(POWERWALL_WAVESHARE_S3_LCD_4B)
    const bool connected = WiFi.status() == WL_CONNECTED;
    const String localIp = connected ? WiFi.localIP().toString() : String();
    if (settings_sim_ip_title_) lv_label_set_text(settings_sim_ip_title_, connected ? "ESP32 IP" : "AP SSID");
    if (settings_sim_ip_value_) lv_label_set_text(settings_sim_ip_value_, connected ? localIp.c_str() : "powerwall-dashboard");
    if (settings_http_title_) lv_label_set_text(settings_http_title_, connected ? "ESP32 HTTP URL" : "AP HTTP URL");
#endif
  }

#if defined(POWERWALL_UI_480)
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
    lv_label_set_text(settings_uptime_label_, text);
  }
#endif

#if defined(POWERWALL_UI_480)
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
    lv_obj_set_pos(label, x, y);
  }

  static lv_obj_t* makeCenteredSettingsButtonLabel(lv_obj_t* button, const char* text,
                                                   const lv_font_t* font, uint32_t color) {
    lv_obj_t* label = makeDashboardLabel(button, text, 0, 0, 0, font, color, LV_TEXT_ALIGN_LEFT);
    centerSettingsButtonLabel(button, label);
    return label;
  }

  void showSettingsSubpage(bool network) {
    settings_network_page_active_ = network;
    if (settings_controls_page_) {
      if (network) lv_obj_add_flag(settings_controls_page_, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_clear_flag(settings_controls_page_, LV_OBJ_FLAG_HIDDEN);
    }
    if (settings_network_page_) {
      if (network) lv_obj_clear_flag(settings_network_page_, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(settings_network_page_, LV_OBJ_FLAG_HIDDEN);
    }
  }

  void initializeSettingsPage480() {
    makeDashboardLabel(page_settings_, "裝置設定", 0, 18, 720,
                      &font_dashboard_cjk_48, 0xf1f5f9, LV_TEXT_ALIGN_CENTER);
    auto makeTab = [&](const char* text, int x, bool network) {
      lv_obj_t* button = lv_btn_create(page_settings_);
      lv_obj_set_pos(button, x, 96); lv_obj_set_size(button, 300, 70);
      lv_obj_set_style_bg_color(button, lv_color_hex(network ? 0x075985 : 0x047857), 0);
      lv_obj_set_style_border_width(button, 0, 0); lv_obj_set_style_radius(button, 18, 0);
      lv_obj_set_style_pad_all(button, 0, 0);
      makeCenteredSettingsButtonLabel(button, text, &font_dashboard_cjk_48, 0xffffff);
      lv_obj_add_event_cb(button, [](lv_event_t* event) {
        auto* self = static_cast<UI720x720*>(lv_event_get_user_data(event));
        const bool network = lv_obj_get_user_data(lv_event_get_target(event)) != nullptr;
        if (self && lv_event_get_code(event) == LV_EVENT_CLICKED)
          self->showSettingsSubpage(network);
        lv_event_stop_bubbling(event);
      }, LV_EVENT_CLICKED, this);
      // The tab identity is stored independently to keep the callback C API simple.
      lv_obj_set_user_data(button, reinterpret_cast<void*>(static_cast<uintptr_t>(network ? 1 : 0)));
      return button;
    };
    (void)makeTab("控制", 40, false);
    (void)makeTab("網路", 380, true);

    settings_controls_page_ = lv_obj_create(page_settings_);
    settings_network_page_ = lv_obj_create(page_settings_);
    lv_obj_t* subpages[] = {settings_controls_page_, settings_network_page_};
    for (auto* page : subpages) {
      lv_obj_set_pos(page, 0, 178); lv_obj_set_size(page, 720, 542);
      lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_width(page, 0, 0); lv_obj_set_style_radius(page, 0, 0);
      lv_obj_set_style_pad_all(page, 0, 0); lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    }
    lv_obj_add_flag(settings_network_page_, LV_OBJ_FLAG_HIDDEN);

    makeDashboardLabel(settings_controls_page_, "顯示亮度", 28, 8, 420,
                       &font_dashboard_cjk_48, 0xf1f5f9);
    settings_brightness_value_ = makeDashboardLabel(settings_controls_page_, "80%", 500, 8, 180,
                       &lv_font_montserrat_48, 0x34d399, LV_TEXT_ALIGN_RIGHT);
    settings_brightness_slider_ = lv_slider_create(settings_controls_page_);
    lv_obj_set_pos(settings_brightness_slider_, 28, 68); lv_obj_set_size(settings_brightness_slider_, 664, 30);
    lv_slider_set_range(settings_brightness_slider_, 0, 100);
    lv_obj_set_style_bg_color(settings_brightness_slider_, lv_color_hex(0x263746), LV_PART_MAIN);
    lv_obj_set_style_bg_color(settings_brightness_slider_, lv_color_hex(0x10b981), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(settings_brightness_slider_, lv_color_hex(0xe2e8f0), LV_PART_KNOB);
    lv_obj_set_style_pad_all(settings_brightness_slider_, 16, LV_PART_KNOB);
    lv_obj_add_event_cb(settings_brightness_slider_, [](lv_event_t* event) {
      auto* self = static_cast<UI720x720*>(lv_event_get_user_data(event));
      if (!self) return;
      if (lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED && !self->settings_updating_) {
        self->settings_brightness_ = static_cast<uint8_t>(lv_slider_get_value(self->settings_brightness_slider_));
        self->refreshSettingsLabels();
        if (self->brightness_changed_) self->brightness_changed_(self->settings_context_, self->settings_brightness_);
      }
      lv_event_stop_bubbling(event);
    }, LV_EVENT_ALL, this);

    makeDashboardLabel(settings_controls_page_, "音量", 28, 124, 280,
                       &font_dashboard_cjk_48, 0xf1f5f9);
    settings_volume_value_ = makeDashboardLabel(settings_controls_page_, "85%", 500, 124, 180,
                       &lv_font_montserrat_48, 0x38bdf8, LV_TEXT_ALIGN_RIGHT);
    settings_volume_slider_ = lv_slider_create(settings_controls_page_);
    lv_obj_set_pos(settings_volume_slider_, 28, 184); lv_obj_set_size(settings_volume_slider_, 664, 30);
    lv_slider_set_range(settings_volume_slider_, 0, 100);
    lv_obj_set_style_bg_color(settings_volume_slider_, lv_color_hex(0x263746), LV_PART_MAIN);
    lv_obj_set_style_bg_color(settings_volume_slider_, lv_color_hex(0x0ea5e9), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(settings_volume_slider_, lv_color_hex(0xe2e8f0), LV_PART_KNOB);
    lv_obj_set_style_pad_all(settings_volume_slider_, 16, LV_PART_KNOB);
    lv_obj_add_event_cb(settings_volume_slider_, [](lv_event_t* event) {
      auto* self = static_cast<UI720x720*>(lv_event_get_user_data(event));
      if (!self) return;
      if (lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED && !self->settings_updating_) {
        self->settings_volume_ = static_cast<uint8_t>(lv_slider_get_value(self->settings_volume_slider_));
        self->refreshSettingsLabels();
        if (self->volume_changed_) self->volume_changed_(self->settings_context_, self->settings_volume_);
      }
      lv_event_stop_bubbling(event);
    }, LV_EVENT_ALL, this);

    auto makeActionButton = [&](lv_obj_t* parent, const char* text, int y, uint32_t color) {
      lv_obj_t* button = lv_btn_create(parent);
      lv_obj_set_pos(button, 28, y); lv_obj_set_size(button, 664, 72);
      lv_obj_set_style_bg_color(button, lv_color_hex(color), 0);
      lv_obj_set_style_border_width(button, 0, 0); lv_obj_set_style_radius(button, 18, 0);
      lv_obj_set_style_pad_all(button, 0, 0);
      lv_obj_t* label = makeCenteredSettingsButtonLabel(button, text,
                       &font_dashboard_cjk_48, 0xffffff);
      return std::pair<lv_obj_t*, lv_obj_t*>(button, label);
    };
    auto test = makeActionButton(settings_controls_page_, "播放語音測試", 236, 0x075985);
    lv_obj_add_event_cb(test.first, [](lv_event_t* event) {
      auto* self = static_cast<UI720x720*>(lv_event_get_user_data(event));
      if (self && lv_event_get_code(event) == LV_EVENT_CLICKED && self->test_tone_)
        self->test_tone_(self->settings_context_);
      lv_event_stop_bubbling(event);
    }, LV_EVENT_ALL, this);
    makeDashboardLabel(settings_controls_page_, "語音播報", 28, 328, 300,
                       &font_dashboard_cjk_48, 0xf1f5f9);
    settings_voice_button_label_ = makeDashboardLabel(settings_controls_page_, "ON", 540, 328, 130,
                       &lv_font_montserrat_48, 0x34d399, LV_TEXT_ALIGN_CENTER);
    lv_obj_t* voice_switch = lv_switch_create(settings_controls_page_);
    lv_obj_set_pos(voice_switch, 430, 326);
    lv_obj_set_size(voice_switch, 84, 48);
    lv_obj_set_style_bg_color(voice_switch, lv_color_hex(0x475569), LV_PART_MAIN);
    lv_obj_set_style_bg_color(voice_switch, lv_color_hex(0x059669), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(voice_switch, lv_color_hex(0xffffff), LV_PART_KNOB);
    lv_obj_set_style_pad_all(voice_switch, 4, LV_PART_KNOB);
    lv_obj_add_state(voice_switch, LV_STATE_CHECKED);
    lv_obj_add_event_cb(voice_switch, [](lv_event_t* event) {
      auto* self = static_cast<UI720x720*>(lv_event_get_user_data(event));
      if (!self || lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) return;
      self->settings_voice_enabled_ = lv_obj_has_state(lv_event_get_target(event), LV_STATE_CHECKED);
      lv_obj_set_style_text_color(self->settings_voice_button_label_,
          lv_color_hex(self->settings_voice_enabled_ ? 0x34d399 : 0xf87171), 0);
      self->refreshSettingsLabels();
      if (self->voice_changed_) self->voice_changed_(self->settings_context_, self->settings_voice_enabled_);
      lv_event_stop_bubbling(event);
    }, LV_EVENT_VALUE_CHANGED, this);
    settings_voice_switch_ = voice_switch;
    auto restart = lv_btn_create(settings_controls_page_);
    lv_obj_set_pos(restart, 28, 404); lv_obj_set_size(restart, 664, 72);
    lv_obj_set_style_bg_color(restart, lv_color_hex(0x9f1239), 0);
    lv_obj_set_style_border_width(restart, 0, 0); lv_obj_set_style_radius(restart, 18, 0);
    lv_obj_set_style_pad_all(restart, 0, 0);
    restart_button_label_ = makeCenteredSettingsButtonLabel(restart, "RESTART",
                       &lv_font_montserrat_48, 0xffffff);
    lv_obj_add_event_cb(restart, [](lv_event_t* event) {
      auto* self = static_cast<UI720x720*>(lv_event_get_user_data(event));
      if (!self || lv_event_get_code(event) != LV_EVENT_CLICKED) return;
      const uint32_t now = millis();
      if (self->restart_armed_ && static_cast<uint32_t>(now - self->restart_armed_at_ms_) <= 3000) {
        self->restart_armed_ = false; lv_label_set_text(self->restart_button_label_, "RESTART");
        self->centerSettingsButtonLabel(lv_obj_get_parent(self->restart_button_label_), self->restart_button_label_);
        if (self->restart_) self->restart_(self->settings_context_);
      } else {
        self->restart_armed_ = true; self->restart_armed_at_ms_ = now;
        lv_label_set_text(self->restart_button_label_, "TAP AGAIN");
        self->centerSettingsButtonLabel(lv_obj_get_parent(self->restart_button_label_), self->restart_button_label_);
      }
      lv_event_stop_bubbling(event);
    }, LV_EVENT_ALL, this);

    settings_sim_ip_title_ = makeDashboardLabel(settings_network_page_, "模擬 ESP32 IP", 28, 8, 664,
                       &font_dashboard_cjk_48, 0x94a3b8);
    settings_sim_ip_value_ = makeDashboardLabel(settings_network_page_, "192.0.2.15", 28, 62, 664,
                       &lv_font_montserrat_48, 0xf1f5f9);
    lv_label_set_long_mode(settings_sim_ip_value_, LV_LABEL_LONG_WRAP);
    settings_http_title_ = makeDashboardLabel(settings_network_page_, "ESP32 HTTP URL", 28, 150, 664,
                       &font_dashboard_cjk_48, 0x94a3b8);
    settings_gateway_url_label_ = makeDashboardLabel(settings_network_page_, "http://192.0.2.15/", 28, 204, 664,
                       &lv_font_montserrat_48, 0x38bdf8);
    lv_label_set_long_mode(settings_gateway_url_label_, LV_LABEL_LONG_WRAP);
    makeDashboardLabel(settings_network_page_, "Powerwall Gateway IP", 28, 300, 664,
                       &lv_font_montserrat_48, 0x94a3b8);
    settings_gateway_ip_label_ = makeDashboardLabel(settings_network_page_, "--", 28, 354, 664,
                       &lv_font_montserrat_48, 0xf1f5f9);
    settings_uptime_label_ = makeDashboardLabel(settings_network_page_, "Uptime: 0d 00:00:00", 28, 450, 664,
                       &lv_font_montserrat_32, 0x94a3b8);
    refreshSettingsUptime();
    refreshSettingsLabels();
    lv_slider_set_value(settings_brightness_slider_, settings_brightness_, LV_ANIM_OFF);
    lv_slider_set_value(settings_volume_slider_, settings_volume_, LV_ANIM_OFF);
  }
#endif

  void initializeSettingsPage() {
#if defined(POWERWALL_UI_480)
    initializeSettingsPage480();
    return;
#endif
    lv_obj_t* brightness_card = makeDashboardCard(page_settings_, 14, 92, 692, 240);
    makeDashboardLabel(brightness_card, "顯示亮度", 28, 18, 390, &font_dashboard_cjk_28, 0xf1f5f9);
    settings_brightness_value_ = makeDashboardLabel(brightness_card, "80%", 500, 18, 155,
                                                    &lv_font_montserrat_32, 0x34d399, LV_TEXT_ALIGN_RIGHT);
    makeDashboardLabel(brightness_card, "DISPLAY", 28, 61, 220, &lv_font_montserrat_18, 0x94a3b8);
    settings_brightness_slider_ = lv_slider_create(brightness_card);
    lv_obj_set_pos(settings_brightness_slider_, 30, 118);
    lv_obj_set_size(settings_brightness_slider_, 628, 24);
    lv_slider_set_range(settings_brightness_slider_, 0, 100);
    lv_obj_set_style_bg_color(settings_brightness_slider_, lv_color_hex(0x263746), LV_PART_MAIN);
    lv_obj_set_style_bg_color(settings_brightness_slider_, lv_color_hex(0x10b981), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(settings_brightness_slider_, lv_color_hex(0xe2e8f0), LV_PART_KNOB);
    lv_obj_set_style_pad_all(settings_brightness_slider_, 16, LV_PART_KNOB);
    lv_obj_add_event_cb(settings_brightness_slider_, [](lv_event_t* event) {
      UI720x720* self = static_cast<UI720x720*>(lv_event_get_user_data(event));
      if (!self) return;
      if (lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED && !self->settings_updating_) {
        self->settings_brightness_ = static_cast<uint8_t>(lv_slider_get_value(self->settings_brightness_slider_));
        self->refreshSettingsLabels();
        if (self->brightness_changed_) self->brightness_changed_(self->settings_context_, self->settings_brightness_);
      }
      lv_event_stop_bubbling(event);
    }, LV_EVENT_ALL, this);
    makeDashboardLabel(brightness_card, "0", 28, 171, 50, &lv_font_montserrat_18, 0x94a3b8);
    makeDashboardLabel(brightness_card, "100", 607, 171, 50, &lv_font_montserrat_18, 0x94a3b8, LV_TEXT_ALIGN_RIGHT);

    lv_obj_t* volume_card = makeDashboardCard(page_settings_, 14, 352, 692, 300);
    makeDashboardLabel(volume_card, "音量", 28, 18, 390, &font_dashboard_cjk_28, 0xf1f5f9);
    settings_volume_value_ = makeDashboardLabel(volume_card, "85%", 500, 18, 155,
                                                &lv_font_montserrat_32, 0x38bdf8, LV_TEXT_ALIGN_RIGHT);
    makeDashboardLabel(volume_card, "SOUND", 28, 61, 220, &lv_font_montserrat_18, 0x94a3b8);
    settings_volume_slider_ = lv_slider_create(volume_card);
    lv_obj_set_pos(settings_volume_slider_, 30, 112);
    lv_obj_set_size(settings_volume_slider_, 628, 24);
    lv_slider_set_range(settings_volume_slider_, 0, 100);
    lv_obj_set_style_bg_color(settings_volume_slider_, lv_color_hex(0x263746), LV_PART_MAIN);
    lv_obj_set_style_bg_color(settings_volume_slider_, lv_color_hex(0x0ea5e9), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(settings_volume_slider_, lv_color_hex(0xe2e8f0), LV_PART_KNOB);
    lv_obj_set_style_pad_all(settings_volume_slider_, 16, LV_PART_KNOB);
    lv_obj_add_event_cb(settings_volume_slider_, [](lv_event_t* event) {
      UI720x720* self = static_cast<UI720x720*>(lv_event_get_user_data(event));
      if (!self) return;
      if (lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED && !self->settings_updating_) {
        self->settings_volume_ = static_cast<uint8_t>(lv_slider_get_value(self->settings_volume_slider_));
        self->refreshSettingsLabels();
        if (self->volume_changed_) self->volume_changed_(self->settings_context_, self->settings_volume_);
      }
      lv_event_stop_bubbling(event);
    }, LV_EVENT_ALL, this);
    lv_obj_t* test_button = lv_btn_create(volume_card);
    lv_obj_set_pos(test_button, 177, 184);
    lv_obj_set_size(test_button, 338, 68);
    lv_obj_set_style_bg_color(test_button, lv_color_hex(0x075985), 0);
    lv_obj_set_style_radius(test_button, 18, 0);
    makeDashboardLabel(test_button, "播放測試音", 94, 13, 210, &font_dashboard_cjk_28, 0xffffff);
    lv_obj_add_event_cb(test_button, [](lv_event_t* event) {
      UI720x720* self = static_cast<UI720x720*>(lv_event_get_user_data(event));
      if (!self) return;
      if (lv_event_get_code(event) == LV_EVENT_CLICKED && self->test_tone_)
        self->test_tone_(self->settings_context_);
      lv_event_stop_bubbling(event);
    }, LV_EVENT_ALL, this);
    restart_button_ = lv_btn_create(page_settings_);
    lv_obj_set_pos(restart_button_, 210, 663);
    lv_obj_set_size(restart_button_, 300, 48);
    lv_obj_set_style_bg_color(restart_button_, lv_color_hex(0x9f1239), 0);
    lv_obj_set_style_radius(restart_button_, 14, 0);
    restart_button_label_ = makeDashboardLabel(restart_button_, "RESTART", 0, 7, 300,
                                               &lv_font_montserrat_24, 0xffffff, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_event_cb(restart_button_, [](lv_event_t* event) {
      UI720x720* self = static_cast<UI720x720*>(lv_event_get_user_data(event));
      if (!self) return;
      if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        const uint32_t now = millis();
        if (self->restart_armed_ && static_cast<uint32_t>(now - self->restart_armed_at_ms_) <= 3000) {
          self->restart_armed_ = false;
          lv_label_set_text(self->restart_button_label_, "RESTART");
          if (self->restart_) self->restart_(self->settings_context_);
        } else {
          self->restart_armed_ = true;
          self->restart_armed_at_ms_ = now;
          lv_label_set_text(self->restart_button_label_, "TAP AGAIN");
        }
      }
      lv_event_stop_bubbling(event);
    }, LV_EVENT_ALL, this);
    refreshSettingsLabels();
    lv_slider_set_value(settings_brightness_slider_, settings_brightness_, LV_ANIM_OFF);
    lv_slider_set_value(settings_volume_slider_, settings_volume_, LV_ANIM_OFF);
  }

  void initializeAddedPages(lv_obj_t* screen) {
    dashboard_screen_ = screen;
    page_powerwall_ = lv_obj_create(screen);
    page_home_grid_ = lv_obj_create(screen);
    page_settings_ = lv_obj_create(screen);
    lv_obj_t* pages[] = {page_powerwall_, page_home_grid_, page_settings_};
    for (lv_obj_t* page : pages) {
      lv_obj_set_pos(page, 0, 0);
      lv_obj_set_size(page, 720, 720);
      lv_obj_set_style_bg_color(page, lv_color_hex(0x0b0f14), 0);
      lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
      lv_obj_set_style_border_width(page, 0, 0);
      lv_obj_set_style_radius(page, 0, 0);
      lv_obj_set_style_pad_all(page, 0, 0);
      lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
    }

    initializeWeeklyPage(screen);
    initializeSettingsPage();
    // One shared header keeps the two added pages aligned and leaves the original page untouched.
    page_header_ = lv_obj_create(screen);
    lv_obj_set_pos(page_header_, 0, 0);
    lv_obj_set_size(page_header_, 720, 88);
    lv_obj_set_style_bg_opa(page_header_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(page_header_, 0, 0);
    lv_obj_set_style_radius(page_header_, 0, 0);
    lv_obj_set_style_pad_all(page_header_, 0, 0);
    lv_obj_clear_flag(page_header_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(page_header_, LV_OBJ_FLAG_HIDDEN);
    page_title_ = makeDashboardLabel(page_header_, "POWERWALL", 20, 8, 250, &font_cjk_20, 0xf1f5f9);
    page_site_ = makeDashboardLabel(page_header_, "我的家", 20, 35, 220, &font_cjk_12, 0x94a3b8);
    pw_header_title_ = makeDashboardLabel(page_header_, "POWERWALL", 16, 10, 430,
                                           &font_home_cjk_40, 0xf1f5f9);
    // Home clock is nested inside the hero at screen (8, 8). Match its screen coordinates.
    pw_header_clock_ = makeDashboardLabel(page_header_, "--:--", 548, 20, 112,
                                           &font_home_cjk_30, 0xf1f5f9, LV_TEXT_ALIGN_RIGHT);
    pw_header_date_ = makeDashboardLabel(page_header_, "--/--", 548, 56, 112,
                                          &lv_font_montserrat_24, 0x94a3b8, LV_TEXT_ALIGN_RIGHT);
    pw_header_status_dot_ = lv_obj_create(page_header_);
    lv_obj_set_pos(pw_header_status_dot_, 682, 33);
    lv_obj_set_size(pw_header_status_dot_, 12, 12);
    lv_obj_set_style_radius(pw_header_status_dot_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(pw_header_status_dot_, lv_color_hex(0x34d399), 0);
    lv_obj_set_style_bg_opa(pw_header_status_dot_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pw_header_status_dot_, 0, 0);
    lv_obj_clear_flag(pw_header_status_dot_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    page_status_pill_ = lv_obj_create(page_header_);
    lv_obj_set_pos(page_status_pill_, 452, 17);
    lv_obj_set_size(page_status_pill_, 104, 26);
    lv_obj_set_style_bg_color(page_status_pill_, lv_color_hex(0x064e3b), 0);
    lv_obj_set_style_bg_opa(page_status_pill_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(page_status_pill_, lv_color_hex(0x10b981), 0);
    lv_obj_set_style_border_width(page_status_pill_, 1, 0);
    lv_obj_set_style_radius(page_status_pill_, 13, 0);
#if defined(POWERWALL_UI_480)
    flattenDashboardPill(page_status_pill_, 0x10b981);
#endif
    lv_obj_set_style_pad_all(page_status_pill_, 0, 0);
    lv_obj_clear_flag(page_status_pill_, LV_OBJ_FLAG_SCROLLABLE);
    page_status_text_ = makeDashboardLabel(page_status_pill_, "即時資料", 0, 4, 104, &font_cjk_12, 0x6ee7b7, LV_TEXT_ALIGN_CENTER);
    page_clock_ = makeDashboardLabel(page_header_, "--:--", 568, 8, 132, &font_cjk_18, 0xf1f5f9, LV_TEXT_ALIGN_RIGHT);
    page_date_ = makeDashboardLabel(page_header_, "--月--日", 568, 34, 132, &font_cjk_12, 0x94a3b8, LV_TEXT_ALIGN_RIGHT);

    // Powerwall page: state, usable energy, compact metrics, and the shared daily SoE history.
    lv_obj_t* pwHero = makeDashboardCard(page_powerwall_, 14, 78, 692, 232);
    pw_arc_ = lv_arc_create(pwHero);
    lv_obj_set_pos(pw_arc_, 18, 8);
    lv_obj_set_size(pw_arc_, 216, 216);
    lv_arc_set_range(pw_arc_, 0, 100);
    lv_arc_set_bg_angles(pw_arc_, 135, 45);
    lv_arc_set_rotation(pw_arc_, 0);
    lv_arc_set_value(pw_arc_, 0);
    lv_obj_set_style_arc_color(pw_arc_, lv_color_hex(0x273341), LV_PART_MAIN);
    lv_obj_set_style_arc_width(pw_arc_, 20, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(pw_arc_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_color(pw_arc_, lv_color_hex(0x11e58e), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(pw_arc_, 20, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(pw_arc_, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(pw_arc_, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_opa(pw_arc_, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(pw_arc_, 0, LV_PART_KNOB);
    lv_obj_clear_flag(pw_arc_, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    pw_reserve_marker_ = lv_line_create(pwHero);
    lv_line_set_points(pw_reserve_marker_, pw_reserve_marker_points_, 2);
    lv_obj_set_style_line_color(pw_reserve_marker_, lv_color_hex(0xe2e8f0), 0);
    lv_obj_set_style_line_width(pw_reserve_marker_, 5, 0);
    lv_obj_set_style_line_rounded(pw_reserve_marker_, true, 0);
    lv_obj_clear_flag(pw_reserve_marker_, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    pw_soc_text_ = makeDashboardLabel(pwHero, "--%", 26, 72, 200, &lv_font_montserrat_48, 0x11e58e, LV_TEXT_ALIGN_CENTER);
    // LVGL's bundled Montserrat font has no bold face; a 1 px overprint thickens
    // the enlarged live percentage while keeping the label centered in the gauge.
    pw_soc_bold_pass_ = makeDashboardLabel(pwHero, "--%", 27, 72, 200, &lv_font_montserrat_48, 0x11e58e, LV_TEXT_ALIGN_CENTER);
    makeDashboardLabel(pwHero, "目前電量", 26, 138, 200,
                       &font_home_cjk_24, 0xcbd5e1, LV_TEXT_ALIGN_CENTER);
#if defined(POWERWALL_UI_480)
    // The native 720→480 nearest-neighbour flush can drop one-pixel glyph rows.
    // A one-row overprint keeps the small caption legible on the compact panel.
    makeDashboardLabel(pwHero, "目前電量", 26, 139, 200, &font_home_cjk_24, 0xcbd5e1, LV_TEXT_ALIGN_CENTER);
#endif
    makeDashboardRule(pwHero, 252, 18, 1, 196);

    pw_state_pill_ = lv_obj_create(pwHero);
    lv_obj_set_pos(pw_state_pill_, 282, 8);
    lv_obj_set_size(pw_state_pill_, 386, 38);
    lv_obj_set_style_bg_color(pw_state_pill_, lv_color_hex(0x064e3b), 0);
    lv_obj_set_style_bg_opa(pw_state_pill_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(pw_state_pill_, lv_color_hex(0x10b981), 0);
    lv_obj_set_style_border_width(pw_state_pill_, 1, 0);
    lv_obj_set_style_radius(pw_state_pill_, 10, 0);
#if defined(POWERWALL_UI_480)
    flattenDashboardPill(pw_state_pill_, 0x10b981);
#endif
    lv_obj_set_style_pad_all(pw_state_pill_, 0, 0);
    lv_obj_clear_flag(pw_state_pill_, LV_OBJ_FLAG_SCROLLABLE);
    pw_state_text_ = makeDashboardLabel(pw_state_pill_, "待機中", 12, 6, 360, &font_home_cjk_24, 0x6ee7b7);
    makeDashboardLabel(pwHero, "預估可用時間 / 備援可用時間", 286, 64, 382, &font_cjk_20, 0x94a3b8);
    pw_eta_text_ = makeDashboardLabel(pwHero, "-- #94a3b8 / --#", 286, 90, 402, &lv_font_montserrat_40, 0x38bdf8);
    lv_label_set_recolor(pw_eta_text_, true);
    pw_mode_text_ = makeDashboardLabel(pwHero, "備援保留 24%   |   自動模式", 286, 163, 382, &font_home_cjk_24, 0xcbd5e1);

    lv_obj_t* stats[3] = {
      makeDashboardCard(page_powerwall_, 14, 322, 224, 120),
      makeDashboardCard(page_powerwall_, 248, 322, 224, 120),
      makeDashboardCard(page_powerwall_, 482, 322, 224, 120)
    };
    makeDashboardLabel(stats[0], "今日自給率", 12, 12, 200, &font_home_cjk_24, 0x94a3b8);
    pw_self_value_ = makeDashboardLabel(stats[0], "--", 12, 48, 196, &lv_font_montserrat_36, 0x11e58e, LV_TEXT_ALIGN_CENTER);
    makeDashboardLabel(stats[1], "電池電壓", 12, 12, 200, &font_home_cjk_24, 0x94a3b8);
    pw_voltage_value_ = makeDashboardLabel(stats[1], "--", 12, 48, 200, &lv_font_montserrat_36, 0xf1f5f9, LV_TEXT_ALIGN_CENTER);
    makeDashboardLabel(stats[2], "剩餘電量", 12, 12, 200, &font_home_cjk_24, 0x94a3b8);
    pw_capacity_current_value_ = makeDashboardLabel(stats[2], "--", 12, 46, 0, &lv_font_montserrat_32, 0xf1f5f9);
    const int unitY = 46 + lv_font_montserrat_32.line_height - lv_font_montserrat_32.base_line
      - lv_font_montserrat_24.line_height + lv_font_montserrat_24.base_line;
    pw_capacity_current_unit_ = makeDashboardLabel(stats[2], "kWh", 12, unitY, 0, &lv_font_montserrat_24, 0xf1f5f9);
    pw_capacity_total_value_ = makeDashboardLabel(stats[2], "/-- kWh", 12, 84, 0, &lv_font_montserrat_18, 0xcbd5e1, LV_TEXT_ALIGN_RIGHT);
#if defined(POWERWALL_UI_480)
    const char* statTitles[] = {"今日自給率", "電池電壓", "剩餘電量"};
    for (int i = 0; i < 3; ++i)
      makeDashboardLabel(stats[i], statTitles[i], 12, 13, 200, &font_home_cjk_24, 0x94a3b8);
#endif

    pw_chart_card_ = makeDashboardCard(page_powerwall_, 14, kPwChartCardY, 692, kPwChartCardH);
    dashboard_chart_buf_ = allocPixelBuf(kChartW * kDashboardChartH);
    if (dashboard_chart_buf_) {
      pw_chart_canvas_ = lv_canvas_create(pw_chart_card_);
      lv_canvas_set_buffer(pw_chart_canvas_, dashboard_chart_buf_, kChartW, kDashboardChartH, LV_IMG_CF_TRUE_COLOR);
      lv_obj_set_pos(pw_chart_canvas_, kPwChartPlotX, kPwChartPlotY);
      lv_obj_clear_flag(pw_chart_canvas_, LV_OBJ_FLAG_SCROLLABLE);
    }
    const char* pwAxis[] = {"100%", "75%", "50%", "25%", "0%"};
    for (int i = 0; i < 5; ++i) {
      const int y = kPwChartPlotY + 3 + static_cast<int>(roundf(i * (kDashboardChartH - 5) / 4.0f))
        - lv_font_montserrat_20.line_height / 2;
      makeDashboardLabel(pw_chart_card_, pwAxis[i], 0, y, kPwChartPlotX, &lv_font_montserrat_20, 0x94a3b8, LV_TEXT_ALIGN_RIGHT);
    }
    static const char* pwFixedTicks[] = {"00", "06", "12", "18", "24"};
    for (int i = 0; i < 5; ++i) {
      constexpr int tickWidth = 40;
      const int x = i == 0 ? kPwChartPlotX :
                    i == 4 ? kPwChartPlotX + kChartW - tickWidth :
                    kPwChartPlotX + static_cast<int>(roundf(i * kChartW / 4.0f)) - tickWidth / 2;
      const lv_text_align_t align = i == 0 ? LV_TEXT_ALIGN_LEFT :
                                    i == 4 ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_CENTER;
      pw_time_ticks_[i] = makeDashboardLabel(pw_chart_card_, pwFixedTicks[i], x, kPwChartTimeY,
                                              tickWidth, &lv_font_montserrat_20, 0xcbd5e1, align);
    }
    for (int i = 0; i < 4; ++i) {
      pw_tou_segments_[i] = lv_obj_create(pw_chart_card_);
      lv_obj_set_pos(pw_tou_segments_[i], kPwChartPlotX, kPwChartBandY);
      lv_obj_set_size(pw_tou_segments_[i], 1, kPwChartTouH);
      lv_obj_set_style_border_width(pw_tou_segments_[i], 0, 0);
      lv_obj_set_style_radius(pw_tou_segments_[i], 0, 0);
      lv_obj_clear_flag(pw_tou_segments_[i], LV_OBJ_FLAG_SCROLLABLE);
    }
    pw_now_line_ = lv_obj_create(pw_chart_card_);
    lv_obj_set_size(pw_now_line_, 2, kPwChartCursorH);
    lv_obj_set_pos(pw_now_line_, kPwChartPlotX, kPwChartPlotY);
    lv_obj_set_style_bg_color(pw_now_line_, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(pw_now_line_, LV_OPA_60, 0);
    lv_obj_set_style_border_width(pw_now_line_, 0, 0);
    pw_now_dot_ = lv_obj_create(pw_chart_card_);
    lv_obj_set_size(pw_now_dot_, 10, 10);
    lv_obj_set_style_radius(pw_now_dot_, 5, 0);
    lv_obj_set_style_bg_color(pw_now_dot_, lv_color_hex(0x11e58e), 0);
    lv_obj_set_style_bg_opa(pw_now_dot_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(pw_now_dot_, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_border_width(pw_now_dot_, 2, 0);
    lv_obj_add_flag(pw_now_dot_, LV_OBJ_FLAG_HIDDEN);
    pw_reserve_chart_label_ = makeDashboardLabel(pw_chart_card_, "", 0, 0, 0, &font_home_cjk_24, 0x7dd3fc, LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_width(pw_reserve_chart_label_, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(pw_reserve_chart_label_, lv_color_make(9, 10, 15), 0);
    lv_obj_set_style_bg_opa(pw_reserve_chart_label_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(pw_reserve_chart_label_, 3, 0);

    // Home/grid page: one TOU schedule band, meter rows, and source-colored load history.
    lv_obj_t* touCard = makeDashboardCard(page_home_grid_, 14, 68, 692, 126);
    home_tou_pill_ = lv_obj_create(touCard);
    lv_obj_set_pos(home_tou_pill_, 12, 11);
    lv_obj_set_size(home_tou_pill_, 126, 34);
    lv_obj_set_style_bg_color(home_tou_pill_, lv_color_hex(0x064e3b), 0);
    lv_obj_set_style_bg_opa(home_tou_pill_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(home_tou_pill_, lv_color_hex(0x10b981), 0);
    lv_obj_set_style_border_width(home_tou_pill_, 1, 0);
    lv_obj_set_style_radius(home_tou_pill_, 16, 0);
#if defined(POWERWALL_UI_480)
    flattenDashboardPill(home_tou_pill_, 0x10b981);
#endif
    lv_obj_set_style_pad_all(home_tou_pill_, 0, 0);
    lv_obj_clear_flag(home_tou_pill_, LV_OBJ_FLAG_SCROLLABLE);
    home_tou_pill_text_ = makeDashboardLabel(home_tou_pill_, "目前：離峰", 0, 6, 126, &font_cjk_14, 0x6ee7b7, LV_TEXT_ALIGN_CENTER);
    home_interval_start_ = makeDashboardLabel(touCard, "00:00", 150, 18, 55, &font_cjk_18, 0xf1f5f9);
    home_interval_rule_ = lv_obj_create(touCard);
    lv_obj_set_pos(home_interval_rule_, 205, 27);
    lv_obj_set_size(home_interval_rule_, 14, 2);
    lv_obj_set_style_bg_color(home_interval_rule_, lv_color_hex(0xf1f5f9), 0);
    lv_obj_set_style_bg_opa(home_interval_rule_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(home_interval_rule_, 0, 0);
    lv_obj_set_style_radius(home_interval_rule_, 1, 0);
    lv_obj_clear_flag(home_interval_rule_, LV_OBJ_FLAG_SCROLLABLE);
    home_interval_end_ = makeDashboardLabel(touCard, "06:00", 224, 18, 60, &font_cjk_18, 0xf1f5f9);
    home_price_ = makeDashboardLabel(touCard, "NT$1.99 / 度", 12, 48, 170, &font_cjk_18, 0xf1f5f9);
    home_countdown_ = makeDashboardLabel(touCard, "下一次尖峰 06:00 · 還有 --", 184, 52, 490, &font_cjk_12, 0xcbd5e1);
    for (int i = 0; i < 4; ++i) {
      home_tou_segments_[i] = lv_obj_create(touCard);
      lv_obj_set_pos(home_tou_segments_[i], 12, 77);
      lv_obj_set_size(home_tou_segments_[i], 1, 18);
      lv_obj_set_style_border_width(home_tou_segments_[i], 0, 0);
      lv_obj_set_style_radius(home_tou_segments_[i], 0, 0);
      lv_obj_clear_flag(home_tou_segments_[i], LV_OBJ_FLAG_SCROLLABLE);
      home_tou_segment_labels_[i] = makeDashboardLabel(home_tou_segments_[i], "", 0, 1, 1, &font_cjk_12, 0xffffff, LV_TEXT_ALIGN_CENTER);
    }
    for (int i = 0; i < 5; ++i) {
      home_tou_ticks_[i] = makeDashboardLabel(touCard, "", 0, 99, 54, &font_cjk_12, 0x94a3b8,
                                               i == 0 ? LV_TEXT_ALIGN_LEFT : (i == 4 ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_CENTER));
    }
    home_tou_now_line_ = lv_obj_create(touCard);
    lv_obj_set_size(home_tou_now_line_, 2, 31);
    lv_obj_set_style_bg_color(home_tou_now_line_, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(home_tou_now_line_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(home_tou_now_line_, 0, 0);
    lv_obj_set_style_radius(home_tou_now_line_, 0, 0);
    lv_obj_clear_flag(home_tou_now_line_, LV_OBJ_FLAG_SCROLLABLE);
    home_tou_now_dot_ = lv_obj_create(touCard);
    lv_obj_set_size(home_tou_now_dot_, 8, 8);
    lv_obj_set_style_radius(home_tou_now_dot_, 4, 0);
    lv_obj_set_style_bg_color(home_tou_now_dot_, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(home_tou_now_dot_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(home_tou_now_dot_, 0, 0);

    lv_obj_t* gridCard = makeDashboardCard(page_home_grid_, 14, 204, 692, 142);
    makeDashboardLabel(gridCard, "電網  GRID", 12, 8, 220, &font_cjk_18, 0xf1f5f9);
    home_grid_status_ = makeDashboardLabel(gridCard, "—", 416, 11, 260, &font_cjk_12, 0x38bdf8, LV_TEXT_ALIGN_RIGHT);
    makeDashboardRule(gridCard, 12, 34, 668);
    home_grid_power_title_ = makeDashboardLabel(gridCard, "輸入功率", 20, 42, 205, &font_cjk_12, 0x94a3b8);
    home_grid_power_value_ = makeDashboardLabel(gridCard, "— kW", 20, 59, 205, &font_cjk_20, 0x38bdf8);
    makeDashboardRule(gridCard, 238, 43, 1, 40);
    makeDashboardLabel(gridCard, "電流", 254, 42, 170, &font_cjk_12, 0x94a3b8);
    home_grid_current_value_ = makeDashboardLabel(gridCard, "— A", 254, 59, 170, &font_cjk_20, 0xf1f5f9);
    makeDashboardRule(gridCard, 452, 43, 1, 40);
    makeDashboardLabel(gridCard, "電壓", 468, 42, 190, &font_cjk_12, 0x94a3b8);
    home_grid_voltage_value_ = makeDashboardLabel(gridCard, "— V", 468, 59, 190, &font_cjk_20, 0xf1f5f9);
    makeDashboardRule(gridCard, 12, 91, 668);
    makeDashboardLabel(gridCard, "累計購電", 20, 98, 280, &font_cjk_12, 0x94a3b8);
    home_grid_import_value_ = makeDashboardLabel(gridCard, "— kWh", 20, 114, 280, &font_cjk_18, 0xf1f5f9);
    makeDashboardRule(gridCard, 338, 98, 1, 34);
    makeDashboardLabel(gridCard, "累計回送", 356, 98, 280, &font_cjk_12, 0x94a3b8);
    home_grid_export_value_ = makeDashboardLabel(gridCard, "— kWh", 356, 114, 280, &font_cjk_18, 0xf1f5f9);

    lv_obj_t* homeCard = makeDashboardCard(page_home_grid_, 14, 356, 692, 144);
    makeDashboardLabel(homeCard, "住家  HOME", 12, 8, 220, &font_cjk_18, 0xf1f5f9);
    home_solar_value_ = makeDashboardLabel(homeCard, "太陽能 — kW", 420, 11, 256, &font_cjk_12, 0xfbbf24, LV_TEXT_ALIGN_RIGHT);
    makeDashboardRule(homeCard, 12, 34, 668);
    makeDashboardLabel(homeCard, "用電功率", 20, 42, 205, &font_cjk_12, 0x94a3b8);
    home_load_value_ = makeDashboardLabel(homeCard, "— kW", 20, 59, 205, &font_cjk_20, 0x38bdf8);
    makeDashboardRule(homeCard, 238, 43, 1, 40);
    makeDashboardLabel(homeCard, "電流", 254, 42, 170, &font_cjk_12, 0x94a3b8);
    home_load_current_value_ = makeDashboardLabel(homeCard, "— A", 254, 59, 170, &font_cjk_20, 0xf1f5f9);
    makeDashboardRule(homeCard, 452, 43, 1, 40);
    makeDashboardLabel(homeCard, "電壓", 468, 42, 190, &font_cjk_12, 0x94a3b8);
    home_load_voltage_value_ = makeDashboardLabel(homeCard, "— V", 468, 59, 190, &font_cjk_20, 0xf1f5f9);
    makeDashboardRule(homeCard, 12, 91, 668);
    home_power_factor_ = makeDashboardLabel(homeCard, "功率因數 —", 20, 104, 300, &font_cjk_12, 0xcbd5e1);
    home_energy_value_ = makeDashboardLabel(homeCard, "累計用電 — kWh", 330, 104, 346, &font_cjk_12, 0xcbd5e1, LV_TEXT_ALIGN_RIGHT);

    lv_obj_t* homeChartCard = makeDashboardCard(page_home_grid_, 14, 510, 692, 198);
    makeDashboardLabel(homeChartCard, "24 小時住家用電", 14, 7, 240, &font_cjk_18, 0xf1f5f9);
    makeDashboardLabel(homeChartCard, "未知", 425, 10, 44, &font_cjk_12, 0x94a3b8);
    lv_obj_t* unknownSwatch = lv_obj_create(homeChartCard);
    lv_obj_set_pos(unknownSwatch, 409, 14); lv_obj_set_size(unknownSwatch, 10, 3);
    lv_obj_set_style_bg_color(unknownSwatch, lv_color_hex(0x94a3b8), 0); lv_obj_set_style_border_width(unknownSwatch, 0, 0);
    makeDashboardLabel(homeChartCard, "電網", 506, 10, 42, &font_cjk_12, 0x38bdf8);
    lv_obj_t* gridSwatch = lv_obj_create(homeChartCard);
    lv_obj_set_pos(gridSwatch, 490, 14); lv_obj_set_size(gridSwatch, 10, 3);
    lv_obj_set_style_bg_color(gridSwatch, lv_color_hex(0x38bdf8), 0); lv_obj_set_style_border_width(gridSwatch, 0, 0);
    makeDashboardLabel(homeChartCard, "Powerwall", 586, 10, 92, &font_cjk_12, 0x11e58e);
    lv_obj_t* pwSwatch = lv_obj_create(homeChartCard);
    lv_obj_set_pos(pwSwatch, 570, 14); lv_obj_set_size(pwSwatch, 10, 3);
    lv_obj_set_style_bg_color(pwSwatch, lv_color_hex(0x11e58e), 0); lv_obj_set_style_border_width(pwSwatch, 0, 0);
    if (dashboard_chart_buf_) {
      home_chart_canvas_ = lv_canvas_create(homeChartCard);
      lv_canvas_set_buffer(home_chart_canvas_, dashboard_chart_buf_, kChartW, 100, LV_IMG_CF_TRUE_COLOR);
      lv_obj_set_pos(home_chart_canvas_, 54, 39);
      lv_obj_clear_flag(home_chart_canvas_, LV_OBJ_FLAG_SCROLLABLE);
    }
    makeDashboardLabel(homeChartCard, "6", 8, 37, 38, &font_cjk_12, 0x64748b, LV_TEXT_ALIGN_RIGHT);
    makeDashboardLabel(homeChartCard, "3", 8, 78, 38, &font_cjk_12, 0x64748b, LV_TEXT_ALIGN_RIGHT);
    makeDashboardLabel(homeChartCard, "0", 8, 117, 38, &font_cjk_12, 0x64748b, LV_TEXT_ALIGN_RIGHT);
    home_now_line_ = lv_obj_create(homeChartCard);
    lv_obj_set_size(home_now_line_, 2, 114);
    lv_obj_set_pos(home_now_line_, 54, 39);
    lv_obj_set_style_bg_color(home_now_line_, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(home_now_line_, LV_OPA_60, 0);
    lv_obj_set_style_border_width(home_now_line_, 0, 0);
    for (int i = 0; i < 4; ++i) {
      home_chart_tou_segments_[i] = lv_obj_create(homeChartCard);
      lv_obj_set_pos(home_chart_tou_segments_[i], 54, 145);
      lv_obj_set_size(home_chart_tou_segments_[i], 1, 8);
      lv_obj_set_style_border_width(home_chart_tou_segments_[i], 0, 0);
      lv_obj_set_style_radius(home_chart_tou_segments_[i], 0, 0);
      lv_obj_clear_flag(home_chart_tou_segments_[i], LV_OBJ_FLAG_SCROLLABLE);
    }
    lv_obj_move_foreground(home_now_line_);
    home_now_dot_ = lv_obj_create(homeChartCard);
    lv_obj_set_size(home_now_dot_, 9, 9);
    lv_obj_set_style_radius(home_now_dot_, 5, 0);
    lv_obj_set_style_bg_color(home_now_dot_, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(home_now_dot_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(home_now_dot_, lv_color_hex(0x0b0f14), 0);
    lv_obj_set_style_border_width(home_now_dot_, 1, 0);
    lv_obj_add_flag(home_now_dot_, LV_OBJ_FLAG_HIDDEN);
    initializeTimeAxis(home_time_axis_, homeChartCard, 54, kChartW, 155, &font_cjk_12);
    home_data_stamp_ = makeDashboardLabel(homeChartCard, "", 370, 177, 300, &font_cjk_12, 0x64748b, LV_TEXT_ALIGN_RIGHT);
  }

  void registerPageTapRouting(lv_obj_t* root) {
    lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(root, [](lv_event_t* event) {
      if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
      UI720x720* self = static_cast<UI720x720*>(lv_event_get_user_data(event));
      if (!self) return;
#if defined(POWERWALL_UI_480)
      // Settings controls and tabs bubble clicks to the screen. Keep the 480
      // settings page open on background/text taps; PWRKEY still uses the
      // shared nextDashboardPage() route directly.
      if (self->active_dashboard_page_ == 4) return;
#endif
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
    lv_obj_set_size(page_transition_overlay_, 720, 720);
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
      UI720x720* self = static_cast<UI720x720*>(lv_event_get_user_data(event));
      if (self && lv_event_get_code(event) == LV_EVENT_CLICKED) {
        ++self->page_transition_consumed_clicks_;
      }
      lv_event_stop_bubbling(event);
    }, LV_EVENT_ALL, this);
  }


  static void pageTransitionAnimationExec(void* var, int32_t value) {
    UI720x720* self = static_cast<UI720x720*>(var);
    if (!self || !self->page_transition_overlay_) return;
    const int32_t bounded = std::max<int32_t>(LV_OPA_TRANSP,
      std::min<int32_t>(LV_OPA_COVER, value));
    self->page_transition_opa_ = static_cast<lv_opa_t>(bounded);
    // Animate only the background opacity. Group opacity would allocate an LVGL layer.
    lv_obj_set_style_bg_opa(self->page_transition_overlay_, self->page_transition_opa_, 0);
  }

  static void pageTransitionAnimationReady(lv_anim_t* animation) {
    if (!animation) return;
    UI720x720* self = static_cast<UI720x720*>(animation->var);
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
    active_dashboard_page_ = page_transition_target_;
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
    finishPageTransition();
  }

  void applyDashboardPageVisibility() {
#if defined(POWERWALL_UI_480)
    if (active_dashboard_page_ != 0) hideHome480WifiCard();
#endif
    if (active_dashboard_page_ == 3) lv_obj_clear_flag(page_week_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(page_week_, LV_OBJ_FLAG_HIDDEN);
    if (active_dashboard_page_ == 4) lv_obj_clear_flag(page_settings_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(page_settings_, LV_OBJ_FLAG_HIDDEN);
    const bool onOriginal = active_dashboard_page_ == 0;
    if (onOriginal) {
      lv_obj_clear_flag(hero_card_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(chart_card_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(page_powerwall_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(page_home_grid_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(page_header_, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(hero_card_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(chart_card_, LV_OBJ_FLAG_HIDDEN);
      if (active_dashboard_page_ == 1) {
        lv_obj_clear_flag(page_powerwall_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(page_home_grid_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(page_title_, "POWERWALL");
      } else {
        lv_obj_add_flag(page_powerwall_, LV_OBJ_FLAG_HIDDEN);
        if (active_dashboard_page_ == 2) lv_obj_clear_flag(page_home_grid_, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(page_home_grid_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(page_title_, active_dashboard_page_ == 3 ? "7 日用電與電量總表" :
                          active_dashboard_page_ == 4 ? "設定" : "住家與電網");
      }
      const bool onPowerwallPage = active_dashboard_page_ == 1;
      lv_obj_set_height(page_header_, onPowerwallPage ? 88 : 55);
      if (onPowerwallPage) {
        lv_obj_add_flag(page_title_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(page_site_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(page_status_pill_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(page_clock_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(page_date_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(pw_header_title_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(pw_header_clock_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(pw_header_date_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(pw_header_status_dot_, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_clear_flag(page_title_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(page_site_, LV_OBJ_FLAG_HIDDEN);
        if (active_dashboard_page_ >= 3) lv_obj_add_flag(page_status_pill_, LV_OBJ_FLAG_HIDDEN);
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
#if defined(POWERWALL_UI_480)
    if (active_dashboard_page_ == 3 || active_dashboard_page_ == 4)
      lv_obj_add_flag(page_header_, LV_OBJ_FLAG_HIDDEN);
#endif
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
      return std::max(2, std::min(height - 2,
        static_cast<int>(roundf((1.0f - normalized) * (height - 5))) + 2));
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

  void refreshAddedPages() {
    if (!page_powerwall_ || !page_home_grid_) return;
    refreshSettingsLabels();
    const PowerwallTelemetry& t = last_telemetry_;
    updateAddedPageClock();
    refreshWeeklyPage();

    char text[96];
    const float soe = fminf(100.0f, fmaxf(0.0f, t.soe_percent));
    lv_arc_set_value(pw_arc_, static_cast<int>(roundf(soe)));
    snprintf(text, sizeof(text), "%d%%", static_cast<int>(roundf(soe)));
    lv_label_set_text(pw_soc_text_, text);
    lv_label_set_text(pw_soc_bold_pass_, text);

    const float batteryPower = t.has_battery_power ? t.battery_power_w : NAN;
    const char* state = !std::isfinite(batteryPower) ? "狀態未知" :
                        batteryPower > 60.0f ? "放電中" :
                        batteryPower < -60.0f ? "充電中" : "待機中";
    const uint32_t stateColor = batteryPower > 60.0f ? 0x38bdf8 :
                                std::isfinite(batteryPower) ? 0x11e58e : 0x94a3b8;
    const uint32_t stateBg = batteryPower > 60.0f ? 0x082f49 :
                             std::isfinite(batteryPower) ? 0x064e3b : 0x1e293b;
    if (std::isfinite(batteryPower)) {
      if (fabsf(batteryPower) < 5.0f) snprintf(text, sizeof(text), "%s - 0 kW", state);
      else snprintf(text, sizeof(text), "%s - %.2f kW", state, fabsf(batteryPower) / 1000.0f);
    } else {
      snprintf(text, sizeof(text), "%s - ? kW", state);
    }
    lv_label_set_text(pw_state_text_, text);
    lv_obj_set_style_text_color(pw_state_text_, lv_color_hex(stateColor), 0);
    lv_obj_set_style_bg_color(pw_state_pill_, lv_color_hex(stateBg), 0);
    lv_obj_set_style_border_color(pw_state_pill_, lv_color_hex(stateColor), 0);

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
    char etaColored[144];
    // Keep ETA text stable as changing minute digits alter its width.
    lv_obj_set_style_text_font(pw_eta_text_, &lv_font_montserrat_40, 0);
    snprintf(etaColored, sizeof(etaColored), "%s #94a3b8 / %s#", text, backupEta);
    lv_label_set_text(pw_eta_text_, etaColored);

    const char* mode = t.real_mode == "autonomous" ? "自動模式" :
                       t.real_mode == "backup" ? "備援模式" :
                       t.real_mode == "self_consumption" ? "自我消耗" : t.real_mode.c_str();
    snprintf(text, sizeof(text), "備援保留 %d%%   |   %s",
             static_cast<int>(roundf(t.backup_reserve_percent)), mode);
    lv_label_set_text(pw_mode_text_, text);
    snprintf(text, sizeof(text), "備援 %d%%", static_cast<int>(roundf(t.backup_reserve_percent)));
    lv_label_set_text(pw_reserve_chart_label_, text);
    const float reservePct = fminf(100.0f, fmaxf(0.0f, t.backup_reserve_percent));
    const int reserveY = static_cast<int>(roundf((1.0f - reservePct / 100.0f) * (kDashboardChartH - 4))) + 2;
    lv_obj_update_layout(pw_reserve_chart_label_);
    const int reserveLabelH = lv_obj_get_height(pw_reserve_chart_label_);
    const int reserveLabelW = lv_obj_get_width(pw_reserve_chart_label_);
    lv_obj_set_pos(pw_reserve_chart_label_, kPwChartPlotX + kChartW - reserveLabelW - 5,
                   kPwChartPlotY + std::max(3, reserveY - reserveLabelH - 4));
    // The arc runs clockwise from 135 to 405 degrees. Mark the configured
    // reserve at the same angle as its percentage on that arc.
    const float reserveFraction = fminf(1.0f, fmaxf(0.0f, t.backup_reserve_percent / 100.0f));
    const float reserveAngle = (135.0f + 270.0f * reserveFraction) * 0.01745329252f;
    const float reserveCos = cosf(reserveAngle);
    const float reserveSin = sinf(reserveAngle);
    pw_reserve_marker_points_[0] = {
      static_cast<lv_coord_t>(roundf(126.0f + 82.0f * reserveCos)),
      static_cast<lv_coord_t>(roundf(116.0f + 82.0f * reserveSin))
    };
    pw_reserve_marker_points_[1] = {
      static_cast<lv_coord_t>(roundf(126.0f + 114.0f * reserveCos)),
      static_cast<lv_coord_t>(roundf(116.0f + 114.0f * reserveSin))
    };
    lv_line_set_points(pw_reserve_marker_, pw_reserve_marker_points_, 2);

    if (t.has_battery_voltage && t.battery_voltage_v > 0.0f) {
      snprintf(text, sizeof(text), "%.1f V", t.battery_voltage_v);
      lv_label_set_text(pw_voltage_value_, text);
    } else {
      lv_label_set_text(pw_voltage_value_, "—");
    }
    if (t.has_battery_capacity) {
      snprintf(text, sizeof(text), "%.2f", t.nominal_remaining_kwh);
      lv_label_set_text(pw_capacity_current_value_, text);
      snprintf(text, sizeof(text), "/%.2f kWh", t.nominal_full_kwh);
      lv_label_set_text(pw_capacity_total_value_, text);
    } else {
      lv_label_set_text(pw_capacity_current_value_, "--");
      lv_label_set_text(pw_capacity_total_value_, "/-- kWh");
    }
    lv_obj_update_layout(pw_capacity_current_value_);
    lv_obj_update_layout(pw_capacity_current_unit_);
    const int capacityWidth = lv_obj_get_width(pw_capacity_current_value_);
    const int unitWidth = lv_obj_get_width(pw_capacity_current_unit_);
    const int capacityX = (224 - capacityWidth - 6 - unitWidth) / 2;
    lv_obj_set_x(pw_capacity_current_value_, capacityX);
    lv_obj_set_x(pw_capacity_current_unit_, capacityX + capacityWidth + 6);
    lv_obj_update_layout(pw_capacity_total_value_);
    const int capacityRight = capacityX + capacityWidth + 6 + unitWidth;
    lv_obj_set_x(pw_capacity_total_value_, capacityRight - lv_obj_get_width(pw_capacity_total_value_));

    size_t sourceCount = 0, powerwallCount = 0;
    for (size_t i = 0; i < t.history_count; ++i) {
      if (t.history[i].power_source == HistoryPowerSource::Grid ||
          t.history[i].power_source == HistoryPowerSource::Powerwall) {
        ++sourceCount;
        if (t.history[i].power_source == HistoryPowerSource::Powerwall) ++powerwallCount;
      }
    }
    if (sourceCount > 0) {
      snprintf(text, sizeof(text), "%d%%", static_cast<int>(roundf(100.0f * powerwallCount / sourceCount)));
      lv_label_set_text(pw_self_value_, text);
    } else {
      lv_label_set_text(pw_self_value_, "—");
    }

    if (t.has_grid_status) {
      const bool connected = t.grid_status == "SystemGridConnected";
      lv_label_set_text(home_grid_status_, connected ? "已併網" : t.grid_status.c_str());
      lv_obj_set_style_text_color(home_grid_status_, lv_color_hex(connected ? 0x38bdf8 : 0xfbbf24), 0);
    } else {
      lv_label_set_text(home_grid_status_, "—");
    }

    if (t.has_site_power) {
      lv_label_set_text(home_grid_power_title_, t.site_power_w < -60.0f ? "回送功率" : "輸入功率");
      snprintf(text, sizeof(text), "%.2f kW", fabsf(t.site_power_w) / 1000.0f);
      lv_label_set_text(home_grid_power_value_, text);
      lv_obj_set_style_text_color(home_grid_power_value_, lv_color_hex(t.site_power_w < -60.0f ? 0x11e58e : 0x38bdf8), 0);
    } else {
      lv_label_set_text(home_grid_power_value_, "— kW");
      lv_obj_set_style_text_color(home_grid_power_value_, lv_color_hex(0x38bdf8), 0);
    }
    if (t.has_site_current) snprintf(text, sizeof(text), "%.1f A", t.site_current_a);
    else snprintf(text, sizeof(text), "— A");
    lv_label_set_text(home_grid_current_value_, text);
    if (t.has_site_voltage) snprintf(text, sizeof(text), "%.1f V", t.site_voltage_v);
    else snprintf(text, sizeof(text), "— V");
    lv_label_set_text(home_grid_voltage_value_, text);
    if (t.has_site_import_energy) snprintf(text, sizeof(text), "%.2f kWh", t.site_energy_imported_wh / 1000.0f);
    else snprintf(text, sizeof(text), "— kWh");
    lv_label_set_text(home_grid_import_value_, text);
    if (t.has_site_export_energy) snprintf(text, sizeof(text), "%.2f kWh", t.site_energy_exported_wh / 1000.0f);
    else snprintf(text, sizeof(text), "— kWh");
    lv_label_set_text(home_grid_export_value_, text);

    if (t.has_load_power) snprintf(text, sizeof(text), "%.2f kW", t.load_power_w / 1000.0f);
    else snprintf(text, sizeof(text), "— kW");
    lv_label_set_text(home_load_value_, text);
    if (t.has_load_current) snprintf(text, sizeof(text), "%.1f A", t.load_current_a);
    else snprintf(text, sizeof(text), "— A");
    lv_label_set_text(home_load_current_value_, text);
    if (t.has_load_voltage) snprintf(text, sizeof(text), "%.1f V", t.load_voltage_v);
    else snprintf(text, sizeof(text), "— V");
    lv_label_set_text(home_load_voltage_value_, text);
    if (t.has_solar_power) snprintf(text, sizeof(text), "太陽能 %.2f kW", t.solar_power_w / 1000.0f);
    else snprintf(text, sizeof(text), "太陽能 — kW");
    lv_label_set_text(home_solar_value_, text);
    if (t.has_load_power && t.has_load_voltage && t.has_load_current &&
        t.load_voltage_v > 0.0f && t.load_current_a > 0.0f) {
      const float pf = fabsf(t.load_power_w) / (t.load_voltage_v * t.load_current_a);
      snprintf(text, sizeof(text), "功率因數 %.2f", fminf(1.0f, pf));
    } else snprintf(text, sizeof(text), "功率因數 —");
    lv_label_set_text(home_power_factor_, text);
    if (t.has_load_energy) snprintf(text, sizeof(text), "累計用電 %.2f kWh", t.load_energy_imported_wh / 1000.0f);
    else snprintf(text, sizeof(text), "累計用電 — kWh");
    lv_label_set_text(home_energy_value_, text);

    String dataSource = t.demo ? "SIM" : "--";
#if defined(ESP32)
    if (!t.demo) dataSource = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "--";
#endif
    const String stamp = t.timestamp + " · " + dataSource;
    if (pw_data_stamp_) lv_label_set_text(pw_data_stamp_, stamp.c_str());
    lv_label_set_text(home_data_stamp_, stamp.c_str());

    struct tm localTime = {};
    const int minuteOfDay = hasValidLocalTime(localTime)
      ? localTime.tm_hour * 60 + localTime.tm_min : -1;
    if (minuteOfDay >= 0) {
      const int currentX = static_cast<int>(roundf((minuteOfDay / 1440.0f) * (kChartW - 1)));
      lv_obj_set_pos(pw_now_line_, kPwChartPlotX + currentX - 1, kPwChartPlotY);
      lv_obj_set_pos(home_now_line_, 54 + currentX - 1, 39);
    }

    if (dashboard_chart_buf_ && active_dashboard_page_ == 1) {
      renderAreaChartToRgb565(dashboard_chart_buf_, kChartW, kDashboardChartH,
        t.history, t.history_count, true, 100.0f, 17, 229, 142,
        t.backup_reserve_percent, false, 1.0f);
      for (int division = 1; division <= 3; ++division) {
        const int y = static_cast<int>(roundf((division / 4.0f) * (kDashboardChartH - 1)));
        for (int x = 0; x < kChartW; ++x) {
          blendDashboardPixel(dashboard_chart_buf_, kChartW, kDashboardChartH, x, y, 148, 163, 184, 0.12f);
        }
      }
      if (t.history_count > 0) {
        const HistoryPoint& latest = t.history[t.history_count - 1];
        const int x = std::max(0, std::min(kChartW - 1, static_cast<int>(roundf((latest.hour / 24.0f) * (kChartW - 1)))));
        const int y = std::max(2, std::min(kDashboardChartH - 2,
          static_cast<int>(roundf((1.0f - fminf(1.0f, fmaxf(0.0f, latest.soe / 100.0f))) * (kDashboardChartH - 5))) + 2));
        lv_obj_set_pos(pw_now_dot_, kPwChartPlotX + x - 5, kPwChartPlotY + y - 5);
        lv_obj_clear_flag(pw_now_dot_, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(pw_now_dot_, LV_OBJ_FLAG_HIDDEN);
      }
      lv_obj_invalidate(pw_chart_canvas_);
    }

    if (dashboard_chart_buf_ && active_dashboard_page_ == 2) {
      renderHomeLoadChartToRgb565(dashboard_chart_buf_, kChartW, 100, t.history, t.history_count);
      if (t.history_count > 0) {
        const HistoryPoint& latest = t.history[t.history_count - 1];
        const int x = std::max(0, std::min(kChartW - 1, static_cast<int>(roundf((latest.hour / 24.0f) * (kChartW - 1)))));
        const int y = std::max(2, std::min(98, static_cast<int>(roundf((1.0f - fminf(1.0f, fmaxf(0.0f, latest.load_kw / 6.0f))) * 95.0f) + 2)));
        uint32_t color = latest.power_source == HistoryPowerSource::Grid ? 0x38bdf8 :
                         latest.power_source == HistoryPowerSource::Powerwall ? 0x11e58e : 0x94a3b8;
        lv_obj_set_pos(home_now_dot_, 54 + x - 4, 39 + y - 4);
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
    updateTimeAxis(home_time_axis_, boundaries);

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
      const int bandX = 12 + static_cast<int>(roundf((left / 1440.0f) * 668.0f));
      const int bandW = static_cast<int>(roundf(((right - left) / 1440.0f) * 668.0f));
      const uint32_t color = segments[i].peak ? 0xf43f6e : 0x18b76a;
      lv_obj_clear_flag(home_tou_segments_[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_pos(home_tou_segments_[i], bandX, 77);
      lv_obj_set_size(home_tou_segments_[i], bandW, 18);
      lv_obj_set_style_bg_color(home_tou_segments_[i], lv_color_hex(color), 0);
      lv_obj_set_style_bg_opa(home_tou_segments_[i], LV_OPA_COVER, 0);
      lv_label_set_text(home_tou_segment_labels_[i], segments[i].peak ? "尖峰" : "離峰");
      lv_obj_set_pos(home_tou_segment_labels_[i], 0, 1);
      lv_obj_set_width(home_tou_segment_labels_[i], bandW);

      const int chartX = kPwChartPlotX + static_cast<int>(roundf((left / 1440.0f) * kChartW));
      const int chartW = static_cast<int>(roundf(((right - left) / 1440.0f) * kChartW));
      lv_obj_clear_flag(home_chart_tou_segments_[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_pos(home_chart_tou_segments_[i], chartX, 145);
      lv_obj_set_size(home_chart_tou_segments_[i], chartW, 8);
      lv_obj_set_style_bg_color(home_chart_tou_segments_[i], lv_color_hex(color), 0);
      lv_obj_set_style_bg_opa(home_chart_tou_segments_[i], LV_OPA_COVER, 0);
      lv_obj_clear_flag(pw_tou_segments_[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_pos(pw_tou_segments_[i], chartX, kPwChartBandY);
      lv_obj_set_size(pw_tou_segments_[i], chartW, kPwChartTouH);
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
        lv_label_set_text(home_tou_ticks_[i], weekendTicks[i]);
      } else {
        lv_label_set_text(home_tou_ticks_[i], text);
      }
      const int x = 12 + static_cast<int>(roundf((ticks[i] / 1440.0f) * 668.0f));
      const bool firstTick = ticks[i] == 0;
      const bool lastTick = ticks[i] == 1440;
      lv_obj_set_style_text_align(home_tou_ticks_[i], lastTick ? LV_TEXT_ALIGN_RIGHT :
        (firstTick ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_CENTER), 0);
      lv_obj_set_pos(home_tou_ticks_[i], x - (lastTick ? 54 : (firstTick ? 0 : 27)), 99);
      lv_obj_set_width(home_tou_ticks_[i], 54);
    }
  }

  void updateAddedPageClock() {
    if (!page_header_) return;
    const PowerwallTelemetry& t = last_telemetry_;
    if (t.site_name.length() > 0) lv_label_set_text(page_site_, t.site_name.c_str());

    const bool hasData = t.ok || t.last_update_ms > 0;
    const bool stale = !last_configured_ || !hasData || t.stale || !t.ok;
    if (t.demo) {
      lv_label_set_text(page_status_text_, "模擬資料");
      lv_obj_set_style_bg_color(page_status_pill_, lv_color_hex(0x172554), 0);
      lv_obj_set_style_border_color(page_status_pill_, lv_color_hex(0x3b82f6), 0);
      lv_obj_set_style_text_color(page_status_text_, lv_color_hex(0xbfdbfe), 0);
    } else if (stale) {
      lv_label_set_text(page_status_text_, hasData ? "資料過期" : "連線中");
      lv_obj_set_style_bg_color(page_status_pill_, lv_color_hex(0x422006), 0);
      lv_obj_set_style_border_color(page_status_pill_, lv_color_hex(0xf59e0b), 0);
      lv_obj_set_style_text_color(page_status_text_, lv_color_hex(0xfcd34d), 0);
    } else {
      lv_label_set_text(page_status_text_, "即時資料");
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
      char timeText[8], dateText[32], powerwallDate[6];
      static const char* weekdays[] = {"日", "一", "二", "三", "四", "五", "六"};
      snprintf(timeText, sizeof(timeText), "%02d:%02d", localTime.tm_hour, localTime.tm_min);
      snprintf(dateText, sizeof(dateText), "%02d月%02d日 周%s", localTime.tm_mon + 1, localTime.tm_mday, weekdays[localTime.tm_wday]);
      snprintf(powerwallDate, sizeof(powerwallDate), "%02d/%02d", localTime.tm_mon + 1, localTime.tm_mday);
      lv_label_set_text(page_clock_, timeText);
      lv_label_set_text(page_date_, dateText);
      lv_label_set_text(pw_header_clock_, timeText);
      lv_label_set_text(pw_header_date_, powerwallDate);
      updateTouBandLayout(localTime);
      updateTouLabelsAndMarkers(localTime);
    } else {
      lv_label_set_text(page_clock_, t.timestamp.c_str());
      lv_label_set_text(page_date_, "時間同步中");
      lv_label_set_text(pw_header_clock_, "--:--");
      lv_label_set_text(pw_header_date_, "--/--");
      lv_label_set_text(home_tou_pill_text_, "時間同步中");
      lv_label_set_text(home_interval_start_, "--:--");
      lv_label_set_text(home_interval_end_, "--:--");
      lv_label_set_text(home_price_, "—");
      lv_label_set_text(home_countdown_, "下一次切換 —");
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
    lv_label_set_text(home_tou_pill_text_, peak ? "目前：尖峰" : "目前：離峰");
    lv_obj_set_style_text_decor(home_tou_pill_text_, LV_TEXT_DECOR_NONE, 0);
    lv_obj_set_style_bg_color(home_tou_pill_, lv_color_hex(peak ? 0x4c0519 : 0x064e3b), 0);
    lv_obj_set_style_border_color(home_tou_pill_, lv_color_hex(peak ? 0xf43f5e : 0x10b981), 0);
    lv_obj_set_style_text_color(home_tou_pill_text_, lv_color_hex(peak ? 0xfecdd3 : 0x6ee7b7), 0);
    lv_label_set_text(home_interval_start_, intervalStart);
    lv_label_set_text(home_interval_end_, intervalEnd);
    lv_label_set_text(home_price_, price);

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
          snprintf(when, sizeof(when), "明日 %02d:%02d", nextLocal.tm_hour, nextLocal.tm_min);
        } else {
          snprintf(when, sizeof(when), "%02d/%02d %02d:%02d", nextLocal.tm_mon + 1, nextLocal.tm_mday, nextLocal.tm_hour, nextLocal.tm_min);
        }
      } else {
        snprintf(when, sizeof(when), "%02d:%02d", nextLocal.tm_hour, nextLocal.tm_min);
      }
      snprintf(countdown, sizeof(countdown), "下一次%s %s · 還有 %ld小時%02ld分",
               peak ? "離峰" : "尖峰", when, hoursLeft, minsLeft);
      lv_label_set_text(home_countdown_, countdown);
    } else {
      lv_label_set_text(home_countdown_, "下一次切換 —");
    }

    const int minuteOfDay = localTime.tm_hour * 60 + localTime.tm_min;
    const int barX = 12 + static_cast<int>(roundf((minuteOfDay / 1440.0f) * 668.0f));
    lv_obj_set_pos(home_tou_now_line_, barX - 1, 69);
    lv_obj_set_pos(home_tou_now_dot_, barX - 4, 70);
    const int chartX = kPwChartPlotX + static_cast<int>(roundf((minuteOfDay / 1440.0f) * kChartW));
    lv_obj_set_pos(home_now_line_, chartX - 1, 39);
    lv_obj_set_pos(pw_now_line_, chartX - 1, kPwChartPlotY);
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
      if (restart_button_label_) lv_label_set_text(restart_button_label_, "RESTART");
    }
    // Change only opacity so the status pill keeps its size while blinking.
    const bool live = (state_mode_idx_ == 0 || state_mode_idx_ == 1) &&
                      last_configured_ && last_telemetry_.ok &&
                      !last_telemetry_.stale && !last_telemetry_.demo;
#if defined(POWERWALL_UI_480)
    updateHome480Status();
    refreshSettingsUptime();
#else
    lv_obj_set_style_bg_opa(dot_status_,
                           !live || (now_ms / 500UL) % 2 == 0
                             ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
#endif
    if (pw_now_dot_) lv_obj_set_style_opa(pw_now_dot_,
      (now_ms / 500UL) % 2 == 0 ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    if (pw_header_status_dot_) lv_obj_set_style_bg_opa(pw_header_status_dot_,
      (now_ms / 500UL) % 2 == 0 ? LV_OPA_COVER : LV_OPA_30, 0);
    if (home_now_dot_) lv_obj_set_style_opa(home_now_dot_,
      (now_ms / 500UL) % 2 == 0 ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    const lv_opa_t pointOpa = (now_ms / 500UL) % 2 == 0 ? LV_OPA_COVER : LV_OPA_TRANSP;
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
    lv_label_set_text(lbl_weather_, buf);

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
          lv_label_set_text(lbl_live_status_, buf);
          solarW    = t.solar_power_w;
          pwW       = t.battery_power_w;
          gridW     = t.site_power_w;
          loadW     = t.load_power_w;
          soePct    = roundf(t.soe_percent);
          isOffGrid = (t.grid_status != "SystemGridConnected");
        } else {
          lv_label_set_text(lbl_live_status_, configured ? "連線重試中" : "等待配網設定...");
          solarW = 0; pwW = 0; gridW = 0; loadW = 0; soePct = 0; isOffGrid = false;
          lv_label_set_text(lbl_solar_val_, "-- kW");
          lv_label_set_text(lbl_home_val_, "-- kW");
          lv_label_set_text(lbl_pw_kw_, "-- kW");
          lv_obj_clear_flag(lbl_pw_dot_, LV_OBJ_FLAG_HIDDEN);
          lv_obj_add_flag(img_pw_arrow_, LV_OBJ_FLAG_HIDDEN);
          lv_label_set_text(lbl_pw_soc_, "--%");
          lv_label_set_text(lbl_grid_title_, "GRID");
          lv_obj_set_style_text_color(lbl_grid_title_, lv_color_hex(0xa1a1aa), 0);
          lv_label_set_text(lbl_grid_val_, "-- kW");
          lv_obj_set_style_text_color(lbl_grid_val_, lv_color_hex(0xffffff), 0);
          lv_label_set_text(lbl_stat_home_kw_, "-- kW");
          lv_label_set_text(lbl_stat_home_va_, "-- V · -- A");
          lv_label_set_text(lbl_stat_pw_soc_, "--%");
          lv_label_set_text(lbl_stat_pw_kwh_, "-- / -- kWh");
          lv_label_set_text(lbl_stat_pw_eta_, "--");
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
        lv_label_set_text(lbl_live_status_, buf);
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
      lv_label_set_text(lbl_live_status_, "預覽：太陽能充電 (點擊切換)");
      solarW = 5000; pwW = -3200; gridW = 0; loadW = 1800; soePct = 78; isOffGrid = false;
      is_weekend = false; cursor_pct = 52.0f;
    } else if (state_mode_idx_ == 3) { // charge
      lv_obj_set_style_bg_color(dot_status_, lv_color_hex(0x34d399), 0);
      lv_obj_set_style_text_color(lbl_live_status_, lv_color_hex(0x34d399), 0);
      lv_label_set_text(lbl_live_status_, "預覽：電網充電 住家+PW (點擊切換)");
      solarW = 0; pwW = -5000; gridW = 7200; loadW = 2200; soePct = 68; isOffGrid = false;
      is_weekend = false; cursor_pct = 10.4f;
    } else if (state_mode_idx_ == 4) { // peak
      lv_obj_set_style_bg_color(dot_status_, lv_color_hex(0x34d399), 0);
      lv_obj_set_style_text_color(lbl_live_status_, lv_color_hex(0x34d399), 0);
      lv_label_set_text(lbl_live_status_, "預覽：高峰放電 (點擊切換)");
      solarW = 0; pwW = 3200; gridW = 0; loadW = 3200; soePct = 55; isOffGrid = false;
      is_weekend = false; cursor_pct = 75.0f; tou_is_peak = true;
    } else if (state_mode_idx_ == 5) { // low
      lv_obj_set_style_bg_color(dot_status_, lv_color_hex(0x34d399), 0);
      lv_obj_set_style_text_color(lbl_live_status_, lv_color_hex(0x34d399), 0);
      lv_label_set_text(lbl_live_status_, "預覽：備援底線 24% (點擊切換)");
      solarW = 0; pwW = 0; gridW = 1800; loadW = 1800; soePct = 24; isOffGrid = false;
      is_weekend = false; cursor_pct = 92.0f; tou_is_peak = true;
    } else { // offgrid
      lv_obj_set_style_bg_color(dot_status_, lv_color_hex(0x34d399), 0);
      lv_obj_set_style_text_color(lbl_live_status_, lv_color_hex(0x34d399), 0);
      lv_label_set_text(lbl_live_status_, "預覽：斷網備援 地面紅叉叉 (點擊切換)");
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
      lv_label_set_text(lbl_solar_val_, buf);

      snprintf(buf, sizeof(buf), "%.1f kW", loadW / 1000.0f);
      lv_label_set_text(lbl_home_val_, buf);
      lv_label_set_text(lbl_stat_home_kw_, buf);

      snprintf(buf, sizeof(buf), "%.1f V · %.1f A", t.load_voltage_v, t.load_current_a);
      lv_label_set_text(lbl_stat_home_va_, buf);

      if (fabsf(pwW) > 60.0f) {
        snprintf(buf, sizeof(buf), "%.1f kW", fabsf(pwW) / 1000.0f);
      } else {
        snprintf(buf, sizeof(buf), "0 kW");
      }
      lv_label_set_text(lbl_pw_kw_, buf);

      snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(roundf(soePct)));
      lv_label_set_text(lbl_pw_soc_, buf);
      lv_label_set_text(lbl_stat_pw_soc_, buf);

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
      lv_label_set_text(lbl_stat_pw_kwh_, buf);
      formatUsableEta(buf, sizeof(buf), remKwh, fullKwh, reservePct, loadW / 1000.0f, false);
      lv_label_set_text(lbl_stat_pw_eta_, buf);

      snprintf(buf, sizeof(buf), "Powerwall (備援 %d%%)", static_cast<int>(roundf(reservePct)));
      lv_label_set_text(lbl_stat_reserve_, buf);

      if (isOffGrid) {
        lv_label_set_text(lbl_grid_title_, "GRID");
        lv_obj_set_style_text_color(lbl_grid_title_, lv_color_hex(0xfb7185), 0);
        lv_label_set_text(lbl_grid_val_, "孤島供電");
        lv_obj_set_style_text_color(lbl_grid_val_, lv_color_hex(0xfb7185), 0);
      } else {
        lv_label_set_text(lbl_grid_title_, "GRID");
        lv_obj_set_style_text_color(lbl_grid_title_, lv_color_hex(0xa1a1aa), 0);
        if (fabsf(gridW) > 60.0f) {
          snprintf(buf, sizeof(buf), "%.1f kW", gridW / 1000.0f);
        } else {
          snprintf(buf, sizeof(buf), "0 kW");
        }
        lv_label_set_text(lbl_grid_val_, buf);
        lv_obj_set_style_text_color(lbl_grid_val_, lv_color_hex(0xffffff), 0);
      }
    }

    lv_obj_update_layout(chart_card_);
    lv_obj_set_x(lbl_stat_pw_kwh_, lv_obj_get_x(lbl_stat_pw_soc_) + lv_obj_get_width(lbl_stat_pw_soc_) + 7);

    if (t.site_name.length() > 0) {
      lv_label_set_text(lbl_site_title_, t.site_name.c_str());
    }
    lv_label_set_text(lbl_tou_pill_, tou_txt);
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
      lv_label_set_text(lbl_tou_track_, "週末費率");
      lv_obj_set_style_text_color(lbl_tou_track_, lv_color_hex(0x34d399), 0);
      lv_obj_clear_flag(tou_bar_we_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(tou_bar_wk_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(tou_bar_wk_summer_, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_label_set_text(lbl_tou_track_, "週間費率");
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

    int cursor_x = 66 + static_cast<int>(roundf((cursor_pct / 100.0f) * kChartW));
    if (cursor_x < 66) cursor_x = 66;
    if (cursor_x > 66 + kChartW - 2) cursor_x = 66 + kChartW - 2;
    lv_obj_set_pos(cursor_line_, cursor_x - 1, 56);

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
      const int soeY = std::max(3, std::min(kChart2H - 2,
        static_cast<int>(roundf((1.0f - soeNorm) * (kChart2H - 5)) + 3)));
      const uint32_t loadColor = latest.power_source == HistoryPowerSource::Grid ? 0x38bdf8 :
        latest.power_source == HistoryPowerSource::Powerwall ? 0x11e58e : 0x94a3b8;
      lv_obj_set_style_bg_color(chart1_now_dot_, lv_color_hex(loadColor), 0);
      lv_obj_set_pos(chart1_now_dot_, 66 + x - 4, 56 + loadY - 4);
      const int batteryX = std::max(0, std::min(kChart2W - 1,
        static_cast<int>(roundf((latest.hour / 24.0f) * (kChart2W - 1)))));
#if defined(POWERWALL_UI_480)
      // LVGL coordinates are deferred until layout; use the computed point
      // directly rather than reading back the previous frame's position.
      lv_obj_set_pos(chart2_now_dot_, std::min(66 + kChart2W - 9, 66 + batteryX - 4),
                     kHome480ChartY + soeY - 4);
#else
      lv_obj_set_pos(chart2_now_dot_, 66 + batteryX - 4, 140 + soeY - 4);
#endif
      lv_obj_clear_flag(chart1_now_dot_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(chart2_now_dot_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(chart1_now_dot_);
      lv_obj_move_foreground(chart2_now_dot_);
    } else {
      lv_obj_add_flag(chart1_now_dot_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(chart2_now_dot_, LV_OBJ_FLAG_HIDDEN);
    }

#if defined(POWERWALL_UI_480)
    const int home_cursor_x = std::max(66, std::min(66 + kChart2W - 1,
      66 + static_cast<int>(roundf((cursor_pct / 100.0f) * (kChart2W - 1)))));
    lv_obj_set_pos(cursor_line_, std::min(66 + kChart2W - 2, home_cursor_x - 1), kHome480ChartY);
    lv_obj_add_flag(chart1_now_dot_, LV_OBJ_FLAG_HIDDEN);
    updateHome480Status();
#endif
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
      if (x < 540 && !needs_legacy_moon_cleanup) return;
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
          if (moon.x < 540 && !needs_legacy_moon_cleanup) continue;
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

#if defined(POWERWALL_UI_480)
    if (background_changed || phase_effects_changed) updateHome480Backdrop();
#endif
    if (stage_canvas_) {
      if (background_changed) lv_obj_invalidate(stage_canvas_);
      else {
        lv_area_t coords;
        lv_obj_get_coords(stage_canvas_, &coords);
#if defined(POWERWALL_UI_480)
        // LVGL's image zoom also applies to animated dirty rectangles.
        dirty.x1 = dirty.x1 * kHome480Zoom / 256 - 2;
        dirty.y1 = dirty.y1 * kHome480Zoom / 256 - 2;
        dirty.x2 = (dirty.x2 + 1) * kHome480Zoom / 256 + 2;
        dirty.y2 = (dirty.y2 + 1) * kHome480Zoom / 256 + 2;
#endif
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

#if defined(POWERWALL_UI_480)
    auto drawReserve = [&]() {
      if (!is_soe || !std::isfinite(reserve_pct) || reserve_pct < 0.0f) return;
      const float reserve = std::min(100.0f, reserve_pct);
      const int ry = static_cast<int>(roundf((1.0f - reserve / 100.0f) * (h - 4))) + 2;
      // Two logical rows survive the 720 -> 480 nearest-neighbour sampling,
      // including the discarded 3n+2 row. Draw above the green area fill.
      for (int x = 0; x < w; ++x) if ((x / 5) % 2 == 0) {
        blendPx(x, ry,     56, 189, 248, 0.75f);
        blendPx(x, ry + 1, 56, 189, 248, 0.75f);
      }
    };
#else
    if (is_soe && reserve_pct > 0.0f) {
      int ry = static_cast<int>(roundf((1.0f - reserve_pct / 100.0f) * (h - 4))) + 2;
      for (int x = 0; x < w; ++x) {
        if ((x / 5) % 2 == 0) blendPx(x, ry, 56, 189, 248, 0.55f);
      }
    }
#endif

    if (!pts || count == 0) {
#if defined(POWERWALL_UI_480)
      drawReserve();
#endif
      return;
    }

    int last_px = 0, last_py = h - 2;
    for (size_t i = 0; i < count; ++i) {
      float v1 = is_soe ? pts[i].soe : pts[i].load_kw;
      float norm1 = fminf(1.0f, fmaxf(0.0f, v1 / max_y));
      int x1 = static_cast<int>(roundf((pts[i].hour / 24.0f) * (w - 1)));
      if (x1 < 0) x1 = 0;
      if (x1 >= w) x1 = w - 1;
      const int topInset = is_soe ? 3 : 2;
      int y1 = static_cast<int>(roundf((1.0f - norm1) * (h - 5))) + topInset;
      if (y1 < topInset) y1 = topInset;
      if (y1 > h - 2) y1 = h - 2;
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
        int y2 = static_cast<int>(roundf((1.0f - norm2) * (h - 5))) + topInset;
        if (y2 < topInset) y2 = topInset;
        if (y2 > h - 2) y2 = h - 2;
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
            blendPx(x, y_top + 1, segment_r, segment_g, segment_b, 0.85f);
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
      for (int dy = -4; dy <= 4; ++dy) {
        for (int dx = -4; dx <= 4; ++dx) {
          int d2 = dx * dx + dy * dy;
          if (d2 <= 6) {
            blendPx(last_px + dx, last_py + dy, 255, 255, 255, 1.0f);
          } else if (d2 <= 16) {
            blendPx(last_px + dx, last_py + dy, dot_r, dot_g, dot_b, 1.0f);
          }
        }
      }
    }
#if defined(POWERWALL_UI_480)
    drawReserve();
#endif
  }
};

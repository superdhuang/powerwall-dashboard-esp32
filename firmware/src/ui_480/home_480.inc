// Compact homepage uses native framebuffer coordinates and font rasters.
static constexpr int kHome480ChartTop = 379;
static constexpr int kHome480ChartY = 5;
static constexpr int kHome480HeroH = 373;
// Preserve the current house center (source 300,203) to within one logical pixel.
static constexpr int kHome480SceneX = 21;
static constexpr int kHome480SceneY = 35;
static constexpr int kHome480LeftW = 26;
static constexpr int kHome480SkyH = 41;
// Native pixel buffers for the sky strip above and the edge strip left of the
// scene. The scene's first column sits kHome480LeftWPx pixels into the sky strip.
static constexpr int kHome480SkyWPx = 480;
static constexpr int kHome480SkyHPx = 41;
static constexpr int kHome480LeftWPx = 21 - -5;
static constexpr int kHome480LeftHPx = 337;
static constexpr int kHome480BakedEdgePx = 11;
static constexpr int kHome480SkySourceRowsPx = 21;
lv_point_t home480_leader_points_[4][3] = {};
lv_obj_t* home480_sky_ = nullptr;
lv_obj_t* home480_left_edge_ = nullptr;
lv_obj_t* home480_ground_ = nullptr;
lv_color_t* home480_sky_pixels_ = nullptr;
lv_color_t* home480_left_pixels_ = nullptr;
lv_obj_t* home480_clock_ = nullptr;
lv_obj_t* home480_reserve_ = nullptr;
lv_obj_t* home480_axis_top_ = nullptr;
lv_obj_t* home480_axis_bottom_ = nullptr;
lv_obj_t* home480_ticks_[5] = {};
lv_obj_t* home480_wifi_card_ = nullptr;
lv_obj_t* home480_wifi_title_ = nullptr;
lv_obj_t* home480_wifi_network_ = nullptr;
lv_obj_t* home480_wifi_url_ = nullptr;
lv_obj_t* home480_wifi_instruction_ = nullptr;
lv_obj_t* home480_wifi_progress_ = nullptr;
lv_obj_t* home480_wifi_shimmer_ = nullptr;

static void home480WifiShimmerExec(void* var, int32_t value) {
  lv_obj_set_x(static_cast<lv_obj_t*>(var), static_cast<lv_coord_t>(value));
}

static void home480WifiCardFadeExec(void* var, int32_t value) {
  lv_obj_set_style_opa(static_cast<lv_obj_t*>(var), static_cast<lv_opa_t>(value), 0);
}

static lv_obj_t* createHome480WifiLabel(lv_obj_t* parent, const char* text,
                                        int y, int height, int width) {
  auto* label = lv_label_create(parent);
  setLabelText480(label, text);
  lv_obj_set_pos(label, (387 - width) / 2, y);
  lv_obj_set_size(label, width, height);
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(label, native480_fonts::home_cjk_16(), 0);
  lv_obj_set_style_text_color(label, lv_color_hex(0xf1f5f9), 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_clear_flag(label, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  return label;
}

void ensureHome480WifiCard() {
  if (home480_wifi_card_) return;
  auto* screen = lv_scr_act();
  home480_wifi_card_ = lv_obj_create(screen);
  lv_obj_set_size(home480_wifi_card_, 387, 173);
  lv_obj_align(home480_wifi_card_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(home480_wifi_card_, lv_color_hex(0x11161f), 0);
  lv_obj_set_style_bg_opa(home480_wifi_card_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(home480_wifi_card_, lv_color_hex(0x354151), 0);
  lv_obj_set_style_border_width(home480_wifi_card_, 2, 0);
  lv_obj_set_style_radius(home480_wifi_card_, 15, 0);
  lv_obj_set_style_pad_all(home480_wifi_card_, 0, 0);
  lv_obj_clear_flag(home480_wifi_card_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(home480_wifi_card_, LV_OBJ_FLAG_HIDDEN);

  home480_wifi_title_ = createHome480WifiLabel(home480_wifi_card_, "正在連接 Wi-Fi", 25, 29, 347);
  home480_wifi_network_ = createHome480WifiLabel(home480_wifi_card_, "SSID: Home-WiFi", 69, 29, 344);
  home480_wifi_url_ = createHome480WifiLabel(home480_wifi_card_, "http://192.168.4.1", 105, 28, 344);
  home480_wifi_instruction_ = createHome480WifiLabel(home480_wifi_card_, "", 129, 20, 344);
  for (auto* label : {home480_wifi_title_, home480_wifi_network_, home480_wifi_url_})
    lv_obj_set_style_text_font(label, native480_fonts::home_cjk_22(), 0);
  lv_obj_set_style_text_color(home480_wifi_network_, lv_color_hex(0xcbd5e1), 0);
  lv_obj_set_style_text_color(home480_wifi_url_, lv_color_hex(0x94a3b8), 0);
  lv_obj_add_flag(home480_wifi_url_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_text_font(home480_wifi_instruction_, native480_fonts::home_cjk_16(), 0);
  lv_obj_set_style_text_color(home480_wifi_instruction_, lv_color_hex(0xcbd5e1), 0);
  lv_obj_add_flag(home480_wifi_instruction_, LV_OBJ_FLAG_HIDDEN);

  home480_wifi_progress_ = lv_obj_create(home480_wifi_card_);
  lv_obj_set_pos(home480_wifi_progress_, 43, 144);
  lv_obj_set_size(home480_wifi_progress_, 301, 5);
  lv_obj_set_style_bg_color(home480_wifi_progress_, lv_color_hex(0x27313d), 0);
  lv_obj_set_style_bg_opa(home480_wifi_progress_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(home480_wifi_progress_, 0, 0);
  lv_obj_set_style_radius(home480_wifi_progress_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_clip_corner(home480_wifi_progress_, true, 0);
  lv_obj_set_style_pad_all(home480_wifi_progress_, 0, 0);
  lv_obj_clear_flag(home480_wifi_progress_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

  home480_wifi_shimmer_ = lv_obj_create(home480_wifi_progress_);
  lv_obj_set_pos(home480_wifi_shimmer_, -41, 0);
  lv_obj_set_size(home480_wifi_shimmer_, 41, 5);
  lv_obj_set_style_bg_opa(home480_wifi_shimmer_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(home480_wifi_shimmer_, 0, 0);
  lv_obj_set_style_pad_all(home480_wifi_shimmer_, 0, 0);
  lv_obj_clear_flag(home480_wifi_shimmer_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  auto* glow = lv_obj_create(home480_wifi_shimmer_);
  lv_obj_set_size(glow, 41, 5);
  lv_obj_set_style_bg_color(glow, lv_color_hex(0x38bdf8), 0);
  lv_obj_set_style_bg_opa(glow, LV_OPA_50, 0);
  lv_obj_set_style_border_width(glow, 0, 0);
  lv_obj_set_style_radius(glow, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_pad_all(glow, 0, 0);
  lv_obj_clear_flag(glow, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  auto* highlight = lv_obj_create(home480_wifi_shimmer_);
  lv_obj_set_pos(highlight, 13, 0);
  lv_obj_set_size(highlight, 16, 5);
  lv_obj_set_style_bg_color(highlight, lv_color_hex(0xdff6ff), 0);
  lv_obj_set_style_bg_opa(highlight, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(highlight, 0, 0);
  lv_obj_set_style_radius(highlight, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_pad_all(highlight, 0, 0);
  lv_obj_clear_flag(highlight, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
}

void stopHome480WifiShimmer() {
  if (home480_wifi_shimmer_) lv_anim_del(home480_wifi_shimmer_, home480WifiShimmerExec);
}

void hideHome480WifiCard() {
  if (!home480_wifi_card_) return;
  lv_anim_del(home480_wifi_card_, home480WifiCardFadeExec);
  lv_obj_add_flag(home480_wifi_card_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_opa(home480_wifi_card_, LV_OPA_COVER, 0);
  stopHome480WifiShimmer();
  if (home480_wifi_progress_) lv_obj_add_flag(home480_wifi_progress_, LV_OBJ_FLAG_HIDDEN);
}

void startHome480WifiShimmer() {
  if (!home480_wifi_shimmer_) return;
  stopHome480WifiShimmer();
  lv_obj_set_x(home480_wifi_shimmer_, 0);
  lv_anim_t animation;
  lv_anim_init(&animation);
  lv_anim_set_var(&animation, home480_wifi_shimmer_);
  lv_anim_set_values(&animation, 0, 390);
  lv_anim_set_time(&animation, 1100);
  lv_anim_set_playback_time(&animation, 1100);
  lv_anim_set_repeat_count(&animation, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&animation, lv_anim_path_linear);
  lv_anim_set_exec_cb(&animation, home480WifiShimmerExec);
  lv_anim_start(&animation);
}

void showHome480WifiCard() {
  ensureHome480WifiCard();
  lv_anim_del(home480_wifi_card_, home480WifiCardFadeExec);
  lv_obj_clear_flag(home480_wifi_card_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(home480_wifi_card_);
  lv_obj_set_style_opa(home480_wifi_card_, LV_OPA_COVER, 0);
}

void setHome480WifiCardMode(uint8_t mode, const char* ssid, const char* setupUrl) {
  if (mode > 7) mode = 3;
  if (mode == 3) {
    hideHome480WifiCard();
    return;
  }

  ensureHome480WifiCard();
  showHome480WifiCard();
  char network_label[128];
  if (mode == 1) {
    snprintf(network_label, sizeof(network_label), "SSID: powerwall-dashboard");
    setLabelText480(home480_wifi_network_, network_label);
    lv_obj_clear_flag(home480_wifi_network_, LV_OBJ_FLAG_HIDDEN);
  } else if (mode == 6) {
    setLabelText480(home480_wifi_network_, "正在重試");
    lv_obj_clear_flag(home480_wifi_network_, LV_OBJ_FLAG_HIDDEN);
  } else if (ssid && *ssid) {
    snprintf(network_label, sizeof(network_label), "SSID: %s", ssid);
    setLabelText480(home480_wifi_network_, network_label);
    lv_obj_clear_flag(home480_wifi_network_, LV_OBJ_FLAG_HIDDEN);
  } else {
    setLabelText480(home480_wifi_network_, "");
    lv_obj_add_flag(home480_wifi_network_, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_add_flag(home480_wifi_url_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(home480_wifi_instruction_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(home480_wifi_progress_, LV_OBJ_FLAG_HIDDEN);
  stopHome480WifiShimmer();

  if (mode == 0) {
    setLabelText480(home480_wifi_title_, "正在連接 Wi-Fi");
    lv_obj_clear_flag(home480_wifi_progress_, LV_OBJ_FLAG_HIDDEN);
    startHome480WifiShimmer();
  } else if (mode == 1) {
    setLabelText480(home480_wifi_title_, "等待 Wi-Fi 設定");
    setLabelText480(home480_wifi_url_, setupUrl && *setupUrl ? setupUrl : "http://192.168.4.1");
    lv_obj_clear_flag(home480_wifi_url_, LV_OBJ_FLAG_HIDDEN);
  } else if (mode == 7) {
    setLabelText480(home480_wifi_title_, "請設定 Powerwall");
    setLabelText480(home480_wifi_network_, "手機連同一 Wi-Fi 後開啟");
    setLabelText480(home480_wifi_url_, setupUrl && *setupUrl ? setupUrl : "IP pending");
    setLabelText480(home480_wifi_instruction_, "輸入 Gateway IP / 密碼");
    lv_obj_clear_flag(home480_wifi_network_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(home480_wifi_url_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(home480_wifi_instruction_, LV_OBJ_FLAG_HIDDEN);
  } else if (mode == 6) {
    setLabelText480(home480_wifi_title_, "Powerwall 連線失敗");
    setLabelText480(home480_wifi_network_, "檢查 Gateway IP / 密碼");
    setLabelText480(home480_wifi_url_, setupUrl && *setupUrl ? setupUrl : "IP pending");
    setLabelText480(home480_wifi_instruction_, "手機同 Wi-Fi 開啟設定");
    lv_obj_clear_flag(home480_wifi_network_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(home480_wifi_url_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(home480_wifi_instruction_, LV_OBJ_FLAG_HIDDEN);
  } else if (mode == 4 || mode == 5) {
    setLabelText480(home480_wifi_title_, mode == 4
      ? "正在取得Powerwall資料" : "正在取得天氣資料");
    lv_obj_clear_flag(home480_wifi_progress_, LV_OBJ_FLAG_HIDDEN);
    startHome480WifiShimmer();
  } else {
    setLabelText480(home480_wifi_title_, "Wi-Fi 已連線");
    lv_anim_t fade;
    lv_anim_init(&fade);
    lv_anim_set_var(&fade, home480_wifi_card_);
    lv_anim_set_values(&fade, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&fade, 650);
    lv_anim_set_delay(&fade, 180);
    lv_anim_set_exec_cb(&fade, home480WifiCardFadeExec);
    lv_anim_start(&fade);
  }
}

void initializeHome480() {
  lv_obj_set_height(hero_card_, 373);
  lv_obj_add_flag(hero_card_, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_set_style_clip_corner(hero_card_, false, 0);
  for (uint32_t i = 0; i < lv_obj_get_child_cnt(hero_card_); ++i) {
    auto* child = lv_obj_get_child(hero_card_, i);
    if (child != stage_canvas_) lv_obj_add_flag(child, LV_OBJ_FLAG_HIDDEN);
  }
  if (stage_canvas_) {
    lv_img_set_pivot(stage_canvas_, 0, 0);
    lv_img_set_antialias(stage_canvas_, false);
    lv_obj_set_pos(stage_canvas_, 21, 35);
  }
  // Extend the same animated sky into the title area. Only the narrow exposed
  // strips need extra pixels; the house and its effects keep their existing buffer.
  home480_sky_pixels_ = allocPixelBuf(kHome480SkyWPx * kHome480SkyHPx);
  home480_left_pixels_ = allocPixelBuf(kHome480LeftWPx * kHome480LeftHPx);
  if (home480_sky_pixels_) {
    memset(home480_sky_pixels_, 0, kHome480SkyWPx * kHome480SkyHPx * sizeof(lv_color_t));
    home480_sky_ = lv_canvas_create(hero_card_);
    lv_canvas_set_buffer(home480_sky_, home480_sky_pixels_, kHome480SkyWPx, kHome480SkyHPx, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(home480_sky_, -5, -5);
  }
  if (home480_left_pixels_) {
    memset(home480_left_pixels_, 0, kHome480LeftWPx * kHome480LeftHPx * sizeof(lv_color_t));
    home480_left_edge_ = lv_canvas_create(hero_card_);
    lv_canvas_set_buffer(home480_left_edge_, home480_left_pixels_, kHome480LeftWPx, kHome480LeftHPx, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(home480_left_edge_, -5, 35);
  }
  // Extend the ground fill from the stage bottom through the hero card edge.
  const int scene_bottom_px = 35 + kStageH;
  home480_ground_ = lv_obj_create(hero_card_);
  lv_obj_set_pos(home480_ground_, -5, scene_bottom_px);
  lv_obj_set_size(home480_ground_, 480, 373 - scene_bottom_px);
  lv_obj_set_style_bg_opa(home480_ground_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(home480_ground_, 0, 0);
  lv_obj_set_style_radius(home480_ground_, 0, 0);
  lv_obj_clear_flag(home480_ground_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_move_background(home480_ground_);
  auto reveal = [&](lv_obj_t* obj, int x, int y, int width, const lv_font_t* font) {
    if (!obj) return;
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(obj, x, y);
    if (width > 0) lv_obj_set_width(obj, width);
    if (font) lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(0xf1f5f9), 0);
  };
  // A transparent title overlay keeps the animated sky continuous to the top.
  auto* header = lv_obj_create(hero_card_);
  lv_obj_set_pos(header, 0, 0); lv_obj_set_size(header, 469, 60);
  lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_color(header, lv_color_hex(0x0b0c0f), 0);
  lv_obj_set_style_border_width(header, 0, 0); lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0); lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_parent(lbl_site_title_, header);
  lv_obj_set_style_align(lbl_site_title_, LV_ALIGN_TOP_LEFT, 0);
  reveal(lbl_site_title_, 11, 7, 213, native480_fonts::home_cjk_27());
  lv_label_set_long_mode(lbl_site_title_, LV_LABEL_LONG_CLIP);
  home480_clock_ = makeDashboardLabel(header, "--/-- --- --:--", rightAlignedHeaderClockX(header, 201), 11, 201,
    native480_i18n::kEnglish ? native480_fonts::montserrat_16() : native480_fonts::cjk_13(),
    0xf1f5f9, LV_TEXT_ALIGN_RIGHT);
  lv_obj_set_parent(dot_status_, header);
  reveal(dot_status_, 280, 26, 8, nullptr);
  lv_obj_set_size(dot_status_, 8, 8); lv_obj_set_style_radius(dot_status_, LV_RADIUS_CIRCLE, 0);
  positionHeaderDotBeforeClock(home480_clock_, dot_status_);
  // Indicators do not act as hidden preview controls; console previews still work.
  lv_obj_clear_flag(dot_status_, LV_OBJ_FLAG_CLICKABLE);

  auto leader = [&](int index, int text_x, int text_y, int source_x, int source_y) {
    const int anchor_x = kHome480SceneX + source_x;
    const int anchor_y = kHome480SceneY + source_y;
    const int left = std::min(text_x, anchor_x), top = std::min(text_y, anchor_y);
    auto* points = home480_leader_points_[index];
    // Line points are native pixels relative to the line object.
    const int nl = left, nt = top;
    points[0] = {static_cast<lv_coord_t>(text_x - nl), static_cast<lv_coord_t>(text_y - nt)};
    points[1] = {static_cast<lv_coord_t>(text_x - nl), static_cast<lv_coord_t>(anchor_y - nt)};
    points[2] = {static_cast<lv_coord_t>(anchor_x - nl), static_cast<lv_coord_t>(anchor_y - nt)};
    auto* line = lv_line_create(hero_card_);
    lv_obj_set_pos(line, nl, nt);
    lv_line_set_points(line, points, text_x == anchor_x ? 2 : 3);
    lv_obj_set_style_line_color(line, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_line_opa(line, LV_OPA_60, 0);
    lv_obj_set_style_line_width(line, 1, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  };
  makeDashboardLabel(hero_card_, "太陽能", 190, 28, 147,
    native480_i18n::kEnglish ? native480_fonts::montserrat_16() : native480_fonts::grid_cjk_16(),
    0xcbd5e1, LV_TEXT_ALIGN_CENTER);
  reveal(lbl_solar_val_, 190, 47, 147, native480_fonts::montserrat_27());
  leader(0, 263, 77, 242, 104);
  makeDashboardLabel(hero_card_, "住家", 271, 108, 147,
    native480_i18n::kEnglish ? native480_fonts::montserrat_16() : native480_fonts::grid_cjk_16(),
    0xcbd5e1, LV_TEXT_ALIGN_CENTER);
  reveal(lbl_home_val_, 271, 127, 147, native480_fonts::montserrat_27());
  leader(1, 344, 157, 323, 171);

  leader(2, 206, 304, 186, 237);
  reveal(lbl_pw_title_, 68, 305, 207, native480_fonts::montserrat_16());
  reveal(pw_val_row_, 61, 323, 220, nullptr);
  lv_obj_set_height(pw_val_row_, 30);
  lv_obj_set_style_text_font(lbl_pw_kw_, native480_fonts::montserrat_27(), 0);
  lv_obj_set_style_text_font(lbl_pw_soc_, native480_fonts::montserrat_27(), 0);
  reveal(lbl_stat_pw_eta_, 75, 353, 193, native480_fonts::montserrat_16());
  leader(3, 326, 323, 305, 249);
  reveal(lbl_grid_title_, 246, 324, 160, native480_i18n::kEnglish
    ? native480_fonts::montserrat_16() : native480_fonts::grid_cjk_16());
  reveal(lbl_grid_val_, 246, 342, 160, native480_fonts::home_cjk_27());

  lv_obj_set_pos(chart_card_, 0, 379);
  lv_obj_set_size(chart_card_, 480, 95);
  for (uint32_t i = 0; i < lv_obj_get_child_cnt(chart_card_); ++i)
    lv_obj_add_flag(lv_obj_get_child(chart_card_, i), LV_OBJ_FLAG_HIDDEN);
  reveal(chart2_canvas_, 44, 5, 0, nullptr);
  reveal(cursor_line_, 44, 5, 1, nullptr);
  lv_obj_set_height(cursor_line_, 72);
  reveal(tou_bar_wk_, 44, 69, 0, nullptr);
  reveal(tou_bar_wk_summer_, 44, 69, 0, nullptr);
  reveal(tou_bar_we_, 44, 71, 0, nullptr);
  // Use the same inset timeline for the curve and every rate segment.
  for (auto* band : {tou_bar_wk_, tou_bar_wk_summer_, tou_bar_we_}) lv_obj_set_width(band, 428);
  auto resizeSegments = [&](lv_obj_t* band, const float* hours, int count) {
    for (int i = 0; i < count; ++i) {
      auto* segment = lv_obj_get_child(band, i);
      const int left = static_cast<int>(roundf(hours[i] * kChart2W / 24.0f));
      const int right = static_cast<int>(roundf(hours[i + 1] * kChart2W / 24.0f));
      lv_obj_set_x(segment, left); lv_obj_set_width(segment, right - left);
    }
  };
  const float weekdayHours[] = {0, 6, 11, 14, 24};
  const float summerHours[] = {0, 9, 24};
  resizeSegments(tou_bar_wk_, weekdayHours, 4);
  resizeSegments(tou_bar_wk_summer_, summerHours, 2);
  // Keep the long weekday/weekend label hidden; the actual rate band remains.
  // Axis label centers follow the same 3px/2px curve insets as the renderer.
  home480_axis_top_ = makeDashboardLabel(chart_card_, "100%", 0, 0, 40, native480_fonts::montserrat_13(), 0x94a3b8, LV_TEXT_ALIGN_RIGHT);
  home480_axis_bottom_ = makeDashboardLabel(chart_card_, "0%", 0, 57, 40, native480_fonts::montserrat_13(), 0x94a3b8, LV_TEXT_ALIGN_RIGHT);
  home480_reserve_ = makeDashboardLabel(chart_card_, "", 260, 0, 200,
    native480_i18n::kEnglish ? native480_fonts::montserrat_16() : native480_fonts::home_cjk_16(),
    0x7dd3fc, LV_TEXT_ALIGN_RIGHT);
  lv_obj_set_width(home480_reserve_, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(home480_reserve_, lv_color_make(9, 10, 15), 0);
  lv_obj_set_style_bg_opa(home480_reserve_, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(home480_reserve_, 2, 0);
  static const char* ticks[] = {"00", "06", "12", "18", "24"};
  for (int i = 0; i < 5; ++i) {
    int x = 44 + i * (kChart2W - 1) / 4 - 13;
    x = std::max(0, std::min(44 + kChart2W - 27, x));
    home480_ticks_[i] = makeDashboardLabel(chart_card_, ticks[i], x, 79, 27, native480_fonts::montserrat_13(), 0xcbd5e1, LV_TEXT_ALIGN_CENTER);
  }
  updateHome480Status();
}

void updateHome480Status() {
  if (!home480_clock_) return;
  struct tm local = {};
  char clock[20];
  const bool valid = hasValidLocalTime(local);
  ui480::formatHeaderClock(clock, sizeof(clock), valid ? &local : nullptr);
  if (strcmp(lv_label_get_text(home480_clock_), clock) != 0) setLabelText480(home480_clock_, clock);
  positionHeaderDotBeforeClock(home480_clock_, dot_status_);
  if (home480_reserve_) {
    char reserve[40];
    const float reserve_pct = std::isfinite(last_telemetry_.backup_reserve_percent)
      ? std::max(0.0f, std::min(100.0f, last_telemetry_.backup_reserve_percent)) : 24.0f;
    snprintf(reserve, sizeof(reserve), "備援 %d%%", static_cast<int>(roundf(reserve_pct)));
    if (strcmp(lv_label_get_text(home480_reserve_), reserve) != 0) setLabelText480(home480_reserve_, reserve);
    lv_obj_update_layout(home480_reserve_);
    const int height = lv_obj_get_height(home480_reserve_);
    const int line_y = static_cast<int>(roundf((1.0f - reserve_pct / 100.0f) * (kChart2H - 4))) + 2;
    const int label_y = std::max(0, std::min(kChart2H - height, line_y - height - 2));
    lv_obj_set_pos(home480_reserve_, 469 - lv_obj_get_width(home480_reserve_),
                   kHome480ChartY + label_y);
    lv_obj_move_foreground(home480_reserve_);
  }
  const bool live = (state_mode_idx_ == 0 || state_mode_idx_ == 1) && last_configured_ &&
                   last_telemetry_.ok && !last_telemetry_.stale && !last_telemetry_.demo;
  lv_obj_set_style_bg_color(dot_status_, lv_color_hex(live ? 0x34d399 : 0xff5252), 0);
  lv_obj_set_style_bg_opa(dot_status_, (millis() / 500UL) % 2 == 0 ? LV_OPA_COVER : LV_OPA_30, 0);
}

void updateHome480Backdrop() {
  if (!stage_canvas_buf_) return;
  // The shipped scene contains a rounded-card edge in its top-left 16px of
  // sky. Remove that baked border before extending it into a full-bleed header.
  // This region contains no house, moon, solar shading or powerflow pixels.
  for (int y = 0; y < kHome480BakedEdgePx; ++y) {
    const auto sky = stage_canvas_buf_[y * kStageW + kHome480BakedEdgePx];
    for (int x = 0; x < kHome480BakedEdgePx; ++x) stage_canvas_buf_[y * kStageW + x] = sky;
  }
  bool sky_changed = false, edge_changed = false;
  if (home480_sky_pixels_) {
    for (int y = 0; y < kHome480SkyHPx; ++y) {
      // Extend only the original top 32 sky rows. Sampling farther down would
      // mirror the roof or moon into the header when the sky band grows taller.
      const int sy = (kHome480SkyHPx - 1 - y) * (kHome480SkySourceRowsPx - 1) / (kHome480SkyHPx - 1);
      for (int x = 0; x < kHome480SkyWPx; ++x) {
        const int scene_x = x < kHome480LeftWPx ? kHome480LeftWPx - 1 - x : x - kHome480LeftWPx;
        const int sx = std::min(kStageW - 1, scene_x);
        const auto color = stage_canvas_buf_[sy * kStageW + sx];
        auto& pixel = home480_sky_pixels_[y * kHome480SkyWPx + x];
        if (pixel.full != color.full) { pixel = color; sky_changed = true; }
      }
    }
  }
  if (home480_left_pixels_) {
    for (int y = 0; y < kHome480LeftHPx; ++y) {
      const int sy = std::min(kStageH - 1, y);
      for (int x = 0; x < kHome480LeftWPx; ++x) {
        const int sx = std::min(kStageW - 1, kHome480LeftWPx - 1 - x);
        const auto color = stage_canvas_buf_[sy * kStageW + sx];
        auto& pixel = home480_left_pixels_[y * kHome480LeftWPx + x];
        if (pixel.full != color.full) { pixel = color; edge_changed = true; }
      }
    }
  }
  if (sky_changed) lv_obj_invalidate(home480_sky_);
  if (edge_changed) lv_obj_invalidate(home480_left_edge_);
  if (home480_ground_) lv_obj_set_style_bg_color(home480_ground_, stage_canvas_buf_[(kStageH - 1) * kStageW], 0);
}

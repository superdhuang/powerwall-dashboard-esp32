// Compact homepage only. Coordinates remain in the shared 720 logical space;
// fonts are selected for their final 2/3-size presentation on the 480 panel.
static constexpr int kHome480ChartTop = 569;
static constexpr int kHome480ChartY = 8;
static constexpr int kHome480HeroH = 559;
static constexpr int kHome480Zoom = 256; // Original asset scale; shared by the scene and every animated pixel.
// Preserve the current house center (source 300,203) to within one logical pixel.
static constexpr int kHome480SceneX = 32;
static constexpr int kHome480SceneY = 53;
static constexpr int kHome480LeftW = 8 + kHome480SceneX;
static constexpr int kHome480SkyH = 8 + kHome480SceneY;
lv_point_t home480_leader_points_[4][3] = {};
lv_obj_t* home480_sky_ = nullptr;
lv_obj_t* home480_left_edge_ = nullptr;
lv_obj_t* home480_ground_ = nullptr;
lv_color_t* home480_sky_pixels_ = nullptr;
lv_color_t* home480_left_pixels_ = nullptr;
lv_obj_t* home480_clock_ = nullptr;
lv_obj_t* home480_date_ = nullptr;
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
  lv_label_set_text(label, text);
  lv_obj_set_pos(label, (580 - width) / 2, y);
  lv_obj_set_size(label, width, height);
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(label, &font_home_cjk_24, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(0xf1f5f9), 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_clear_flag(label, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  return label;
}

void ensureHome480WifiCard() {
  if (home480_wifi_card_) return;
  auto* screen = lv_scr_act();
  home480_wifi_card_ = lv_obj_create(screen);
  lv_obj_set_size(home480_wifi_card_, 580, 260);
  lv_obj_align(home480_wifi_card_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(home480_wifi_card_, lv_color_hex(0x11161f), 0);
  lv_obj_set_style_bg_opa(home480_wifi_card_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(home480_wifi_card_, lv_color_hex(0x354151), 0);
  lv_obj_set_style_border_width(home480_wifi_card_, 3, 0);
  lv_obj_set_style_radius(home480_wifi_card_, 22, 0);
  lv_obj_set_style_pad_all(home480_wifi_card_, 0, 0);
  lv_obj_clear_flag(home480_wifi_card_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(home480_wifi_card_, LV_OBJ_FLAG_HIDDEN);

  home480_wifi_title_ = createHome480WifiLabel(home480_wifi_card_, "正在連接 Wi-Fi", 37, 44, 520);
  home480_wifi_network_ = createHome480WifiLabel(home480_wifi_card_, "SSID: Home-WiFi", 103, 44, 516);
  home480_wifi_url_ = createHome480WifiLabel(home480_wifi_card_, "http://192.168.4.1", 157, 42, 516);
  home480_wifi_instruction_ = createHome480WifiLabel(home480_wifi_card_, "", 193, 30, 516);
  for (auto* label : {home480_wifi_title_, home480_wifi_network_, home480_wifi_url_})
    lv_obj_set_style_text_font(label, &font_home_cjk_33, 0);
  lv_obj_set_style_text_color(home480_wifi_network_, lv_color_hex(0xcbd5e1), 0);
  lv_obj_set_style_text_color(home480_wifi_url_, lv_color_hex(0x94a3b8), 0);
  lv_obj_add_flag(home480_wifi_url_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_text_font(home480_wifi_instruction_, &font_home_cjk_24, 0);
  lv_obj_set_style_text_color(home480_wifi_instruction_, lv_color_hex(0xcbd5e1), 0);
  lv_obj_add_flag(home480_wifi_instruction_, LV_OBJ_FLAG_HIDDEN);

  home480_wifi_progress_ = lv_obj_create(home480_wifi_card_);
  lv_obj_set_pos(home480_wifi_progress_, 64, 216);
  lv_obj_set_size(home480_wifi_progress_, 452, 8);
  lv_obj_set_style_bg_color(home480_wifi_progress_, lv_color_hex(0x27313d), 0);
  lv_obj_set_style_bg_opa(home480_wifi_progress_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(home480_wifi_progress_, 0, 0);
  lv_obj_set_style_radius(home480_wifi_progress_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_clip_corner(home480_wifi_progress_, true, 0);
  lv_obj_set_style_pad_all(home480_wifi_progress_, 0, 0);
  lv_obj_clear_flag(home480_wifi_progress_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

  home480_wifi_shimmer_ = lv_obj_create(home480_wifi_progress_);
  lv_obj_set_pos(home480_wifi_shimmer_, -62, 0);
  lv_obj_set_size(home480_wifi_shimmer_, 62, 8);
  lv_obj_set_style_bg_opa(home480_wifi_shimmer_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(home480_wifi_shimmer_, 0, 0);
  lv_obj_set_style_pad_all(home480_wifi_shimmer_, 0, 0);
  lv_obj_clear_flag(home480_wifi_shimmer_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  auto* glow = lv_obj_create(home480_wifi_shimmer_);
  lv_obj_set_size(glow, 62, 8);
  lv_obj_set_style_bg_color(glow, lv_color_hex(0x38bdf8), 0);
  lv_obj_set_style_bg_opa(glow, LV_OPA_50, 0);
  lv_obj_set_style_border_width(glow, 0, 0);
  lv_obj_set_style_radius(glow, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_pad_all(glow, 0, 0);
  lv_obj_clear_flag(glow, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  auto* highlight = lv_obj_create(home480_wifi_shimmer_);
  lv_obj_set_pos(highlight, 19, 0);
  lv_obj_set_size(highlight, 24, 8);
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
    lv_label_set_text(home480_wifi_network_, network_label);
    lv_obj_clear_flag(home480_wifi_network_, LV_OBJ_FLAG_HIDDEN);
  } else if (mode == 6) {
    lv_label_set_text(home480_wifi_network_, "正在重試");
    lv_obj_clear_flag(home480_wifi_network_, LV_OBJ_FLAG_HIDDEN);
  } else if (ssid && *ssid) {
    snprintf(network_label, sizeof(network_label), "SSID: %s", ssid);
    lv_label_set_text(home480_wifi_network_, network_label);
    lv_obj_clear_flag(home480_wifi_network_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_label_set_text(home480_wifi_network_, "");
    lv_obj_add_flag(home480_wifi_network_, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_add_flag(home480_wifi_url_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(home480_wifi_instruction_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(home480_wifi_progress_, LV_OBJ_FLAG_HIDDEN);
  stopHome480WifiShimmer();

  if (mode == 0) {
    lv_label_set_text(home480_wifi_title_, "正在連接 Wi-Fi");
    lv_obj_clear_flag(home480_wifi_progress_, LV_OBJ_FLAG_HIDDEN);
    startHome480WifiShimmer();
  } else if (mode == 1) {
    lv_label_set_text(home480_wifi_title_, "等待 Wi-Fi 設定");
    lv_label_set_text(home480_wifi_url_, setupUrl && *setupUrl ? setupUrl : "http://192.168.4.1");
    lv_obj_clear_flag(home480_wifi_url_, LV_OBJ_FLAG_HIDDEN);
  } else if (mode == 7) {
    lv_label_set_text(home480_wifi_title_, "請設定 Powerwall");
    lv_label_set_text(home480_wifi_network_, "手機連同一 Wi-Fi 後開啟");
    lv_label_set_text(home480_wifi_url_, setupUrl && *setupUrl ? setupUrl : "IP pending");
    lv_label_set_text(home480_wifi_instruction_, "輸入 Gateway IP / 密碼");
    lv_obj_clear_flag(home480_wifi_network_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(home480_wifi_url_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(home480_wifi_instruction_, LV_OBJ_FLAG_HIDDEN);
  } else if (mode == 6) {
    lv_label_set_text(home480_wifi_title_, "Powerwall 連線失敗");
    lv_label_set_text(home480_wifi_network_, "檢查 Gateway IP / 密碼");
    lv_label_set_text(home480_wifi_url_, setupUrl && *setupUrl ? setupUrl : "IP pending");
    lv_label_set_text(home480_wifi_instruction_, "手機同 Wi-Fi 開啟設定");
    lv_obj_clear_flag(home480_wifi_network_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(home480_wifi_url_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(home480_wifi_instruction_, LV_OBJ_FLAG_HIDDEN);
  } else if (mode == 4 || mode == 5) {
    lv_label_set_text(home480_wifi_title_, mode == 4
      ? "正在取得Powerwall資料" : "正在取得天氣資料");
    lv_obj_clear_flag(home480_wifi_progress_, LV_OBJ_FLAG_HIDDEN);
    startHome480WifiShimmer();
  } else {
    lv_label_set_text(home480_wifi_title_, "Wi-Fi 已連線");
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
  lv_obj_set_height(hero_card_, kHome480HeroH);
  lv_obj_add_flag(hero_card_, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_set_style_clip_corner(hero_card_, false, 0);
  for (uint32_t i = 0; i < lv_obj_get_child_cnt(hero_card_); ++i) {
    auto* child = lv_obj_get_child(hero_card_, i);
    if (child != stage_canvas_) lv_obj_add_flag(child, LV_OBJ_FLAG_HIDDEN);
  }
  if (stage_canvas_) {
    lv_img_set_pivot(stage_canvas_, 0, 0);
    lv_img_set_zoom(stage_canvas_, kHome480Zoom);
    lv_img_set_antialias(stage_canvas_, false);
    lv_obj_set_pos(stage_canvas_, kHome480SceneX, kHome480SceneY);
  }
  // Extend the same animated sky into the title area. Only the narrow exposed
  // strips need extra pixels; the house and its effects keep their existing buffer.
  home480_sky_pixels_ = allocPixelBuf(720 * kHome480SkyH);
  home480_left_pixels_ = allocPixelBuf(kHome480LeftW * (8 + kHome480HeroH - kHome480SkyH));
  if (home480_sky_pixels_) {
    memset(home480_sky_pixels_, 0, 720 * kHome480SkyH * sizeof(lv_color_t));
    home480_sky_ = lv_canvas_create(hero_card_);
    lv_canvas_set_buffer(home480_sky_, home480_sky_pixels_, 720, kHome480SkyH, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(home480_sky_, -8, -8);
  }
  if (home480_left_pixels_) {
    memset(home480_left_pixels_, 0, kHome480LeftW * (8 + kHome480HeroH - kHome480SkyH) * sizeof(lv_color_t));
    home480_left_edge_ = lv_canvas_create(hero_card_);
    lv_canvas_set_buffer(home480_left_edge_, home480_left_pixels_, kHome480LeftW, 8 + kHome480HeroH - kHome480SkyH, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(home480_left_edge_, -8, kHome480SceneY);
  }
  const int scene_bottom = kHome480SceneY + kStageH * kHome480Zoom / 256;
  home480_ground_ = lv_obj_create(hero_card_);
  lv_obj_set_pos(home480_ground_, -8, scene_bottom);
  lv_obj_set_size(home480_ground_, 720, kHome480HeroH - scene_bottom);
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
  lv_obj_set_pos(header, 0, 0); lv_obj_set_size(header, 704, 90);
  lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_color(header, lv_color_hex(0x0b0c0f), 0);
  lv_obj_set_style_border_width(header, 0, 0); lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0); lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_parent(lbl_site_title_, header);
  lv_obj_set_style_align(lbl_site_title_, LV_ALIGN_TOP_LEFT, 0);
  reveal(lbl_site_title_, 16, 10, 430, &font_home_cjk_40);
  lv_label_set_long_mode(lbl_site_title_, LV_LABEL_LONG_CLIP);
  home480_clock_ = makeDashboardLabel(header, "--:--", 540, 12, 112,
                                    &font_home_cjk_30, 0xf1f5f9, LV_TEXT_ALIGN_RIGHT);
  home480_date_ = makeDashboardLabel(header, "--/--", 540, 48, 112,
                                    &lv_font_montserrat_24, 0x94a3b8, LV_TEXT_ALIGN_RIGHT);
  lv_obj_set_parent(dot_status_, header);
  reveal(dot_status_, 674, 25, 12, nullptr);
  lv_obj_set_size(dot_status_, 12, 12); lv_obj_set_style_radius(dot_status_, LV_RADIUS_CIRCLE, 0);
  // Indicators do not act as hidden preview controls; console previews still work.
  lv_obj_clear_flag(dot_status_, LV_OBJ_FLAG_CLICKABLE);

  auto leader = [&](int index, int text_x, int text_y, int source_x, int source_y) {
    const int anchor_x = kHome480SceneX + source_x * kHome480Zoom / 256;
    const int anchor_y = kHome480SceneY + source_y * kHome480Zoom / 256;
    const int left = std::min(text_x, anchor_x), top = std::min(text_y, anchor_y);
    auto* points = home480_leader_points_[index];
    points[0] = {static_cast<lv_coord_t>(text_x - left), static_cast<lv_coord_t>(text_y - top)};
    points[1] = {static_cast<lv_coord_t>(text_x - left), static_cast<lv_coord_t>(anchor_y - top)};
    points[2] = {static_cast<lv_coord_t>(anchor_x - left), static_cast<lv_coord_t>(anchor_y - top)};
    auto* line = lv_line_create(hero_card_);
    lv_obj_set_pos(line, left, top);
    lv_line_set_points(line, points, text_x == anchor_x ? 2 : 3);
    lv_obj_set_style_line_color(line, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_line_opa(line, LV_OPA_60, 0);
    lv_obj_set_style_line_width(line, 2, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  };
  makeDashboardLabel(hero_card_, "SOLAR", 285, 42, 220, &lv_font_montserrat_24, 0xcbd5e1, LV_TEXT_ALIGN_CENTER);
  reveal(lbl_solar_val_, 285, 70, 220, &lv_font_montserrat_40);
  leader(0, 395, 116, 363, 156);
  makeDashboardLabel(hero_card_, "HOME", 406, 162, 220, &lv_font_montserrat_24, 0xcbd5e1, LV_TEXT_ALIGN_CENTER);
  reveal(lbl_home_val_, 406, 190, 220, &lv_font_montserrat_40);
  leader(1, 516, 236, 484, 256);

  leader(2, 309, 456, 279, 356);
  reveal(lbl_pw_title_, 102, 458, 310, &lv_font_montserrat_24);
  reveal(pw_val_row_, 92, 485, 330, nullptr);
  lv_obj_set_height(pw_val_row_, 45);
  lv_obj_set_style_text_font(lbl_pw_kw_, &lv_font_montserrat_40, 0);
  lv_obj_set_style_text_font(lbl_pw_soc_, &lv_font_montserrat_40, 0);
  reveal(lbl_stat_pw_eta_, 112, 530, 290, &lv_font_montserrat_24);
  leader(3, 489, 484, 457, 373);
  reveal(lbl_grid_title_, 369, 486, 240, &lv_font_montserrat_24);
  reveal(lbl_grid_val_, 369, 513, 240, &font_home_cjk_40);

  lv_obj_set_pos(chart_card_, 0, kHome480ChartTop);
  lv_obj_set_size(chart_card_, 720, 143);
  for (uint32_t i = 0; i < lv_obj_get_child_cnt(chart_card_); ++i)
    lv_obj_add_flag(lv_obj_get_child(chart_card_, i), LV_OBJ_FLAG_HIDDEN);
  reveal(chart2_canvas_, 66, kHome480ChartY, 0, nullptr);
  reveal(cursor_line_, 66, kHome480ChartY, 2, nullptr);
  lv_obj_set_height(cursor_line_, 108);
  reveal(tou_bar_wk_, 66, 104, 0, nullptr);
  reveal(tou_bar_wk_summer_, 66, 104, 0, nullptr);
  reveal(tou_bar_we_, 66, 107, 0, nullptr);
  // Use the same inset timeline for the curve and every rate segment.
  for (auto* band : {tou_bar_wk_, tou_bar_wk_summer_, tou_bar_we_}) lv_obj_set_width(band, kChart2W);
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
  const int axis_half_height = lv_font_montserrat_20.line_height / 2;
  home480_axis_top_ = makeDashboardLabel(chart_card_, "100%", 0,
    kHome480ChartY + 3 - axis_half_height, 60, &lv_font_montserrat_20, 0x94a3b8, LV_TEXT_ALIGN_RIGHT);
  home480_axis_bottom_ = makeDashboardLabel(chart_card_, "0%", 0,
    kHome480ChartY + kChart2H - 2 - axis_half_height, 60, &lv_font_montserrat_20, 0x94a3b8, LV_TEXT_ALIGN_RIGHT);
  home480_reserve_ = makeDashboardLabel(chart_card_, "", 390, 0, 300,
                         &lv_font_montserrat_24, 0x7dd3fc, LV_TEXT_ALIGN_RIGHT);
  lv_obj_set_width(home480_reserve_, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(home480_reserve_, lv_color_make(9, 10, 15), 0);
  lv_obj_set_style_bg_opa(home480_reserve_, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(home480_reserve_, 3, 0);
  static const char* ticks[] = {"00", "06", "12", "18", "24"};
  for (int i = 0; i < 5; ++i) {
    int x = 66 + i * (kChart2W - 1) / 4 - 20;
    x = std::max(0, std::min(66 + kChart2W - 40, x));
    home480_ticks_[i] = makeDashboardLabel(chart_card_, ticks[i], x, 118, 40, &lv_font_montserrat_20, 0xcbd5e1, LV_TEXT_ALIGN_CENTER);
  }
  updateHome480Status();
}

void updateHome480Status() {
  if (!home480_clock_) return;
  struct tm local = {};
  char clock[8] = "--:--";
  char date[6] = "--/--";
  if (getLocalTime(&local, 0) && local.tm_year >= 120) {
    snprintf(clock, sizeof(clock), "%02d:%02d", local.tm_hour, local.tm_min);
    snprintf(date, sizeof(date), "%02d/%02d", local.tm_mon + 1, local.tm_mday);
  }
  if (strcmp(lv_label_get_text(home480_clock_), clock) != 0) lv_label_set_text(home480_clock_, clock);
  if (home480_date_ && strcmp(lv_label_get_text(home480_date_), date) != 0)
    lv_label_set_text(home480_date_, date);
  if (home480_reserve_) {
    char reserve[40];
    const float reserve_pct = std::isfinite(last_telemetry_.backup_reserve_percent)
      ? std::max(0.0f, std::min(100.0f, last_telemetry_.backup_reserve_percent)) : 24.0f;
    snprintf(reserve, sizeof(reserve), "Reserve %d%%", static_cast<int>(roundf(reserve_pct)));
    if (strcmp(lv_label_get_text(home480_reserve_), reserve) != 0) lv_label_set_text(home480_reserve_, reserve);
    lv_obj_update_layout(home480_reserve_);
    const int height = lv_obj_get_height(home480_reserve_);
    const int line_y = static_cast<int>(roundf((1.0f - reserve_pct / 100.0f) * (kChart2H - 4))) + 2;
    const int label_y = std::max(0, std::min(kChart2H - height, line_y - height - 2));
    lv_obj_set_pos(home480_reserve_, 66 + kChart2W - lv_obj_get_width(home480_reserve_) - 5,
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
  for (int y = 0; y < 16; ++y) {
    const auto sky = stage_canvas_buf_[y * kStageW + 16];
    for (int x = 0; x < 16; ++x) stage_canvas_buf_[y * kStageW + x] = sky;
  }
  bool sky_changed = false, edge_changed = false;
  if (home480_sky_pixels_) {
    for (int y = 0; y < kHome480SkyH; ++y) {
      // Extend only the original top 32 sky rows. Sampling farther down would
      // mirror the roof or moon into the header when the sky band grows taller.
      const int sy = (kHome480SkyH - 1 - y) * 31 / (kHome480SkyH - 1);
      for (int x = 0; x < 720; ++x) {
        const int scene_x = x < kHome480LeftW ? kHome480LeftW - 1 - x : x - kHome480LeftW;
        const int sx = std::min(kStageW - 1, scene_x * 256 / kHome480Zoom);
        const auto color = stage_canvas_buf_[sy * kStageW + sx];
        auto& pixel = home480_sky_pixels_[y * 720 + x];
        if (pixel.full != color.full) { pixel = color; sky_changed = true; }
      }
    }
  }
  if (home480_left_pixels_) {
    for (int y = 0; y < 8 + kHome480HeroH - kHome480SkyH; ++y) {
      const int sy = std::min(kStageH - 1, y * 256 / kHome480Zoom);
      for (int x = 0; x < kHome480LeftW; ++x) {
        const int sx = std::min(kStageW - 1, (kHome480LeftW - 1 - x) * 256 / kHome480Zoom);
        const auto color = stage_canvas_buf_[sy * kStageW + sx];
        auto& pixel = home480_left_pixels_[y * kHome480LeftW + x];
        if (pixel.full != color.full) { pixel = color; edge_changed = true; }
      }
    }
  }
  if (sky_changed) lv_obj_invalidate(home480_sky_);
  if (edge_changed) lv_obj_invalidate(home480_left_edge_);
  if (home480_ground_) lv_obj_set_style_bg_color(home480_ground_, stage_canvas_buf_[(kStageH - 1) * kStageW], 0);
}

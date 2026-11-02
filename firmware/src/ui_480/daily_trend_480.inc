// Today trend page shared by the 480 S3 firmware and native480 simulator.
DailyTrendHistory daily_trend_history_;
lv_obj_t* daily_trend_metric_icon_[4] = {};
lv_obj_t* daily_trend_metric_title_[4] = {};
lv_obj_t* daily_trend_metric_value_[4] = {};
lv_obj_t* daily_trend_metric_unit_[4] = {};
lv_obj_t* daily_trend_metric_subtitle_[4] = {};
lv_obj_t* daily_trend_rows_[4] = {};
lv_obj_t* daily_trend_canvas_[4] = {};
lv_color_t* daily_trend_buffers_[4] = {};
lv_obj_t* daily_trend_title_[4] = {};
lv_obj_t* daily_trend_axis_[4][3] = {};
lv_obj_t* daily_trend_latest_dot_[4] = {};
lv_obj_t* daily_trend_now_glow_ = nullptr;
lv_obj_t* daily_trend_now_line_ = nullptr;
lv_obj_t* daily_trend_tou_segments_[4] = {};
lv_obj_t* daily_trend_tou_ticks_[5] = {};
lv_obj_t* daily_trend_house_icon_lines_[5] = {};
lv_obj_t* daily_trend_grid_icon_lines_[6] = {};
static constexpr int kDailyTrendPlotX = 36;
static constexpr int kDailyTrendPagePlotX = 44;
static constexpr int kDailyTrendPlotW = 428;
static constexpr int kDailyTrendPlotH = 80;
static constexpr int kDailyTrendSoePlotH = 66;
static constexpr int kDailyTrendPlotY = 0;
static constexpr int kDailyTrendSoePlotY = 14;
static constexpr int kDailyTrendCursorY = 100;
static constexpr int kDailyTrendCursorHeight = 358;
static constexpr int kDailyTrendTouBarY = 450;
static constexpr int kDailyTrendTouBarH = 4;
static constexpr int kDailyTrendTimeTicksY = 459;
static constexpr int kDailyTrendRowY[4] = {100, 188, 276, 364};
static constexpr int kDailyTrendRowH = 82;
static constexpr int kDailyTrendMetricWidth = 116;
static constexpr int kDailyTrendMetricInset = 5;
static constexpr int kDailyTrendMetricValueWidth = 106;
static constexpr int kDailyTrendMetricTitleX = 25;
static constexpr int kDailyTrendMetricIconWidth = 17;
static constexpr int kDailyTrendMetricValueY = 22;
static constexpr int kDailyTrendMetricUnitY = 40;
static constexpr int kDailyTrendMetricSubtitleY = 59;
static constexpr uint32_t kDailyTrendColors[4] = {0xfbbf24, 0x38bdf8, 0xe879f9, 0x34d399};
static constexpr const char* kDailyTrendTitles[4] = {"太陽能", "住家用電", "電網功率", "電池電量"};
static constexpr const char* kDailyTrendMetricIcons[4] = {"\uf185", nullptr, nullptr, "\uf240"};
static constexpr const char* kDailyTrendChartTitles[4] = {"太陽能 kW", "住家用電 kW", "電網功率 kW", "電池電量 %"};
static constexpr const char* kDailyTrendTimeTicks[5] = {"00:00", "06:00", "12:00", "18:00", "24:00"};
static constexpr lv_point_t kHomeGridIconGridLines[6][2] = {
  {{7, 3}, {13, 3}}, {{10, 0}, {10, 19}}, {{8, 3}, {2, 19}},
  {{12, 3}, {18, 19}}, {{3, 11}, {17, 11}}, {{2, 19}, {18, 19}},
};
static constexpr lv_point_t kHomeGridIconHouseLines[5][2] = {
  {{0, 8}, {9, 1}}, {{9, 1}, {19, 8}}, {{3, 7}, {3, 19}},
  {{16, 7}, {16, 19}}, {{3, 19}, {16, 19}},
};

static int dailyTrendPlotHeight(int series) {
  return series == 3 ? kDailyTrendSoePlotH : kDailyTrendPlotH;
}

static int dailyTrendPlotOffsetY(int series) {
  return series == 3 ? kDailyTrendSoePlotY : kDailyTrendPlotY;
}

static int dailyTrendBucketIndex(float hour) {
  if (!std::isfinite(hour) || hour < 0.0f || hour >= 24.0f) return -1;
  const int bucket = static_cast<int>(hour * 12.0f + 0.001f);
  return bucket >= 0 && bucket < static_cast<int>(DailyTrendHistory::kCapacity) ? bucket : -1;
}

static float dailyTrendBucketStartHour(int bucket) {
  return bucket >= 0 && bucket < static_cast<int>(DailyTrendHistory::kCapacity)
    ? static_cast<float>(bucket) / 12.0f : -1.0f;
}

static bool dailyTrendPlotBucketsConnect(int previousBucket, int currentBucket) {
  const int gap = currentBucket - previousBucket;
  return gap > 0 && gap <= 3; // Up to 15 minutes, inclusive.
}

static void formatDailyTrendGridEnergySubtitle(bool importValid, float importedWh,
                                                bool exportValid, float exportedWh,
                                                char* text, size_t textSize) {
  char imported[20] = "—";
  char exported[20] = "—";
  if (importValid && std::isfinite(importedWh) && importedWh >= 0.0f)
    snprintf(imported, sizeof(imported), "%.1f", importedWh / 1000.0f);
  if (exportValid && std::isfinite(exportedWh) && exportedWh >= 0.0f)
    snprintf(exported, sizeof(exported), "%.1f", exportedWh / 1000.0f);
  snprintf(text, textSize, native480_i18n::kEnglish ? "In %s/Out %skWh" : "購%s／回%skWh",
           imported, exported);
}

static int dailyTrendUtf8GlyphCount(const char* text) {
  int count = 0;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p)
    if ((*p & 0xc0U) != 0x80U) ++count;
  return count;
}

static const lv_font_t* dailyTrendGridEnergySubtitleFont(const char* text) {
  const lv_font_t* regular = native480_fonts::cjk_12();
  auto fitsWithAtMostOnePixelSpacing = [text](const lv_font_t* font) {
    lv_point_t extent{};
    lv_txt_get_size(&extent, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return extent.x - std::max(0, dailyTrendUtf8GlyphCount(text) - 1) <= kDailyTrendMetricValueWidth;
  };
  lv_point_t regularExtent{};
  lv_txt_get_size(&regularExtent, text, regular, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  if (regularExtent.x <= kDailyTrendMetricValueWidth || fitsWithAtMostOnePixelSpacing(regular)) return regular;
  const lv_font_t* compact = native480_fonts::cjk_9();
  if (fitsWithAtMostOnePixelSpacing(compact)) return compact;
  return native480_fonts::cjk_8();
}

static int dailyTrendGridEnergySubtitleLetterSpace(const char* text, const lv_font_t* font) {
  lv_point_t extent{};
  lv_txt_get_size(&extent, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  return extent.x <= kDailyTrendMetricValueWidth ? 0 : -1;
}

static int dailyTrendTickPixel(int tick) {
  return static_cast<int>(roundf(tick * (kDailyTrendPlotW - 1) / 4.0f));
}

static int dailyTrendAxisTickLabelX(int tick) {
  int homeLabelX = kDailyTrendPagePlotX + tick * (kDailyTrendPlotW - 1) / 4 - 13;
  homeLabelX = std::max(0, std::min(kDailyTrendPagePlotX + kDailyTrendPlotW - 27, homeLabelX));
  return homeLabelX + 13 - 20;
}

static lv_obj_t* makeHomeGridIconLine(lv_obj_t* parent, const lv_point_t* points,
                                      int x, int y, uint32_t color) {
  lv_obj_t* line = lv_line_create(parent);
  lv_line_set_points(line, points, 2);
  lv_obj_set_pos(line, x, y);
  lv_obj_set_style_line_color(line, lv_color_hex(color), 0);
  lv_obj_set_style_line_width(line, 2, 0);
  lv_obj_set_style_line_rounded(line, true, 0);
  lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  return line;
}

bool simulatorDailyTrendMetricIconIsValid(int index) const {
  if (index == 0 || index == 3) {
    return daily_trend_metric_icon_[index] &&
      lv_obj_get_x(daily_trend_metric_icon_[index]) == index * kDailyTrendMetricWidth + kDailyTrendMetricInset &&
      kDailyTrendMetricIcons[index] &&
      !strcmp(lv_label_get_text(daily_trend_metric_icon_[index]), kDailyTrendMetricIcons[index]);
  }
  if (index == 1) {
    for (int i = 0; i < 5; ++i) {
      auto* line = daily_trend_house_icon_lines_[i];
      if (!line || lv_obj_get_x(line) != index * kDailyTrendMetricWidth + kDailyTrendMetricInset - 1 ||
          lv_obj_get_y(line) != 2) return false;
    }
    return true;
  }
  if (index == 2) {
    for (int i = 0; i < 6; ++i) {
      auto* line = daily_trend_grid_icon_lines_[i];
      if (!line || lv_obj_get_x(line) != index * kDailyTrendMetricWidth + kDailyTrendMetricInset - 2 ||
          lv_obj_get_y(line) != 2) return false;
    }
    return true;
  }
  return false;
}

void initializeDailyTrendPage() {
  auto* metrics = makeDashboardCard(page_today_trend_, 8, 8, 464, 88);
  for (int i = 0; i < 4; ++i) {
    const int x = i * kDailyTrendMetricWidth;
    if (i) makeDashboardRule(metrics, x, 4, 1, 76);
    const uint32_t color = kDailyTrendColors[i];
    const char* title = i == 3 ? "Powerwall" : kDailyTrendTitles[i];
    if (i == 1) {
      for (int n = 0; n < 5; ++n)
        daily_trend_house_icon_lines_[n] = makeHomeGridIconLine(metrics, kHomeGridIconHouseLines[n], x + 4, 2, color);
    } else if (i == 2) {
      for (int n = 0; n < 6; ++n)
        daily_trend_grid_icon_lines_[n] = makeHomeGridIconLine(metrics, kHomeGridIconGridLines[n], x + 3, 2, color);
    } else {
      daily_trend_metric_icon_[i] = makeDashboardLabel(metrics, kDailyTrendMetricIcons[i], x + kDailyTrendMetricInset, 2,
        kDailyTrendMetricIconWidth, native480_fonts::montserrat_16(), color, LV_TEXT_ALIGN_CENTER);
    }
    daily_trend_metric_title_[i] = makeDashboardLabel(metrics, title, x + kDailyTrendMetricTitleX, i < 3 ? -1 : 2,
      kDailyTrendMetricWidth - kDailyTrendMetricTitleX - kDailyTrendMetricInset,
      i == 3 ? native480_fonts::montserrat_16() : native480_fonts::grid_cjk_20(), color);
    daily_trend_metric_value_[i] = makeDashboardLabel(metrics, "—", x + kDailyTrendMetricInset, kDailyTrendMetricValueY,
      kDailyTrendMetricValueWidth, native480_fonts::montserrat_32(), color);
    if (i < 3) {
      daily_trend_metric_unit_[i] = makeDashboardLabel(metrics, "kW", x + kDailyTrendMetricInset, kDailyTrendMetricUnitY, 0,
        native480_fonts::montserrat_13(), color);
      lv_obj_add_flag(daily_trend_metric_unit_[i], LV_OBJ_FLAG_HIDDEN);
    }
    const char* subtitle = i == 0 ? "今日 —kWh\n等待樣本" :
      i == 1 ? "--V\n--A" : i == 2 ? "購電 +\n回送 -" : "容量資料不足\n備援資料不足";
    daily_trend_metric_subtitle_[i] = makeDashboardLabel(metrics, subtitle, x + kDailyTrendMetricInset, kDailyTrendMetricSubtitleY,
      kDailyTrendMetricValueWidth, native480_fonts::cjk_12(), 0x94a3b8);
    lv_obj_set_height(daily_trend_metric_subtitle_[i], 30);
  }

  for (int i = 0; i < 4; ++i) {
    auto* row = daily_trend_rows_[i] = makeDashboardCard(page_today_trend_, 8, kDailyTrendRowY[i], 464, kDailyTrendRowH);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x0d131a), 0);
    const int plotHeight = dailyTrendPlotHeight(i);
    const int plotOffsetY = dailyTrendPlotOffsetY(i);
    const int axisFontHeight = native480_fonts::montserrat_12()->line_height;
    const int axisLabelY[3] = {plotOffsetY, plotOffsetY + (plotHeight - axisFontHeight) / 2,
                               plotOffsetY + plotHeight - axisFontHeight};
    daily_trend_axis_[i][0] = makeDashboardLabel(row, "", -1, axisLabelY[0], 33,
      native480_fonts::montserrat_12(), 0x718096, LV_TEXT_ALIGN_RIGHT);
    daily_trend_axis_[i][1] = makeDashboardLabel(row, "", -1, axisLabelY[1], 33,
      native480_fonts::montserrat_12(), 0x718096, LV_TEXT_ALIGN_RIGHT);
    daily_trend_axis_[i][2] = makeDashboardLabel(row, "", -1, axisLabelY[2], 33,
      native480_fonts::montserrat_12(), 0x718096, LV_TEXT_ALIGN_RIGHT);
    daily_trend_buffers_[i] = allocPixelBuf(kDailyTrendPlotW * plotHeight);
    if (daily_trend_buffers_[i]) {
      for (int p = 0; p < kDailyTrendPlotW * plotHeight; ++p)
        daily_trend_buffers_[i][p] = lv_color_hex(0x090e13);
      daily_trend_canvas_[i] = lv_canvas_create(row);
      lv_canvas_set_buffer(daily_trend_canvas_[i], daily_trend_buffers_[i],
                           kDailyTrendPlotW, plotHeight, LV_IMG_CF_TRUE_COLOR);
      lv_obj_set_size(daily_trend_canvas_[i], kDailyTrendPlotW, plotHeight);
      lv_obj_set_pos(daily_trend_canvas_[i], kDailyTrendPlotX, plotOffsetY);
      lv_obj_clear_flag(daily_trend_canvas_[i], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    }
    daily_trend_title_[i] = makeDashboardLabel(page_today_trend_, kDailyTrendChartTitles[i],
      kDailyTrendPagePlotX + kDailyTrendPlotW - 124,
      i == 3 ? kDailyTrendRowY[i] - 3 : kDailyTrendRowY[i] + 1, 124,
      native480_fonts::cjk_13(), kDailyTrendColors[i], LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_style_bg_color(daily_trend_title_[i], lv_color_hex(0x090e13), 0);
    lv_obj_set_style_bg_opa(daily_trend_title_[i], i == 3 ? LV_OPA_TRANSP : LV_OPA_70, 0);
    lv_obj_set_style_border_width(daily_trend_title_[i], 0, 0);
    lv_obj_set_style_pad_hor(daily_trend_title_[i], 2, 0);
    lv_obj_clear_flag(daily_trend_title_[i], LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    // Latest valid observations are page-level siblings so they can be raised
    // above the shared page cursor without inheriting a row-local offset.
    daily_trend_latest_dot_[i] = lv_obj_create(page_today_trend_);
    lv_obj_set_size(daily_trend_latest_dot_[i], 6, 6);
    lv_obj_set_style_radius(daily_trend_latest_dot_[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(daily_trend_latest_dot_[i], lv_color_hex(kDailyTrendColors[i]), 0);
    lv_obj_set_style_bg_opa(daily_trend_latest_dot_[i], LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(daily_trend_latest_dot_[i], lv_color_hex(0xf8fafc), 0);
    lv_obj_set_style_border_width(daily_trend_latest_dot_[i], 1, 0);
    lv_obj_add_flag(daily_trend_latest_dot_[i], LV_OBJ_FLAG_HIDDEN);
  }

  for (int i = 0; i < 4; ++i) {
    daily_trend_tou_segments_[i] = lv_obj_create(page_today_trend_);
    lv_obj_set_pos(daily_trend_tou_segments_[i], kDailyTrendPagePlotX, kDailyTrendTouBarY);
    lv_obj_set_size(daily_trend_tou_segments_[i], 1, kDailyTrendTouBarH);
    lv_obj_set_style_border_width(daily_trend_tou_segments_[i], 0, 0);
    lv_obj_set_style_radius(daily_trend_tou_segments_[i], 0, 0);
    lv_obj_clear_flag(daily_trend_tou_segments_[i], LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  }
  for (int i = 0; i < 5; ++i) {
    daily_trend_tou_ticks_[i] = makeDashboardLabel(page_today_trend_, kDailyTrendTimeTicks[i],
      dailyTrendAxisTickLabelX(i), kDailyTrendTimeTicksY, 40,
      native480_fonts::montserrat_12(), 0x94a3b8, LV_TEXT_ALIGN_CENTER);
  }

  daily_trend_now_glow_ = lv_obj_create(page_today_trend_);
  lv_obj_set_size(daily_trend_now_glow_, 5, kDailyTrendCursorHeight);
  lv_obj_set_pos(daily_trend_now_glow_, kDailyTrendPagePlotX - 2, kDailyTrendCursorY);
  lv_obj_set_style_bg_color(daily_trend_now_glow_, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_bg_opa(daily_trend_now_glow_, LV_OPA_20, 0);
  lv_obj_set_style_radius(daily_trend_now_glow_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(daily_trend_now_glow_, 0, 0);
  lv_obj_clear_flag(daily_trend_now_glow_, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  daily_trend_now_line_ = lv_obj_create(page_today_trend_);
  lv_obj_set_size(daily_trend_now_line_, 1, kDailyTrendCursorHeight);
  lv_obj_set_pos(daily_trend_now_line_, kDailyTrendPagePlotX, kDailyTrendCursorY);
  lv_obj_set_style_bg_color(daily_trend_now_line_, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_bg_opa(daily_trend_now_line_, LV_OPA_70, 0);
  lv_obj_set_style_border_width(daily_trend_now_line_, 0, 0);
  lv_obj_clear_flag(daily_trend_now_line_, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  refreshDailyTrendPage();
}

static float dailyTrendPleasantMaximum(float raw) {
  if (!std::isfinite(raw) || raw <= 0.0f) return 1.0f;
  const float target = raw * 1.08f;
  const float magnitude = powf(10.0f, floorf(log10f(target)));
  const float fraction = target / magnitude;
  const float step = fraction <= 1.0f ? 1.0f : fraction <= 2.0f ? 2.0f :
                     fraction <= 2.5f ? 2.5f : fraction <= 5.0f ? 5.0f : 10.0f;
  return step * magnitude;
}

bool dailyTrendValue(const DailyTrendPoint& p, int series, float& value) const {
  const uint8_t bit = series == 0 ? kDailyTrendSolarValid :
                      series == 1 ? kDailyTrendHomeValid :
                      series == 2 ? kDailyTrendGridValid : kDailyTrendSoeValid;
  if (!(p.valid_mask & bit)) return false;
  value = series == 0 ? p.solar_kw : series == 1 ? p.home_kw : series == 2 ? p.grid_kw : p.soe;
  return std::isfinite(value);
}

bool dailyTrendHistoryValue(const HistoryPoint& p, int series, float& value) const {
  if (series == 1) value = p.load_kw;
  else if (series == 3) value = p.soe;
  else return false;
  return std::isfinite(value) && (series != 3 || (value >= 0.0f && value <= 100.0f));
}

void updateDailyTrendAxes(int series, float scale, const char* lowText, const char* midText,
                          const char* highText) {
  const char* labels[3] = {highText, midText, lowText};
  const lv_font_t* regular = native480_fonts::montserrat_12();
  const lv_font_t* compact = native480_fonts::montserrat_9();
  for (int i = 0; i < 3; ++i) {
    setLabelText480(daily_trend_axis_[series][i], labels[i]);
    lv_point_t extent{};
    lv_txt_get_size(&extent, labels[i], regular, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_obj_set_style_text_font(daily_trend_axis_[series][i],
      extent.x <= 33 ? regular : compact, 0);
  }
  (void)series;
  (void)scale;
}

void drawDailyTrendChart(int series, float scale, const struct tm* localNow) {
  const int plotHeight = dailyTrendPlotHeight(series);
  auto* buffer = daily_trend_buffers_[series];
  if (!buffer || !daily_trend_canvas_[series]) return;
  for (int p = 0; p < kDailyTrendPlotW * plotHeight; ++p)
    buffer[p] = lv_color_hex(0x090e13);
  const int zeroY = series == 2 ? (plotHeight - 1) / 2 : plotHeight - 1;
  for (int x = 0; x < kDailyTrendPlotW; ++x) {
    for (int y : {0, (plotHeight - 1) / 2, plotHeight - 1})
      blendDashboardPixel(buffer, kDailyTrendPlotW, plotHeight, x, y, 94, 108, 122, y == zeroY ? 0.24f : 0.13f);
    if (x == dailyTrendTickPixel(0) || x == dailyTrendTickPixel(1) || x == dailyTrendTickPixel(2) ||
        x == dailyTrendTickPixel(3) || x == dailyTrendTickPixel(4)) {
      for (int y = 0; y < plotHeight; ++y)
        blendDashboardPixel(buffer, kDailyTrendPlotW, plotHeight, x, y, 94, 108, 122, 0.10f);
    }
  }

  float rawMax = 0.0f;
  const bool hasOldFallback = series == 1 || series == 3;
  const size_t historyCount = std::min<size_t>(last_telemetry_.history_count, PowerwallTelemetry::kHistoryCapacity);
  int latestVisibleBucket = 287;
  if (localNow) latestVisibleBucket = localNow->tm_hour * 12 + localNow->tm_min / 5;
  size_t ti = 0, hi = 0;
  for (int bucket = 0; bucket < 288; ++bucket) {
    if (bucket > latestVisibleBucket) break;
    const float bucketHour = bucket / 12.0f;
    while (ti < daily_trend_history_.count && dailyTrendBucketIndex(daily_trend_history_.points[ti].hour) < bucket) ++ti;
    while (hi < historyCount && dailyTrendBucketIndex(last_telemetry_.history[hi].hour) < bucket) ++hi;
    const DailyTrendPoint* trend = ti < daily_trend_history_.count &&
      dailyTrendBucketIndex(daily_trend_history_.points[ti].hour) == bucket
        ? &daily_trend_history_.points[ti] : nullptr;
    const HistoryPoint* old = hi < historyCount &&
      dailyTrendBucketIndex(last_telemetry_.history[hi].hour) == bucket
        ? &last_telemetry_.history[hi] : nullptr;
    float value = 0.0f;
    bool valid = trend && dailyTrendValue(*trend, series, value);
    if (!valid && hasOldFallback && old) valid = dailyTrendHistoryValue(*old, series, value);
    if (!valid) continue;
    if (series == 2) rawMax = fmaxf(rawMax, fabsf(value));
    else if (series != 3) rawMax = fmaxf(rawMax, value);
    (void)bucketHour;
  }
  if (series == 2) scale = dailyTrendPleasantMaximum(rawMax);
  else if (series != 3) scale = dailyTrendPleasantMaximum(rawMax);

  bool havePrevious = false;
  float previousValue = 0.0f;
  int previousBucket = -1;
  float latestHour = 0.0f, latestValue = 0.0f;
  bool haveLatest = false;
  ti = 0; hi = 0;
  const uint8_t red = static_cast<uint8_t>((kDailyTrendColors[series] >> 16) & 0xff);
  const uint8_t green = static_cast<uint8_t>((kDailyTrendColors[series] >> 8) & 0xff);
  const uint8_t blue = static_cast<uint8_t>(kDailyTrendColors[series] & 0xff);
  auto mapY = [series, scale, plotHeight](float v) {
    const float normalized = series == 3 ? fminf(1.0f, fmaxf(0.0f, v / 100.0f)) :
      series == 2 ? fminf(1.0f, fmaxf(-1.0f, v / scale)) : fminf(1.0f, fmaxf(0.0f, v / scale));
    return series == 2 ? static_cast<int>(roundf((0.5f - normalized * 0.5f) * (plotHeight - 1))) :
      static_cast<int>(roundf((1.0f - normalized) * (plotHeight - 1)));
  };
  auto fillSegment = [&](int x0, int y0, int x1, int y1) {
    if (x1 < x0) { std::swap(x0, x1); std::swap(y0, y1); }
    const int width = std::max(1, x1 - x0);
    // A shared bucket edge is filled by the previous segment only once.
    // Reblending x0 here makes repeated vertical strips at every sample.
    for (int x = x0 + 1; x <= x1; ++x) {
      const float ratio = static_cast<float>(x - x0) / width;
      const int y = std::max(0, std::min(plotHeight - 1,
        static_cast<int>(roundf(y0 + (y1 - y0) * ratio))));
      if (series == 2) {
        const int baseline = zeroY;
        const int from = std::min(y, baseline), to = std::max(y, baseline);
        for (int fillY = from; fillY <= to; ++fillY) {
          const float fade = 1.0f - static_cast<float>(abs(fillY - y)) / std::max(1, abs(y - baseline));
          blendDashboardPixel(buffer, kDailyTrendPlotW, plotHeight, x, fillY, red, green, blue, 0.30f * fade);
        }
      } else {
        for (int fillY = y; fillY < plotHeight; ++fillY) {
          const float fade = 1.0f - static_cast<float>(fillY - y) / std::max(1, plotHeight - 1 - y);
          blendDashboardPixel(buffer, kDailyTrendPlotW, plotHeight, x, fillY, red, green, blue, 0.30f * fade);
        }
      }
    }
    drawDashboardLine(buffer, kDailyTrendPlotW, plotHeight, x0, y0, x1, y1, red, green, blue);
  };

  for (int bucket = 0; bucket <= latestVisibleBucket; ++bucket) {
    while (ti < daily_trend_history_.count && dailyTrendBucketIndex(daily_trend_history_.points[ti].hour) < bucket) ++ti;
    while (hi < historyCount && dailyTrendBucketIndex(last_telemetry_.history[hi].hour) < bucket) ++hi;
    const DailyTrendPoint* trend = ti < daily_trend_history_.count &&
      dailyTrendBucketIndex(daily_trend_history_.points[ti].hour) == bucket
        ? &daily_trend_history_.points[ti] : nullptr;
    const HistoryPoint* old = hi < historyCount &&
      dailyTrendBucketIndex(last_telemetry_.history[hi].hour) == bucket
        ? &last_telemetry_.history[hi] : nullptr;
    float value = 0.0f;
    bool valid = trend && dailyTrendValue(*trend, series, value);
    if (!valid && hasOldFallback && old) {
      valid = dailyTrendHistoryValue(*old, series, value);
    }
    // Missing buckets do not reset the prior observation. The next valid
    // sample either bridges a short outage or starts a new segment below.
    if (!valid) continue;
    const float hour = dailyTrendBucketStartHour(bucket);
    const int x = std::max(0, std::min(kDailyTrendPlotW - 1,
      static_cast<int>(roundf((hour / 24.0f) * (kDailyTrendPlotW - 1)))));
    const int y = mapY(value);
    if (havePrevious && dailyTrendPlotBucketsConnect(previousBucket, bucket)) {
      const int previousX = std::max(0, std::min(kDailyTrendPlotW - 1,
        static_cast<int>(roundf((dailyTrendBucketStartHour(previousBucket) / 24.0f) * (kDailyTrendPlotW - 1)))));
      fillSegment(previousX, mapY(previousValue), x, y);
    } else {
      if (series == 2) {
        const int from = std::min(y, zeroY), to = std::max(y, zeroY);
        for (int fillY = from; fillY <= to; ++fillY) {
        const float fade = 1.0f - static_cast<float>(abs(fillY - y)) / std::max(1, abs(y - zeroY));
          blendDashboardPixel(buffer, kDailyTrendPlotW, plotHeight, x, fillY, red, green, blue, 0.30f * fade);
        }
      } else {
        for (int fillY = y; fillY < plotHeight; ++fillY) {
          const float fade = 1.0f - static_cast<float>(fillY - y) / std::max(1, plotHeight - 1 - y);
          blendDashboardPixel(buffer, kDailyTrendPlotW, plotHeight, x, fillY, red, green, blue, 0.30f * fade);
        }
      }
    }
    havePrevious = true;
    previousBucket = bucket;
    previousValue = value;
    latestHour = hour;
    latestValue = value;
    haveLatest = true;
  }

  if (series == 0) {
    char high[16], middle[16], low[16];
    snprintf(high, sizeof(high), scale >= 10.0f ? "%.0f" : "%.1f", scale);
    snprintf(middle, sizeof(middle), scale / 2.0f >= 10.0f ? "%.0f" : "%.1f", scale / 2.0f);
    snprintf(low, sizeof(low), "0");
    updateDailyTrendAxes(series, scale, low, middle, high);
  } else if (series == 1) {
    char high[16], middle[16];
    snprintf(high, sizeof(high), scale >= 10.0f ? "%.0f" : "%.1f", scale);
    snprintf(middle, sizeof(middle), scale / 2.0f >= 10.0f ? "%.0f" : "%.1f", scale / 2.0f);
    updateDailyTrendAxes(series, scale, "0", middle, high);
  } else if (series == 2) {
    char high[16], low[16];
    snprintf(high, sizeof(high), scale >= 10.0f ? "+%.0f" : "+%.1f", scale);
    snprintf(low, sizeof(low), scale >= 10.0f ? "-%.0f" : "-%.1f", scale);
    updateDailyTrendAxes(series, scale, low, "0", high);
  } else {
    updateDailyTrendAxes(series, 100.0f, "0%", "50%", "100%");
  }

  if (series == 3) {
    if (localNow) {
      const bool reserveKnown = last_configured_ &&
        (last_telemetry_.last_update_ms > 0 || last_telemetry_.demo) &&
        std::isfinite(last_telemetry_.backup_reserve_percent);
      if (reserveKnown) {
        const float reserve = fminf(100.0f, fmaxf(0.0f, last_telemetry_.backup_reserve_percent));
        const int y = mapY(reserve);
        for (int x = 0; x < kDailyTrendPlotW; x += 6)
          for (int n = 0; n < 3 && x + n < kDailyTrendPlotW; ++n)
            putDashboardPixel(buffer, kDailyTrendPlotW, plotHeight, x + n, y, 92, 111, 130);
      }
    }
  }
  lv_obj_invalidate(daily_trend_canvas_[series]);

  if (haveLatest) {
    const int x = std::max(0, std::min(kDailyTrendPlotW - 1,
      static_cast<int>(roundf((latestHour / 24.0f) * (kDailyTrendPlotW - 1)))));
    const int y = mapY(latestValue);
    lv_obj_set_pos(daily_trend_latest_dot_[series], kDailyTrendPagePlotX + x - 3,
      kDailyTrendRowY[series] + dailyTrendPlotOffsetY(series) + y - 3);
    lv_obj_clear_flag(daily_trend_latest_dot_[series], LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(daily_trend_latest_dot_[series], LV_OBJ_FLAG_HIDDEN);
  }
}

void updateDailyTrendTou(const struct tm& localTime) {
  struct TouSegment { int start; int end; bool peak; } segments[4] = {};
  int count = 0;
  if (isWeekend(localTime)) {
    segments[0] = {0, 1440, false};
    count = 1;
  } else if (isSummerTouMonth(localTime)) {
    segments[0] = {0, 540, false};
    segments[1] = {540, 1440, true};
    count = 2;
  } else {
    segments[0] = {0, 360, false};
    segments[1] = {360, 660, true};
    segments[2] = {660, 840, false};
    segments[3] = {840, 1440, true};
    count = 4;
  }
  for (int i = 0; i < 4; ++i) {
    if (i >= count) {
      lv_obj_add_flag(daily_trend_tou_segments_[i], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    const int segmentStart = kDailyTrendPagePlotX + static_cast<int>(roundf(
      segments[i].start / 1440.0f * kDailyTrendPlotW));
    const int segmentEnd = kDailyTrendPagePlotX + static_cast<int>(roundf(
      segments[i].end / 1440.0f * kDailyTrendPlotW));
    const int width = std::max(1, segmentEnd - segmentStart);
    const uint32_t color = segments[i].peak ? 0xb9312f : 0x26a64c;
    lv_obj_clear_flag(daily_trend_tou_segments_[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(daily_trend_tou_segments_[i], segmentStart, kDailyTrendTouBarY);
    lv_obj_set_size(daily_trend_tou_segments_[i], width, kDailyTrendTouBarH);
    lv_obj_set_style_bg_color(daily_trend_tou_segments_[i], lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(daily_trend_tou_segments_[i], LV_OPA_COVER, 0);
  }
}

bool dailyTrendHasSolarPoint() const {
  for (size_t i = 0; i < daily_trend_history_.count; ++i)
    if (daily_trend_history_.points[i].valid_mask & kDailyTrendSolarValid) return true;
  return false;
}

void setDailyTrendPowerMetric(int index, bool valid, float value, bool signedValue = false) {
  char text[32];
  const int valueY = kDailyTrendMetricValueY + (index == 2 ? 3 : 0);
  if (valid && std::isfinite(value)) {
    const float magnitude = fabsf(value);
    if (magnitude >= 1000.0f) snprintf(text, sizeof(text), signedValue ? "%+.0f" : "%.0f", value);
    else if (magnitude >= 100.0f) snprintf(text, sizeof(text), signedValue ? "%+.1f" : "%.1f", value);
    else snprintf(text, sizeof(text), signedValue ? "%+.2f" : "%.2f", value);

    // The grid value refreshes every telemetry tick. Keep its typography stable
    // across sign and digit-count changes; 27px fits ordinary signed readings.
    const bool fixedGridFont = index == 2;
    const lv_font_t* mainFont = fixedGridFont ? native480_fonts::montserrat_27() : native480_fonts::montserrat_32();
    lv_obj_set_style_text_font(daily_trend_metric_value_[index], mainFont, 0);
    lv_obj_set_y(daily_trend_metric_value_[index], valueY);
    setLabelText480(daily_trend_metric_value_[index], text);
    lv_point_t textSize{}, unitSize{};
    lv_txt_get_size(&textSize, text, mainFont, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    const lv_font_t* unitFont = native480_fonts::montserrat_13();
    lv_txt_get_size(&unitSize, "kW", unitFont, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    const bool gridUnitOnSecondLine = fixedGridFont &&
      textSize.x + 2 + unitSize.x > kDailyTrendMetricValueWidth;
    if (!fixedGridFont && textSize.x + 2 + unitSize.x > kDailyTrendMetricValueWidth) {
      mainFont = native480_fonts::montserrat_27();
      lv_obj_set_style_text_font(daily_trend_metric_value_[index], mainFont, 0);
      lv_txt_get_size(&textSize, text, mainFont, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    }
    if (!fixedGridFont && textSize.x + 2 + unitSize.x > kDailyTrendMetricValueWidth) {
      mainFont = native480_fonts::montserrat_24();
      lv_obj_set_style_text_font(daily_trend_metric_value_[index], mainFont, 0);
      lv_txt_get_size(&textSize, text, mainFont, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    }
    if (!fixedGridFont && textSize.x + 2 + unitSize.x > kDailyTrendMetricValueWidth) {
      mainFont = native480_fonts::montserrat_21();
      lv_obj_set_style_text_font(daily_trend_metric_value_[index], mainFont, 0);
      lv_txt_get_size(&textSize, text, mainFont, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    }
    if (daily_trend_metric_unit_[index]) {
      const int columnX = index * kDailyTrendMetricWidth;
      const int valueBaseline = valueY +
        (mainFont->line_height - mainFont->base_line);
      const int unitY = valueBaseline - (unitFont->line_height - unitFont->base_line);
      lv_obj_set_style_text_font(daily_trend_metric_unit_[index], unitFont, 0);
      lv_obj_set_pos(daily_trend_metric_unit_[index], gridUnitOnSecondLine
        ? columnX + kDailyTrendMetricInset
        : columnX + kDailyTrendMetricInset + textSize.x + 2,
        gridUnitOnSecondLine ? kDailyTrendMetricUnitY + 3 : unitY);
      lv_obj_clear_flag(daily_trend_metric_unit_[index], LV_OBJ_FLAG_HIDDEN);
    }
  } else {
    snprintf(text, sizeof(text), index == 2 ? "--" : "—");
    lv_obj_set_style_text_font(daily_trend_metric_value_[index], index == 2
      ? native480_fonts::montserrat_27() : native480_fonts::cjk_14(), 0);
    lv_obj_set_y(daily_trend_metric_value_[index], valueY);
    if (daily_trend_metric_unit_[index]) lv_obj_add_flag(daily_trend_metric_unit_[index], LV_OBJ_FLAG_HIDDEN);
  }
  setLabelText480(daily_trend_metric_value_[index], text);
}

void refreshDailyTrendPage() {
  if (!page_today_trend_) return;
  const PowerwallTelemetry& t = last_telemetry_;
  char text[48];
  const bool hasLive = t.ok || t.last_update_ms > 0;
  struct tm localTime{};
  const bool hasLocalTime = hasValidLocalTime(localTime);
  setDailyTrendPowerMetric(0, t.has_solar_power, t.solar_power_w / 1000.0f);
  if (dailyTrendHasSolarPoint()) {
    snprintf(text, sizeof(text), "今日 %.1fkWh", daily_trend_history_.solar_kwh_today);
  } else snprintf(text, sizeof(text), "今日 —kWh");
  setLabelText480(daily_trend_metric_subtitle_[0], text);
  setDailyTrendPowerMetric(1, t.has_load_power, t.load_power_w / 1000.0f);
  if (hasLive && (t.has_load_voltage || t.has_load_current)) {
    char voltage[16] = "--V", current[16] = "--A";
    if (t.has_load_voltage && std::isfinite(t.load_voltage_v)) snprintf(voltage, sizeof(voltage), "%.1fV", t.load_voltage_v);
    if (t.has_load_current && std::isfinite(t.load_current_a)) snprintf(current, sizeof(current), "%.1fA", t.load_current_a);
    snprintf(text, sizeof(text), "%s · %s", voltage, current);
  } else snprintf(text, sizeof(text), "電壓/電流不足");
  setLabelText480(daily_trend_metric_subtitle_[1], text);
  setDailyTrendPowerMetric(2, t.has_site_power, t.site_power_w / 1000.0f, true);
  formatDailyTrendGridEnergySubtitle(t.has_site_import_energy, t.site_energy_imported_wh,
    t.has_site_export_energy, t.site_energy_exported_wh, text, sizeof(text));
  const char* localizedEnergyText = native480_i18n::text(text);
  const lv_font_t* energyFont = dailyTrendGridEnergySubtitleFont(localizedEnergyText);
  lv_obj_set_style_text_font(daily_trend_metric_subtitle_[2], energyFont, 0);
  lv_obj_set_style_text_letter_space(daily_trend_metric_subtitle_[2],
    dailyTrendGridEnergySubtitleLetterSpace(localizedEnergyText, energyFont), 0);
  lv_label_set_text(daily_trend_metric_subtitle_[2], localizedEnergyText);
  if (hasLive && std::isfinite(t.soe_percent) && t.soe_percent >= 0.0f && t.soe_percent <= 100.0f) {
    snprintf(text, sizeof(text), "%.0f%%", t.soe_percent);
    lv_obj_set_style_text_font(daily_trend_metric_value_[3], native480_fonts::montserrat_32(), 0);
  } else {
    snprintf(text, sizeof(text), "—");
    lv_obj_set_style_text_font(daily_trend_metric_value_[3], native480_fonts::cjk_14(), 0);
  }
  setLabelText480(daily_trend_metric_value_[3], text);
  if (hasLive && t.has_battery_capacity && std::isfinite(t.nominal_remaining_kwh) &&
      std::isfinite(t.nominal_full_kwh) && std::isfinite(t.backup_reserve_percent))
    snprintf(text, sizeof(text), "%.1f/%.1fkWh\n備援%.0f%%", t.nominal_remaining_kwh, t.nominal_full_kwh, t.backup_reserve_percent);
  else snprintf(text, sizeof(text), "容量資料不足\n備援資料不足");
  setLabelText480(daily_trend_metric_subtitle_[3], text);

  float maxima[4] = {};
  (void)maxima;
  drawDailyTrendChart(0, 1.0f, hasLocalTime ? &localTime : nullptr);
  drawDailyTrendChart(1, 1.0f, hasLocalTime ? &localTime : nullptr);
  drawDailyTrendChart(2, 1.0f, hasLocalTime ? &localTime : nullptr);
  drawDailyTrendChart(3, 100.0f, hasLocalTime ? &localTime : nullptr);

  if (hasLocalTime) {
    const int minuteOfDay = localTime.tm_hour * 60 + localTime.tm_min;
    const int x = kDailyTrendPagePlotX + static_cast<int>(roundf(minuteOfDay / 1440.0f * (kDailyTrendPlotW - 1)));
    lv_obj_set_x(daily_trend_now_glow_, x - 2);
    lv_obj_set_x(daily_trend_now_line_, x);
    lv_obj_clear_flag(daily_trend_now_glow_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(daily_trend_now_line_, LV_OBJ_FLAG_HIDDEN);
    updateDailyTrendTou(localTime);
    // Draw one shared cursor over charts and TOU. Promote valid observation
    // dots last so they remain distinct from the live-time cursor.
    lv_obj_move_foreground(daily_trend_now_glow_);
    lv_obj_move_foreground(daily_trend_now_line_);
    for (auto* title : daily_trend_title_)
      if (title) lv_obj_move_foreground(title);
    for (auto* dot : daily_trend_latest_dot_)
      if (dot && !lv_obj_has_flag(dot, LV_OBJ_FLAG_HIDDEN)) lv_obj_move_foreground(dot);
  } else {
    lv_obj_add_flag(daily_trend_now_glow_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(daily_trend_now_line_, LV_OBJ_FLAG_HIDDEN);
  }
}

// Included inside UI720x720; shared by the ESP32 and host firmware simulator.
lv_obj_t* page_week_ = nullptr;
WeeklyHistory weekly_history_;
lv_obj_t* week_rows_[7] = {};
lv_obj_t* week_dates_[7] = {};
lv_obj_t* week_badges_[7] = {};
lv_obj_t* week_stats_[7][3] = {};
lv_obj_t* week_stat_units_[7][3] = {};
lv_obj_t* week_empty_[7] = {};
lv_obj_t* week_axes_[7] = {};
lv_obj_t* week_mid_axes_[7] = {};
lv_obj_t* week_reserve_labels_[7] = {};
lv_obj_t* week_canvas_[7] = {};
lv_color_t* week_buffers_[7] = {};
DashboardTimeAxis week_time_axis_;
static constexpr int kWeekW = 434;
#if defined(POWERWALL_UI_480)
static constexpr int kWeekH = 93;
#else
static constexpr int kWeekH = 77;
#endif

static int weekGraphY(int y) {
#if defined(POWERWALL_UI_480)
  return (y * (kWeekH - 1) + 38) / 76;
#else
  return y;
#endif
}

enum class WeeklyStatStyle { Average, Detail };

void updateWeeklyStatValue(lv_obj_t* value, lv_obj_t* unit, const char* text,
                           WeeklyStatStyle style, int top) {
  static constexpr int kGap = 2;
  const bool isAverage = style == WeeklyStatStyle::Average;
  const int left = isAverage ? 8 : 104;
  const int availableWidth = 86;
  const lv_font_t* unitFont = isAverage ? &font_cjk_12 : &font_dashboard_cjk_10;
  const lv_font_t* valueFonts[] = {
    &lv_font_montserrat_28, &lv_font_montserrat_24, &font_cjk_20, &font_cjk_18
  };
  lv_point_t unitSize{};
  lv_txt_get_size(&unitSize, "kW", unitFont, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);

  const lv_font_t* selectedFont = isAverage ? valueFonts[3] : &font_cjk_14;
  lv_point_t valueSize{};
  if (isAverage && strcmp(text, "—") == 0) {
    // Montserrat has no em dash glyph; use the CJK font to keep the missing
    // value visible instead of showing a missing-glyph box.
    selectedFont = &font_cjk_20;
  } else if (isAverage) {
    for (const lv_font_t* font : valueFonts) {
      lv_txt_get_size(&valueSize, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
      if (valueSize.x + kGap + unitSize.x <= availableWidth) {
        selectedFont = font;
        break;
      }
    }
  }
  lv_txt_get_size(&valueSize, text, selectedFont, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  lv_label_set_text(value, text);
  lv_obj_set_style_text_font(value, selectedFont, 0);
  lv_obj_set_width(value, valueSize.x);
  lv_obj_set_pos(value, left, top);
  lv_obj_set_pos(unit, left + valueSize.x + kGap,
                 top + (selectedFont->line_height - unitFont->line_height) / 2);
  lv_obj_set_style_text_font(unit, unitFont, 0);
}

void initializeWeeklyPage(lv_obj_t* screen) {
  page_week_ = makeDashboardCard(screen, 0, 0, 720, 720);
  lv_obj_set_style_radius(page_week_, 0, 0);
  lv_obj_set_style_border_width(page_week_, 0, 0);
  lv_obj_add_flag(page_week_, LV_OBJ_FLAG_HIDDEN);
  const char* names[] = {"Avg.", "Peak Avg", "Off-Peak Avg"};
  const uint32_t colors[] = {0x38bdf8, 0xfb7185, 0x34d399};
  for (int d = 0; d < 7; ++d) {
    int rowY;
    int rowH;
#if defined(POWERWALL_UI_480)
    rowY = d * 720 / 7;
    rowH = (d + 1) * 720 / 7 - rowY;
#else
    rowY = 58 + d*93;
    rowH = 88;
#endif
    auto* row = week_rows_[d] = makeDashboardCard(page_week_, 14, rowY, 692, rowH);
    lv_obj_set_style_radius(row, 10, 0);
#if defined(POWERWALL_UI_480)
    lv_obj_set_style_radius(row, 0, 0);
#endif
    week_dates_[d] = makeDashboardLabel(row, "", 8, 3, 136, &font_cjk_14, 0xf1f5f9);
    week_badges_[d] = makeDashboardLabel(row, "", 145, 5, 42, &font_cjk_12, 0x64748b, LV_TEXT_ALIGN_RIGHT);
    makeDashboardLabel(row, names[0], 8, 26, 80, &font_dashboard_cjk_10, colors[0]);
    week_stats_[d][0] = makeDashboardLabel(row, "—", 8, 38, 86, &lv_font_montserrat_28, colors[0]);
    week_stat_units_[d][0] = makeDashboardLabel(row, "kW", 8, 44, 0, &font_cjk_12, 0xf1f5f9);
    updateWeeklyStatValue(week_stats_[d][0], week_stat_units_[d][0], "—", WeeklyStatStyle::Average, 38);
    makeDashboardRule(row, 96, 25, 1, 55);
    makeDashboardLabel(row, names[1], 104, 23, 86, &font_dashboard_cjk_10, colors[1]);
    week_stats_[d][1] = makeDashboardLabel(row, "—", 104, 35, 86, &font_cjk_14, colors[1]);
    week_stat_units_[d][1] = makeDashboardLabel(row, "kW", 104, 38, 0, &font_dashboard_cjk_10, 0xf1f5f9);
    updateWeeklyStatValue(week_stats_[d][1], week_stat_units_[d][1], "—", WeeklyStatStyle::Detail, 35);
    makeDashboardRule(row, 104, 52, 86);
    makeDashboardLabel(row, names[2], 104, 54, 86, &font_dashboard_cjk_10, colors[2]);
    week_stats_[d][2] = makeDashboardLabel(row, "—", 104, 66, 86, &font_cjk_14, colors[2]);
    week_stat_units_[d][2] = makeDashboardLabel(row, "kW", 104, 69, 0, &font_dashboard_cjk_10, 0xf1f5f9);
    updateWeeklyStatValue(week_stats_[d][2], week_stat_units_[d][2], "—", WeeklyStatStyle::Detail, 66);
    makeDashboardRule(row, 195, 4, 1, 74);
    const lv_font_t* axisFont = &font_dashboard_cjk_10;
    makeDashboardLabel(row, "住家", 200, 18, 20, axisFont, 0x38bdf8);
    week_axes_[d] = makeDashboardLabel(row, "-- kW", 218, weekGraphY(1), 32, axisFont, 0x94a3b8, LV_TEXT_ALIGN_RIGHT);
    week_mid_axes_[d] = makeDashboardLabel(row, "-- kW", 218, weekGraphY(18), 32, axisFont, 0x94a3b8, LV_TEXT_ALIGN_RIGHT);
    makeDashboardLabel(row, "0 kW", 218, weekGraphY(31), 32, axisFont, 0x94a3b8, LV_TEXT_ALIGN_RIGHT);
    makeDashboardLabel(row, "PW", 200, weekGraphY(56), 20, axisFont, 0x11e58e);
    makeDashboardLabel(row, "100%", 218, weekGraphY(44), 32, axisFont, 0x94a3b8, LV_TEXT_ALIGN_RIGHT);
    week_reserve_labels_[d] = makeDashboardLabel(row, "24%", 218, weekGraphY(62), 32, axisFont, 0x38bdf8, LV_TEXT_ALIGN_RIGHT);
    makeDashboardLabel(row, "0%", 218, weekGraphY(73), 32, axisFont, 0x94a3b8, LV_TEXT_ALIGN_RIGHT);
    week_buffers_[d] = allocPixelBuf(kWeekW*kWeekH);
    if (week_buffers_[d]) {
      memset(week_buffers_[d], 0, kWeekW*kWeekH*sizeof(lv_color_t));
      week_canvas_[d] = lv_canvas_create(row);
      lv_canvas_set_buffer(week_canvas_[d], week_buffers_[d], kWeekW, kWeekH, LV_IMG_CF_TRUE_COLOR);
      lv_obj_set_pos(week_canvas_[d], 251, 5);
    }
#if defined(POWERWALL_UI_480)
    const int emptyY = (rowH - font_cjk_12.line_height) / 2;
#else
    const int emptyY = 20;
#endif
    week_empty_[d] = makeDashboardLabel(row, "尚無紀錄", 365, emptyY, 200,
      &font_cjk_12, 0x64748b, LV_TEXT_ALIGN_CENTER);
  }
#if !defined(POWERWALL_UI_480)
  initializeTimeAxis(week_time_axis_, page_week_, 266, kWeekW, 706, &font_dashboard_cjk_10);
  const char* legend[] = {"電網", "PW", "未知"};
  const uint32_t legendColors[] = {0x38bdf8, 0x11e58e, 0x94a3b8};
  for(int i=0;i<3;++i) {
    auto* line=makeDashboardRule(page_week_,22+i*75,710,12,2);
    lv_obj_set_style_bg_color(line,lv_color_hex(legendColors[i]),0);
    makeDashboardLabel(page_week_,legend[i],39+i*75,706,48,&font_dashboard_cjk_10,0x94a3b8);
  }
#endif
}

void refreshWeeklyPage() {
  if (!page_week_ || active_dashboard_page_ != 3) return;
  time_t now = time(nullptr); tm today{}; localtime_r(&now, &today);
  if (!WeeklyHistory::validTime(now)) {
    const char* names[] = {"週日", "週一", "週二", "週三", "週四", "週五", "週六"};
    char text[40];
    for (int d = 0; d < 7; ++d) {
      snprintf(text, sizeof(text), "%s  --/--", names[d]);
      lv_label_set_text(week_dates_[d], text);
      lv_label_set_text(week_empty_[d], "等待時間同步");
      lv_obj_clear_flag(week_empty_[d], LV_OBJ_FLAG_HIDDEN);
    }
    return;
  }
  weekly_history_.roll(now);
#if !defined(POWERWALL_UI_480)
  bool boundaries[25] = {};
  for (int d = 0; d < 7; ++d) {
    const tm date = WeeklyHistory::dayAt(now, (today.tm_wday - d + 7) % 7);
    addTouAxisBoundaries(date, boundaries);
  }
  // Use the canvas's actual content origin (including the row border).
  lv_obj_update_layout(page_week_);
  lv_area_t rowContent, pageCoords;
  lv_obj_get_content_coords(week_rows_[0], &rowContent);
  lv_obj_get_coords(page_week_, &pageCoords);
  week_time_axis_.left = rowContent.x1 + 251 - pageCoords.x1;
  updateTimeAxis(week_time_axis_, boundaries);
#endif
  float middleKw = 3;
  for (const auto& day : weekly_history_.days) for (const auto& p : day.points)
    middleKw = fmaxf(middleKw, ceilf(p.load_kw));
  const float scale = middleKw * 2;
  const char* names[] = {"週日", "週一", "週二", "週三", "週四", "週五", "週六"};
  char text[80];
  for (int d=0; d<7; ++d) {
    const auto& day = weekly_history_.days[d];
    tm date = WeeklyHistory::dayAt(now, (today.tm_wday-d+7)%7);
    bool current = d == today.tm_wday;
    snprintf(text, sizeof(text), "%s  %02d/%02d", names[d], date.tm_mon+1, date.tm_mday);
    lv_label_set_text(week_dates_[d], text);
    lv_label_set_text(week_badges_[d], current ? "今日" : (d==0 || d==6 ? "週末" : "週間"));
    lv_obj_set_style_text_color(week_badges_[d], lv_color_hex(current ? 0x38bdf8 : d==0 || d==6 ? 0x34d399 : 0x64748b), 0);
    lv_obj_set_style_border_color(week_rows_[d], lv_color_hex(current ? 0x38bdf8 : 0x263746), 0);
    lv_obj_set_style_bg_color(week_rows_[d], lv_color_hex(current ? 0x122332 : 0x101820), 0);
    auto avg = WeeklyHistory::averages(day,date);
    for (int s=0; s<3; ++s) {
      if (std::isfinite(avg[s])) snprintf(text,sizeof(text),"%.2f",avg[s]); else snprintf(text,sizeof(text),"—");
      updateWeeklyStatValue(week_stats_[d][s], week_stat_units_[d][s], text,
                            s == 0 ? WeeklyStatStyle::Average : WeeklyStatStyle::Detail,
                            s == 0 ? 38 : s == 1 ? 35 : 66);
    }
    snprintf(text,sizeof(text),"%.0f kW",scale); lv_label_set_text(week_axes_[d],text);
    snprintf(text,sizeof(text),"%.0f kW",middleKw); lv_label_set_text(week_mid_axes_[d],text);
    const float reserve = std::isfinite(last_telemetry_.backup_reserve_percent)
      ? fminf(100, fmaxf(0, last_telemetry_.backup_reserve_percent)) : 24.f;
    snprintf(text,sizeof(text),"%.0f%%",reserve);
    lv_label_set_text(week_reserve_labels_[d],text);
    const int reserveY = weekGraphY(72 - int(reserve*31/100));
    lv_label_set_text(week_empty_[d], week_buffers_[d] ? "尚無紀錄" : "圖表不可用");
    if (day.points.empty() || !week_buffers_[d]) lv_obj_clear_flag(week_empty_[d], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(week_empty_[d], LV_OBJ_FLAG_HIDDEN);
    auto* buf = week_buffers_[d]; if (!buf) continue;
    for (int p=0; p<kWeekW*kWeekH; ++p) buf[p] = lv_color_hex(0x090e13);
    for (int x=0;x<kWeekW;++x) {
      for (int y : {2,19,36,41,56,72}) putDashboardPixel(buf,kWeekW,kWeekH,x,weekGraphY(y),27,37,47);
      bool peak = WeeklyHistory::peak(date, x*24.f/(kWeekW-1));
      for (int y=weekGraphY(74); y<kWeekH; ++y) putDashboardPixel(buf,kWeekW,kWeekH,x,y,peak?185:38,peak?49:166,peak?47:76);
    }
    for (int i=0;i<5;++i) for (int y=0;y<=weekGraphY(72);++y) putDashboardPixel(buf,kWeekW,kWeekH,i*(kWeekW-1)/4,y,22,30,39);
    auto px = [](float hour) { return int(hour*(kWeekW-1)/24.f); };
    const int loadTopY = weekGraphY(2), loadBottomY = weekGraphY(36);
    const int socTopY = weekGraphY(41), socBottomY = weekGraphY(72);
    auto py = [scale, loadTopY, loadBottomY](float load) {
      return loadBottomY-int(fminf(scale,fmaxf(0,load))/scale*(loadBottomY-loadTopY));
    };
    auto sy = [socTopY, socBottomY](float soe) {
      return socBottomY-int(soe*(socBottomY-socTopY)/100);
    };
    for (size_t i=0;i<day.points.size();++i) {
      const auto& p=day.points[i]; int x=px(p.hour);
      uint8_t r=148,g=163,b=184;
      if(p.power_source==HistoryPowerSource::Grid){r=56;g=189;b=248;}
      if(p.power_source==HistoryPowerSource::Powerwall){r=17;g=229;b=142;}
      putDashboardPixel(buf,kWeekW,kWeekH,x,py(p.load_kw),r,g,b);
      putDashboardPixel(buf,kWeekW,kWeekH,x,sy(p.soe),17,229,142);
      if (!i || p.hour-day.points[i-1].hour > .17f) continue;
      const auto& prev=day.points[i-1];
      for(int xx=px(prev.hour);xx<=x;++xx){
        float ratio=x==px(prev.hour)?1.f:float(xx-px(prev.hour))/(x-px(prev.hour));
        int yy=sy(prev.soe)+(sy(p.soe)-sy(prev.soe))*ratio;
        for(int y=yy+1;y<=socBottomY;++y) putDashboardPixel(buf,kWeekW,kWeekH,xx,y,10,37,31);
      }
      drawDashboardLine(buf,kWeekW,kWeekH,px(prev.hour),py(prev.load_kw),x,py(p.load_kw),r,g,b);
      drawDashboardLine(buf,kWeekW,kWeekH,px(prev.hour),sy(prev.soe),x,sy(p.soe),17,229,142);
    }
    // Draw the reserve guide over the filled area and both curves.
    for (int x=0;x<kWeekW;x+=7) for(int n=0;n<4 && x+n<kWeekW;++n) putDashboardPixel(buf,kWeekW,kWeekH,x+n,reserveY,33,88,107);
    if(current) {
      int x=px(today.tm_hour+today.tm_min/60.f);
      drawDashboardLine(buf,kWeekW,kWeekH,x,0,x,kWeekH-1,225,237,245);
    }
    lv_obj_invalidate(week_canvas_[d]);
  }
}

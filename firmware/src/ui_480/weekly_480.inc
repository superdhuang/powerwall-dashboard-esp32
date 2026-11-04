// Included inside Ui480; shared by the ESP32 and host firmware simulator.
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
static constexpr int kWeekW = 289, kWeekH = 60;

static int weekChartY(int y) {
  return (y * (kWeekH - 1) + 25) / 50;
}

static int weeklyStatValueTop(int captionTop) {
  return captionTop + native480_fonts::dashboard_cjk_9()->line_height + 1;
}

enum class WeeklyStatStyle { Average, Detail };

void updateWeeklyStatValue(lv_obj_t* value, lv_obj_t* unit, const char* text,
                           WeeklyStatStyle style, int top) {
  const bool isAverage = style == WeeklyStatStyle::Average;
  const int left = isAverage ? 5 : 69;
  const lv_font_t* unitFont = isAverage ? native480_fonts::cjk_9() : native480_fonts::dashboard_cjk_9();
  const lv_font_t* valueFonts[] = {
    native480_fonts::montserrat_19(), native480_fonts::montserrat_16(), native480_fonts::cjk_13(), native480_fonts::cjk_12()
  };
  lv_point_t unitSize{};
  lv_txt_get_size(&unitSize, "kW", unitFont, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);

  const lv_font_t* selectedFont = isAverage ? valueFonts[3] : native480_fonts::cjk_12();
  lv_point_t valueSize{};
  if (isAverage && strcmp(text, "—") == 0) {
    // Montserrat has no em dash glyph; use the CJK font to keep the missing
    // value visible instead of showing a missing-glyph box.
    selectedFont = native480_fonts::cjk_13();
  } else if (isAverage) {
    for (const lv_font_t* font : valueFonts) {
      lv_txt_get_size(&valueSize, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
      const int combinedWidth = valueSize.x + unitSize.x;
      const bool fits = combinedWidth < 56 ||
        (combinedWidth == 56 && !(valueSize.x & 1) && !(unitSize.x & 1));
      if (fits) {
        selectedFont = font;
        break;
      }
    }
  }
  lv_txt_get_size(&valueSize, text, selectedFont, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  setLabelText480(value, text);
  lv_obj_set_style_text_font(value, selectedFont, 0);
  lv_obj_set_width(value, valueSize.x);

  int averageUnitTopOffset = 0;
  if (isAverage) {
    // Center the value and its baseline-aligned unit in the left stat area,
    // which starts below the caption and ends above the row's bottom inset.
    averageUnitTopOffset = (selectedFont->line_height - selectedFont->base_line)
                         - (unitFont->line_height - unitFont->base_line);
    const int groupTopOffset = averageUnitTopOffset < 0 ? averageUnitTopOffset : 0;
    const int groupBottomOffset =
      averageUnitTopOffset + unitFont->line_height > selectedFont->line_height
        ? averageUnitTopOffset + unitFont->line_height
        : selectedFont->line_height;
    const int groupHeight = groupBottomOffset - groupTopOffset;
    const int rowBottom = lv_obj_get_height(lv_obj_get_parent(value)) - 5;
    const int availableHeight = rowBottom - top;
    const int freeHeight = availableHeight - groupHeight;
    if (freeHeight > 0) top += freeHeight / 2 - groupTopOffset;
  }

  lv_obj_set_pos(value, left, top);
  const int unitTop = isAverage
    ? top + averageUnitTopOffset
    : top + (selectedFont->line_height - unitFont->line_height) / 2;
  lv_obj_set_pos(unit, left + valueSize.x + 1,
                 unitTop);
  lv_obj_set_style_text_font(unit, unitFont, 0);
}

void initializeWeeklyPage(lv_obj_t* screen) {
  page_week_ = makeDashboardCard(screen, 0, 0, 480, 480);
  lv_obj_set_style_radius(page_week_, 0, 0);
  lv_obj_set_style_border_width(page_week_, 0, 0);
  lv_obj_add_flag(page_week_, LV_OBJ_FLAG_HIDDEN);
  const char* names[] = {"平均", "尖峰平均", "離峰平均"};
  const uint32_t colors[] = {0x38bdf8, 0xfb7185, 0x34d399};
  for (int d = 0; d < 7; ++d) {
    const int rowY = d * 480 / 7;
    const int rowH = (d + 1) * 480 / 7 - rowY;
    auto* row = week_rows_[d] = makeDashboardCard(page_week_, 9, rowY, 461, rowH);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    week_dates_[d] = makeDashboardLabel(row, "", 5, 2, 91, native480_fonts::cjk_12(), 0xf1f5f9);
    week_badges_[d] = makeDashboardLabel(row, "", 87, 3, 38, native480_fonts::cjk_9(), 0x64748b, LV_TEXT_ALIGN_RIGHT);
    makeDashboardLabel(row, names[0], 5, 27, 53, native480_fonts::dashboard_cjk_9(), colors[0]);
    week_stats_[d][0] = makeDashboardLabel(row, "—", 5, 28, 57, native480_fonts::montserrat_19(), colors[0]);
    week_stat_units_[d][0] = makeDashboardLabel(row, "kW", 5, 29, 0, native480_fonts::cjk_8(), 0xf1f5f9);
    updateWeeklyStatValue(week_stats_[d][0], week_stat_units_[d][0], "—", WeeklyStatStyle::Average,
                          weeklyStatValueTop(17));
    makeDashboardRule(row, 64, 21, 1, rowH - 5 - 21);
    makeDashboardLabel(row, names[1], 69, 15, 57, native480_fonts::dashboard_cjk_9(), colors[1]);
    week_stats_[d][1] = makeDashboardLabel(row, "—", 69, 26, 57, native480_fonts::cjk_12(), colors[1]);
    week_stat_units_[d][1] = makeDashboardLabel(row, "kW", 69, 28, 0, native480_fonts::dashboard_cjk_9(), 0xf1f5f9);
    updateWeeklyStatValue(week_stats_[d][1], week_stat_units_[d][1], "—", WeeklyStatStyle::Detail,
                          weeklyStatValueTop(15));
    makeDashboardRule(row, 69, 40, 57);
    makeDashboardLabel(row, names[2], 69, 41, 57, native480_fonts::dashboard_cjk_9(), colors[2]);
    week_stats_[d][2] = makeDashboardLabel(row, "—", 69, 52, 57, native480_fonts::cjk_12(), colors[2]);
    week_stat_units_[d][2] = makeDashboardLabel(row, "kW", 69, 54, 0, native480_fonts::dashboard_cjk_9(), 0xf1f5f9);
    updateWeeklyStatValue(week_stats_[d][2], week_stat_units_[d][2], "—", WeeklyStatStyle::Detail,
                          weeklyStatValueTop(41));
    makeDashboardRule(row, 130, 7, 1, rowH - 5 - 7);
    const lv_font_t* axisFont = native480_fonts::dashboard_cjk_7();
    makeDashboardLabel(row, "住家", 133, weekChartY(12), 13, axisFont, 0x38bdf8);
    week_axes_[d] = makeDashboardLabel(row, "-- kW", 145, weekChartY(1), 21, axisFont, 0x94a3b8, LV_TEXT_ALIGN_RIGHT);
    week_mid_axes_[d] = makeDashboardLabel(row, "-- kW", 145, weekChartY(12), 21, axisFont, 0x94a3b8, LV_TEXT_ALIGN_RIGHT);
    makeDashboardLabel(row, "0 kW", 145, weekChartY(21), 21, axisFont, 0x94a3b8, LV_TEXT_ALIGN_RIGHT);
    makeDashboardLabel(row, "PW", 133, weekChartY(37), 13, axisFont, 0x11e58e);
    makeDashboardLabel(row, "100%", 145, weekChartY(29), 21, axisFont, 0x94a3b8, LV_TEXT_ALIGN_RIGHT);
    week_reserve_labels_[d] = makeDashboardLabel(row, "24%", 145, weekChartY(41), 21, axisFont, 0x38bdf8, LV_TEXT_ALIGN_RIGHT);
    makeDashboardLabel(row, "0%", 145, weekChartY(49), 21, axisFont, 0x94a3b8, LV_TEXT_ALIGN_RIGHT);
    week_buffers_[d] = allocPixelBuf(kWeekW*kWeekH);
    if (week_buffers_[d]) {
      memset(week_buffers_[d], 0, kWeekW*kWeekH*sizeof(lv_color_t));
      week_canvas_[d] = lv_canvas_create(row);
      lv_canvas_set_buffer(week_canvas_[d], week_buffers_[d], kWeekW, kWeekH, LV_IMG_CF_TRUE_COLOR);
      lv_obj_set_pos(week_canvas_[d], 167, 4);
    }
    week_empty_[d] = makeDashboardLabel(row, "尚無紀錄", 243,
      (rowH - native480_fonts::cjk_8()->line_height) / 2, 133,
      native480_fonts::cjk_8(), 0x64748b, LV_TEXT_ALIGN_CENTER);
  }
}

void refreshWeeklyPage() {
  if (!page_week_ || active_dashboard_page_ != 4) return;
  time_t now = time(nullptr); tm today{}; localtime_r(&now, &today);
  if (!WeeklyHistory::validTime(now)) {
    const char* englishDays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const char* chineseDays[] = {"週日", "週一", "週二", "週三", "週四", "週五", "週六"};
    const char** names = native480_i18n::kEnglish ? englishDays : chineseDays;
    char text[40];
    for (int d = 0; d < 7; ++d) {
      lv_obj_set_style_border_width(week_rows_[d], 0, 0);
      lv_obj_set_style_radius(week_rows_[d], 0, 0);
      lv_obj_set_style_border_color(week_rows_[d], lv_color_hex(0x263746), 0);
      lv_obj_set_style_bg_color(week_rows_[d], lv_color_hex(0x101820), 0);
      snprintf(text, sizeof(text), "%s  --/--", names[d]);
      setLabelText480(week_dates_[d], text);
      setLabelText480(week_badges_[d], "");
      lv_obj_set_style_text_color(week_badges_[d], lv_color_hex(0x64748b), 0);
      setLabelText480(week_empty_[d], "等待時間同步");
      lv_obj_clear_flag(week_empty_[d], LV_OBJ_FLAG_HIDDEN);
    }
    return;
  }
  weekly_history_.roll(now);
  float middleKw = 3;
  for (const auto& day : weekly_history_.days) for (const auto& p : day.points)
    middleKw = fmaxf(middleKw, ceilf(p.load_kw));
  const float scale = middleKw * 2;
  const char* englishDays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  const char* chineseDays[] = {"週日", "週一", "週二", "週三", "週四", "週五", "週六"};
  const char** names = native480_i18n::kEnglish ? englishDays : chineseDays;
  char text[80];
  for (int d=0; d<7; ++d) {
    const auto& day = weekly_history_.days[d];
    tm date = WeeklyHistory::dayAt(now, (today.tm_wday-d+7)%7);
    bool current = d == today.tm_wday;
    snprintf(text, sizeof(text), "%s  %02d/%02d", names[d], date.tm_mon+1, date.tm_mday);
    setLabelText480(week_dates_[d], text);
    setLabelText480(week_badges_[d], current ? "今日" : (d==0 || d==6 ? "週末" : "週間"));
    lv_obj_set_style_text_color(week_badges_[d], lv_color_hex(current ? 0x38bdf8 : d==0 || d==6 ? 0x34d399 : 0x64748b), 0);
    lv_obj_set_style_border_color(week_rows_[d], lv_color_hex(current ? 0x38bdf8 : 0x263746), 0);
    lv_obj_set_style_border_width(week_rows_[d], current ? 1 : 0, 0);
    lv_obj_set_style_radius(week_rows_[d], current ? 4 : 0, 0);
    lv_obj_set_style_bg_color(week_rows_[d], lv_color_hex(current ? 0x122332 : 0x101820), 0);
    auto avg = WeeklyHistory::averages(day,date);
    for (int s=0; s<3; ++s) {
      if (std::isfinite(avg[s])) snprintf(text,sizeof(text),"%.2f",avg[s]); else snprintf(text,sizeof(text),"—");
      updateWeeklyStatValue(week_stats_[d][s], week_stat_units_[d][s], text,
                            s == 0 ? WeeklyStatStyle::Average : WeeklyStatStyle::Detail,
                            weeklyStatValueTop(s == 0 ? 17 : s == 1 ? 15 : 41));
    }
    snprintf(text,sizeof(text),"%.0f kW",scale); setLabelText480(week_axes_[d],text);
    snprintf(text,sizeof(text),"%.0f kW",middleKw); setLabelText480(week_mid_axes_[d],text);
    const float reserve = std::isfinite(last_telemetry_.backup_reserve_percent)
      ? fminf(100, fmaxf(0, last_telemetry_.backup_reserve_percent)) : 24.f;
    snprintf(text,sizeof(text),"%.0f%%",reserve);
    setLabelText480(week_reserve_labels_[d],text);
    const int reserveY = weekChartY(48 - int(reserve*21/100));
    setLabelText480(week_empty_[d], week_buffers_[d] ? "尚無紀錄" : "圖表不可用");
    if (day.points.empty() || !week_buffers_[d]) lv_obj_clear_flag(week_empty_[d], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(week_empty_[d], LV_OBJ_FLAG_HIDDEN);
    auto* buf = week_buffers_[d]; if (!buf) continue;
    auto put = [buf](int x, int y, uint8_t r, uint8_t g, uint8_t b) {
      putDashboardPixel(buf, kWeekW, kWeekH, x, y, r, g, b);
    };
    auto line = [buf](int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b) {
      drawDashboardLine(buf, kWeekW, kWeekH, x0, y0, x1, y1, r, g, b);
    };
    for (int p=0; p<kWeekW*kWeekH; ++p) buf[p] = lv_color_hex(0x090e13);
    for (int x=0;x<kWeekW;++x) {
      for (int y : {1,13,24,27,37,48}) put(x,weekChartY(y),27,37,47);
      bool peak = WeeklyHistory::peak(date, x*24.f/(kWeekW-1));
      for (int y=weekChartY(49); y<kWeekH; ++y) put(x,y,peak?185:38,peak?49:166,peak?47:76);
    }
    for (int i=0;i<5;++i) for (int y=0;y<=weekChartY(48);++y) put(i*(kWeekW-1)/4,y,22,30,39);
    auto px = [](float hour) { return int(hour*(kWeekW-1)/24.f); };
    const int loadTopY = weekChartY(1), loadBottomY = weekChartY(24);
    const int socTopY = weekChartY(27), socBottomY = weekChartY(48);
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
      put(x,py(p.load_kw),r,g,b);
      put(x,sy(p.soe),17,229,142);
      if (!i || p.hour-day.points[i-1].hour > .17f) continue;
      const auto& prev=day.points[i-1];
      for(int xx=px(prev.hour);xx<=x;++xx){
        float ratio=x==px(prev.hour)?1.f:float(xx-px(prev.hour))/(x-px(prev.hour));
        int yy=sy(prev.soe)+(sy(p.soe)-sy(prev.soe))*ratio;
        for(int y=yy+1;y<=socBottomY;++y) put(xx,y,10,37,31);
      }
      line(px(prev.hour),py(prev.load_kw),x,py(p.load_kw),r,g,b);
      line(px(prev.hour),sy(prev.soe),x,sy(p.soe),17,229,142);
    }
    // Draw the reserve guide over the filled area and both curves.
    for (int x=0;x<kWeekW;x+=7) for(int n=0;n<4 && x+n<kWeekW;++n) put(x+n,reserveY,33,88,107);
    if(current) {
      int x=px(today.tm_hour+today.tm_min/60.f);
      line(x,0,x,kWeekH-1,225,237,245);
    }
    lv_obj_invalidate(week_canvas_[d]);
  }
}

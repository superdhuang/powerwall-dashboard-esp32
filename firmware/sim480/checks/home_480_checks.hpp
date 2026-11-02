#pragma once

// Host-only integration checks: exercise actual LVGL rendering, not a mock UI.
static int runHome480Checks(Ui480& ui) {
  // Fixed weekdays and midnight rollover must not depend on the host locale.
  const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  struct tm date = {};
  date.tm_mon = 10; date.tm_mday = 1; date.tm_hour = 4; date.tm_min = 20;
  char clock[20], expected[20];
  for (int day = 0; day < 7; ++day) {
    date.tm_wday = day;
    ui480::formatHeaderClock(clock, sizeof(clock), &date);
    if (native480_i18n::kEnglish)
      snprintf(expected, sizeof(expected), "11/01 %s 04:20", days[day]);
    else
      snprintf(expected, sizeof(expected), "11月01日 04:20");
    if (strcmp(clock, expected) != 0) return 10;
  }
  date.tm_mon = 11; date.tm_mday = 31; date.tm_wday = 4;
  date.tm_hour = 23; date.tm_min = 59;
  ui480::formatHeaderClock(clock, sizeof(clock), &date);
  if (strcmp(clock, native480_i18n::kEnglish ? "12/31 Thu 23:59" : "12月31日 23:59") != 0) return 10;
  date.tm_mon = 0; date.tm_mday = 1; date.tm_wday = 5;
  date.tm_hour = 0; date.tm_min = 0;
  ui480::formatHeaderClock(clock, sizeof(clock), &date);
  if (strcmp(clock, native480_i18n::kEnglish ? "01/01 Fri 00:00" : "01月01日 00:00") != 0) return 10;
  ui480::formatHeaderClock(clock, sizeof(clock), nullptr);
  if (strcmp(clock, "--/-- --- --:--") != 0) return 10;
  ui.setDashboardPageForSimulator(0);
  ui.setAnimationActive(false);
  PowerwallTelemetry tel;
  tel.ok = true; tel.site_name = native480_i18n::kDefaultSiteTitle; tel.grid_status = "SystemGridConnected";
  tel.solar_power_w = 2400; tel.load_power_w = 2700; tel.site_power_w = 300;
  tel.battery_power_w = -3200; tel.soe_percent = 82;
  tel.nominal_full_kwh = 14.6; tel.nominal_remaining_kwh = 11.97;
  tel.backup_reserve_percent = 24;
  tel.history_count = 49;
  for (size_t i = 0; i < tel.history_count; ++i) {
    tel.history[i].hour = i * 0.5f;
    tel.history[i].soe = 50 + 30 * sinf(i * 0.13f);
  }
  WeatherSnapshot weather;
  weather.ok = true; weather.mode = "night"; weather.godot_moon_phase = 0.5f;
  std::vector<uint16_t> partial(kWinW * kWinH);
  auto save = [&](const char* name) {
    const std::string path = std::string("/tmp/powerwall-home480-") + name + ".ppm";
    FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) return false;
    std::fprintf(file, "P6\n480 480\n255\n");
    for (int y = 0; y < 480; ++y) for (int x = 0; x < 480; ++x) {
      const uint16_t px = g_fb[y * kWinW + x];
      const uint8_t rgb[] = {static_cast<uint8_t>(((px >> 11) & 31) * 255 / 31),
                            static_cast<uint8_t>(((px >> 5) & 63) * 255 / 63),
                            static_cast<uint8_t>((px & 31) * 255 / 31)};
      std::fwrite(rgb, 1, sizeof(rgb), file);
    }
    return std::fclose(file) == 0;
  };
  auto check = [&](const char* name, bool configured, bool live) {
    ui.render(tel, weather, configured);
    ui.setAnimationActive(true);
    SDL_Delay(2); // Advance the host's custom LVGL tick before servicing timers.
    lv_timer_handler();
    ui.setAnimationActive(false);
    lv_obj_update_layout(lv_scr_act());
    lv_refr_now(nullptr);
    if (!ui.simulatorHome480Valid(live)) {
      std::fprintf(stderr, "[Home480] FAIL: %s status or visible bounds\n", name); return false;
    }
    // Verify the reserve dash on the rendered native 480 framebuffer.
    lv_area_t plot; ui.simulatorHome480PlotBounds(plot);
    const int line_y = plot.y1 + static_cast<int>(roundf(
      (1.0f - tel.backup_reserve_percent / 100.0f) * (plot.y2 - plot.y1 + 1 - 3))) + 1;
    unsigned blue_pixels = 0;
    for (int py = 0; py < 480; ++py) {
      const int sy = py;
      if (sy < line_y || sy > line_y + 1) continue;
      for (int px = 0; px < 480; ++px) {
        const int sx = px;
        if (sx < plot.x1 + 67 || sx > plot.x1 + 233) continue;
        const uint16_t pixel = g_fb[sy * kWinW + sx];
        const int r = ((pixel >> 11) & 31) * 255 / 31;
        const int g = ((pixel >> 5) & 63) * 255 / 63;
        const int b = (pixel & 31) * 255 / 31;
        if (r < 110 && g > 60 && b > g + 5) ++blue_pixels;
      }
    }
    if (blue_pixels < 20) {
      std::fprintf(stderr, "[Home480] FAIL: %s reserve dash invisible at physical 480 sampling (%u blue pixels)\n", name, blue_pixels);
      return false;
    }
    std::copy(g_fb, g_fb + partial.size(), partial.begin());
    lv_obj_invalidate(lv_scr_act()); lv_refr_now(nullptr);
    if (!std::equal(partial.begin(), partial.end(), g_fb)) {
      std::fprintf(stderr, "[Home480] FAIL: %s partial redraw differs from full redraw\n", name); return false;
    }
    return save(name);
  };
  // Connection indicator semantics, both blink phases, and long numeric values.
  const char* states[] = {"live", "stale", "offline", "demo", "unconfigured"};
  for (int i = 0; i < 5; ++i) {
    tel.ok = i != 2; tel.stale = i == 1; tel.demo = i == 3;
    tel.soe_percent = i % 2 ? 0 : 100;
    tel.load_power_w = 12500; tel.battery_power_w = i % 2 ? -12500 : 12500;
    tel.site_power_w = -12500;
    for (int phase = 0; phase < 2; ++phase) {
      simMillisOffset().fetch_add(500);
      if (!check(states[i], i != 4, i == 0)) return 10;
    }
  }
  tel.ok = true; tel.stale = false; tel.demo = true; tel.soe_percent = 82;
  for (int reserve : {0, 24, 50, 100}) {
    tel.backup_reserve_percent = reserve;
    char name[40]; std::snprintf(name, sizeof(name), "reserve-%d", reserve);
    if (!check(name, true, false)) return 15;
  }
  tel.backup_reserve_percent = 24;
  // Exercise repeated layout updates at the same location as the reported
  // missing point, including both ends of the SOC range and both blink phases.
  tel.history[tel.history_count - 1].hour = 21.2f;
  for (float soe : {100.0f, 0.0f, 82.0f}) {
    tel.history[tel.history_count - 1].soe = soe;
    if (!check("point-before", true, false)) return 16;
    const lv_opa_t before = ui.simulatorHome480PointOpacity();
    if (soe == 100.0f) save(before == LV_OPA_COVER ? "point-visible" : "point-hidden");
    simMillisOffset().fetch_add(500);
    if (!check("point-after", true, false)) return 16;
    if (before == ui.simulatorHome480PointOpacity()) {
      std::fprintf(stderr, "[Home480] FAIL: current point did not blink\n"); return 16;
    }
    if (soe == 100.0f) save(ui.simulatorHome480PointOpacity() == LV_OPA_COVER
      ? "point-visible" : "point-hidden");
  }
  tel.load_power_w = 2700; tel.battery_power_w = -3200; tel.site_power_w = 300;
  char response[512];
  double frame_ms = 0; unsigned frames = 0;
  for (int mode = 0; mode < 10; ++mode) {
    ui.handlePhaseConsoleLine("w", response, sizeof(response));
    tel.grid_status = mode % 3 == 0 ? "SystemIslandedActive" : "SystemGridConnected";
    tel.battery_power_w = mode % 3 == 0 ? 2700 : mode % 3 == 1 ? -3200 : 0;
    uint64_t first = 0; bool animated = false;
    for (int frame = 0; frame < 3; ++frame) {
      simMillisOffset().fetch_add(217);
      const auto start = std::chrono::steady_clock::now();
      char name[40]; std::snprintf(name, sizeof(name), "weather-%02d-%d", mode + 1, frame);
      if (!check(name, true, false)) return 11;
      frame_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
      ++frames;
      const auto hash = ui.simulatorStageChecksum();
      if (!frame) first = hash; else animated |= hash != first;
    }
    // Rain, storm, snow, wind have animated background frames even at standby.
    if (mode >= 3 && mode <= 6 && !animated) {
      std::fprintf(stderr, "[Home480] FAIL: weather %d stopped animating\n", mode + 1); return 12;
    }
  }
  for (int phase = 0; phase < 8; ++phase) {
    ui.handlePhaseConsoleLine("m", response, sizeof(response));
    char name[32]; std::snprintf(name, sizeof(name), "moon-%d", phase);
    if (!check(name, true, false)) return 13;
  }
  for (int phase = 0; phase < 9; ++phase) {
    ui.handlePhaseConsoleLine("s", response, sizeof(response));
    char name[32]; std::snprintf(name, sizeof(name), "sun-%d", phase);
    if (!check(name, true, false)) return 14;
  }
  for (const auto& card : {std::pair<uint8_t, const char*>{7, "gateway-setup"},
                           std::pair<uint8_t, const char*>{6, "gateway-retry"}}) {
    ui.setSimulatorWifiCardMode(card.first, "Home-WiFi", "http://192.0.2.13");
    ui.render(tel, weather, true);
    ui.setAnimationActive(true);
    SDL_Delay(2);
    lv_timer_handler();
    ui.setAnimationActive(false);
    lv_obj_update_layout(lv_scr_act());
    lv_refr_now(nullptr);
    if (!save(card.second)) {
      std::fprintf(stderr, "[Home480] FAIL: could not capture %s setup card\n", card.second);
      return 17;
    }
  }
  simMillisOffset().store(0);
  std::printf("[Home480] PASS: live/stale/offline/demo/unconfigured, blink phases, long values, "
              "10 weather modes, 8 moon phases, 9 sun positions; partial/full redraws match.\n");
  std::printf("[Home480] Captured setup and retry cards with dynamic simulator URL http://192.0.2.13.\n");
  std::printf("[Home480] Host render + full-redraw comparison + capture: %.2f ms/case (%u weather frames).\n",
              frame_ms / frames, frames);
  return 0;
}

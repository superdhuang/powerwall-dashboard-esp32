#pragma once

static int runPowerflowChecks(UI720x720& ui, DisplayDriver720& driver, bool realtime) {
  struct Scenario {
    const char* name;
    float solar, battery, grid, load;
    bool offgrid;
    uint8_t mask;
  };
  const Scenario cases[] = {
    {"grid-home", 0, 0, 1800, 1800, false, 0x3},
    {"grid-charge", 0, -1500, 3000, 1500, false, 0x7},
    {"battery-home", 0, 1800, 0, 1800, false, 0x6},
    {"solar-export", 4000, -1000, -1000, 2000, false, 0xf},
    {"offgrid", 1000, 1000, 0, 2000, true, 0xe},
    {"idle", 0, 0, 0, 0, false, 0},
  };
  PowerwallTelemetry tel;
  tel.ok = true; tel.site_name = "Flow regression"; tel.soe_percent = 82;
  WeatherSnapshot weather;
  weather.ok = true; weather.mode = "clear"; weather.is_day = true;
  ui.setDashboardPageForSimulator(0);
  ui.setAnimationActive(true);
  g_check_scaled_flush = true;
  auto scaledMatches = []() {
    for (int y = 0; y < 480; ++y) for (int x = 0; x < 480; ++x)
      if (g_scaled_fb[y * 480 + x] != g_fb[(y * 3 / 2) * kWinW + x * 3 / 2]) return false;
    return true;
  };
  for (const auto& test : cases) {
    tel.solar_power_w = test.solar; tel.battery_power_w = test.battery;
    tel.site_power_w = test.grid; tel.load_power_w = test.load;
    tel.grid_status = test.offgrid ? "SystemIslandedActive" : "SystemGridConnected";
    ui.render(tel, weather, true);
    lv_obj_invalidate(lv_scr_act()); lv_refr_now(nullptr);
    const uint32_t started = ui.simulatorPowerflowStartedAt();
    if (ui.simulatorPowerflowMask() != test.mask) {
      std::fprintf(stderr, "[Powerflow] FAIL: %s wrong enabled branches\n", test.name); return 20;
    }
    uint64_t previous[4] = {};
    unsigned unchanged[4] = {};
    // 5 seconds per state, 30 seconds total, through the actual 33 ms LVGL timer.
    for (unsigned frame = 0; frame < 152; ++frame) {
      if (realtime) std::this_thread::sleep_for(std::chrono::milliseconds(33));
      else simMillisOffset().fetch_add(33);
      driver.tick(); lv_timer_handler(); lv_refr_now(nullptr);
      for (unsigned wire = 0; wire < 4; ++wire) {
        const uint64_t hash = ui.simulatorWireChecksum(wire);
        unchanged[wire] = hash == previous[wire] ? unchanged[wire] + 1 : 0;
        previous[wire] = hash;
        if ((test.mask & (1 << wire)) && unchanged[wire] >= 15) {
          std::fprintf(stderr, "[Powerflow] FAIL: %s wire %u frozen for 0.5s\n", test.name, wire); return 21;
        }
      }
      if (!scaledMatches()) {
        std::fprintf(stderr, "[Powerflow] FAIL: %s partial scaled flush lost pixels\n", test.name); return 22;
      }
      if (frame >= 30 && frame <= 60 && frame % 10 == 0) {
        char path[160];
        std::snprintf(path, sizeof(path), "/tmp/powerwall-flow-%d-%s-%03u.ppm",
                      kWindowSize, test.name, frame);
        if (!saveFramebufferPpm(path)) return 26;
      }
      if (frame % 30 == 0) {
        // Poll updates change values, without changing the route or its phase.
        tel.load_power_w = test.load ? test.load + frame : 0;
        ui.render(tel, weather, true);
        if (ui.simulatorPowerflowStartedAt() != started) {
          std::fprintf(stderr, "[Powerflow] FAIL: %s telemetry restarted flow\n", test.name); return 23;
        }
        lv_refr_now(nullptr);
        const std::vector<uint16_t> partial(g_fb, g_fb + kWinW * kWinH);
        lv_obj_invalidate(lv_scr_act()); lv_refr_now(nullptr);
        if (!std::equal(partial.begin(), partial.end(), g_fb) || !scaledMatches()) {
          std::fprintf(stderr, "[Powerflow] FAIL: %s partial/full redraw mismatch\n", test.name); return 24;
        }
      }
    }
    std::printf("[Powerflow] PASS: %s, 5s rendered frames, phase continuity, scaled partial/full redraw.\n", test.name);
  }
  if (!g_tall_scaled_flushes) {
    std::fprintf(stderr, "[Powerflow] FAIL: no tall flush reproduced\n"); return 25;
  }
  std::printf("[Powerflow] PASS: %u tall LVGL flushes delivered; 30s total %s animation.\n",
              g_tall_scaled_flushes, realtime ? "wall-clock" : "simulated");
  return 0;
}

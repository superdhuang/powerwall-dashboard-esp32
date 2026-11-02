#pragma once

// Host-only integration check for the Powerwall gauge intro animation:
// entering the Powerwall page sweeps the SOC arc, the centre percent text, and
// the white backup-reserve marker up from 0 to their configured targets.
static int runPwGaugeIntroChecks(Ui480& ui) {
  WeatherSnapshot weather;
  weather.ok = true; weather.mode = "clear";
  PowerwallTelemetry tel;
  tel.ok = true; tel.grid_status = "SystemGridConnected"; tel.has_grid_status = true;
  tel.solar_power_w = 4000; tel.load_power_w = 700; tel.site_power_w = 0;
  tel.battery_power_w = -3300; tel.has_battery_power = true;
  tel.soe_percent = 93; tel.backup_reserve_percent = 24;
  tel.nominal_full_kwh = 14.59; tel.nominal_remaining_kwh = 13.59;
  tel.has_battery_capacity = true; tel.has_load_power = true;

  auto markerAt = [](float pct) {
    const float a = (135.0f + 270.0f * pct / 100.0f) * 0.01745329252f;
    return lv_point_t{static_cast<lv_coord_t>(roundf(117.0f + 116.0f * cosf(a))),
                      static_cast<lv_coord_t>(roundf(121.0f + 116.0f * sinf(a)))};
  };
  // Angle of the marker along the gauge, as a percent of the 270-degree sweep.
  auto markerPct = [](lv_point_t p) {
    float deg = atan2f(static_cast<float>(p.y) - 121.0f, static_cast<float>(p.x) - 117.0f) * 57.2957795f;
    while (deg < 135.0f - 1.0f) deg += 360.0f;
    return (deg - 135.0f) / 2.7f;
  };
  auto pump = [] { SDL_Delay(4); lv_timer_handler(); };
  auto capture = [](const char* name) {
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(nullptr);
    const std::string path = std::string("/tmp/powerwall-gauge-intro-") + name + ".ppm";
    return saveFramebufferPpm(path.c_str());
  };

  struct Case { float soc; float reserve; const char* tag; };
  const Case cases[] = {{93, 24, "high"}, {10, 24, "low"}, {70, 40, "mid"}};
  for (const Case& c : cases) {
    tel.soe_percent = c.soc; tel.backup_reserve_percent = c.reserve;
    ui.setDashboardPageForSimulator(0);
    ui.render(tel, weather, true);
    pump();
    ui.nextDashboardPage();  // Original -> Powerwall through the real fade route.

    bool sawStart = false, sawMid = false, finished = false, refreshedMid = false;
    int32_t lastArc = -1; float lastMarker = -1.0f;
    const uint32_t deadline = SDL_GetTicks() + 3000;
    while (SDL_GetTicks() < deadline) {
      pump();
      const auto s = ui.powerwallGaugeStateForSimulator();
      if (s.page != 1) continue;
      const float mPct = markerPct(s.marker_outer);
      if (!sawStart) {
        if (!s.intro_active || s.arc_value > 150 || mPct > 8.0f) {
          std::fprintf(stderr, "[PwGaugeIntro] FAIL %s: intro did not start near 0 (active=%d arc=%d marker=%.1f%%)\n",
                       c.tag, s.intro_active, static_cast<int>(s.arc_value), mPct);
          return 2;
        }
        sawStart = true;
        if (std::strcmp(c.tag, "high") == 0) capture("start");
      }
      if (s.arc_value + 1 < lastArc || mPct + 1.0f < lastMarker) {
        std::fprintf(stderr, "[PwGaugeIntro] FAIL %s: sweep went backwards (arc %d->%d, marker %.1f->%.1f)\n",
                     c.tag, static_cast<int>(lastArc), static_cast<int>(s.arc_value), lastMarker, mPct);
        return 3;
      }
      if (mPct > c.reserve + 1.0f || s.arc_value > static_cast<int32_t>(c.soc * 10.0f) + 1) {
        std::fprintf(stderr, "[PwGaugeIntro] FAIL %s: overshoot (arc=%d marker=%.1f%%)\n",
                     c.tag, static_cast<int>(s.arc_value), mPct);
        return 4;
      }
      lastArc = s.arc_value; lastMarker = mPct;
      if (s.intro_active) {
        // Shared progress: marker fraction of reserve tracks arc fraction of SOC.
        const float arcFrac = s.arc_value / (c.soc * 10.0f);
        const float markerFrac = mPct / c.reserve;
        if (fabsf(arcFrac - markerFrac) > 0.06f) {
          std::fprintf(stderr, "[PwGaugeIntro] FAIL %s: marker not in step with arc (arc %.2f vs marker %.2f)\n",
                       c.tag, arcFrac, markerFrac);
          return 9;
        }
      }
      if (s.intro_active && !refreshedMid && s.arc_value > 50) {
        // A telemetry refresh mid-animation must not snap the gauge to its target.
        ui.render(tel, weather, true);
        const auto r = ui.powerwallGaugeStateForSimulator();
        if (!r.intro_active || r.arc_value != s.arc_value) {
          std::fprintf(stderr, "[PwGaugeIntro] FAIL %s: telemetry refresh interrupted intro (arc %d->%d)\n",
                       c.tag, static_cast<int>(s.arc_value), static_cast<int>(r.arc_value));
          return 5;
        }
        refreshedMid = true;
      }
      if (s.intro_active && !sawMid && s.arc_value >= static_cast<int32_t>(c.soc * 5.0f)) {
        sawMid = true;
        if (std::strcmp(c.tag, "high") == 0) capture("mid");
      }
      if (!s.intro_active) { finished = true; break; }
    }
    const auto end = ui.powerwallGaugeStateForSimulator();
    char expectedText[8];
    std::snprintf(expectedText, sizeof(expectedText), "%d%%", static_cast<int>(roundf(c.soc)));
    const lv_point_t expectedMarker = markerAt(c.reserve);
    if (!sawStart || !finished || end.arc_value != static_cast<int32_t>(c.soc * 10.0f) ||
        std::strcmp(end.percent_text, expectedText) != 0 ||
        end.marker_outer.x != expectedMarker.x || end.marker_outer.y != expectedMarker.y) {
      std::fprintf(stderr, "[PwGaugeIntro] FAIL %s: final state arc=%d text=%s marker=(%d,%d) expected arc=%d text=%s marker=(%d,%d)\n",
                   c.tag, static_cast<int>(end.arc_value), end.percent_text,
                   end.marker_outer.x, end.marker_outer.y,
                   static_cast<int>(c.soc * 10.0f), expectedText, expectedMarker.x, expectedMarker.y);
      return 6;
    }
    if (std::strcmp(c.tag, "high") == 0) capture("end");

    // Staying on the Powerwall page: new telemetry must not replay the intro.
    tel.soe_percent = c.soc + 1.0f;
    ui.render(tel, weather, true);
    pump();
    const auto again = ui.powerwallGaugeStateForSimulator();
    if (again.intro_active || again.arc_value != static_cast<int32_t>((c.soc + 1.0f) * 10.0f)) {
      std::fprintf(stderr, "[PwGaugeIntro] FAIL %s: telemetry on Powerwall page replayed intro (active=%d arc=%d)\n",
                   c.tag, again.intro_active, static_cast<int>(again.arc_value));
      return 7;
    }
    std::printf("[PwGaugeIntro] PASS %s: SOC %.0f%% / reserve %.0f%% swept from 0\n", c.tag, c.soc, c.reserve);
  }

  // Capture hook must cancel a running intro and show final values immediately.
  tel.soe_percent = 80; tel.backup_reserve_percent = 30;
  ui.setDashboardPageForSimulator(0);
  ui.render(tel, weather, true);
  pump();
  ui.nextDashboardPage();
  const uint32_t deadline = SDL_GetTicks() + 1000;
  while (SDL_GetTicks() < deadline && !ui.powerwallGaugeStateForSimulator().intro_active) pump();
  ui.setDashboardPageForSimulator(1);
  const auto hooked = ui.powerwallGaugeStateForSimulator();
  const lv_point_t hookedMarker = markerAt(30);
  if (hooked.intro_active || hooked.arc_value != 800 ||
      hooked.marker_outer.x != hookedMarker.x || hooked.marker_outer.y != hookedMarker.y) {
    std::fprintf(stderr, "[PwGaugeIntro] FAIL: capture hook did not finish intro (active=%d arc=%d)\n",
                 hooked.intro_active, static_cast<int>(hooked.arc_value));
    return 8;
  }
  std::printf("[PwGaugeIntro] PASS: capture hook cancels intro; captures /tmp/powerwall-gauge-intro-{start,mid,end}.ppm\n");
  return 0;
}

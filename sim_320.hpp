// Compact desktop preview. Included by sim_main.cpp so it shares the same
// telemetry, weather, history and 3D compositing code as the 720px simulator.
static constexpr int MINI_STAGE_W = 304;
static constexpr int MINI_STAGE_H = 190;
static constexpr int MINI_CHART_W = 252;
static constexpr int MINI_CHART_H = 23;
static uint8_t mini_stage_buf[MINI_STAGE_W * MINI_STAGE_H * 4];
static uint8_t mini_load_buf[MINI_CHART_W * MINI_CHART_H * 4];
static uint8_t mini_soe_buf[MINI_CHART_W * MINI_CHART_H * 4];
static lv_obj_t *mini_stage, *mini_load_chart, *mini_soe_chart;
static lv_obj_t *mini_title, *mini_status, *mini_weather;
static lv_obj_t *mini_solar, *Home-WiFi, *mini_powerwall, *mini_grid;
static lv_obj_t *mini_load, *mini_soc, *mini_energy, *mini_eta;
static lv_obj_t *mini_mode_btn, *mini_weather_btn;

static lv_obj_t* mini_label(lv_obj_t* parent, const char* value, lv_font_t* font,
                            uint32_t color, int x, int y, int width = 0) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_label_set_text(label, value);
    if (width) {
        lv_obj_set_width(label, width);
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    }
    lv_obj_set_pos(label, x, y);
    return label;
}

static lv_obj_t* mini_card(lv_obj_t* parent, int x, int y, int w, int h) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x131820), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x303641), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 9, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

static void update_dashboard_320_ui();

static void mini_mode_clicked(lv_event_t*) {
    static const std::vector<std::string> modes = {"live", "solar", "charge", "peak", "low", "offgrid"};
    auto it = std::find(modes.begin(), modes.end(), g_state_mode);
    g_state_mode = modes[(it == modes.end() ? 0 : std::distance(modes.begin(), it) + 1) % modes.size()];
    update_dashboard_320_ui();
}

static void mini_weather_clicked(lv_event_t*) {
    static const std::vector<std::string> modes = {"auto", "clear", "partly", "rain", "storm", "snow", "night"};
    auto it = std::find(modes.begin(), modes.end(), g_weather_mode);
    g_weather_mode = modes[(it == modes.end() ? 0 : std::distance(modes.begin(), it) + 1) % modes.size()];
    update_dashboard_320_ui();
}

static void build_lvgl_320_ui() {
    const char* font_path = access("assets/STHeiti-Medium.ttc", R_OK) == 0
        ? "assets/STHeiti-Medium.ttc" : "/System/Library/Fonts/STHeiti Medium.ttc";
    auto font = [&](int size, bool bold, const lv_font_t* fallback) -> lv_font_t* {
        lv_font_t* f = lv_freetype_font_create(font_path, LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
            size, bold ? LV_FREETYPE_FONT_STYLE_BOLD : LV_FREETYPE_FONT_STYLE_NORMAL);
        return f ? f : const_cast<lv_font_t*>(fallback);
    };
    lv_font_t* f10 = font(10, false, &lv_font_montserrat_12);
    lv_font_t* f11 = font(11, false, &lv_font_montserrat_12);
    lv_font_t* f12 = font(12, false, &lv_font_montserrat_12);
    lv_font_t* f14 = font(14, true, &lv_font_montserrat_14);
    lv_font_t* f17 = font(17, true, &lv_font_montserrat_18);

    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0b0c0f), 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* scene = mini_card(scr, 8, 8, 304, 190);
    lv_obj_set_style_clip_corner(scene, true, 0);
    mini_stage = lv_canvas_create(scene);
    lv_canvas_set_buffer(mini_stage, mini_stage_buf, MINI_STAGE_W, MINI_STAGE_H, LV_COLOR_FORMAT_ARGB8888);
    lv_obj_set_pos(mini_stage, 0, 0);

    // The two header areas are tap targets for state and weather previews.
    mini_mode_btn = lv_obj_create(scene);
    lv_obj_set_pos(mini_mode_btn, 0, 0);
    lv_obj_set_size(mini_mode_btn, 151, 33);
    lv_obj_set_style_bg_opa(mini_mode_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mini_mode_btn, 0, 0);
    lv_obj_set_style_pad_all(mini_mode_btn, 0, 0);
    lv_obj_add_event_cb(mini_mode_btn, mini_mode_clicked, LV_EVENT_CLICKED, nullptr);
    mini_title = mini_label(mini_mode_btn, "我的家", f14, 0xffffff, 8, 2, 137);
    mini_status = mini_label(mini_mode_btn, "即時", f10, 0x6ee7b7, 9, 19, 137);

    mini_weather_btn = lv_obj_create(scene);
    lv_obj_set_pos(mini_weather_btn, 155, 0);
    lv_obj_set_size(mini_weather_btn, 149, 33);
    lv_obj_set_style_bg_opa(mini_weather_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mini_weather_btn, 0, 0);
    lv_obj_set_style_pad_all(mini_weather_btn, 0, 0);
    lv_obj_add_event_cb(mini_weather_btn, mini_weather_clicked, LV_EVENT_CLICKED, nullptr);
    mini_weather = mini_label(mini_weather_btn, "台北 · 夜晚", f11, 0xbae6fd, 1, 9, 143);
    lv_obj_set_style_text_align(mini_weather, LV_TEXT_ALIGN_RIGHT, 0);

    mini_label(scene, "太陽能", f10, 0xb8c0c8, 113, 38);
    mini_solar = mini_label(scene, "0 kW", f14, 0xffffff, 107, 51);
    mini_label(scene, "住家", f10, 0xb8c0c8, 210, 67);
    Home-WiFi = mini_label(scene, "2.8 kW", f14, 0xffffff, 203, 80);
    mini_label(scene, "POWERWALL", f10, 0xb8c0c8, 34, 145);
    mini_powerwall = mini_label(scene, "0 kW · 100%", f12, 0xffffff, 34, 158);
    mini_label(scene, "電網", f10, 0xb8c0c8, 204, 145);
    mini_grid = mini_label(scene, "2.8 kW", f12, 0xffffff, 204, 158);

    lv_obj_t* stats = mini_card(scr, 8, 204, 304, 53);
    mini_label(stats, "住家用量", f10, 0xa1a1aa, 9, 4);
    mini_load = mini_label(stats, "2.8 kW", f17, 0x38bdf8, 9, 18);
    mini_label(stats, "Powerwall", f10, 0xa1a1aa, 115, 4);
    mini_soc = mini_label(stats, "100%", f17, 0x00e676, 115, 18);
    mini_energy = mini_label(stats, "14.59 / 14.59 kWh", f10, 0xc2c7cc, 184, 15, 113);
    mini_eta = mini_label(stats, "· --", f11, 0xe5e7eb, 184, 29, 113);

    lv_obj_t* charts = mini_card(scr, 8, 263, 304, 49);
    mini_label(charts, "kW", f10, 0x38bdf8, 5, 7);
    mini_label(charts, "%", f10, 0x00e676, 8, 28);
    mini_load_chart = lv_canvas_create(charts);
    lv_canvas_set_buffer(mini_load_chart, mini_load_buf, MINI_CHART_W, MINI_CHART_H, LV_COLOR_FORMAT_ARGB8888);
    lv_obj_set_pos(mini_load_chart, 46, 1);
    mini_soe_chart = lv_canvas_create(charts);
    lv_canvas_set_buffer(mini_soe_chart, mini_soe_buf, MINI_CHART_W, MINI_CHART_H, LV_COLOR_FORMAT_ARGB8888);
    lv_obj_set_pos(mini_soe_chart, 46, 23);
}

static void update_dashboard_320_ui() {
    NvsConfig cfg;
    TelemetryData tel;
    WeatherData weather;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        cfg = g_nvs;
        tel = g_telemetry;
        weather = g_weather;
    }
    const bool live = g_state_mode == "live" || g_state_mode == "live_ip";
    double solar = tel.solar_w, pw = tel.pw_w, grid = tel.grid_w;
    double load = tel.load_w, soe = tel.soe_pct;
    bool offgrid = tel.grid_status != "SystemGridConnected";
    const char* mode_label = "即時";
    if (g_state_mode == "solar") {
        solar = 5000; pw = -3200; grid = 0; load = 1800; soe = 78; offgrid = false; mode_label = "太陽能預覽";
    } else if (g_state_mode == "charge") {
        solar = 0; pw = -5000; grid = 7200; load = 2200; soe = 68; offgrid = false; mode_label = "電網充電預覽";
    } else if (g_state_mode == "peak") {
        solar = 0; pw = 3200; grid = 0; load = 3200; soe = 55; offgrid = false; mode_label = "高峰放電預覽";
    } else if (g_state_mode == "low") {
        solar = 0; pw = 0; grid = 1800; load = 1800; soe = 24; offgrid = false; mode_label = "備援底線預覽";
    } else if (g_state_mode == "offgrid") {
        solar = 0; pw = 2500; grid = 0; load = 2500; soe = 82; offgrid = true; mode_label = "斷網預覽";
    } else if (!cfg.configured || !tel.ok) {
        solar = pw = grid = load = 0; soe = 0; offgrid = false;
        mode_label = cfg.configured ? "等待即時資料" : "等待配網";
    }
    lv_label_set_text(mini_title, tel.site_name.c_str());
    lv_label_set_text(mini_status, mode_label);
    lv_obj_set_style_text_color(mini_status, lv_color_hex(live && (!cfg.configured || !tel.ok) ? 0xfcd34d : 0x6ee7b7), 0);
    g_active_stage_key = g_weather_mode == "auto" ? (g_state_mode == "solar" ? "clear" : weather.mode) : g_weather_mode;
    std::string weather_text = weather.city + " · " +
        (g_weather_mode == "auto" ? weather.label : g_weather_mode) + " " +
        std::to_string((int)round(weather.temp_c)) + "°";
    lv_label_set_text(mini_weather, weather_text.c_str());

    char buf[80];
    snprintf(buf, sizeof(buf), "%.1f kW", solar / 1000.0); lv_label_set_text(mini_solar, buf);
    snprintf(buf, sizeof(buf), "%.1f kW", load / 1000.0); lv_label_set_text(Home-WiFi, buf);
    snprintf(buf, sizeof(buf), "%.1f kW · %d%%", fabs(pw) / 1000.0, (int)round(soe));
    lv_label_set_text(mini_powerwall, buf);
    lv_label_set_text(mini_grid, offgrid ? "斷網" : (snprintf(buf, sizeof(buf), "%.1f kW", fabs(grid) / 1000.0), buf));
    lv_obj_set_style_text_color(mini_grid, lv_color_hex(offgrid ? 0xfb7185 : 0xffffff), 0);
    snprintf(buf, sizeof(buf), "%.1f kW", load / 1000.0); lv_label_set_text(mini_load, buf);
    snprintf(buf, sizeof(buf), "%d%%", (int)round(soe)); lv_label_set_text(mini_soc, buf);
    double full = tel.nom_full_kwh > 0 ? tel.nom_full_kwh : 14.6;
    double rem = live ? tel.nom_rem_kwh : full * soe / 100.0;
    snprintf(buf, sizeof(buf), "%.2f / %.2f kWh", rem, full); lv_label_set_text(mini_energy, buf);
    formatUsableEta(buf, sizeof(buf), rem, full, tel.backup_reserve_pct, load / 1000.0);
    lv_label_set_text(mini_eta, buf);

    g_active_solar_w = solar; g_active_pw_w = pw; g_active_grid_w = grid;
    g_active_load_w = load; g_active_soe_pct = soe; g_active_offgrid = offgrid;
    update_3d_route(solar, pw, grid, load, offgrid);
    render_area_chart_to_buf(mini_load_buf, MINI_CHART_W, MINI_CHART_H, tel.history,
                             false, 6.0, 56, 189, 248, 0);
    render_area_chart_to_buf(mini_soe_buf, MINI_CHART_W, MINI_CHART_H, tel.history,
                             true, 100.0, 0, 230, 118, tel.backup_reserve_pct);
    lv_obj_invalidate(mini_load_chart);
    lv_obj_invalidate(mini_soe_chart);
}

static void render_stage_320_frame() {
    render_3d_stage_frame();
    // 704:440 and 304:190 have the same aspect ratio. Bilinear sampling keeps
    // the pre-rendered scene legible while all 320px labels remain native LVGL.
    for (int y = 0; y < MINI_STAGE_H; ++y) {
        const double sy = (y + .5) * STAGE_H / MINI_STAGE_H - .5;
        const int y0 = std::max(0, std::min(STAGE_H - 1, (int)floor(sy)));
        const int y1 = std::min(STAGE_H - 1, y0 + 1);
        const double fy = std::max(0.0, std::min(1.0, sy - y0));
        for (int x = 0; x < MINI_STAGE_W; ++x) {
            const double sx = (x + .5) * STAGE_W / MINI_STAGE_W - .5;
            const int x0 = std::max(0, std::min(STAGE_W - 1, (int)floor(sx)));
            const int x1 = std::min(STAGE_W - 1, x0 + 1);
            const double fx = std::max(0.0, std::min(1.0, sx - x0));
            for (int c = 0; c < 4; ++c) {
                const double a = g_stage_buf[(y0 * STAGE_W + x0) * 4 + c] * (1 - fx) +
                                 g_stage_buf[(y0 * STAGE_W + x1) * 4 + c] * fx;
                const double b = g_stage_buf[(y1 * STAGE_W + x0) * 4 + c] * (1 - fx) +
                                 g_stage_buf[(y1 * STAGE_W + x1) * 4 + c] * fx;
                mini_stage_buf[(y * MINI_STAGE_W + x) * 4 + c] = (uint8_t)round(a * (1 - fy) + b * fy);
            }
        }
    }
    lv_obj_invalidate(mini_stage);
}

#include <Arduino.h>
#include <cstdarg>
#include <limits>
#include <string>
#include "gateway_console.hpp"
#include "console_preview.hpp"
#include "history_save_console.hpp"

static unsigned long nowMs = 10000;
extern "C" unsigned long millis(void) { return nowMs; }

struct Capture {
  std::string text;
  void printf(const char* format, ...) {
    char row[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(row, sizeof(row), format, args);
    va_end(args);
    text += row;
  }
};

#define CHECK(c, message) do { if (!(c)) { std::fprintf(stderr, "FAIL: %s (line %d)\n", message, __LINE__); return 1; } } while (0)

int main() {
  CHECK(isHistorySaveKey('6') && !isConsolePreviewKey('6') && !isHistorySaveKey('5'), "6 is a separate history diagnostic key");
  HistorySaveRequest request;
  CHECK(!request.take() && request.request() && !request.request(), "pending requests coalesce");
  CHECK(request.take() && !request.take() && !request.request(), "a running write cannot be reentered or queued again");
  request.finish();
  CHECK(request.request() && request.take(), "next key works after completion");
  request.finish();
  Capture historyLog;
  unsigned calls = 0;
  CHECK(runHistorySave(historyLog, "daily", "manual", [&] { ++calls; nowMs += 37; return true; }), "successful writer result");
  CHECK(!runHistorySave(historyLog, "weekly", "auto", [&] { ++calls; nowMs += 11; return false; }), "failed writer result");
  CHECK(calls == 2 && historyLog.text.find("source=manual target=daily BEGIN") != std::string::npos &&
        historyLog.text.find("source=manual target=daily END OK elapsed_ms=37") != std::string::npos &&
        historyLog.text.find("source=auto target=weekly END FAILED elapsed_ms=11") != std::string::npos,
        "diagnostics distinguish trigger, target, real writer call, duration and failure");
  nowMs = 10000;
  PowerwallTelemetry live;
  live.ok = true;
  live.load_power_w = 3280;
  live.has_load_power = true;
  live.load_voltage_v = 111.6f;
  live.has_load_voltage = true;
  live.load_current_a = 29.4f;
  live.has_load_current = true;
  live.site_phase_current_a[0] = 19.53f;
  live.has_site_phase_current[0] = true;
  live.site_phase_power_w[0] = 1929;
  live.has_site_phase_power[0] = true;
  live.site_phase_current_a[1] = 12.88f;
  live.has_site_phase_current[1] = true;
  live.site_phase_power_w[1] = 1322;
  live.has_site_phase_power[1] = true;
  live.history_count = 1;
  live.history[0] = {12, 50, 3.28f, HistoryPowerSource::Grid};
  live.last_update_ms = nowMs;

  Capture plain;
  printGatewaySummary(plain, live, true, 0, false);
  CHECK(plain.text.find("[--:--:--] Gateway") != std::string::npos, "unsynced clock must not show a 1970 timestamp");
  CHECK(plain.text.find("3.28 kW |  111.6 V |   29.4 A") != std::string::npos, "load units and precision");
  CHECK(plain.text.find("19.53 A /  1.929 kW") != std::string::npos, "phase A precision");
  CHECK(plain.text.find("12.88 A /  1.322 kW") != std::string::npos, "phase B stays independent");
  CHECK(plain.text.find("\033[") == std::string::npos, "NO_COLOR/redirection stays plain");
  CHECK(plain.text.find("太陽能      -- kW") != std::string::npos, "missing data is not zero");

  live.has_solar_power = true; // Real zero must remain distinguishable from missing.
  Capture zero;
  printGatewaySummary(zero, live, true, 0, true);
  CHECK(zero.text.find("\033[32m") != std::string::npos, "ANSI green battery on Serial");
  CHECK(zero.text.find("太陽能    0.00 kW") != std::string::npos, "valid zero stays zero");
  live.solar_power_w = std::numeric_limits<float>::quiet_NaN();
  Capture invalid;
  printGatewaySummary(invalid, live, true, 0, false);
  CHECK(invalid.text.find("nan") == std::string::npos, "non-finite values masked");
  live.last_error = "HTTP 401";
  Capture failure;
  printGatewaySummary(failure, live, false, 0, true);
  CHECK(failure.text.find("Gateway 更新失敗 | HTTP 401") != std::string::npos, "failure reason printed");
  CHECK(failure.text.find("住家") == std::string::npos, "failed query does not print stale numbers as success");

  WeatherSnapshot weather;
  ConsolePreview preview;
  preview.handle('2', live);
  preview.handle('3', live);
  preview.handle('4', live);
  preview.handle('5', live);
  PowerwallTelemetry shown = live;
  WeatherSnapshot shownWeather = weather;
  bool configured = true;
  preview.apply(shown, shownWeather, configured);
  CHECK(shown.demo && shown.grid_status == "SystemIslandedActive", "islanded preview marked DEMO");
  CHECK(shown.battery_power_w == -3200 && shown.has_battery_power, "charging preview");
  CHECK(shownWeather.ok && shownWeather.isStale(), "stale preview works even before first weather poll");
  CHECK(!configured && !shown.ok, "waiting-for-WiFi preview");
  CHECK(live.grid_status == "SystemGridConnected" && live.battery_power_w == 0 && live.ok && !live.demo,
        "UI previews never mutate real telemetry");
  CHECK(live.history_count == 1 && live.history[0].load_kw == 3.28f && nowMs == 10000,
        "history and time untouched by offline preview");
  CHECK(!weather.ok && !weather.stale, "real weather snapshot unchanged");
  preview.handle('4', live);
  shown = live; configured = true;
  preview.apply(shown, shownWeather, configured);
  CHECK(shown.battery_power_w == 2800, "discharging cycle");
  preview.handle('4', live);
  shown = live;
  preview.apply(shown, shownWeather, configured);
  CHECK(shown.battery_power_w == 0 && shown.has_battery_power, "standby cycle");
  preview.handle('1', live);
  CHECK(!preview.active(), "1 resets every preview");
  // A new successful poll must immediately become visible after LIVE/reset.
  live.load_power_w = 4000;
  shown = live; shownWeather = weather; configured = true;
  preview.apply(shown, shownWeather, configured);
  CHECK(shown.load_power_w == 4000 && !shown.demo && configured && !shownWeather.stale, "LIVE uses newest snapshots");
  preview.handle('2', live);
  preview.reset(); // z uses the same reset path on both frontends.
  CHECK(!preview.active(), "z reset");
  std::puts("PASS: history write request coalescing/logs, shared Gateway summary, missing/zero/NaN, failures, 1-5 previews, LIVE reset, history/time isolation");
}

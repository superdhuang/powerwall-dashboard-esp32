#pragma once

#include "powerwall_client.hpp"
#include <cstdio>
#include <ctime>

template <typename Output>
inline void printGatewaySummary(Output& output, const PowerwallTelemetry& t, bool ok, time_t now, bool color) {
  const char* reset = color ? "\033[0m" : "";
  auto ink = [color](const char* code) { return color ? code : ""; };
  // A small ring keeps each argument alive until printf consumes the row.
  char numbers[8][32] = {};
  unsigned next_number = 0;
  auto value = [&](float number, bool available, int precision = 2) -> const char* {
    if (!available || !std::isfinite(number)) return "--";
    char* text = numbers[next_number++ % 8];
    std::snprintf(text, sizeof(numbers[0]), "%.*f", precision, number);
    return text;
  };
  char clock[16] = "--:--:--";
  struct tm local{};
  if (localtime_r(&now, &local) && local.tm_year >= 120) std::strftime(clock, sizeof(clock), "%H:%M:%S", &local);
  if (!ok) {
    output.printf("\n%s[%s] Gateway 更新失敗 | %s%s\n", ink("\033[31m"), clock,
                t.last_error.length() ? t.last_error.c_str() : "無法取得資料", reset);
    return;
  }
  output.printf("\n%s[%s] Gateway | %s | %s%s\n", ink("\033[1m"), clock,
              t.site_name.c_str(), t.gateway_ip.c_str(), reset);
  output.printf("%s  住家    %6s kW | %6s V | %6s A%s\n", ink("\033[36m"),
              value(t.load_power_w / 1000, t.has_load_power),
              value(t.load_voltage_v, t.has_load_voltage, 1),
              value(t.load_current_a, t.has_load_current, 1), reset);
  output.printf("%s  電網    %6s kW | %6s V | %6s A | %s%s\n", ink("\033[38;5;117m"),
              value(t.site_power_w / 1000, t.has_site_power),
              value(t.site_voltage_v, t.has_site_voltage, 1),
              value(t.site_current_a, t.has_site_current, 1),
              t.has_grid_status ? t.grid_status.c_str() : "--", reset);
  output.printf("%s  相位    A：%6s A / %6s kW | B：%6s A / %6s kW%s\n", ink("\033[38;5;117m"),
              value(t.site_phase_current_a[0], t.has_site_phase_current[0]),
              value(t.site_phase_power_w[0] / 1000, t.has_site_phase_power[0], 3),
              value(t.site_phase_current_a[1], t.has_site_phase_current[1]),
              value(t.site_phase_power_w[1] / 1000, t.has_site_phase_power[1], 3), reset);
  output.printf("%s  電池    %6s %%  | %6s kW | %6s V | %s / %s kWh%s\n", ink("\033[32m"),
              value(t.soe_percent, true, 1),
              value(t.battery_power_w / 1000, t.has_battery_power),
              value(t.battery_voltage_v, t.has_battery_voltage, 1),
              value(t.nominal_remaining_kwh, t.has_battery_capacity),
              value(t.nominal_full_kwh, t.has_battery_capacity), reset);
  output.printf("%s  太陽能  %6s kW%s\n", ink("\033[33m"),
              value(t.solar_power_w / 1000, t.has_solar_power), reset);
  output.printf("  設定    備援 %s %% | %s | Gateway %s\n",
              value(t.backup_reserve_percent, true, 0), t.real_mode.c_str(), t.version.c_str());
  output.printf("  累計    購電 %s | 回送 %s | 住家 %s kWh\n",
              value(t.site_energy_imported_wh / 1000, t.has_site_import_energy),
              value(t.site_energy_exported_wh / 1000, t.has_site_export_energy),
              value(t.load_energy_imported_wh / 1000, t.has_load_energy));
}

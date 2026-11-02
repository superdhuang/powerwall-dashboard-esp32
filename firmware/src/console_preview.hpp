#pragma once

#include "powerwall_client.hpp"
#include "weather_client.hpp"

inline bool isConsolePreviewKey(char key) { return key >= '1' && key <= '5'; }

inline const char* consolePreviewHelpText() {
  return "[preview] 1 = LIVE; 2 = grid/island; 3 = stale weather; "
         "4 = battery charge/discharge/standby; 5 = waiting for Wi-Fi; z = LIVE\n"
         "[preview] UI snapshots only; Gateway polling, saved settings and history stay LIVE";
}

// Owned by the UI task. Never pass the network client's own telemetry to apply().
class ConsolePreview {
public:
  void reset() {
    grid_override_ = false;
    stale_weather_ = false;
    battery_mode_ = 0;
    waiting_wifi_ = false;
  }

  bool active() const {
    return grid_override_ || stale_weather_ || battery_mode_ != 0 || waiting_wifi_;
  }

  const char* handle(char key, const PowerwallTelemetry& live) {
    switch (key) {
      case '1': reset(); return "[preview] LIVE: waiting for current Gateway data";
      case '2':
        islanded_ = grid_override_ ? !islanded_ : live.grid_status == "SystemGridConnected";
        grid_override_ = true;
        return islanded_ ? "[preview] OFF-GRID / 孤島供電" : "[preview] GRID-CONNECTED";
      case '3': stale_weather_ = true; return "[preview] Weather 過期 (UI only)";
      case '4':
        battery_mode_ = battery_mode_ % 3 + 1;
        return battery_mode_ == 1 ? "[preview] Battery CHARGING (-3.2 kW)" :
               battery_mode_ == 2 ? "[preview] Battery DISCHARGING (+2.8 kW)" :
                                    "[preview] Battery STANDBY (0.0 kW)";
      case '5':
        waiting_wifi_ = !waiting_wifi_;
        return waiting_wifi_ ? "[preview] 等待配網設定 (UI only)" : "[preview] Wi-Fi display restored";
      default: return "[preview] Unknown key";
    }
  }

  void apply(PowerwallTelemetry& telemetry, WeatherSnapshot& weather, bool& configured) const {
    if (!active()) return;
    telemetry.demo = true;
    if (grid_override_ || battery_mode_) telemetry.ok = true;
    if (grid_override_) {
      telemetry.has_grid_status = true;
      telemetry.grid_status = islanded_ ? "SystemIslandedActive" : "SystemGridConnected";
    }
    if (battery_mode_) {
      telemetry.has_battery_power = true;
      telemetry.battery_power_w = battery_mode_ == 1 ? -3200.0f : battery_mode_ == 2 ? 2800.0f : 0.0f;
    }
    if (stale_weather_) {
      if (!weather.ok) {
        weather.ok = true;
        weather.label = "多雲";
        weather.mode = "partly";
        weather.temp_c = 26.4f;
      }
      weather.stale = true;
    }
    if (waiting_wifi_) {
      configured = false;
      telemetry.ok = false;
    }
  }

private:
  bool grid_override_ = false;
  bool islanded_ = false;
  bool stale_weather_ = false;
  uint8_t battery_mode_ = 0;
  bool waiting_wifi_ = false;
};

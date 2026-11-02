#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>
#include <time.h>

struct WeatherSnapshot {
  static constexpr unsigned long kStaleThresholdMs = 1800000UL; // 30 minutes

  bool ok = false;
  bool stale = false;
  unsigned long last_update_ms = 0;
  String city = "台北";
  String mode = "night";
  String label = "載入中";
  float temp_c = 0.0f;
  int cloud_cover = 0;
  int is_day = 0;
  String sunrise = "--:--";
  String sunset = "--:--";
  String moon_label = "🌒 眉月";
  int moon_illum_pct = 39;
  float astro_moon_phase = 0.39f;
  float godot_moon_phase = 0.61f;
  bool godot_is_crescent = true;

  bool isStale(unsigned long now_ms = millis()) const {
    if (!ok) return false;
    return stale || (now_ms - last_update_ms > kStaleThresholdMs);
  }
};

class LocalWeatherClient {
public:
  static constexpr unsigned long kRefreshIntervalMs = 600000UL;  // 10 minutes
  static constexpr unsigned long kRetryIntervalMs   = 30000UL;   // 30 seconds on failure
  static constexpr unsigned long kStaleThresholdMs  = WeatherSnapshot::kStaleThresholdMs;
  static constexpr uint16_t kConnectTimeoutMs = 3000;
  static constexpr uint16_t kReadTimeoutMs    = 4000;

  WeatherSnapshot current;

  void refreshState() {
    computeMoonInfo();
    current.stale = current.isStale(millis());
  }

  bool update(bool force = false) {
    refreshState();
    unsigned long now = millis();

    if (!force && last_attempt_ms_ > 0) {
      unsigned long min_interval = (current.ok && last_fetch_ok_) ? kRefreshIntervalMs : kRetryIntervalMs;
      if (now - last_attempt_ms_ < min_interval) {
        return current.ok;
      }
    }
    last_attempt_ms_ = now;

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(kReadTimeoutMs / 1000);
    HTTPClient http;
    http.setConnectTimeout(kConnectTimeoutMs);
    http.setTimeout(kReadTimeoutMs);

    String url = "https://api.open-meteo.com/v1/forecast?latitude=25.0530&longitude=121.5259"
                 "&current=temperature_2m,is_day,precipitation,rain,showers,snowfall,weather_code,cloud_cover,wind_speed_10m"
                 "&daily=sunrise,sunset&timezone=auto&forecast_days=1";
    if (!http.begin(client, url)) {
      last_fetch_ok_ = false;
      current.stale = current.isStale(now);
      return false;
    }

    bool fetched_ok = false;
    int code = http.GET();
    if (code == 200) {
      JsonDocument doc;
      if (deserializeJson(doc, http.getString()) == DeserializationError::Ok &&
          doc["current"].is<JsonObject>()) {
        JsonObject cur = doc["current"];
        current.temp_c = cur["temperature_2m"] | current.temp_c;
        current.is_day = cur["is_day"] | 0;
        current.cloud_cover = cur["cloud_cover"] | 0;
        float rain_mm = (cur["rain"] | 0.0f) + (cur["showers"] | 0.0f);
        float snow_cm = cur["snowfall"] | 0.0f;
        int wcode = cur["weather_code"] | 1;

        if (wcode >= 95) { current.mode = "storm"; current.label = "強雨／閃電"; }
        else if (wcode == 71 || wcode == 73 || wcode == 75 || wcode == 77 || wcode == 85 || wcode == 86 || snow_cm > 0.0f) { current.mode = "snow"; current.label = "下雪"; }
        else if (wcode >= 51 || rain_mm > 0.0f) { current.mode = "rain"; current.label = "下雨"; }
        else if (!current.is_day) { current.mode = "night"; current.label = "夜晚"; }
        else if (wcode == 3 || wcode == 45 || wcode == 48 || current.cloud_cover >= 80) { current.mode = "overcast"; current.label = "陰天"; }
        else if (wcode == 1 || wcode == 2 || current.cloud_cover >= 25) { current.mode = "partly"; current.label = "多雲"; }
        else { current.mode = "clear"; current.label = "晴朗"; }

        JsonObject daily = doc["daily"];
        if (daily) {
          const char* sr = daily["sunrise"][0].as<const char*>();
          const char* ss = daily["sunset"][0].as<const char*>();
          if (sr) current.sunrise = extractTimeHM(sr, current.sunrise.c_str());
          if (ss) current.sunset  = extractTimeHM(ss, current.sunset.c_str());
        }

        current.ok = true;
        current.stale = false;
        current.last_update_ms = now;
        fetched_ok = true;
      }
    }
    http.end();

    last_fetch_ok_ = fetched_ok;
    current.stale = current.isStale(now);
    return fetched_ok;
  }

private:
  unsigned long last_attempt_ms_ = 0;
  bool last_fetch_ok_ = false;

  static String extractTimeHM(const char* iso_str, const char* fallback) {
    if (!iso_str || iso_str[0] == '\0') return String(fallback);
    const char* t_pos = strchr(iso_str, 'T');
    const char* start = t_pos ? (t_pos + 1) : iso_str;
    if (strlen(start) >= 5) {
      char buf[6];
      memcpy(buf, start, 5);
      buf[5] = '\0';
      return String(buf);
    }
    return String(fallback);
  }

  void computeMoonInfo() {
    time_t now_utc = time(nullptr);
    if (now_utc < 1000000000L) return;
    // Reference New Moon: 2000-01-06 18:14:00 UTC (947182440)
    const double ref_new_moon = 947182440.0;
    const double synodic_days = 29.530588853;
    double elapsed_days = (double(now_utc) - ref_new_moon) / 86400.0;
    double astro_phase = fmod(elapsed_days, synodic_days) / synodic_days;
    if (astro_phase < 0) astro_phase += 1.0;

    current.astro_moon_phase = static_cast<float>(astro_phase);
    current.moon_illum_pct = int(round((1.0 - cos(astro_phase * 2.0 * M_PI)) * 50.0));
    if (astro_phase < 0.03 || astro_phase >= 0.97) current.moon_label = "🌑 新月";
    else if (astro_phase < 0.22) current.moon_label = "🌒 眉月";
    else if (astro_phase < 0.28) current.moon_label = "🌓 上弦月";
    else if (astro_phase < 0.47) current.moon_label = "🌔 盈凸月";
    else if (astro_phase < 0.53) current.moon_label = "🌕 滿月";
    else if (astro_phase < 0.72) current.moon_label = "🌖 虧凸月";
    else if (astro_phase < 0.78) current.moon_label = "🌗 下弦月";
    else current.moon_label = "🌘 殘月";
  }
};

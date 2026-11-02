#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <cmath>
#include "native480_i18n.hpp"

enum class HistoryPowerSource : uint8_t {
  Grid = 0,
  Powerwall = 1,
  Unknown = 2,
};

struct HistoryPoint {
  float hour;
  float soe;
  float load_kw;
  HistoryPowerSource power_source = HistoryPowerSource::Unknown;
  uint8_t reserved[3] = {};
};
static_assert(sizeof(HistoryPoint) == 16, "HistoryPoint storage format changed");

struct PowerwallTelemetry {
  static constexpr size_t kHistoryCapacity = 288;

  bool ok = false;
  bool stale = false;
  bool demo = false;
  String gateway_ip = "";
  String site_name = native480_i18n::kDefaultSiteTitle;
  String version = "--";
  String real_mode = "autonomous";
  float backup_reserve_percent = 24.0f;

  float soe_percent = 100.0f;
  float nominal_full_kwh = 14.62f;
  float nominal_remaining_kwh = 14.62f;
  float battery_power_w = 0.0f;

  float site_power_w = 0.0f;
  float load_power_w = 0.0f;
  float load_voltage_v = 111.5f;
  float load_current_a = 0.0f;
  float solar_power_w = 0.0f;
  float battery_voltage_v = 0.0f;
  float site_voltage_v = 0.0f;
  float site_current_a = 0.0f;
  float site_phase_current_a[2] = {}; // A, B phases from /api/meters/site.
  float site_phase_power_w[2] = {};
  bool has_site_phase_current[2] = {};
  bool has_site_phase_power[2] = {};
  float site_energy_imported_wh = 0.0f;
  float site_energy_exported_wh = 0.0f;
  float load_energy_imported_wh = 0.0f;
  bool has_battery_capacity = false;
  bool has_battery_power = false;
  bool has_site_power = false;
  bool has_site_voltage = false;
  bool has_site_current = false;
  bool has_site_import_energy = false;
  bool has_site_export_energy = false;
  bool has_load_power = false;
  bool has_load_voltage = false;
  bool has_load_current = false;
  bool has_load_energy = false;
  bool has_solar_power = false;
  bool has_battery_voltage = false;
  bool has_grid_status = false;

  String grid_status = "SystemGridConnected";
  String timestamp = "--:--:--";
  unsigned long last_update_ms = 0;
  String last_error = "";

  HistoryPoint history[kHistoryCapacity];
  size_t history_count = 0;
};

class PowerwallLocalClient {
public:
  static constexpr size_t kHistoryCapacity = PowerwallTelemetry::kHistoryCapacity;
  static constexpr unsigned long kHistoryIntervalMs = 300000UL; // 5 minutes (288 points = 24 hours)
  static constexpr uint16_t kConnectTimeoutMs = 3000;
  static constexpr uint16_t kReadTimeoutMs    = 4000;

  PowerwallTelemetry latest;
  HistoryPoint history[kHistoryCapacity];
  size_t history_count = 0;
  String bearer_token = "";
  String cookie_header = "";

  void seedHistory(const HistoryPoint* pts, size_t count) {
    if (!pts) count = 0;
    if (count > kHistoryCapacity) count = kHistoryCapacity;
    if (count > 0 && pts != history) {
      memcpy(history, pts, count * sizeof(HistoryPoint));
    }
    history_count = count;
    latest.history_count = count;
    if (count > 0) memcpy(latest.history, history, count * sizeof(HistoryPoint));
    history_soe_sum_ = 0.0;
    history_load_sum_ = 0.0;
    history_samples_ = 0;
    last_history_ms_ = 0;
    history_grid_import_sum_w_ = 0.0;
    history_battery_discharge_sum_w_ = 0.0;
    history_source_samples_ = 0;
    history_dirty_ = false;
    last_history_year_ = -1;
    last_history_yday_ = -1;
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10) && timeinfo.tm_year >= 120) {
      last_history_year_ = timeinfo.tm_year;
      last_history_yday_ = timeinfo.tm_yday;
    }
  }

  size_t completedHistoryCount() const {
    return history_count - (history_samples_ > 0 ? 1 : 0);
  }

  bool historyNeedsSave() const { return history_dirty_; }
  void markHistorySaved() { history_dirty_ = false; }
  bool historyIsFor(const struct tm& day) const {
    return last_history_year_ == day.tm_year && last_history_yday_ == day.tm_yday;
  }

  // Label history before polling: the next successful poll may reset it at midnight.
  bool historyDate(char (&date)[11]) const {
    if (last_history_year_ < 120 || last_history_yday_ < 0) return false;
    tm day{};
    day.tm_year = last_history_year_;
    day.tm_mon = 0;
    day.tm_mday = last_history_yday_ + 1;
    day.tm_hour = 12;
    day.tm_isdst = -1;
    mktime(&day);
    return strftime(date, sizeof(date), "%Y-%m-%d", &day) == 10;
  }

  void configure(const char* ip, const char* password, const char* email, const char* site_title = nullptr) {
    const char* safe_ip   = (ip && ip[0] != '\0') ? ip : "";
    const char* safe_pass = password ? password : "";
    bool changed = configured_once_ && ((host_ != safe_ip) || (password_ != safe_pass));
    host_ = safe_ip;
    password_ = safe_pass;
    email_ = (email && email[0] != '\0') ? email : "customer@example.com";
    if (site_title && site_title[0] != '\0') {
      custom_site_title_ = site_title;
      latest.site_name = custom_site_title_;
    }
    latest.gateway_ip = host_;

    if (changed) {
      bearer_token = "";
      cookie_header = "";
      last_login_ms_ = 0;
      Preferences prefs;
      prefs.begin("pw_nvs", false);
      prefs.putString("auth_token", "");
      prefs.end();
    } else if (!configured_once_ && bearer_token.length() == 0) {
      // Load cached token from NVS on first boot configuration
      Preferences prefs;
      prefs.begin("pw_nvs", true);
      String saved_tok = prefs.getString("auth_token", "");
      prefs.end();
      if (saved_tok.length() > 0) {
        bearer_token = saved_tok;
        cookie_header = "AuthCookie=" + bearer_token;
        last_login_ms_ = millis();
      }
    }
    configured_once_ = true;
  }

  // Uses customer password to acquire Bearer Token & AuthCookie from https://<gateway_ip>/api/login/Basic
  bool loginAndAcquireToken() {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(kReadTimeoutMs / 1000);
    HTTPClient https;
    https.setConnectTimeout(kConnectTimeoutMs);
    https.setTimeout(kReadTimeoutMs);

    String url = "https://" + host_ + "/api/login/Basic";
    if (!https.begin(client, url)) {
      latest.last_error = "HTTPS begin failed";
      return false;
    }

    https.addHeader("Content-Type", "application/json");
    const char* headerKeys[] = {"Set-Cookie"};
    https.collectHeaders(headerKeys, 1);

    JsonDocument reqDoc;
    reqDoc["username"] = "customer";
    reqDoc["password"] = password_;
    reqDoc["email"] = email_;
    reqDoc["force_sm_off"] = false;

    String body;
    serializeJson(reqDoc, body);

    int code = https.POST(body);
    if (code != 200) {
      latest.last_error = "Login HTTP " + String(code);
      https.end();
      return false;
    }

    String resp = https.getString();
    String setCookie = https.header("Set-Cookie");
    https.end();

    JsonDocument resDoc;
    if (deserializeJson(resDoc, resp) == DeserializationError::Ok) {
      if (resDoc["token"].is<const char*>()) {
        bearer_token = resDoc["token"].as<String>();
      }
    }

    if (setCookie.length() > 0) {
      int semi = setCookie.indexOf(';');
      cookie_header = (semi > 0) ? setCookie.substring(0, semi) : setCookie;
      if (bearer_token.length() == 0 && cookie_header.startsWith("AuthCookie=")) {
        bearer_token = cookie_header.substring(strlen("AuthCookie="));
      }
    } else if (bearer_token.length() > 0) {
      cookie_header = "AuthCookie=" + bearer_token;
    }

    if (bearer_token.length() == 0 && cookie_header.length() == 0) {
      latest.last_error = "No token returned by Gateway";
      return false;
    }

    // Persist acquired token to NVS only when changed
    Preferences prefs;
    prefs.begin("pw_nvs", false);
    String existing = prefs.getString("auth_token", "");
    if (existing != bearer_token) {
      prefs.putString("auth_token", bearer_token);
    }
    prefs.end();

    last_login_ms_ = millis();
    return true;
  }

  bool pollLiveData(bool forceLogin = false) {
    if (host_.length() == 0 || password_.length() == 0) {
      latest.last_error = "Gateway not configured";
      latest.stale = true;
      return false;
    }
    if (forceLogin || (bearer_token.length() == 0 && cookie_header.length() == 0)) {
      if (!loginAndAcquireToken()) {
        latest.ok = (latest.last_update_ms > 0);
        latest.stale = true;
        return false;
      }
    }

    JsonDocument soeDoc, aggDoc, sysDoc, gridDoc, phaseDoc;
    if (!fetchEndpoint("/api/system_status/soe", soeDoc) ||
        !fetchEndpoint("/api/meters/aggregates", aggDoc)) {
      latest.ok = (latest.last_update_ms > 0);
      latest.stale = true;
      return false;
    }
    bool hasSys  = fetchEndpoint("/api/system_status", sysDoc);
    bool hasGrid = fetchEndpoint("/api/system_status/grid_status", gridDoc);
    bool hasPhases = fetchEndpoint("/api/meters/site", phaseDoc);

    if (millis() - last_static_ms_ > 60000UL || last_static_ms_ == 0) {
      JsonDocument siteDoc, opDoc, statusDoc;
      if (fetchEndpoint("/api/site_info", siteDoc)) {
        const char* api_site = siteDoc["site_name"] | "";
        if (custom_site_title_.length() > 0 && custom_site_title_ != native480_i18n::kDefaultSiteTitle) {
          latest.site_name = custom_site_title_;
        } else if (strlen(api_site) > 0) {
          latest.site_name = api_site;
        } else if (custom_site_title_.length() > 0) {
          latest.site_name = custom_site_title_;
        } else {
          latest.site_name = native480_i18n::kDefaultSiteTitle;
        }
      }
      if (fetchEndpoint("/api/operation", opDoc)) {
        latest.real_mode = opDoc["real_mode"] | "autonomous";
        latest.backup_reserve_percent = opDoc["backup_reserve_percent"] | 24.0f;
      }
      if (fetchEndpoint("/api/status", statusDoc)) {
        latest.version = statusDoc["version"] | "--";
      }
      last_static_ms_ = millis();
    }

    latest.soe_percent = soeDoc["percentage"] | latest.soe_percent;
    latest.has_battery_capacity = false;
    if (hasSys) {
      float full_wh = sysDoc["nominal_full_pack_energy"] | (latest.nominal_full_kwh * 1000.0f);
      float rem_wh  = sysDoc["nominal_energy_remaining"] | (latest.nominal_remaining_kwh * 1000.0f);
      latest.nominal_full_kwh = full_wh / 1000.0f;
      latest.nominal_remaining_kwh = rem_wh / 1000.0f;
      latest.has_battery_capacity =
        !sysDoc["nominal_full_pack_energy"].isNull() &&
        !sysDoc["nominal_energy_remaining"].isNull() &&
        std::isfinite(latest.nominal_full_kwh) && std::isfinite(latest.nominal_remaining_kwh);
    }

    const JsonObjectConst siteMeter = aggDoc["site"].as<JsonObjectConst>();
    const JsonObjectConst loadMeter = aggDoc["load"].as<JsonObjectConst>();
    const JsonObjectConst solarMeter = aggDoc["solar"].as<JsonObjectConst>();
    const JsonObjectConst batteryMeter = aggDoc["battery"].as<JsonObjectConst>();
    readMeterValue(siteMeter["instant_power"], latest.site_power_w, latest.has_site_power);
    readMeterValue(siteMeter["instant_average_voltage"], latest.site_voltage_v, latest.has_site_voltage);
    readMeterValue(siteMeter["instant_average_current"], latest.site_current_a, latest.has_site_current);
    JsonObjectConst phaseMeter;
    if (hasPhases) phaseMeter = phaseDoc[0]["Cached_readings"].as<JsonObjectConst>();
    const char* currentKeys[] = {"i_a_current", "i_b_current"};
    const char* powerKeys[] = {"real_power_a", "real_power_b"};
    for (int phase = 0; phase < 2; ++phase) {
      readMeterValue(phaseMeter[currentKeys[phase]], latest.site_phase_current_a[phase],
                     latest.has_site_phase_current[phase]);
      readMeterValue(phaseMeter[powerKeys[phase]], latest.site_phase_power_w[phase],
                     latest.has_site_phase_power[phase]);
    }
    readMeterValue(siteMeter["energy_imported"], latest.site_energy_imported_wh, latest.has_site_import_energy);
    readMeterValue(siteMeter["energy_exported"], latest.site_energy_exported_wh, latest.has_site_export_energy);
    readMeterValue(loadMeter["instant_power"], latest.load_power_w, latest.has_load_power);
    readMeterValue(loadMeter["instant_average_voltage"], latest.load_voltage_v, latest.has_load_voltage);
    readMeterValue(loadMeter["instant_average_current"], latest.load_current_a, latest.has_load_current);
    readMeterValue(loadMeter["energy_imported"], latest.load_energy_imported_wh, latest.has_load_energy);
    readMeterValue(solarMeter["instant_power"], latest.solar_power_w, latest.has_solar_power);
    readMeterValue(batteryMeter["instant_average_voltage"], latest.battery_voltage_v, latest.has_battery_voltage);
    readMeterValue(batteryMeter["instant_power"], latest.battery_power_w, latest.has_battery_power);
    latest.has_grid_status = false;
    if (hasGrid) {
      if (!gridDoc["grid_status"].isNull()) {
        latest.grid_status = gridDoc["grid_status"].as<String>();
        latest.has_grid_status = latest.grid_status.length() > 0;
      }
    }
    latest.ok = true;
    latest.stale = false;
    latest.last_update_ms = millis();
    latest.last_error = "";

    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10) && timeinfo.tm_year >= 120) {
      char tbuf[16];
      strftime(tbuf, sizeof(tbuf), "%H:%M:%S", &timeinfo);
      latest.timestamp = tbuf;
    } else {
      char tbuf[24];
      snprintf(tbuf, sizeof(tbuf), "+%lus", latest.last_update_ms / 1000UL);
      latest.timestamp = tbuf;
    }

    if (latest.has_load_power) {
      recordHistoryPoint(latest.soe_percent, latest.load_power_w / 1000.0f,
                         latest.site_power_w, latest.battery_power_w,
                         latest.has_site_power && latest.has_battery_power);
    }
    return true;
  }

private:
  bool configured_once_ = false;
  String host_ = "";
  String password_ = "";
  String email_ = "customer@example.com";
  String custom_site_title_ = native480_i18n::kDefaultSiteTitle;
  unsigned long last_login_ms_ = 0;
  unsigned long last_static_ms_ = 0;
  unsigned long last_history_ms_ = 0;
  int last_history_year_ = -1;
  int last_history_yday_ = -1;
  double history_soe_sum_ = 0.0;
  double history_load_sum_ = 0.0;
  uint32_t history_samples_ = 0;
  uint32_t history_source_samples_ = 0;
  bool history_dirty_ = false;
  double history_grid_import_sum_w_ = 0.0;
  double history_battery_discharge_sum_w_ = 0.0;

  static void readMeterValue(JsonVariantConst source, float& destination, bool& valid) {
    valid = false;
    if (source.isNull()) return;
    const float value = source.as<float>();
    if (!std::isfinite(value)) return;
    destination = value;
    valid = true;
  }

  static HistoryPowerSource classifyPowerSource(float sitePowerW, float batteryPowerW) {
    if (!std::isfinite(sitePowerW) || !std::isfinite(batteryPowerW)) {
      return HistoryPowerSource::Unknown;
    }
    // Positive site power is grid import; positive battery power is discharge.
    // Solar output is deliberately excluded from this source comparison.
    const float gridImportW = fmaxf(0.0f, sitePowerW);
    const float batteryDischargeW = fmaxf(0.0f, batteryPowerW);
    if (gridImportW <= 60.0f && batteryDischargeW <= 60.0f) {
      return HistoryPowerSource::Unknown;
    }
    if (fabsf(gridImportW - batteryDischargeW) <= 60.0f) {
      return HistoryPowerSource::Unknown;
    }
    return gridImportW > batteryDischargeW
      ? HistoryPowerSource::Grid : HistoryPowerSource::Powerwall;
  }

  bool fetchEndpoint(const char* path, JsonDocument& outDoc) {
    for (int attempt = 0; attempt < 2; ++attempt) {
      WiFiClientSecure client;
      client.setInsecure();
      client.setTimeout(kReadTimeoutMs / 1000);
      HTTPClient https;
      https.setConnectTimeout(kConnectTimeoutMs);
      https.setTimeout(kReadTimeoutMs);

      String url = "https://" + host_ + String(path);
      if (!https.begin(client, url)) return false;

      if (bearer_token.length() > 0) {
        https.addHeader("Authorization", "Bearer " + bearer_token);
      }
      if (cookie_header.length() > 0) {
        https.addHeader("Cookie", cookie_header);
      }

      int code = https.GET();
      if (code == 200) {
        String payload = https.getString();
        https.end();
        return deserializeJson(outDoc, payload) == DeserializationError::Ok;
      }
      https.end();

      if ((code == 401 || code == 403) && attempt == 0) {
        if (!loginAndAcquireToken()) return false;
      } else {
        latest.last_error = String(path) + " HTTP " + String(code);
        return false;
      }
    }
    return false;
  }

  void recordHistoryPoint(float soe, float load_kw, float site_power_w, float battery_power_w,
                          bool sourceFieldsValid) {
    struct tm timeinfo;
    float hour_float = 0.0f;
    if (getLocalTime(&timeinfo, 10) && timeinfo.tm_year >= 120) {
      if (last_history_yday_ < 0 ||
          timeinfo.tm_year != last_history_year_ ||
          timeinfo.tm_yday != last_history_yday_) {
        // First valid clock sync or a new day starts a fresh daily chart.
        history_count = 0;
        last_history_ms_ = 0;
        history_samples_ = 0;
        history_soe_sum_ = 0.0;
        history_load_sum_ = 0.0;
        history_grid_import_sum_w_ = 0.0;
        history_battery_discharge_sum_w_ = 0.0;
        history_source_samples_ = 0;
        history_dirty_ = true;
      }
      last_history_year_ = timeinfo.tm_year;
      last_history_yday_ = timeinfo.tm_yday;
      hour_float = timeinfo.tm_hour + timeinfo.tm_min / 60.0f + timeinfo.tm_sec / 3600.0f;
    } else if (last_history_yday_ >= 0) {
      // Keep valid clock positions intact during a temporary NTP failure.
      return;
    } else {
      hour_float = fmod((millis() / 3600000.0f), 24.0f);
    }

    if (!std::isfinite(soe) || !std::isfinite(load_kw)) return;

    unsigned long now_ms = millis();
    if (history_samples_ > 0 && now_ms - last_history_ms_ >= kHistoryIntervalMs) {
      // The last point now represents all successful polls in the completed window.
      history_samples_ = 0;
      history_soe_sum_ = 0.0;
      history_load_sum_ = 0.0;
      history_grid_import_sum_w_ = 0.0;
      history_battery_discharge_sum_w_ = 0.0;
      history_source_samples_ = 0;
      history_dirty_ = true;
    }

    if (history_samples_ == 0) {
      if (history_count < kHistoryCapacity) {
        history[history_count++] = {hour_float, 0.0f, 0.0f, HistoryPowerSource::Unknown};
      } else {
        memmove(&history[0], &history[1], (kHistoryCapacity - 1) * sizeof(HistoryPoint));
        history[kHistoryCapacity - 1] = {hour_float, 0.0f, 0.0f, HistoryPowerSource::Unknown};
      }
      last_history_ms_ = now_ms;
    }

    history_soe_sum_ += soe;
    history_load_sum_ += load_kw;
    if (sourceFieldsValid && std::isfinite(site_power_w) && std::isfinite(battery_power_w)) {
      history_grid_import_sum_w_ += fmaxf(0.0f, site_power_w);
      history_battery_discharge_sum_w_ += fmaxf(0.0f, battery_power_w);
      ++history_source_samples_;
    }
    ++history_samples_;
    history[history_count - 1] = {
      hour_float,
      static_cast<float>(history_soe_sum_ / history_samples_),
      static_cast<float>(history_load_sum_ / history_samples_),
      sourceFieldsValid && history_source_samples_ > 0
        ? classifyPowerSource(
            static_cast<float>(history_grid_import_sum_w_ / history_source_samples_),
            static_cast<float>(history_battery_discharge_sum_w_ / history_source_samples_))
        : HistoryPowerSource::Unknown
    };

    latest.history_count = history_count;
    if (history_count > 0) {
      memcpy(latest.history, history, history_count * sizeof(HistoryPoint));
    }
  }
};

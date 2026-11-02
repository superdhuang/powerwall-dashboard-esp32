#pragma once
#include "weekly_history.hpp"

static void importWeeklyJson(WeeklyHistory& history, JsonVariantConst doc) {
  const char* date = doc["date"] | "";
  std::vector<HistoryPoint> points;
  for (JsonObjectConst p : doc["points"].as<JsonArrayConst>()) {
    if (!p["hour"].is<float>() || !p["soe"].is<float>() || !p["load_kw"].is<float>()) continue;
    points.push_back({p["hour"].as<float>(), p["soe"].as<float>(), p["load_kw"].as<float>(), parseHistoryPowerSource(p["power_source"])});
  }
  history.merge(date, points.data(), points.size());
}
static void readWeeklyFile(WeeklyHistory& history, const char* path, bool archive) {
  std::ifstream file(path);
  if (!file) return;
  std::string contents((std::istreambuf_iterator<char>(file)), {});
  JsonDocument doc;
  if (deserializeJson(doc, contents) != DeserializationError::Ok) {
    std::fprintf(stderr, "[WeeklyHistory] Cannot parse %s\n", path); return;
  }
  if (archive) for (JsonVariantConst day : doc["days"].as<JsonArrayConst>()) importWeeklyJson(history, day);
  else importWeeklyJson(history, doc.as<JsonVariantConst>());
}
static bool saveWeeklyHistory(const WeeklyHistory& history) {
  JsonDocument doc;
  doc["version"] = 1;
  JsonArray days = doc["days"].to<JsonArray>();
  for (const auto& day : history.days) {
    JsonObject out = days.add<JsonObject>(); out["date"] = day.date;
    JsonArray points = out["points"].to<JsonArray>();
    for (const auto& p : day.points) {
      JsonObject point = points.add<JsonObject>();
      point["hour"] = p.hour; point["soe"] = p.soe; point["load_kw"] = p.load_kw;
      point["power_source"] = historyPowerSourceText(p.power_source);
    }
  }
  std::string json; serializeJson(doc, json);
  const std::string temp = "history_week.json." + std::to_string(getpid()) + ".tmp";
  FILE* file = std::fopen(temp.c_str(), "wb");
  if (!file) return false;
  bool ok = std::fwrite(json.data(), 1, json.size(), file) == json.size();
  ok = std::fclose(file) == 0 && ok;
  if (ok && std::rename(temp.c_str(), "history_week.json") == 0) return true;
  std::remove(temp.c_str()); return false;
}
static WeeklyHistory demoWeeklyHistory(time_t now) {
  WeeklyHistory history; history.roll(now);
  tm today{}; localtime_r(&now, &today);
  for (int ago = 0; ago < 7; ++ago) {
    tm day = WeeklyHistory::dayAt(now, ago);
    auto& slot = history.days[day.tm_wday];
    float end = ago == 0 ? today.tm_hour + today.tm_min / 60.f : 24.f;
    for (int b = 0; b < 288 && b / 12.f <= end; ++b) {
      float h = b / 12.f;
      float soe = h < 6 ? 24 + h * 12.6f : h < 11 ? 100 - (h-6)*15.2f : h < 14 ? 24 + (h-11)*25.3f : 100 - (h-14)*7.6f;
      float load = 1.6f + .5f * sinf(h*1.8f + ago) + (h > 18 && h < 22 ? 1.6f : 0.f);
      slot.points.push_back({h, soe, load, WeeklyHistory::peak(day,h) ? HistoryPowerSource::Powerwall : HistoryPowerSource::Grid});
    }
  }
  return history;
}

#pragma once

#include <SPIFFS.h>
#include <cstddef>
#include <cstdio>
#include "weekly_history.hpp"

// Independent of the existing daily snapshots. SPIFFS is mounted once by DailyHistoryStore.
// Only completed observations are passed to save(); the UI may also show the live bucket.
class WeeklyHistoryStore {
public:
  bool load(WeeklyHistory& history, time_t now) {
    if (!WeeklyHistory::validTime(now)) return false;
    sequence_ = 0;
    bool found = false;
    for (const char* path : paths_) {
      WeeklyHistory candidate;
      Header header{};
      if (!read(path, header, &candidate)) continue;
      if (found && header.sequence <= sequence_) continue;
      history = candidate;
      sequence_ = header.sequence;
      found = true;
    }
    history.roll(now);
    // A backwards clock correction must not restore observations from later today.
    tm today{}; localtime_r(&now, &today);
    const float hour = today.tm_hour + today.tm_min / 60.f + today.tm_sec / 3600.f;
    auto& points = history.days[today.tm_wday].points;
    const auto end = std::remove_if(points.begin(), points.end(), [hour](const HistoryPoint& p) { return p.hour > hour; });
    if (end != points.end()) { points.erase(end, points.end()); ++history.revision; }
    return found;
  }

  bool save(const WeeklyHistory& history) {
    Header header{kMagic, kVersion, 7, sequence_ + 1, 0};
    uint32_t hash = hashBytes(2166136261UL, &header, offsetof(Header, checksum));
    for (size_t d = 0; d < 7; ++d) {
      const auto& day = history.days[d];
      if (!validDate(day.date, d) || day.points.size() > 288) return false;
      const DayHeader record = dayHeader(day);
      hash = hashBytes(hash, &record, sizeof(record));
      float lastHour = -1;
      for (const auto& point : day.points) {
        if (!WeeklyHistory::validPoint(point) || point.hour <= lastHour) return false;
        lastHour = point.hour;
        hash = hashBytes(hash, &point, sizeof(point));
      }
    }
    header.checksum = hash;
    const char* path = paths_[header.sequence & 1U];
    fs::File file = SPIFFS.open(path, FILE_WRITE);
    if (!file) return false;
    bool ok = file.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header)) == sizeof(header);
    for (const auto& day : history.days) {
      const DayHeader record = dayHeader(day);
      const size_t bytes = day.points.size() * sizeof(HistoryPoint);
      ok = ok && file.write(reinterpret_cast<const uint8_t*>(&record), sizeof(record)) == sizeof(record);
      ok = ok && (!bytes || file.write(reinterpret_cast<const uint8_t*>(day.points.data()), bytes) == bytes);
    }
    file.flush();
    file.close();
    Header verified{};
    if (!ok || !read(path, verified, nullptr) || verified.sequence != header.sequence || verified.checksum != header.checksum) return false;
    sequence_ = header.sequence;
    return true;
  }

private:
  struct Header { uint32_t magic; uint16_t version, days; uint32_t sequence, checksum; };
  struct DayHeader { char date[12]; uint16_t count, reserved; };
  static_assert(sizeof(Header) == 16 && sizeof(DayHeader) == 16, "Weekly snapshot layout changed");
  static constexpr uint32_t kMagic = 0x50575731UL; // PWW1
  static constexpr uint16_t kVersion = 1;
  const char* const paths_[2] = {"/pw_week_a.bin", "/pw_week_b.bin"};
  uint32_t sequence_ = 0;

  static uint32_t hashBytes(uint32_t hash, const void* source, size_t count) {
    const auto* bytes = static_cast<const uint8_t*>(source);
    for (size_t i = 0; i < count; ++i) hash = (hash ^ bytes[i]) * 16777619UL;
    return hash;
  }

  static DayHeader dayHeader(const WeeklyHistoryDay& day) {
    DayHeader record{};
    memcpy(record.date, day.date, sizeof(day.date));
    record.count = static_cast<uint16_t>(day.points.size());
    return record;
  }

  static bool validDate(const char* date, size_t weekday) {
    if (date[10] != '\0') return false;
    int year, month, day;
    if (sscanf(date, "%4d-%2d-%2d", &year, &month, &day) != 3 || year < 2020 || month < 1 || month > 12 || day < 1 || day > 31) return false;
    tm parsed{};
    parsed.tm_year = year - 1900; parsed.tm_mon = month - 1; parsed.tm_mday = day;
    parsed.tm_hour = 12; parsed.tm_isdst = -1;
    mktime(&parsed);
    char normalized[11];
    strftime(normalized, sizeof(normalized), "%Y-%m-%d", &parsed);
    return !strcmp(normalized, date) && parsed.tm_wday == static_cast<int>(weekday);
  }

  static bool read(const char* path, Header& header, WeeklyHistory* output) {
    fs::File file = SPIFFS.open(path, FILE_READ);
    if (!file || file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header) ||
        header.magic != kMagic || header.version != kVersion || header.days != 7) return false;
    uint32_t hash = hashBytes(2166136261UL, &header, offsetof(Header, checksum));
    size_t expectedSize = sizeof(header);
    for (size_t d = 0; d < 7; ++d) {
      DayHeader record{};
      if (file.read(reinterpret_cast<uint8_t*>(&record), sizeof(record)) != sizeof(record) ||
          record.count > 288 || !validDate(record.date, d)) return false;
      hash = hashBytes(hash, &record, sizeof(record));
      expectedSize += sizeof(record) + record.count * sizeof(HistoryPoint);
      if (output) {
        memcpy(output->days[d].date, record.date, sizeof(output->days[d].date));
        output->days[d].points.reserve(record.count);
      }
      float lastHour = -1;
      for (size_t i = 0; i < record.count; ++i) {
        HistoryPoint point{};
        if (file.read(reinterpret_cast<uint8_t*>(&point), sizeof(point)) != sizeof(point) ||
            !WeeklyHistory::validPoint(point) || point.hour <= lastHour) return false;
        lastHour = point.hour;
        hash = hashBytes(hash, &point, sizeof(point));
        if (output) output->days[d].points.push_back(point);
      }
    }
    return expectedSize == file.size() && hash == header.checksum;
  }
};

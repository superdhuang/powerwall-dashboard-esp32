#pragma once

#include <SPIFFS.h>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "daily_trend_history.hpp"

// Independent A/B sidecar snapshots for daily trend data. SPIFFS must already
// be mounted by the owning firmware; this class never mounts or formats it.
class DailyTrendStore {
public:
  // Returns true when a valid snapshot for `today` was loaded. When no valid
  // matching snapshot exists, `history` is initialized as an empty snapshot
  // for that date and false is returned.
  bool loadToday(DailyTrendHistory& history, const struct tm& today) {
    char requested_date[11];
    int64_t current_epoch = 0;
    int seconds_of_day = 0;
    if (!DailyTrendHistory::makeDateAndEpoch(today, requested_date, current_epoch, seconds_of_day))
      return false;

    history.reset(requested_date);
    sequence_ = 0;
    const float current_hour = static_cast<float>(today.tm_hour) + today.tm_min / 60.0f + today.tm_sec / 3600.0f;
    SnapshotHeader headers[2]{};
    bool header_valid[2] = {};
    for (size_t i = 0; i < 2; ++i) header_valid[i] = readHeader(paths_[i], requested_date, headers[i]);
    size_t order[2] = {0, 1};
    if (header_valid[0] && header_valid[1] && headers[1].sequence > headers[0].sequence) {
      order[0] = 1;
      order[1] = 0;
    }

    for (size_t attempt = 0; attempt < 2; ++attempt) {
      const size_t index = order[attempt];
      if (!header_valid[index]) continue;
      SnapshotHeader candidate{};
      if (!readSnapshot(paths_[index], requested_date, candidate, &history, current_hour) ||
          candidate.sequence != headers[index].sequence || candidate.checksum != headers[index].checksum) {
        history.count = 0;
        std::memset(history.points, 0, sizeof(history.points));
        continue;
      }

      const bool clock_regressed = candidate.last_observed_epoch > current_epoch;
      const uint16_t current_bucket = static_cast<uint16_t>(today.tm_hour * 12 + today.tm_min / 5);
      const uint16_t count_before_regression_trim = history.count;
      if (clock_regressed) history.removeFromBucket(current_bucket);
      const bool future_points_excluded = history.count < count_before_regression_trim ||
        candidate.count > count_before_regression_trim;
      history.solar_kwh_today = candidate.solar_kwh_today;
      // Restarts break observed coverage. Retain the total but never bridge
      // the reboot interval with a persisted endpoint.
      history.solar_kwh_complete = false;
      history.solar_endpoint_epoch_ = 0;
      history.solar_endpoint_kw_ = 0.0f;
      history.solar_endpoint_valid_ = false;
      history.solar_day_started_ = true;
      history.solar_gap_seen_ = true;
      history.last_observed_epoch_ = candidate.last_observed_epoch;

      // Any future observation may have contributed to the scalar total, even
      // when its final solar sample was invalid and no endpoint was persisted.
      if (clock_regressed || future_points_excluded) {
        history.solar_kwh_today = 0.0f;
        history.solar_kwh_complete = false;
        history.last_observed_epoch_ = current_epoch;
      }

      sequence_ = candidate.sequence;
      return true;
    }
    history.reset(requested_date);
    return false;
  }

  // Writes the slot opposite the newest valid snapshot for this date, then
  // validates its full contents and checksum before advancing the sequence.
  bool saveToday(const DailyTrendHistory& history) {
    if (!validHistory(history)) return false;

    SnapshotHeader newest{};
    bool found = false;
    for (const char* path : paths_) {
      SnapshotHeader candidate{};
      if (!readSnapshot(path, history.date, candidate)) continue;
      if (!found || candidate.sequence > newest.sequence) {
        newest = candidate;
        found = true;
      }
    }
    if (found && newest.sequence == UINT32_MAX) return false;

    SnapshotHeader header{};
    header.magic = kMagic;
    header.version = kVersion;
    header.header_size = sizeof(SnapshotHeader);
    std::memcpy(header.date, history.date, sizeof(header.date));
    header.count = history.count;
    header.sequence = found ? newest.sequence + 1U : 1U;
    header.solar_kwh_today = history.solar_kwh_today;
    header.solar_kwh_complete = history.solar_kwh_complete ? 1U : 0U;
    header.solar_endpoint_valid = history.solar_endpoint_valid_ ? 1U : 0U;
    header.solar_endpoint_epoch = history.solar_endpoint_epoch_;
    header.solar_endpoint_kw = history.solar_endpoint_kw_;
    header.last_observed_epoch = history.last_observed_epoch_;
    header.checksum = checksum(header, history.points);

    const char* target = paths_[(header.sequence & 1U) ? 0 : 1];
    fs::File file = SPIFFS.open(target, FILE_WRITE);
    if (!file) return false;
    const size_t point_bytes = static_cast<size_t>(header.count) * sizeof(DailyTrendPoint);
    const bool written =
      file.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header)) == sizeof(header) &&
      (point_bytes == 0 || file.write(reinterpret_cast<const uint8_t*>(history.points), point_bytes) == point_bytes);
    file.flush();
    file.close();
    if (!written) return false;

    SnapshotHeader verified{};
    if (!readSnapshot(target, history.date, verified) || verified.sequence != header.sequence ||
        verified.checksum != header.checksum) return false;
    sequence_ = header.sequence;
    return true;
  }

private:
#pragma pack(push, 1)
  struct SnapshotHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    char date[11];
    uint16_t count;
    uint32_t sequence;
    float solar_kwh_today;
    uint8_t solar_kwh_complete;
    uint8_t solar_endpoint_valid;
    uint16_t reserved;
    int64_t solar_endpoint_epoch;
    float solar_endpoint_kw;
    int64_t last_observed_epoch;
    uint32_t checksum;
  };
#pragma pack(pop)

  static_assert(sizeof(DailyTrendPoint) == 32, "Daily trend point layout changed");
  static_assert(sizeof(SnapshotHeader) == 57, "Daily trend snapshot layout changed");
  static constexpr uint32_t kMagic = 0x50575431UL; // PWT1
  static constexpr uint16_t kVersion = 2;
  const char* const paths_[2] = {"/pw_trend_a.bin", "/pw_trend_b.bin"};
  uint32_t sequence_ = 0;

  static uint32_t hashBytes(uint32_t hash, const void* source, size_t bytes) {
    const auto* data = static_cast<const uint8_t*>(source);
    for (size_t i = 0; i < bytes; ++i) hash = (hash ^ data[i]) * 16777619UL;
    return hash;
  }

  static uint32_t checksum(const SnapshotHeader& header, const DailyTrendPoint* points) {
    uint32_t hash = hashBytes(2166136261UL, &header, offsetof(SnapshotHeader, checksum));
    return hashBytes(hash, points, static_cast<size_t>(header.count) * sizeof(DailyTrendPoint));
  }

  static bool validDate(const char* date) {
    if (!date || std::strlen(date) != 10 || date[4] != '-' || date[7] != '-') return false;
    for (int i = 0; i < 10; ++i) {
      if (i == 4 || i == 7) continue;
      if (date[i] < '0' || date[i] > '9') return false;
    }
    int year = 0, month = 0, day = 0;
    if (std::sscanf(date, "%4d-%2d-%2d", &year, &month, &day) != 3 || year < 2020 ||
        month < 1 || month > 12 || day < 1 || day > 31) return false;
    struct tm parsed{};
    parsed.tm_year = year - 1900;
    parsed.tm_mon = month - 1;
    parsed.tm_mday = day;
    parsed.tm_hour = 12;
    parsed.tm_isdst = -1;
    if (std::mktime(&parsed) < 0) return false;
    char normalized[11];
    const int written = std::snprintf(normalized, sizeof(normalized), "%04d-%02d-%02d",
                                      parsed.tm_year + 1900, parsed.tm_mon + 1, parsed.tm_mday);
    return written == 10 && std::strcmp(normalized, date) == 0;
  }

  static bool validPoint(const DailyTrendPoint& point) {
    if (!std::isfinite(point.hour) || point.hour < 0.0f || point.hour >= 24.0f ||
        (point.valid_mask & ~kDailyTrendKnownValidityMask) != 0) return false;
    const float bucket = point.hour * 12.0f;
    if (std::fabs(bucket - std::round(bucket)) > 0.001f) return false;
    if (point.reserved[0] || point.reserved[1] || point.reserved[2]) return false;
    if (!!(point.valid_mask & kDailyTrendSolarValid) != (point.solar_samples != 0) ||
        !!(point.valid_mask & kDailyTrendHomeValid) != (point.home_samples != 0) ||
        !!(point.valid_mask & kDailyTrendGridValid) != (point.grid_samples != 0) ||
        !!(point.valid_mask & kDailyTrendSoeValid) != (point.soe_samples != 0)) return false;
    if ((point.valid_mask & kDailyTrendSolarValid) &&
        (!std::isfinite(point.solar_kw) || point.solar_kw < 0.0f)) return false;
    if ((point.valid_mask & kDailyTrendHomeValid) &&
        (!std::isfinite(point.home_kw) || point.home_kw < 0.0f)) return false;
    if ((point.valid_mask & kDailyTrendGridValid) && !std::isfinite(point.grid_kw)) return false;
    if ((point.valid_mask & kDailyTrendSoeValid) &&
        (!std::isfinite(point.soe) || point.soe < 0.0f || point.soe > 100.0f)) return false;
    return true;
  }

  static bool validHistory(const DailyTrendHistory& history) {
    if (history.date[10] != '\0' || !validDate(history.date) || history.count > DailyTrendHistory::kCapacity ||
        !std::isfinite(history.solar_kwh_today) || history.solar_kwh_today < 0.0f ||
        history.last_observed_epoch_ < 0 || (history.count && history.last_observed_epoch_ == 0) ||
        (!history.solar_endpoint_valid_ && history.solar_endpoint_epoch_ != 0) ||
        (history.solar_endpoint_valid_ && (!std::isfinite(history.solar_endpoint_kw_) ||
          history.solar_endpoint_kw_ < 0.0f || history.solar_endpoint_epoch_ <= 0 ||
          history.solar_endpoint_epoch_ > history.last_observed_epoch_))) return false;
    int last_bucket = -1;
    for (size_t i = 0; i < history.count; ++i) {
      const DailyTrendPoint& point = history.points[i];
      if (!validPoint(point)) return false;
      const int bucket = static_cast<int>(std::round(point.hour * 12.0f));
      if (bucket <= last_bucket) return false;
      last_bucket = bucket;
    }
    return true;
  }

  bool readHeader(const char* path, const char* expected_date, SnapshotHeader& header) {
    header = {};
    fs::File file = SPIFFS.open(path, FILE_READ);
    if (!file || file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header)) {
      file.close();
      return false;
    }
    const size_t point_bytes = static_cast<size_t>(header.count) * sizeof(DailyTrendPoint);
    const size_t file_size = file.size();
    file.close();
    return validHeader(header, expected_date, file_size) &&
      file_size == sizeof(SnapshotHeader) + point_bytes;
  }

  static bool validHeader(const SnapshotHeader& header, const char* expected_date, size_t file_size) {
    const size_t point_bytes = static_cast<size_t>(header.count) * sizeof(DailyTrendPoint);
    return header.magic == kMagic && header.version == kVersion && header.header_size == sizeof(SnapshotHeader) &&
      header.date[10] == '\0' && std::strcmp(header.date, expected_date) == 0 && validDate(header.date) &&
      header.count <= DailyTrendHistory::kCapacity && header.sequence != 0 &&
      header.solar_kwh_complete <= 1 && header.solar_endpoint_valid <= 1 && header.reserved == 0 &&
      std::isfinite(header.solar_kwh_today) && header.solar_kwh_today >= 0.0f &&
      std::isfinite(header.solar_endpoint_kw) && header.solar_endpoint_kw >= 0.0f &&
      header.solar_endpoint_epoch >= 0 && header.last_observed_epoch >= 0 &&
      (!header.count || header.last_observed_epoch > 0) &&
      (!header.solar_endpoint_valid || header.solar_endpoint_epoch <= header.last_observed_epoch) &&
      file_size == sizeof(SnapshotHeader) + point_bytes &&
      (header.solar_endpoint_valid || header.solar_endpoint_epoch == 0);
  }

  bool readSnapshot(const char* path, const char* expected_date, SnapshotHeader& header,
                    DailyTrendHistory* output = nullptr, float max_hour = 24.0f) {
    header = {};
    fs::File file = SPIFFS.open(path, FILE_READ);
    if (!file || file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header) ||
        !validHeader(header, expected_date, file.size())) {
      file.close();
      return false;
    }

    if (output) {
      output->count = 0;
      std::memset(output->points, 0, sizeof(output->points));
    }
    uint32_t hash = hashBytes(2166136261UL, &header, offsetof(SnapshotHeader, checksum));

    int last_bucket = -1;
    uint16_t visible_count = 0;
    for (size_t i = 0; i < header.count; ++i) {
      DailyTrendPoint point{};
      if (file.read(reinterpret_cast<uint8_t*>(&point), sizeof(point)) != sizeof(point) || !validPoint(point)) {
        file.close();
        return false;
      }
      hash = hashBytes(hash, &point, sizeof(point));
      const int bucket = static_cast<int>(std::round(point.hour * 12.0f));
      if (bucket <= last_bucket) {
        file.close();
        return false;
      }
      last_bucket = bucket;
      if (output && point.hour <= max_hour) output->points[visible_count++] = point;
    }
    file.close();
    if (header.checksum != hash) return false;
    if (output) output->count = visible_count;
    return true;
  }
};

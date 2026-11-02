#pragma once

#include <Arduino.h>
#include <SPIFFS.h>
#include <cstddef>
#include <cmath>
#include "powerwall_client.hpp"

// Keep two small snapshots so an interrupted flash write leaves the previous
// completed five-minute averages available on the next boot.
class DailyHistoryStore {
public:
  bool ready() const { return mounted_; }

  bool begin() {
    // Preserve damaged history for recovery; never format on a mount failure.
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
    mounted_ = SPIFFS.begin(false, "/history", 10, "history");
#else
    mounted_ = SPIFFS.begin(false);
#endif
    return mounted_;
  }

  // False means the local clock is not ready yet; call again before the next poll.
  bool loadToday(PowerwallLocalClient& client) {
    struct tm today;
    if (!getLocalTime(&today, 10) || today.tm_year < 120) return false;

    bool found = false;
    sequence_ = 0;
    if (mounted_) {
      const char* slots[] = {"/pw_history_a.bin", "/pw_history_b.bin"};
      const float current_hour = today.tm_hour + today.tm_min / 60.0f + today.tm_sec / 3600.0f;
      for (const char* path : slots) {
        SnapshotHeader header;
        if (!readSnapshot(path, today, header)) continue;
        if (found && header.sequence <= sequence_) continue;

        size_t visible_count = 0;
        for (size_t i = 0; i < header.count; ++i) {
          if (scratch_[i].hour <= current_hour) {
            scratch_[visible_count++] = scratch_[i];
          }
        }
        client.seedHistory(scratch_, visible_count);
        sequence_ = header.sequence;
        found = true;
      }
    }

    if (!found) client.seedHistory(nullptr, 0);
    return true;
  }

  bool saveToday(const PowerwallLocalClient& client) {
    if (!mounted_) return false;
    struct tm today;
    if (!getLocalTime(&today, 10) || today.tm_year < 120) return false;
    if (!client.historyIsFor(today)) return false;

    const size_t count = client.completedHistoryCount();
    if (count > PowerwallLocalClient::kHistoryCapacity) return false;

    SnapshotHeader header{};
    header.magic = kMagic;
    header.version = kVersion;
    header.year = static_cast<uint16_t>(today.tm_year + 1900);
    header.yday = static_cast<uint16_t>(today.tm_yday);
    header.count = static_cast<uint16_t>(count);
    header.sequence = sequence_ + 1;
    header.checksum = checksum(header, client.history);

    const char* path = (header.sequence & 1U) ? "/pw_history_a.bin" : "/pw_history_b.bin";
    fs::File file = SPIFFS.open(path, FILE_WRITE);
    if (!file) return false;
    const size_t point_bytes = count * sizeof(HistoryPoint);
    const bool written =
      file.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header)) == sizeof(header) &&
      (point_bytes == 0 || file.write(reinterpret_cast<const uint8_t*>(client.history), point_bytes) == point_bytes);
    file.flush();
    file.close();
    if (!written) return false;

    SnapshotHeader verified;
    if (!readSnapshot(path, today, verified) || verified.sequence != header.sequence) return false;
    sequence_ = header.sequence;
    return true;
  }

private:
  struct SnapshotHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t year;
    uint16_t yday;
    uint16_t count;
    uint32_t sequence;
    uint32_t checksum;
  };

  static constexpr uint32_t kMagic = 0x50574831UL; // PWH1
  static constexpr uint16_t kVersion = 2;
  struct LegacyHistoryPoint {
    float hour;
    float soe;
    float load_kw;
  };
  static_assert(sizeof(LegacyHistoryPoint) == 12, "Legacy history layout changed");
  static_assert(sizeof(HistoryPoint) == 16, "HistoryPoint storage format changed");
  static_assert(sizeof(SnapshotHeader) == 20, "History snapshot header changed");

  bool mounted_ = false;
  uint32_t sequence_ = 0;
  HistoryPoint scratch_[PowerwallLocalClient::kHistoryCapacity] = {};
  LegacyHistoryPoint legacy_scratch_[PowerwallLocalClient::kHistoryCapacity] = {};

  static uint32_t hashBytes(uint32_t hash, const void* source, size_t bytes) {
    const uint8_t* data = static_cast<const uint8_t*>(source);
    for (size_t i = 0; i < bytes; ++i) {
      hash = (hash ^ data[i]) * 16777619UL;
    }
    return hash;
  }

  static uint32_t checksum(const SnapshotHeader& header, const HistoryPoint* points) {
    uint32_t hash = hashBytes(2166136261UL, &header, offsetof(SnapshotHeader, checksum));
    return hashBytes(hash, points, header.count * sizeof(HistoryPoint));
  }

  static uint32_t legacyChecksum(const SnapshotHeader& header, const LegacyHistoryPoint* points) {
    uint32_t hash = hashBytes(2166136261UL, &header, offsetof(SnapshotHeader, checksum));
    return hashBytes(hash, points, header.count * sizeof(LegacyHistoryPoint));
  }

  bool readSnapshot(const char* path, const struct tm& today, SnapshotHeader& header) {
    header = {};
    fs::File file = SPIFFS.open(path, FILE_READ);
    if (!file) return false;
    bool valid = file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) == sizeof(header);
    valid = valid && header.magic == kMagic && (header.version == 1 || header.version == kVersion) &&
      header.year == static_cast<uint16_t>(today.tm_year + 1900) &&
      header.yday == static_cast<uint16_t>(today.tm_yday) &&
      header.count <= PowerwallLocalClient::kHistoryCapacity;
    const size_t pointSize = header.version == 1 ? sizeof(LegacyHistoryPoint) : sizeof(HistoryPoint);
    valid = valid && file.size() == sizeof(header) + header.count * pointSize;
    if (valid && header.count > 0) {
      const size_t bytes = header.count * pointSize;
      if (header.version == 1) {
        valid = file.read(reinterpret_cast<uint8_t*>(legacy_scratch_), bytes) == bytes;
      } else {
        valid = file.read(reinterpret_cast<uint8_t*>(scratch_), bytes) == bytes;
      }
    }
    file.close();
    if (!valid) return false;

    if (header.version == 1) {
      if (header.checksum != legacyChecksum(header, legacy_scratch_)) return false;
      for (size_t i = 0; i < header.count; ++i) {
        const LegacyHistoryPoint& oldPoint = legacy_scratch_[i];
        if (!std::isfinite(oldPoint.hour) || oldPoint.hour < 0.0f || oldPoint.hour >= 24.0f ||
            !std::isfinite(oldPoint.soe) || !std::isfinite(oldPoint.load_kw)) {
          return false;
        }
        // v1 had no source field. The settled compatibility rule treats it as grid.
        scratch_[i] = {oldPoint.hour, oldPoint.soe, oldPoint.load_kw, HistoryPowerSource::Grid};
      }
    } else {
      if (header.checksum != checksum(header, scratch_)) return false;
      for (size_t i = 0; i < header.count; ++i) {
        const auto source = static_cast<uint8_t>(scratch_[i].power_source);
        if (!std::isfinite(scratch_[i].hour) || scratch_[i].hour < 0.0f || scratch_[i].hour >= 24.0f ||
            !std::isfinite(scratch_[i].soe) || !std::isfinite(scratch_[i].load_kw) ||
            source > static_cast<uint8_t>(HistoryPowerSource::Unknown)) {
          return false;
        }
      }
    }
    return true;
  }
};

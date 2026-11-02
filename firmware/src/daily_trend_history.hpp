#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>

// A field is present only when its independent validity bit is set.  In
// particular, an unavailable measurement is never represented as a zero.
enum DailyTrendValidity : uint8_t {
  kDailyTrendSolarValid = 1U << 0,
  kDailyTrendHomeValid  = 1U << 1,
  kDailyTrendGridValid  = 1U << 2,
  kDailyTrendSoeValid   = 1U << 3,
};

static constexpr uint8_t kDailyTrendKnownValidityMask =
  kDailyTrendSolarValid | kDailyTrendHomeValid | kDailyTrendGridValid | kDailyTrendSoeValid;

struct DailyTrendSample {
  float solar_kw = 0.0f;
  float home_kw = 0.0f;
  float grid_kw = 0.0f; // Signed: positive is import, negative is export.
  float soe = 0.0f;
  uint8_t valid_mask = 0;
};

// `hour` is the start of a five-minute local-time bucket. Repeated polls in
// one bucket replace only the valid fields they contain; other valid fields
// from the same bucket are retained.
#pragma pack(push, 1)
struct DailyTrendPoint {
  float hour = 0.0f;
  float solar_kw = 0.0f;
  float home_kw = 0.0f;
  float grid_kw = 0.0f;
  float soe = 0.0f;
  uint8_t valid_mask = 0;
  uint8_t reserved[3] = {};
  uint16_t solar_samples = 0;
  uint16_t home_samples = 0;
  uint16_t grid_samples = 0;
  uint16_t soe_samples = 0;
};
#pragma pack(pop)

class DailyTrendHistory {
public:
  static constexpr size_t kCapacity = 288;

  // Public snapshot fields used by firmware/UI consumers and the sidecar
  // store. `date` is local YYYY-MM-DD; count is bounded by kCapacity.
  char date[11] = {};
  uint16_t count = 0;
  DailyTrendPoint points[kCapacity] = {};
  float solar_kwh_today = 0.0f;
  bool solar_kwh_complete = false;

  // Starts a new local day when needed. Solar energy integrates trapezoids
  // only between observed, valid solar endpoints no more than ten minutes
  // apart. `solar_kwh_complete` describes observed coverage so far: it starts
  // complete only when the first valid solar poll is within 60 seconds of
  // midnight, and becomes incomplete permanently after a missing/long gap.
  bool ingestPoll(const struct tm& local_time, const DailyTrendSample& sample) {
    char poll_date[11];
    int64_t epoch = 0;
    int seconds_of_day = 0;
    if (!makeDateAndEpoch(local_time, poll_date, epoch, seconds_of_day)) return false;

    if (std::strcmp(date, poll_date) != 0) reset(poll_date);

    const int bucket = local_time.tm_hour * 12 + local_time.tm_min / 5;
    if (bucket < 0 || bucket >= static_cast<int>(kCapacity)) return false;

    if (last_observed_epoch_ > 0 && epoch < last_observed_epoch_) {
      // A same-day clock correction invalidates the active bucket and every
      // later bucket. Discard those observations before accepting the new poll.
      removeFromBucket(static_cast<uint16_t>(bucket));
      solar_kwh_today = 0.0f;
      solar_kwh_complete = false;
      solar_endpoint_epoch_ = 0;
      solar_endpoint_kw_ = 0.0f;
      solar_endpoint_valid_ = false;
      solar_day_started_ = true;
      solar_gap_seen_ = true;
    }

    DailyTrendSample valid = sample;
    valid.valid_mask &= kDailyTrendKnownValidityMask;
    if (!std::isfinite(valid.solar_kw) || valid.solar_kw < 0.0f) valid.valid_mask &= ~kDailyTrendSolarValid;
    if (!std::isfinite(valid.home_kw) || valid.home_kw < 0.0f) valid.valid_mask &= ~kDailyTrendHomeValid;
    if (!std::isfinite(valid.grid_kw)) valid.valid_mask &= ~kDailyTrendGridValid;
    if (!std::isfinite(valid.soe) || valid.soe < 0.0f || valid.soe > 100.0f)
      valid.valid_mask &= ~kDailyTrendSoeValid;

    if (!mergeBucket(static_cast<uint16_t>(bucket), valid)) return false;
    updateSolarEstimate(epoch, seconds_of_day, valid);
    last_observed_epoch_ = epoch;
    return true;
  }

private:
  int64_t solar_endpoint_epoch_ = 0;
  float solar_endpoint_kw_ = 0.0f;
  bool solar_endpoint_valid_ = false;
  bool solar_day_started_ = false;
  bool solar_gap_seen_ = false;
  int64_t last_observed_epoch_ = 0;

  friend class DailyTrendStore;

  void reset(const char* day) {
    std::memset(date, 0, sizeof(date));
    std::memcpy(date, day, sizeof(date));
    count = 0;
    std::memset(points, 0, sizeof(points));
    solar_kwh_today = 0.0f;
    solar_kwh_complete = false;
    solar_endpoint_epoch_ = 0;
    solar_endpoint_kw_ = 0.0f;
    solar_endpoint_valid_ = false;
    solar_day_started_ = false;
    solar_gap_seen_ = false;
    last_observed_epoch_ = 0;
  }

  void removeFromBucket(uint16_t first_removed_bucket) {
    size_t kept = 0;
    while (kept < count && static_cast<int>(points[kept].hour * 12.0f + 0.001f) < first_removed_bucket)
      ++kept;
    if (kept < count) {
      std::memset(points + kept, 0, (count - kept) * sizeof(DailyTrendPoint));
      count = static_cast<uint16_t>(kept);
    }
  }

  bool mergeBucket(uint16_t bucket, const DailyTrendSample& sample) {
    size_t index = 0;
    while (index < count && static_cast<int>(points[index].hour * 12.0f + 0.001f) < bucket) ++index;
    if (index == count || static_cast<int>(points[index].hour * 12.0f + 0.001f) != bucket) {
      if (count >= kCapacity) return false;
      for (size_t move = count; move > index; --move) points[move] = points[move - 1];
      points[index] = {};
      points[index].hour = static_cast<float>(bucket) / 12.0f;
      ++count;
    }

    DailyTrendPoint& point = points[index];
    if (sample.valid_mask & kDailyTrendSolarValid) {
      point.solar_kw = updateMean(point.solar_kw, point.solar_samples, sample.solar_kw);
    }
    if (sample.valid_mask & kDailyTrendHomeValid) {
      point.home_kw = updateMean(point.home_kw, point.home_samples, sample.home_kw);
    }
    if (sample.valid_mask & kDailyTrendGridValid) {
      point.grid_kw = updateMean(point.grid_kw, point.grid_samples, sample.grid_kw);
    }
    if (sample.valid_mask & kDailyTrendSoeValid) {
      point.soe = updateMean(point.soe, point.soe_samples, sample.soe);
    }
    point.valid_mask |= sample.valid_mask;
    return true;
  }

  static float updateMean(float mean, uint16_t& sample_count, float value) {
    if (sample_count == 0) {
      sample_count = 1;
      return value;
    }
    if (sample_count < UINT16_MAX) {
      const uint16_t next_count = static_cast<uint16_t>(sample_count + 1U);
      mean += (value - mean) / static_cast<float>(next_count);
      sample_count = next_count;
    }
    return mean;
  }

  void updateSolarEstimate(int64_t epoch, int seconds_of_day, const DailyTrendSample& sample) {
    if (!(sample.valid_mask & kDailyTrendSolarValid)) {
      solar_kwh_complete = false;
      solar_gap_seen_ = true;
      solar_endpoint_valid_ = false;
      solar_endpoint_epoch_ = 0;
      solar_endpoint_kw_ = 0.0f;
      return;
    }

    if (!solar_day_started_) {
      solar_day_started_ = true;
      solar_kwh_complete = seconds_of_day <= 60 && !solar_gap_seen_;
      if (!solar_kwh_complete) solar_gap_seen_ = true;
    }

    if (solar_endpoint_valid_) {
      const int64_t seconds = epoch - solar_endpoint_epoch_;
      if (seconds > 0 && seconds <= 600) {
        solar_kwh_today += (solar_endpoint_kw_ + sample.solar_kw) * 0.5f *
          (static_cast<float>(seconds) / 3600.0f);
      } else if (seconds != 0) {
        solar_kwh_complete = false;
        solar_gap_seen_ = true;
      }
    }

    solar_endpoint_epoch_ = epoch;
    solar_endpoint_kw_ = sample.solar_kw;
    solar_endpoint_valid_ = true;
  }

  static bool makeDateAndEpoch(const struct tm& input, char (&date_out)[11],
                               int64_t& epoch_out, int& seconds_of_day_out) {
    if (input.tm_year < 120 || input.tm_year > 8099 || input.tm_mon < 0 || input.tm_mon > 11 ||
        input.tm_mday < 1 || input.tm_mday > 31 || input.tm_hour < 0 || input.tm_hour > 23 ||
        input.tm_min < 0 || input.tm_min > 59 || input.tm_sec < 0 || input.tm_sec > 59) return false;

    struct tm normalized = input;
    normalized.tm_isdst = -1;
    const time_t epoch = std::mktime(&normalized);
    if (epoch < 0 || normalized.tm_year != input.tm_year || normalized.tm_mon != input.tm_mon ||
        normalized.tm_mday != input.tm_mday || normalized.tm_hour != input.tm_hour ||
        normalized.tm_min != input.tm_min) return false;

    const int written = std::snprintf(date_out, sizeof(date_out), "%04d-%02d-%02d",
                                      input.tm_year + 1900, input.tm_mon + 1, input.tm_mday);
    if (written != 10) return false;
    epoch_out = static_cast<int64_t>(epoch);
    seconds_of_day_out = input.tm_hour * 3600 + input.tm_min * 60 + input.tm_sec;
    return true;
  }
};

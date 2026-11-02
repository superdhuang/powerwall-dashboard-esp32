#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

#include "daily_trend_store.hpp"

#pragma pack(push, 1)
struct TrendSnapshotHeaderV2Fixture {
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
static_assert(sizeof(TrendSnapshotHeaderV2Fixture) == 57, "trend snapshot v2 fixture layout mismatch");

static int fail(const char* reason) {
  std::fprintf(stderr, "[DailyTrendStoreSmoke] FAIL: %s\n", reason);
  return 1;
}

static struct tm makeTime(int hour, int minute, int day = 1) {
  struct tm value{};
  value.tm_year = 126;
  value.tm_mon = 10;
  value.tm_mday = day;
  value.tm_hour = hour;
  value.tm_min = minute;
  value.tm_isdst = -1;
  return value;
}

static DailyTrendSample solarSample(float solar_kw) {
  DailyTrendSample sample{};
  sample.solar_kw = solar_kw;
  sample.valid_mask = kDailyTrendSolarValid;
  return sample;
}

static bool near(float left, float right, float epsilon = 0.0001f) {
  return std::fabs(left - right) <= epsilon;
}

int main() {
  const std::filesystem::path original_directory = std::filesystem::current_path();
  const std::filesystem::path test_directory = std::filesystem::temp_directory_path() /
    "powerwall_daily_trend_store_smoke";
  std::error_code error;
  std::filesystem::remove_all(test_directory, error);
  error.clear();
  std::filesystem::create_directories(test_directory, error);
  if (error) return fail("could not create isolated test directory");
  std::filesystem::current_path(test_directory, error);
  if (error) return fail("could not enter isolated test directory");

  // Merge independent sources in a bucket without filling absent fields.
  DailyTrendHistory merged{};
  DailyTrendSample first{};
  first.solar_kw = 1.0f;
  first.grid_kw = -0.75f;
  first.valid_mask = kDailyTrendSolarValid | kDailyTrendGridValid;
  if (!merged.ingestPoll(makeTime(0, 0), first)) return fail("first bucket ingest failed");
  DailyTrendSample second{};
  second.solar_kw = 3.0f;
  second.home_kw = 2.25f;
  second.grid_kw = -0.25f;
  second.soe = 61.0f;
  second.valid_mask = kDailyTrendSolarValid | kDailyTrendHomeValid |
    kDailyTrendGridValid | kDailyTrendSoeValid;
  if (!merged.ingestPoll(makeTime(0, 2), second)) return fail("same-bucket merge failed");
  DailyTrendSample next{};
  next.grid_kw = 0.5f;
  next.valid_mask = kDailyTrendGridValid;
  if (!merged.ingestPoll(makeTime(0, 5), next)) return fail("next bucket ingest failed");
  if (merged.count != 2 || merged.points[0].valid_mask != kDailyTrendKnownValidityMask ||
      !near(merged.points[0].solar_kw, 2.0f) || !near(merged.points[0].home_kw, 2.25f) ||
      !near(merged.points[0].grid_kw, -0.5f) || !near(merged.points[0].soe, 61.0f) ||
      merged.points[1].valid_mask != kDailyTrendGridValid) {
    return fail("bucket merge changed missing fields or lost independent validity");
  }
  DailyTrendHistory full_day{};
  for (int bucket = 0; bucket < 288; ++bucket) {
    const int hour = bucket / 12;
    const int minute = (bucket % 12) * 5;
    if (!full_day.ingestPoll(makeTime(hour, minute), solarSample(0.0f)))
      return fail("bounded full-day bucket ingest failed");
  }
  if (full_day.count != DailyTrendHistory::kCapacity ||
      !full_day.ingestPoll(makeTime(23, 59), solarSample(2.0f)) ||
      full_day.count != DailyTrendHistory::kCapacity)
    return fail("daily point count exceeded the 288-bucket bound");

  // Solar kWh uses only valid endpoint intervals at most ten minutes apart.
  DailyTrendHistory energy{};
  if (!energy.ingestPoll(makeTime(0, 0), solarSample(1.0f)) ||
      !energy.ingestPoll(makeTime(0, 5), solarSample(3.0f))) return fail("solar endpoint ingest failed");
  if (!near(energy.solar_kwh_today, 0.1666667f) || !energy.solar_kwh_complete)
    return fail("solar trapezoid estimate or initial completeness was wrong");
  if (!energy.ingestPoll(makeTime(0, 16), solarSample(9.0f))) return fail("long-gap ingest failed");
  if (!near(energy.solar_kwh_today, 0.1666667f) || energy.solar_kwh_complete)
    return fail("solar energy integrated across an interval over ten minutes");
  if (!energy.ingestPoll(makeTime(0, 21), solarSample(3.0f)) ||
      !near(energy.solar_kwh_today, 0.6666667f)) return fail("post-gap solar interval was not integrated");
  DailyTrendSample no_solar{};
  no_solar.home_kw = 1.0f;
  no_solar.valid_mask = kDailyTrendHomeValid;
  if (!energy.ingestPoll(makeTime(0, 22), no_solar) || energy.solar_kwh_complete)
    return fail("missing solar endpoint did not expose incomplete history");
  const float energy_before_rollover = energy.solar_kwh_today;
  if (!energy.ingestPoll(makeTime(0, 0, 2), solarSample(4.0f)) || energy.count != 1 ||
      std::string(energy.date) != "2026-11-02" || energy.solar_kwh_today != 0.0f ||
      !energy.solar_kwh_complete || energy.solar_kwh_today == energy_before_rollover)
    return fail("date rollover did not start an independent day at its first observed endpoint");
  DailyTrendHistory late_start{};
  if (!late_start.ingestPoll(makeTime(8, 0), solarSample(1.0f)) || late_start.solar_kwh_complete)
    return fail("late first solar observation was incorrectly marked complete");
  DailyTrendHistory failed_interval{};
  if (!failed_interval.ingestPoll(makeTime(0, 0), solarSample(1.0f))) return fail("failed-poll fixture setup failed");
  DailyTrendSample failed_poll{};
  if (!failed_interval.ingestPoll(makeTime(0, 2), failed_poll) ||
      !failed_interval.ingestPoll(makeTime(0, 5), solarSample(3.0f)) ||
      !near(failed_interval.solar_kwh_today, 0.0f) || failed_interval.solar_kwh_complete)
    return fail("failed poll did not break solar endpoint continuity");
  DailyTrendHistory rollback{};
  if (!rollback.ingestPoll(makeTime(0, 0), solarSample(1.0f)) ||
      !rollback.ingestPoll(makeTime(0, 5), solarSample(3.0f)) ||
      !rollback.ingestPoll(makeTime(0, 16), solarSample(9.0f)) ||
      !rollback.ingestPoll(makeTime(0, 17), no_solar) ||
      !near(rollback.solar_kwh_today, 0.1666667f) || rollback.solar_kwh_complete)
    return fail("clock-regression fixture did not establish partial solar history");
  if (!rollback.ingestPoll(makeTime(0, 12), solarSample(7.0f)) || rollback.count != 3 ||
      rollback.points[2].hour > 10.0f / 60.0f || rollback.solar_kwh_today != 0.0f ||
      rollback.solar_kwh_complete)
    return fail("same-day clock regression retained future points or solar energy");
  if (!rollback.ingestPoll(makeTime(0, 12), solarSample(9.0f)) || rollback.count != 3 ||
      rollback.solar_kwh_today != 0.0f)
    return fail("equal-epoch repeated poll incorrectly changed regression state");

  // Build a snapshot with future solar history followed by a poll that lacks
  // solar, leaving no endpoint to detect the future scalar contribution.
  DailyTrendHistory saved{};
  if (!saved.ingestPoll(makeTime(0, 0), solarSample(1.0f)) ||
      !saved.ingestPoll(makeTime(0, 5), solarSample(2.0f)) ||
      !saved.ingestPoll(makeTime(0, 10), solarSample(3.0f))) return fail("store fixture ingest failed");
  const float first_snapshot_kwh = saved.solar_kwh_today;
  DailyTrendStore store;
  if (!store.saveToday(saved)) return fail("first A/B snapshot save failed");
  if (!saved.ingestPoll(makeTime(0, 15), solarSample(5.0f)) ||
      !saved.ingestPoll(makeTime(0, 16), no_solar) || !store.saveToday(saved))
    return fail("second A/B snapshot save failed");
  if (fs::formatRequested) return fail("sidecar store requested a SPIFFS format");
  TrendSnapshotHeaderV2Fixture future_gap_header{};
  {
    std::ifstream snapshot("pw_trend_b.bin", std::ios::binary);
    if (!snapshot) return fail("could not inspect future-gap sidecar fixture");
    snapshot.read(reinterpret_cast<char*>(&future_gap_header), sizeof(future_gap_header));
    if (!snapshot) return fail("could not read future-gap sidecar header");
  }
  struct tm future_sample_time = makeTime(0, 16);
  struct tm rollback_time = makeTime(0, 15);
  const int64_t future_sample_epoch = static_cast<int64_t>(std::mktime(&future_sample_time));
  const int64_t rollback_epoch = static_cast<int64_t>(std::mktime(&rollback_time));
  if (future_gap_header.version != 2 || future_gap_header.solar_endpoint_valid != 0 ||
      future_gap_header.solar_kwh_today <= 0.0f ||
      future_gap_header.last_observed_epoch != future_sample_epoch ||
      future_gap_header.last_observed_epoch <= rollback_epoch)
    return fail("fixture did not persist future last-observed time with an invalid solar endpoint");

  DailyTrendHistory truncated_to_now{};
  if (!store.loadToday(truncated_to_now, makeTime(0, 7))) return fail("today snapshot was not loaded");
  if (truncated_to_now.count != 1 || truncated_to_now.points[0].hour != 0.0f ||
      truncated_to_now.solar_kwh_today != 0.0f || truncated_to_now.solar_kwh_complete) {
    std::fprintf(stderr, "load count=%u hours=%.4f/%.4f kwh=%.6f complete=%d\n", truncated_to_now.count,
      truncated_to_now.points[0].hour, truncated_to_now.points[1].hour,
      truncated_to_now.solar_kwh_today, truncated_to_now.solar_kwh_complete);
    return fail("load did not omit future points and conservatively clear future energy");
  }
  DailyTrendHistory same_bucket_rollback{};
  if (!store.loadToday(same_bucket_rollback, makeTime(0, 15)) || same_bucket_rollback.count != 3 ||
      !near(same_bucket_rollback.points[2].hour, 10.0f / 60.0f) ||
      same_bucket_rollback.solar_kwh_today != 0.0f || same_bucket_rollback.solar_kwh_complete)
    return fail("persisted last-observed epoch did not detect rollback within the current bucket");

  // Damage the newer B slot; the older valid A slot must remain loadable.
  {
    std::fstream corrupt("pw_trend_b.bin", std::ios::in | std::ios::out | std::ios::binary);
    if (!corrupt) return fail("could not open newer snapshot for corruption fixture");
    char invalid = 0;
    const std::streamoff point_value_offset = static_cast<std::streamoff>(57 + offsetof(DailyTrendPoint, solar_kw));
    corrupt.seekg(point_value_offset);
    corrupt.read(&invalid, 1);
    if (!corrupt) return fail("could not read point byte for corruption fixture");
    invalid ^= 0x01;
    corrupt.seekp(point_value_offset);
    corrupt.write(&invalid, 1);
    if (!corrupt) return fail("could not corrupt newer snapshot checksum fixture");
  }
  DailyTrendHistory fallback{};
  if (!store.loadToday(fallback, makeTime(0, 20)) || fallback.count != 3 ||
      !near(fallback.solar_kwh_today, first_snapshot_kwh) || fallback.solar_kwh_complete) {
    std::fprintf(stderr, "fallback count=%u kwh=%.7f expected=%.7f complete=%d\n",
                 fallback.count, fallback.solar_kwh_today, first_snapshot_kwh, fallback.solar_kwh_complete);
    return fail("corrupt latest slot did not fall back to the previous valid snapshot");
  }
  DailyTrendHistory after_restart_ingest = fallback;
  if (!after_restart_ingest.ingestPoll(makeTime(0, 12), solarSample(7.0f)) ||
      !near(after_restart_ingest.solar_kwh_today, first_snapshot_kwh) ||
      after_restart_ingest.solar_kwh_complete)
    return fail("reload restored an endpoint and bridged the restart interval");

  DailyTrendHistory other_day{};
  if (store.loadToday(other_day, makeTime(0, 0, 2)) || other_day.count != 0 ||
      std::string(other_day.date) != "2026-11-02")
    return fail("load returned data from a different local date");

  // A partial write may damage only the alternate slot, preserving the latest
  // valid one. The subsequent retry must advance from that valid generation.
  fallback.solar_kwh_today += 0.25f;
  fs::writeBudget = 2;
  if (store.saveToday(fallback)) return fail("injected partial write unexpectedly succeeded");
  fs::writeBudget = std::numeric_limits<size_t>::max();
  DailyTrendHistory after_failure{};
  if (!store.loadToday(after_failure, makeTime(0, 20)) ||
      !near(after_failure.solar_kwh_today, first_snapshot_kwh))
    return fail("failed save damaged the newest valid slot");
  if (!store.saveToday(fallback)) return fail("save retry did not replace damaged alternate slot");
  DailyTrendHistory after_retry{};
  if (!store.loadToday(after_retry, makeTime(0, 20)) ||
      !near(after_retry.solar_kwh_today, first_snapshot_kwh + 0.25f))
    return fail("verified retry was not selected as the newest snapshot");

  std::filesystem::current_path(original_directory, error);
  if (error) return fail("could not restore original working directory");
  std::filesystem::remove_all(test_directory, error);
  std::printf("[DailyTrendStoreSmoke] PASS: bucketing, independent validity, solar gaps, date filtering, A/B fallback and write retry\n");
  return 0;
}

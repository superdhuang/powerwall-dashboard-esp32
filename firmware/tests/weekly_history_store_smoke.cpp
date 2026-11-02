#include <Arduino.h>
#include <filesystem>
#include <fstream>
#include <limits>

static tm testClock{};
static bool clockReady = true;
static unsigned long testMillis = 1;
extern "C" unsigned long millis(void) { return testMillis; }
static bool testGetLocalTime(tm* output, uint32_t = 5000) {
  if (!clockReady) return false;
  *output = testClock;
  return true;
}
#define getLocalTime testGetLocalTime
#include "daily_history_store.hpp"
#include "weekly_history_recorder.hpp"
#undef getLocalTime

#define CHECK(condition, message) do { if (!(condition)) { std::fprintf(stderr, "[WeeklyStoreTest] FAIL: %s (line %d)\n", message, __LINE__); return 1; } } while (0)
static void pass(const char* message) { std::printf("[WeeklyStoreTest] PASS: %s\n", message); }
static time_t setClock(int year, int month, int day, int hour = 12, int minute = 0) {
  testClock = {};
  testClock.tm_year = year - 1900; testClock.tm_mon = month - 1; testClock.tm_mday = day;
  testClock.tm_hour = hour; testClock.tm_min = minute; testClock.tm_isdst = -1;
  return mktime(&testClock);
}
static std::filesystem::path testRoot;
static void scenario(const char* name) {
  std::filesystem::current_path(testRoot);
  std::filesystem::create_directory(name);
  std::filesystem::current_path(name);
  fs::failWriteOpen = false;
  fs::writeBudget = std::numeric_limits<size_t>::max();
  fs::writeOpenCount = 0;
  clockReady = true;
  HTTPClient::fail = false;
}

int main() {
  setenv("TZ", "Asia/Taipei", 1); tzset();
  testRoot = std::filesystem::current_path();
  scenario("snapshots");
  time_t now = setClock(2026, 10, 24, 23, 59);
  WeeklyHistory history; history.roll(now);
  for (auto& day : history.days) {
    for (int b = 0; b < 288; ++b) day.points.push_back({b / 12.f, 50, 2, HistoryPowerSource::Grid});
  }
  WeeklyHistoryStore store;
  CHECK(store.save(history), "save full seven-day archive");
  WeeklyHistory restored;
  WeeklyHistoryStore restart;
  CHECK(restart.load(restored, now), "restart restores archive");
  for (size_t d = 0; d < 7; ++d) {
    CHECK(restored.days[d].points.size() == 288 && !strcmp(restored.days[d].date, history.days[d].date), "all seven days and 2016 points survive");
  }
  history.days[0].points[0].load_kw = 9;
  CHECK(store.save(history), "save newer generation");
  {
    std::fstream damaged("pw_week_a.bin", std::ios::binary | std::ios::in | std::ios::out);
    damaged.seekp(40); damaged.put('\xff');
  }
  WeeklyHistoryStore fallback;
  CHECK(fallback.load(restored, now) && restored.days[0].points[0].load_kw == 2, "checksum failure falls back to previous complete generation");
  fs::writeBudget = 64;
  CHECK(!fallback.save(history), "interrupted write reports failure");
  WeeklyHistoryStore afterInterruption;
  CHECK(afterInterruption.load(restored, now) && restored.days[0].points[0].load_kw == 2, "interrupted write preserves previous snapshot");
  fs::writeBudget = std::numeric_limits<size_t>::max();
  CHECK(fallback.save(history), "retry succeeds without advancing sequence on failure");
  WeeklyHistoryStore repaired;
  CHECK(repaired.load(restored, now) && restored.days[0].points[0].load_kw == 9, "retry restores latest values");
  WeeklyHistory invalid = history;
  invalid.days[0].points[0].load_kw = NAN;
  const size_t writes = fs::writeOpenCount;
  CHECK(!fallback.save(invalid) && fs::writeOpenCount == writes, "invalid observations rejected before opening snapshot");
  now = setClock(2026, 10, 24, 12);
  CHECK(repaired.load(restored, now) && restored.days[6].points.size() == 145, "backwards clock discards future observations today");
  now = setClock(2027, 1, 1);
  CHECK(repaired.load(restored, now), "load archive across year boundary");
  for (const auto& day : restored.days) CHECK(day.points.empty(), "expired dates are cleared after downtime");
  CHECK(!strcmp(restored.days[4].date, "2026-12-31"), "year rollover uses calendar dates");
  pass("2016-point restart, damaged/truncated snapshots, retry, invalid values and clock/year rollover");

  scenario("recorder");
  now = setClock(2026, 10, 24, 23, 51);
  PowerwallLocalClient client;
  WeeklyHistoryRecorder recorder;
  clockReady = false;
  CHECK(client.pollLiveData(), "fixture poll before NTP");
  recorder.update(client, 0);
  CHECK(recorder.flush() && fs::writeOpenCount == 0, "clock not ready never persists uptime as a date");
  CHECK(!recorder.flush(true) && fs::writeOpenCount == 0, "forced save before clock/history readiness reports failure without writing");
  clockReady = true;
  client.seedHistory(nullptr, 0);
  CHECK(client.pollLiveData(), "first dated poll");
  recorder.update(client, now);
  CHECK(recorder.flush(), "initial dated snapshot");
  recorder.snapshot(restored, client);
  CHECK(restored.days[6].points.size() == 1, "UI snapshot includes in-progress bucket");
  WeeklyHistoryStore checkpoint;
  CHECK(checkpoint.load(history, now) && history.days[6].points.empty(), "in-progress bucket is not persisted as complete");
  const size_t beforeForced = fs::writeOpenCount;
  CHECK(!recorder.needsSave() && recorder.flush(true) && recorder.flush(true) &&
        fs::writeOpenCount == beforeForced + 2, "manual saves really write both A/B slots even with unchanged revision");
  WeeklyHistoryStore forcedRestart;
  CHECK(forcedRestart.load(history, now) && history.days[6].points.empty(), "forced saves do not finalize the in-progress bucket");
  fs::failWriteOpen = true;
  CHECK(!recorder.flush(true), "forced save reports storage failure");
  fs::failWriteOpen = false;
  CHECK(recorder.flush(true), "forced save can retry after storage failure");
  const size_t initialWrites = fs::writeOpenCount;
  testMillis += 5000;
  CHECK(client.pollLiveData(), "repeat poll");
  recorder.update(client, now);
  CHECK(recorder.flush() && fs::writeOpenCount == initialWrites, "same incomplete bucket causes no flash write");
  now = setClock(2026, 10, 24, 23, 56);
  testMillis += 300000;
  CHECK(client.pollLiveData(), "complete five-minute bucket");
  recorder.update(client, now);
  fs::failWriteOpen = true;
  CHECK(!recorder.flush(), "failed write keeps archive pending");
  fs::failWriteOpen = false;
  CHECK(recorder.flush(), "pending write retries with no new observation");
  CHECK(checkpoint.load(history, now) && history.days[6].points.size() == 1, "completed five-minute bucket survives restart");
  now = setClock(2026, 10, 25, 0, 1);
  recorder.update(client, now); // before daily reset
  testMillis += 300000;
  CHECK(client.pollLiveData(), "daily client rolls to Sunday");
  recorder.update(client, now);
  CHECK(recorder.flush(), "persist rollover");
  recorder.snapshot(restored, client);
  CHECK(restored.days[6].points.size() == 2 && restored.days[0].points.size() == 1, "UI retains yesterday including last partial bucket and today's live bucket");
  CHECK(checkpoint.load(history, now) && history.days[6].points.size() == 2 && history.days[0].points.empty(), "restart retains yesterday while excluding today's incomplete bucket");
  const size_t beforeFailure = fs::writeOpenCount;
  HTTPClient::fail = true;
  CHECK(!client.pollLiveData(), "network failure fixture");
  recorder.update(client, now);
  CHECK(recorder.flush() && fs::writeOpenCount == beforeFailure, "network outage does not invent zero samples or rewrite unchanged history");
  HTTPClient::fail = false;
  now = setClock(2026, 11, 1, 0, 1);
  recorder.update(client, now);
  recorder.snapshot(restored, client);
  CHECK(restored.days[6].points.empty() && restored.days[0].points.empty(), "old observations expire after seven days");
  pass("real client polls: NTP gate, live/completed buckets, write cadence, retry, midnight and offline retention");

  scenario("poll_crosses_midnight");
  now = setClock(2026, 12, 31, 23, 59);
  PowerwallLocalClient midnightClient;
  WeeklyHistoryRecorder midnightRecorder;
  CHECK(midnightClient.pollLiveData(), "last poll before midnight");
  midnightRecorder.update(midnightClient, now);
  // Simulate a request whose response arrives on the following calendar day.
  midnightRecorder.update(midnightClient, now);
  now = setClock(2027, 1, 1, 0, 1);
  testMillis += 120000;
  CHECK(midnightClient.pollLiveData(), "HTTPS response arrives after midnight");
  midnightRecorder.update(midnightClient, now);
  CHECK(midnightRecorder.flush(), "persist in-flight request rollover");
  WeeklyHistoryStore midnightRestart;
  CHECK(midnightRestart.load(restored, now) && !strcmp(restored.days[4].date, "2026-12-31") &&
    restored.days[4].points.size() == 1 && restored.days[5].points.empty(),
    "request crossing midnight preserves previous day's final bucket across reboot");
  pass("request spanning midnight/year boundary retains the previous live bucket");

  scenario("daily_import");
  now = setClock(2026, 10, 24, 12);
  struct DailyHeader { uint32_t magic; uint16_t version, year, yday, count; uint32_t sequence, checksum; };
  struct OldPoint { float hour, soe, load; };
  const OldPoint old{1, 67, 1.25f};
  DailyHeader header{0x50574831UL, 1, 2026, static_cast<uint16_t>(testClock.tm_yday), 1, 7, 0};
  auto hash = [](uint32_t h, const void* source, size_t count) {
    const auto* p = static_cast<const uint8_t*>(source);
    for (size_t i = 0; i < count; ++i) h = (h ^ p[i]) * 16777619UL;
    return h;
  };
  header.checksum = hash(hash(2166136261UL, &header, offsetof(DailyHeader, checksum)), &old, sizeof(old));
  {
    std::ofstream fixture("pw_history_a.bin", std::ios::binary);
    fixture.write(reinterpret_cast<const char*>(&header), sizeof(header));
    fixture.write(reinterpret_cast<const char*>(&old), sizeof(old));
  }
  DailyHistoryStore daily;
  PowerwallLocalClient imported;
  CHECK(daily.begin() && daily.loadToday(imported), "load old daily file");
  WeeklyHistoryRecorder migration;
  migration.update(imported, now);
  CHECK(migration.flush(), "persist imported day to week");
  WeeklyHistoryStore importedRestart;
  CHECK(importedRestart.load(restored, now) && restored.days[6].points.size() == 1 &&
    restored.days[6].points[0].power_source == HistoryPowerSource::Grid && restored.days[6].points[0].load_kw == 1.25f,
    "legacy daily record retains values and missing source becomes Grid");
  pass("existing v1 daily snapshot imports into persistent week without replacing daily files");
  std::filesystem::current_path(testRoot);
  return 0;
}

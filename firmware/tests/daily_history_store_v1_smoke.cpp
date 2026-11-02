#include <Arduino.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>

#include "daily_history_store.hpp"

extern "C" unsigned long millis(void) { return 0; }

struct SnapshotHeaderV1 {
  uint32_t magic;
  uint16_t version;
  uint16_t year;
  uint16_t yday;
  uint16_t count;
  uint32_t sequence;
  uint32_t checksum;
};

struct LegacyHistoryPointV1 {
  float hour;
  float soe;
  float load_kw;
};

static_assert(sizeof(SnapshotHeaderV1) == 20, "v1 header layout mismatch");
static_assert(sizeof(LegacyHistoryPointV1) == 12, "v1 point layout mismatch");

static uint32_t hashBytes(uint32_t hash, const void* source, size_t bytes) {
  const auto* data = static_cast<const uint8_t*>(source);
  for (size_t i = 0; i < bytes; ++i) hash = (hash ^ data[i]) * 16777619UL;
  return hash;
}

static int fail(const char* reason) {
  std::fprintf(stderr, "[HistoryV1SmokeTest] FAIL: %s\n", reason);
  return 1;
}

int main() {
  struct tm today = {};
  if (!getLocalTime(&today, 10) || today.tm_year < 120) return fail("local clock unavailable");

  SnapshotHeaderV1 header{};
  header.magic = 0x50574831UL;
  header.version = 1;
  header.year = static_cast<uint16_t>(today.tm_year + 1900);
  header.yday = static_cast<uint16_t>(today.tm_yday);
  header.count = 1;
  header.sequence = 7;
  const LegacyHistoryPointV1 old_point{0.0f, 67.5f, 1.25f};
  header.checksum = hashBytes(2166136261UL, &header, offsetof(SnapshotHeaderV1, checksum));
  header.checksum = hashBytes(header.checksum, &old_point, sizeof(old_point));
  {
    std::ofstream fixture("pw_history_a.bin", std::ios::binary | std::ios::trunc);
    if (!fixture) return fail("could not create v1 fixture");
    fixture.write(reinterpret_cast<const char*>(&header), sizeof(header));
    fixture.write(reinterpret_cast<const char*>(&old_point), sizeof(old_point));
    if (!fixture) return fail("could not write v1 fixture");
  }

  PowerwallLocalClient client;
  DailyHistoryStore failedStore;
  fs::mountAvailable = false;
  const size_t writesBeforeMount = fs::writeOpenCount;
  if (failedStore.begin() || fs::formatRequested || failedStore.ready())
    return fail("mount failure must preserve storage without formatting");
  if (failedStore.saveToday(client) || fs::writeOpenCount != writesBeforeMount)
    return fail("failed mount must not write or truncate history");
  fs::mountAvailable = true;
  DailyHistoryStore store;
  if (!store.begin() || !store.loadToday(client)) return fail("store could not load v1 snapshot");
  if (client.history_count != 1) return fail("v1 snapshot record count was not retained");
  const HistoryPoint& migrated = client.history[0];
  if (migrated.power_source != HistoryPowerSource::Grid || migrated.hour != old_point.hour ||
      migrated.soe != old_point.soe || migrated.load_kw != old_point.load_kw) {
    return fail("v1 record did not migrate to Grid with its values intact");
  }
  if (!store.saveToday(client)) return fail("migrated history could not be saved as v2");

  SnapshotHeaderV1 saved_header{};
  HistoryPoint saved_point{};
  {
    std::ifstream saved("pw_history_b.bin", std::ios::binary);
    if (!saved) return fail("v2 snapshot was not written");
    saved.read(reinterpret_cast<char*>(&saved_header), sizeof(saved_header));
    saved.read(reinterpret_cast<char*>(&saved_point), sizeof(saved_point));
    if (!saved) return fail("v2 snapshot was truncated");
  }
  if (saved_header.version != 2 || saved_header.count != 1 ||
      saved_point.power_source != HistoryPowerSource::Grid) {
    return fail("saved v2 snapshot did not retain migrated Grid source");
  }
  std::printf("[HistoryV1SmokeTest] PASS: v1 fixture loaded as Grid and saved as a valid v2 snapshot\n");
  return 0;
}

#pragma once
// Shared firmware/simulator history. Five-minute buckets bound storage to seven days.
#include <array>
#include <vector>
#include <algorithm>
#include <ctime>
#include <cmath>
#include <cstring>
#include <memory>
#include <new>
#include <cstdlib>
#include "powerwall_client.hpp"

#if defined(ESP32)
#include <esp_heap_caps.h>
// Several task/UI copies of a full week must not consume Wi-Fi/TLS's internal RAM.
template<class T> struct WeeklyHistoryAllocator {
  using value_type = T;
  WeeklyHistoryAllocator() = default;
  template<class U> WeeklyHistoryAllocator(const WeeklyHistoryAllocator<U>&) {}
  T* allocate(size_t count) {
    void* p = heap_caps_malloc(count * sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = heap_caps_malloc(count * sizeof(T), MALLOC_CAP_8BIT);
    if (!p) std::abort(); // Same out-of-memory contract as std::vector without exceptions.
    return static_cast<T*>(p);
  }
  void deallocate(T* p, size_t) { heap_caps_free(p); }
  template<class U> bool operator==(const WeeklyHistoryAllocator<U>&) const { return true; }
  template<class U> bool operator!=(const WeeklyHistoryAllocator<U>&) const { return false; }
};
#else
template<class T> using WeeklyHistoryAllocator = std::allocator<T>;
#endif

struct WeeklyHistoryDay {
  char date[11] = {};
  std::vector<HistoryPoint, WeeklyHistoryAllocator<HistoryPoint>> points;
};
struct WeeklyHistory {
  std::array<WeeklyHistoryDay, 7> days;
  uint32_t revision = 0;
  static bool validTime(time_t now) {
    tm day{}; localtime_r(&now, &day);
    return day.tm_year >= 120;
  }
  static bool validPoint(const HistoryPoint& p) {
    return std::isfinite(p.hour) && std::isfinite(p.load_kw) && std::isfinite(p.soe) &&
      p.hour >= 0 && p.hour < 24 && p.load_kw >= 0 && p.soe >= 0 && p.soe <= 100 &&
      static_cast<uint8_t>(p.power_source) <= static_cast<uint8_t>(HistoryPowerSource::Unknown);
  }
  static tm dayAt(time_t now, int daysAgo = 0) {
    tm day{}; localtime_r(&now, &day);
    day.tm_hour = 12; day.tm_min = day.tm_sec = 0;
    day.tm_mday -= daysAgo; day.tm_isdst = -1;
    mktime(&day);
    return day;
  }
  static bool peak(const tm& day, float hour) {
    if (day.tm_wday == 0 || day.tm_wday == 6) return false;
    return day.tm_mon >= 5 && day.tm_mon <= 8 ? hour >= 9 && hour < 24
      : (hour >= 6 && hour < 11) || (hour >= 14 && hour < 24);
  }
  void roll(time_t now) {
    if (!validTime(now)) return;
    for (int ago = 0; ago < 7; ++ago) {
      tm day = dayAt(now, ago); char date[11]; strftime(date, sizeof(date), "%Y-%m-%d", &day);
      auto& slot = days[day.tm_wday];
      if (strcmp(slot.date, date)) { strcpy(slot.date, date); slot.points.clear(); ++revision; }
    }
  }
  // Merge by bucket; repeated polls replace a bucket instead of adding weight.
  bool merge(const char* date, const HistoryPoint* points, size_t count) {
    if (!date || !date[0] || (count && !points)) return false;
    for (auto& day : days) if (!strcmp(day.date, date)) {
      if (!count) return true;
      // The network task has a 16 KB stack: keep this ~12 KB workspace on the heap.
      struct Bucket { HistoryPoint point{}; double load = 0, soe = 0; size_t samples = 0; bool present = false; };
      std::unique_ptr<Bucket[]> bins(new (std::nothrow) Bucket[288]{});
      if (!bins) return false;
      for (const auto& p : day.points) {
        if (!validPoint(p)) continue;
        int b = std::min(287, int(p.hour * 12 + .001f)); bins[b].point = p; bins[b].present = true;
      }
      for (size_t i = 0; i < count; ++i) {
        const auto& p = points[i];
        if (!validPoint(p)) continue;
        auto& bin = bins[std::min(287, int(p.hour * 12 + .001f))];
        bin.load += p.load_kw; bin.soe += p.soe; ++bin.samples;
        bin.point.power_source = p.power_source; bin.present = true;
      }
      size_t resultCount = 0;
      bool changed = false;
      for (int b = 0; b < 288; ++b) if (bins[b].present) {
        auto& bin = bins[b];
        if (bin.samples) { bin.point.hour = b / 12.f; bin.point.load_kw = bin.load / bin.samples; bin.point.soe = bin.soe / bin.samples; }
        if (resultCount >= day.points.size()) changed = true;
        else {
          const auto& old = day.points[resultCount];
          changed = changed || old.hour != bin.point.hour || old.load_kw != bin.point.load_kw ||
            old.soe != bin.point.soe || old.power_source != bin.point.power_source;
        }
        ++resultCount;
      }
      if (!changed && resultCount == day.points.size()) return true;
      day.points.reserve(288);
      day.points.clear();
      for (int b = 0; b < 288; ++b) if (bins[b].present) day.points.push_back(bins[b].point);
      ++revision;
      return true;
    }
    return false;
  }
  // Equal five-minute observations; outages never contribute artificial zeroes.
  static std::array<float, 3> averages(const WeeklyHistoryDay& day, const tm& date) {
    double sum[3] = {}; int n[3] = {};
    for (const auto& p : day.points) {
      int band = peak(date, p.hour) ? 1 : 2;
      sum[0] += p.load_kw; ++n[0]; sum[band] += p.load_kw; ++n[band];
    }
    return {n[0] ? float(sum[0]/n[0]) : NAN, n[1] ? float(sum[1]/n[1]) : NAN, n[2] ? float(sum[2]/n[2]) : NAN};
  }
};

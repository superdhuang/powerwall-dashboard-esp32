#pragma once

#include <cstdio>
#include <cstdlib>

namespace native480_timezone {

inline constexpr int kDefaultOffsetMinutes = 8 * 60;
inline constexpr int kMinimumOffsetMinutes = -12 * 60;
inline constexpr int kMaximumOffsetMinutes = 14 * 60;
inline constexpr int kOffsetStepMinutes = 60;

inline constexpr bool isValidOffsetMinutes(int offsetMinutes) {
  return offsetMinutes >= kMinimumOffsetMinutes &&
         offsetMinutes <= kMaximumOffsetMinutes &&
         offsetMinutes % kOffsetStepMinutes == 0;
}

inline constexpr int offsetSeconds(int offsetMinutes) {
  return offsetMinutes * 60;
}

inline bool parseOffsetMinutes(const char* value, int& offsetMinutes) {
  if (!value || !*value) return false;
  const char* digit = value;
  if (*digit == '+' || *digit == '-') ++digit;
  if (!*digit) return false;
  for (const char* current = digit; *current; ++current)
    if (*current < '0' || *current > '9') return false;
  char* end = nullptr;
  const long parsed = std::strtol(value, &end, 10);
  if (end == value || !end || *end != '\0' ||
      parsed < kMinimumOffsetMinutes || parsed > kMaximumOffsetMinutes ||
      parsed % kOffsetStepMinutes != 0) return false;
  offsetMinutes = static_cast<int>(parsed);
  return true;
}

inline void formatUtcOffset(int offsetMinutes, char* output, size_t outputSize) {
  if (!output || outputSize == 0) return;
  if (!isValidOffsetMinutes(offsetMinutes)) offsetMinutes = kDefaultOffsetMinutes;
  const int absolute = offsetMinutes < 0 ? -offsetMinutes : offsetMinutes;
  std::snprintf(output, outputSize, "UTC%c%02d:%02d",
                offsetMinutes < 0 ? '-' : '+', absolute / 60, absolute % 60);
}

}  // namespace native480_timezone

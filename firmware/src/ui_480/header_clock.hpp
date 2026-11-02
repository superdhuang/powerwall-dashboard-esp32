#pragma once
#include <cstdio>
#include <ctime>

namespace ui480 {
inline void formatHeaderClock(char* text, size_t size, const struct tm* local) {
#if defined(POWERWALL_NATIVE480_LANG_EN)
  static const char* weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
#endif
  if (!local || local->tm_wday < 0 || local->tm_wday > 6) {
    snprintf(text, size, "--/-- --- --:--");
    return;
  }
#if defined(POWERWALL_NATIVE480_LANG_EN)
  snprintf(text, size, "%02d/%02d %s %02d:%02d", local->tm_mon + 1,
           local->tm_mday, weekdays[local->tm_wday], local->tm_hour, local->tm_min);
#else
  snprintf(text, size, "%02d月%02d日 %02d:%02d", local->tm_mon + 1,
           local->tm_mday, local->tm_hour, local->tm_min);
#endif
}
}  // namespace ui480

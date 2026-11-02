#pragma once

#include <cmath>
#include <cstddef>
#include <cstdio>

// Time until the configured backup reserve at the current house load.
inline void formatUsableEta(char* out, size_t size, double remainingKwh,
                            double fullKwh, double reservePct, double loadKw,
                            bool withSeparator = true) {
  if (!out || size == 0) return;
  const char* prefix = withSeparator ? "· " : "";
  const double full = std::isfinite(fullKwh) && fullKwh != 0.0 ? fullKwh : 14.6;
  const double remaining = std::isfinite(remainingKwh) ? remainingKwh : 0.0;
  const double reserve = std::isfinite(reservePct) ? reservePct : 24.0;
  const double usable = std::fmax(0.0, remaining - full * reserve / 100.0);
  if (usable <= 0.02) {
    std::snprintf(out, size, "%s0m", prefix);
  } else if (!std::isfinite(loadKw) || loadKw <= 0.05) {
    std::snprintf(out, size, "%s--", prefix);
  } else {
    const long minutes = std::lround(usable / loadKw * 60.0);
    if (minutes <= 0) std::snprintf(out, size, "%s0m", prefix);
    else if (minutes < 60) std::snprintf(out, size, "%s~%ldm", prefix, minutes);
    else if (minutes % 60 == 0) std::snprintf(out, size, "%s~%ldh", prefix, minutes / 60);
    else std::snprintf(out, size, "%s~%ldh %ldm", prefix, minutes / 60, minutes % 60);
  }
}

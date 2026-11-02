#pragma once
#include <stdint.h>
#include <cmath>

namespace powerflow {
constexpr uint32_t kPeriodMs = 1000;
constexpr float kTailSeconds = 0.8f;

// Repeat along the whole route, so a new pulse starts before the previous one
// reaches the end. Signed branch distance retains source -> junction -> sink.
inline float brightness(uint32_t elapsed_ms, float distance, float speed) {
  if (speed <= 0.0f) return 0.0f;
  const float spacing = speed * (kPeriodMs / 1000.0f);
  const float tail = speed * kTailSeconds;
  float age = fmodf((elapsed_ms % kPeriodMs) * (speed / 1000.0f) - distance, spacing);
  if (age < 0.0f) age += spacing;
  if (age >= tail) return 0.0f;
  const float t = 1.0f - age / tail;
  const float wave = t * t;
  return wave * wave * (3.0f - 2.0f * wave);
}
} // namespace powerflow

#include "scaled_panel_flush.hpp"
#include "powerflow_animation.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <random>
#include <vector>

struct Pixel { uint16_t full; };

static void checkRectangle(int x1, int y1, int x2, int y2) {
  const int stride = x2 - x1 + 1;
  std::vector<Pixel> source(stride * (y2 - y1 + 1));
  for (int y = y1; y <= y2; ++y) for (int x = x1; x <= x2; ++x)
    source[(y - y1) * stride + x - x1].full = static_cast<uint16_t>(x * 113 + y * 317);
  constexpr uint16_t sentinel = 0xa55a;
  std::vector<uint16_t> scratch(480 * 40 + 2, sentinel);
  std::vector<int> actual(480 * 480, -1);
  unsigned calls = 0;
  flushScaled720To480(x1, y1, x2, y2, source.data(), scratch.data() + 1, 40,
    [&](int left, int top, uint16_t* pixels, int width, int rows) {
      assert(rows > 0 && rows <= 40 && width > 0 && width <= 480);
      assert(left >= 0 && top >= 0 && left + width <= 480 && top + rows <= 480);
      for (int y = 0; y < rows; ++y) for (int x = 0; x < width; ++x) {
        int& target = actual[(top + y) * 480 + left + x];
        assert(target == -1); // no duplicated output rows or pixels
        target = pixels[y * width + x];
      }
      ++calls;
    });
  assert(scratch.front() == sentinel && scratch.back() == sentinel);
  unsigned expected_pixels = 0;
  // Independent reference: each physical pixel samples floor(physical * 3/2).
  for (int y = 0; y < 480; ++y) for (int x = 0; x < 480; ++x) {
    const int sx = x * 3 / 2, sy = y * 3 / 2;
    const bool inside = sx >= x1 && sx <= x2 && sy >= y1 && sy <= y2;
    const int expected = inside ? static_cast<uint16_t>(sx * 113 + sy * 317) : -1;
    assert(actual[y * 480 + x] == expected);
    expected_pixels += inside;
  }
  assert((calls > 0) == (expected_pixels > 0));
  if (x1 == 263 && y1 == 203 && x2 == 538 && y2 == 306) assert(calls == 2);
}

int main() {
  checkRectangle(263, 203, 538, 306); // actual 104-row LVGL animation chunk: used to be dropped
  checkRectangle(0, 0, 719, 719);
  checkRectangle(719, 0, 719, 719);
  checkRectangle(0, 719, 719, 719);
  // All rounding residues, tiny rectangles, and right/bottom edges.
  for (int y : {0, 1, 2, 201, 202, 203, 717, 718, 719})
    for (int x : {0, 1, 2, 262, 263, 264, 717, 718, 719})
      for (int h = 1; h <= 3; ++h) for (int w = 1; w <= 3; ++w)
        checkRectangle(x, y, std::min(719, x + w - 1), std::min(719, y + h - 1));
  std::mt19937 random(0x4b);
  for (int i = 0; i < 200; ++i) {
    int x1 = random() % 720, x2 = random() % 720;
    int y1 = random() % 720, y2 = random() % 720;
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);
    checkRectangle(x1, y1, x2, y2);
  }
  constexpr float speed = 4.5f * 0.8169308f / 3.984375f;
  for (uint32_t ms = 0; ms < 30000; ms += 33) {
    for (float length : {1.8287096f, 0.6522216f, 0.8169308f, 0.4026279f}) {
      bool illuminated = false;
      for (int i = 0; i <= 100; ++i) {
        const float distance = length * i / 100;
        const float light = powerflow::brightness(ms, distance, speed);
        assert(light >= 0 && light <= 1);
        assert(std::fabs(light - powerflow::brightness(ms + powerflow::kPeriodMs, distance, speed)) < 1e-5f);
        illuminated |= light > 0.001f;
      }
      assert(illuminated); // each branch has visible flow at every sampled time
    }
  }
  // A pulse travels forward along a source->sink route, independent of its length.
  // Sample just behind the sharp head, away from floating-point seam rounding.
  assert(powerflow::brightness(200, speed * 0.19f, speed) > 0.99f);
  assert(powerflow::brightness(400, speed * 0.39f, speed) > 0.99f);
  assert(powerflow::brightness(0, 0, 0) == 0);
  std::puts("[DisplayAnimation] PASS: tall dirty rectangles, full screen, edges, rounding, random ROIs; 30s continuous flow.");
}

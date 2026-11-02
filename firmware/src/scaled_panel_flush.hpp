#pragma once
#include <stdint.h>

// LVGL limits partial draws by pixel count, not by height. A narrow dirty
// rectangle can therefore exceed the scratch buffer's row count after scaling.
// Keep the same global nearest-neighbour mapping for every chunk and ROI.
template <typename Pixel, typename Draw>
void flushScaled720To480(int x1, int y1, int x2, int y2,
                        const Pixel* source, uint16_t* scratch,
                        int max_rows, Draw draw) {
  if (!source || !scratch || max_rows <= 0 || x1 < 0 || y1 < 0 ||
      x2 >= 720 || y2 >= 720 || x2 < x1 || y2 < y1) return;
  const int dx1 = (x1 * 2 + 2) / 3;
  const int dy1 = (y1 * 2 + 2) / 3;
  const int dx2 = ((x2 + 1) * 2 + 2) / 3;
  const int dy2 = ((y2 + 1) * 2 + 2) / 3;
  const int width = dx2 - dx1;
  if (width <= 0) return;
  const int stride = x2 - x1 + 1;
  for (int top = dy1; top < dy2; top += max_rows) {
    const int rows = dy2 - top < max_rows ? dy2 - top : max_rows;
    for (int y = 0; y < rows; ++y) {
      const int sy = ((top + y) * 3) / 2 - y1;
      for (int x = 0; x < width; ++x) {
        const int sx = ((dx1 + x) * 3) / 2 - x1;
        scratch[y * width + x] = source[sy * stride + sx].full;
      }
    }
    draw(dx1, top, scratch, width, rows);
  }
}

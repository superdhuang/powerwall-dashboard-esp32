#pragma once
#include <stdint.h>
#include <stddef.h>

struct StageOffGridXPixel {
  uint16_t x, y;
  uint8_t r, g, b;
};

struct StageSoePixel {
  uint16_t x, y;
  uint8_t dr, dg, db;
  float frac;
};

struct StageWirePixel {
  uint16_t x, y;
  uint8_t wid;
  float u_val;
  int16_t dr, dg, db;
};

struct StageMoonPixel {
  uint16_t x, y;
  uint16_t phase_rgb565[8]; // phases 0..0.875 in eighth-cycle steps; 1.0 wraps to 0
};

struct StageSolarSpan {
  uint16_t x, y, count;
  uint8_t uv_x0, uv_x1, uv_y0, uv_y1;
  uint8_t day_delta0[3], day_delta1[3];
  uint8_t night_delta0[3], night_delta1[3];
};

extern const StageOffGridXPixel kStageOffGridXPixels[];
extern const size_t kStageOffGridXPixelCount;

extern const StageSoePixel kStageSoePixels[];
extern const size_t kStageSoePixelCount;

extern const StageWirePixel kStageWirePixels[];
extern const size_t kStageWirePixelCount;

extern const StageMoonPixel kStageMoonPixels[];
extern const size_t kStageMoonPixelCount;

extern const StageSolarSpan kStageSolarSpans[];
extern const size_t kStageSolarSpanCount;
extern const float kStageSolarUvMinX;
extern const float kStageSolarUvRangeX;
extern const float kStageSolarUvMinY;
extern const float kStageSolarUvRangeY;

bool decodeStage3DFrame(const char* key, uint16_t* dst_rgb565);

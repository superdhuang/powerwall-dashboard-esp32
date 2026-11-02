#pragma once
#include <cstdint>

class TwoWire {
public:
  void begin(int = -1, int = -1, uint32_t = 0) {}
  void beginTransmission(uint8_t) {}
  size_t write(uint8_t) { return 1; }
  uint8_t endTransmission(bool = true) { return 2; } // NACK on host (touch handled by SDL2 mouse)
  uint8_t requestFrom(uint8_t, uint8_t) { return 0; }
  int read() { return 0; }
};

static inline TwoWire Wire;

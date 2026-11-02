#pragma once
#include "Arduino.h"

class WiFiClientSecure {
public:
  void setInsecure() {}
  void setTimeout(uint32_t seconds) { timeout_s_ = seconds; }
  uint32_t timeoutSeconds() const { return timeout_s_; }
private:
  uint32_t timeout_s_ = 4;
};

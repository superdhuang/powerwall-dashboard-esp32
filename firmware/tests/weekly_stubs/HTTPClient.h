#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>

// Deterministic Gateway responses: these tests never contact a network or use credentials.
class HTTPClient {
public:
  inline static bool fail = false;
  void setConnectTimeout(int32_t) {}
  void setTimeout(uint16_t) {}
  bool begin(WiFiClientSecure&, const String&) { return true; }
  void addHeader(const String&, const String&) {}
  void collectHeaders(const char*[], size_t) {}
  int GET() { return fail ? -1 : 200; }
  int POST(const String&) { return fail ? -1 : 200; }
  String header(const char*) const { return ""; }
  String getString() const {
    return R"({"token":"fixture","percentage":50,"load":{"instant_power":2000},"site":{"instant_power":3000},"battery":{"instant_power":0},"nominal_full_pack_energy":10000,"nominal_energy_remaining":5000,"grid_status":"SystemGridConnected"})";
  }
  void end() {}
};

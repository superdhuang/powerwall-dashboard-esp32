#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

unsigned long millis(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <string>
#include <chrono>
#include <thread>
#include <atomic>

class String : public std::string {
public:
  String() : std::string() {}
  String(const char* s) : std::string(s ? s : "") {}
  String(const std::string& s) : std::string(s) {}
  explicit String(int v) : std::string(std::to_string(v)) {}
  explicit String(unsigned int v) : std::string(std::to_string(v)) {}
  explicit String(long v) : std::string(std::to_string(v)) {}
  explicit String(unsigned long v) : std::string(std::to_string(v)) {}

  int indexOf(char c) const {
    auto pos = find(c);
    return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
  }

  bool startsWith(const char* prefix) const {
    if (!prefix) return false;
    return rfind(prefix, 0) == 0;
  }

  String substring(size_t from) const {
    if (from >= size()) return String("");
    return String(substr(from));
  }

  String substring(size_t from, size_t to) const {
    if (from >= size() || to <= from) return String("");
    return String(substr(from, to - from));
  }

  String operator+(const String& rhs) const {
    return String(static_cast<const std::string&>(*this) + static_cast<const std::string&>(rhs));
  }

  String operator+(const char* rhs) const {
    return String(static_cast<const std::string&>(*this) + (rhs ? rhs : ""));
  }

  friend String operator+(const char* lhs, const String& rhs) {
    return String(std::string(lhs ? lhs : "") + static_cast<const std::string&>(rhs));
  }

  size_t write(uint8_t c) {
    push_back(static_cast<char>(c));
    return 1;
  }

  size_t write(const uint8_t* s, size_t n) {
    if (s && n > 0) {
      append(reinterpret_cast<const char*>(s), n);
      return n;
    }
    return 0;
  }
};

inline std::atomic<unsigned long>& simMillisOffset() {
  static std::atomic<unsigned long> offset_ms{0};
  return offset_ms;
}

inline bool& simNtpSynced() {
  static bool synced = true;
  return synced;
}

inline void delay(unsigned long ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

inline bool getLocalTime(struct tm* info, uint32_t /*ms*/ = 5000) {
  if (!info || !simNtpSynced()) return false;
  time_t now = time(nullptr);
  localtime_r(&now, info);
  return true;
}

struct SerialMock {
  void begin(unsigned long) {}
  void println(const char* s = "") { std::printf("%s\n", s ? s : ""); std::fflush(stdout); }
  void println(const String& s) { std::printf("%s\n", s.c_str()); std::fflush(stdout); }
  void printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    std::vprintf(fmt, args);
    va_end(args);
    std::fflush(stdout);
  }
};

static inline SerialMock Serial;
#endif

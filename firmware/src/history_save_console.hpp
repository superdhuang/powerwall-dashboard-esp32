#pragma once

#include <Arduino.h>
#include <atomic>
#include <time.h>

inline bool isHistorySaveKey(char key) { return key == '6'; }
inline const char* historySaveConsoleHelpText() {
  return "[history] 6 = force daily + weekly history writes (no Enter); watch the display and BEGIN/END timestamps";
}

// UI/Serial submits requests; only the history owner performs file I/O.
// Remain busy until both writes finish so a burst of keys cannot queue more writes.
class HistorySaveRequest {
public:
  bool request() {
    State expected = State::Idle;
    return state_.compare_exchange_strong(expected, State::Pending);
  }
  bool take() {
    State expected = State::Pending;
    return state_.compare_exchange_strong(expected, State::Writing);
  }
  void finish() { state_.store(State::Idle); }

private:
  enum class State : uint8_t { Idle, Pending, Writing };
  std::atomic<State> state_{State::Idle};
};

template <typename Output>
inline void logHistorySaveEvent(Output& out, const char* target, const char* source,
                                const char* event, unsigned long elapsedMs = 0) {
  const time_t now = time(nullptr);
  tm local{};
  char timestamp[32] = "clock-unsynced";
  if (localtime_r(&now, &local) && local.tm_year >= 120)
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &local);
  out.printf("[history] %s uptime_ms=%lu source=%s target=%s %s elapsed_ms=%lu\n",
             timestamp, millis(), source, target, event, elapsedMs);
}

template <typename Output, typename Save>
inline bool runHistorySave(Output& out, const char* target, const char* source, Save save) {
#if defined(POWERWALL_NATIVE480_RELEASE)
  // Keep the normal writes, but omit development timing traces from release images.
  (void)out;
  (void)target;
  (void)source;
  return save();
#else
  logHistorySaveEvent(out, target, source, "BEGIN");
  const unsigned long started = millis();
  const bool ok = save();
  logHistorySaveEvent(out, target, source, ok ? "END OK" : "END FAILED", millis() - started);
  return ok;
#endif
}

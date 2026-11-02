#pragma once

#include "weekly_history_store.hpp"

// Owned only by the polling task. UI snapshots are copied under the application's mutex.
class WeeklyHistoryRecorder {
public:
  // Call before AND after polling: recordHistoryPoint may discard yesterday at midnight.
  void update(const PowerwallLocalClient& client, time_t now) {
    if (!WeeklyHistory::validTime(now)) return;
    if (!ready_) {
      store_.load(history_, now);
      ready_ = true;
    }
    history_.roll(now);
    char date[11]{};
    if (!client.historyDate(date)) return;
    tm today{}; localtime_r(&now, &today);
    char todayDate[11]{}; strftime(todayDate, sizeof(todayDate), "%Y-%m-%d", &today);
    // HTTPS can start before midnight and finish after the daily client has reset.
    // Keep the preceding live bucket independently until its date has been finalized.
    if (hasPending_ && strcmp(pendingDate_, date)) history_.merge(pendingDate_, &pendingPoint_, 1);
    // Preserve yesterday's final partial window before the daily client resets it.
    const size_t count = strcmp(date, todayDate) ? client.history_count : client.completedHistoryCount();
    history_.merge(date, client.history, count);
    hasPending_ = !strcmp(date, todayDate) && client.history_count > client.completedHistoryCount();
    if (hasPending_) {
      memcpy(pendingDate_, date, sizeof(pendingDate_));
      pendingPoint_ = client.history[client.history_count - 1];
    }
  }

  bool ready() const { return ready_; }
  bool needsSave() const { return ready_ && savedRevision_ != history_.revision; }

  bool flush(bool force = false) {
    if (!ready_) return !force;
    if (!force && !needsSave()) return true;
    if (!store_.save(history_)) return false;
    savedRevision_ = history_.revision;
    return true;
  }

  void snapshot(WeeklyHistory& output, const PowerwallLocalClient& client) const {
    output = history_;
    char date[11]{};
    if (ready_ && client.historyDate(date)) output.merge(date, client.history, client.history_count);
  }

private:
  WeeklyHistory history_;
  WeeklyHistoryStore store_;
  uint32_t savedRevision_ = 0;
  bool ready_ = false;
  char pendingDate_[11]{};
  HistoryPoint pendingPoint_{};
  bool hasPending_ = false;
};

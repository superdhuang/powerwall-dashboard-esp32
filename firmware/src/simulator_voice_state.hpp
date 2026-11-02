#pragma once

#include <cstdint>

enum class SimulatorVoiceState : uint8_t {
  Unknown,
  Idle,
  Charging,
  Discharging,
  OffGrid,
};

// Pure transition detector so simulator voice announcements can be checked
// without opening an audio device.  The first settled state is always silent.
class SimulatorVoiceStateTracker {
 public:
  static constexpr float kPowerDeadbandW = 200.0f;

  SimulatorVoiceState observe(bool valid, bool has_grid_status,
                              bool grid_connected, bool has_battery_power,
                              float battery_power_w) {
    if (!valid) {
      candidate_ = SimulatorVoiceState::Unknown;
      candidate_samples_ = 0;
      return SimulatorVoiceState::Unknown;
    }

    if (has_grid_status && !grid_connected) {
      candidate_ = SimulatorVoiceState::OffGrid;
      candidate_samples_ = 0;
      return commit(candidate_);
    }
    if (!has_battery_power) {
      candidate_ = SimulatorVoiceState::Unknown;
      candidate_samples_ = 0;
      return SimulatorVoiceState::Unknown;
    }

    SimulatorVoiceState next = SimulatorVoiceState::Idle;
    if (battery_power_w < -kPowerDeadbandW) next = SimulatorVoiceState::Charging;
    else if (battery_power_w > kPowerDeadbandW) next = SimulatorVoiceState::Discharging;

    if (next == SimulatorVoiceState::Idle) {
      candidate_ = next;
      candidate_samples_ = 0;
      return commit(next);
    }

    if (candidate_ != next) {
      candidate_ = next;
      candidate_samples_ = 1;
      return SimulatorVoiceState::Unknown;
    }
    if (candidate_samples_ < 2) ++candidate_samples_;
    if (candidate_samples_ < 2) return SimulatorVoiceState::Unknown;
    return commit(next);
  }

  SimulatorVoiceState state() const { return state_; }
  bool initialized() const { return initialized_; }

 private:
  SimulatorVoiceState commit(SimulatorVoiceState next) {
    if (!initialized_) {
      state_ = next;
      initialized_ = true;
      return SimulatorVoiceState::Unknown;
    }
    if (state_ == next) return SimulatorVoiceState::Unknown;
    state_ = next;
    return next;
  }

  SimulatorVoiceState state_ = SimulatorVoiceState::Unknown;
  SimulatorVoiceState candidate_ = SimulatorVoiceState::Unknown;
  uint8_t candidate_samples_ = 0;
  bool initialized_ = false;
};

inline const char* simulatorVoicePhrase(SimulatorVoiceState state) {
  switch (state) {
#if defined(POWERWALL_NATIVE480_LANG_EN)
    case SimulatorVoiceState::Charging: return "Battery charging";
    case SimulatorVoiceState::Discharging: return "Battery discharging";
    case SimulatorVoiceState::OffGrid: return "Grid disconnected";
#else
    case SimulatorVoiceState::Charging: return "充電中";
    case SimulatorVoiceState::Discharging: return "放電中";
    case SimulatorVoiceState::OffGrid: return "電網已斷開";
#endif
    default: return nullptr;
  }
}

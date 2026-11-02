#pragma once

#include <stdint.h>

// GPIO0 is active-low; the 4B PWRKEY is active-high on expander EXIO4.
// Feed already-normalized pressed states every ~20 ms. Actions occur on release.
class ButtonNavigation {
 public:
  enum class Action : uint8_t { None, NextPage, Settings };

  Action update(uint32_t now_ms, bool power_pressed, bool boot_pressed) {
    if (!initialized_) {
      initialized_ = true;
      power_.reset(now_ms, power_pressed);
      boot_.reset(now_ms, boot_pressed);
      suppress_until_released_ = power_pressed || boot_pressed;
      return Action::None;
    }

    const bool power_released = power_.update(now_ms, power_pressed);
    const bool boot_released = boot_.update(now_ms, boot_pressed);
    const bool any_pressed = power_.stable || boot_.stable;
    const bool both_pressed = power_.stable && boot_.stable;

    if (suppress_until_released_) {
      if (!any_pressed) suppress_until_released_ = false;
      return Action::None;
    }

    if (both_pressed) {
      // PWRKEY also controls the PMIC and BOOT is a strapping pin. Never
      // restart from a two-button chord; suppress both single-button actions.
      chord_seen_ = true;
      return Action::None;
    }

    if (chord_seen_) {
      if (!any_pressed) chord_seen_ = false;
      return Action::None;
    }

    if (power_released && !boot_.stable) return Action::NextPage;
    if (boot_released && !power_.stable) return Action::Settings;
    return Action::None;
  }

 private:
  struct DebouncedButton {
    bool stable = false;
    bool candidate = false;
    uint32_t candidate_since_ms = 0;

    void reset(uint32_t now_ms, bool pressed) {
      stable = candidate = pressed;
      candidate_since_ms = now_ms;
    }

    bool update(uint32_t now_ms, bool pressed) {
      if (pressed != candidate) {
        candidate = pressed;
        candidate_since_ms = now_ms;
      }
      if (candidate == stable || static_cast<uint32_t>(now_ms - candidate_since_ms) < 30)
        return false;
      const bool was_pressed = stable;
      stable = candidate;
      return was_pressed && !stable;
    }
  };

  DebouncedButton power_, boot_;
  bool initialized_ = false;
  bool suppress_until_released_ = false;
  bool chord_seen_ = false;
};

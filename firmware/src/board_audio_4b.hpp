#pragma once

#include <Arduino.h>

class BoardAudio4B {
 public:
  using AmpEnableFn = bool (*)(bool enabled);
  enum class VoicePhrase : uint8_t { Charging = 1, Discharging = 2, OffGrid = 3 };

// Initializes the 4B ES8311 playback path. The callback is owned by the
// display driver and controls TCA9554 EXIO3 without transferring expander
// ownership to this module.
  bool begin(AmpEnableFn amp_enable);

// ES8311 output volume in the inclusive range 0..100.
  bool setVolume(uint8_t percent);
  uint8_t volume();

// Queue a short 440 Hz tone. Playback runs on a worker task, so this returns
// immediately. The duration is capped at one second.
  bool playTestTone(uint16_t duration_ms = 700);
  bool playVoice(VoicePhrase phrase);
  bool playTestVoice();

// Call from the main loop to complete deferred amp shutdown after playback.
  void update();
  void stop();
  bool isReady();
};

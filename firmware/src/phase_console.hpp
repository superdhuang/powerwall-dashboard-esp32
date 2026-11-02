#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum class PhaseCommandKind : uint8_t {
  Invalid,
  Live,
  SunNext,
  MoonNext,
  WeatherNext,
  Help,
};

struct PhaseConsoleCommand {
  PhaseCommandKind kind = PhaseCommandKind::Invalid;
  float value = 0.0f;
  char preset_key = '\0';
};

static constexpr size_t kPhaseConsoleMaxLineLength = 63;

inline char normalizePhaseConsolePresetKey(char key) {
  if (key >= 'A' && key <= 'Z') return static_cast<char>(key - 'A' + 'a');
  return key;
}

inline bool isPhaseConsolePresetKey(char key) {
  key = normalizePhaseConsolePresetKey(key);
  return key == 's' || key == 'm' || key == 'w' || key == 'z' || key == 'h';
}

inline const char* phaseConsoleHelpText() {
  return "[phase] Serial @115200: s = next sun phase (10..90); m = next moon phase; w = next weather; z = LIVE; h = help\n"
         "[phase] Moon: 新月, 眉月, 上弦月, 盈凸月, 滿月, 虧凸月, 下弦月, 殘月\n"
         "[phase] Weather: 晴天, 多雲, 陰天, 下雨, 強雨／閃電, 下雪／結霜, 強風, 日出, 日落, 夜晚／月亮";
}

inline int phaseConsoleSunPercent(uint8_t index) {
  return static_cast<int>(index % 9 + 1) * 10;
}

inline float phaseConsoleMoonPercent(uint8_t index) {
  return static_cast<float>(index % 8) * 12.5f;
}

inline const char* phaseConsoleMoonName(float phase_percent) {
  const float phase = phase_percent / 100.0f;
  if (phase < 0.03f || phase >= 0.97f) return "🌑 新月";
  if (phase < 0.22f) return "🌒 眉月";
  if (phase < 0.28f) return "🌓 上弦月";
  if (phase < 0.47f) return "🌔 盈凸月";
  if (phase < 0.53f) return "🌕 滿月";
  if (phase < 0.72f) return "🌖 虧凸月";
  if (phase < 0.78f) return "🌗 下弦月";
  return "🌘 殘月";
}

inline const char* phaseConsolePresetName(char key) {
  key = normalizePhaseConsolePresetKey(key);
  if (key == 's') return "sun-cycle";
  if (key == 'm') return "moon-cycle";
  if (key == 'w') return "weather-cycle";
  if (key == 'z') return "LIVE";
  return "none";
}

inline bool parsePhaseConsolePreset(char key, PhaseConsoleCommand& command) {
  key = normalizePhaseConsolePresetKey(key);
  if (!isPhaseConsolePresetKey(key)) return false;
  command.preset_key = key;
  if (key == 's') command.kind = PhaseCommandKind::SunNext;
  else if (key == 'm') command.kind = PhaseCommandKind::MoonNext;
  else if (key == 'w') command.kind = PhaseCommandKind::WeatherNext;
  else if (key == 'h') command.kind = PhaseCommandKind::Help;
  else command.kind = PhaseCommandKind::Live;
  return true;
}

inline bool parsePhaseConsoleCommand(const char* line, PhaseConsoleCommand& command) {
  command = {};
  if (!line) return false;

  char tokens[3][16] = {};
  size_t token_count = 0;
  const char* cursor = line;
  while (*cursor) {
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r') ++cursor;
    if (!*cursor) break;
    if (token_count >= 3) return false;
    size_t token_length = 0;
    while (*cursor && *cursor != ' ' && *cursor != '\t' && *cursor != '\r') {
      if (token_length + 1 >= sizeof(tokens[0])) return false;
      tokens[token_count][token_length++] = *cursor++;
    }
    tokens[token_count][token_length] = '\0';
    ++token_count;
  }

  if (token_count == 1 && tokens[0][0] != '\0' && tokens[0][1] == '\0' &&
      parsePhaseConsolePreset(tokens[0][0], command)) {
    return true;
  }

  return false;
}

enum class PhaseConsoleLineResult : uint8_t { None, Ready, TooLong };

class PhaseConsoleLineBuffer {
public:
  bool empty() const { return length_ == 0 && !overflow_; }

  PhaseConsoleLineResult push(char input, char* output, size_t output_capacity) {
    if (input == '\n' || input == '\r') {
      if (overflow_) {
        reset();
        return PhaseConsoleLineResult::TooLong;
      }
      if (length_ == 0) return PhaseConsoleLineResult::None;
      if (!output || output_capacity <= length_) {
        reset();
        return PhaseConsoleLineResult::TooLong;
      }
      memcpy(output, buffer_, length_);
      output[length_] = '\0';
      reset();
      return PhaseConsoleLineResult::Ready;
    }
    if (overflow_) return PhaseConsoleLineResult::None;
    if (length_ >= kPhaseConsoleMaxLineLength) {
      overflow_ = true;
      return PhaseConsoleLineResult::None;
    }
    buffer_[length_++] = input;
    return PhaseConsoleLineResult::None;
  }

private:
  char buffer_[kPhaseConsoleMaxLineLength + 1] = {};
  size_t length_ = 0;
  bool overflow_ = false;

  void reset() {
    length_ = 0;
    overflow_ = false;
  }
};

struct GodotMoonPhaseParams {
  float phase = 0.0f;
  bool is_crescent = true;
};

// Port of Night.gd set_moon_phase() used by the committed 3D preview.
inline GodotMoonPhaseParams mapGodotMoonPhase(float astronomical_phase) {
  float p = astronomical_phase - floorf(astronomical_phase);
  float remapped = 0.0f;
  bool is_crescent = true;
  if (p <= 0.25f) {
    remapped = p / 0.25f * 0.5f;
  } else if (p > 0.75f) {
    remapped = 0.5f + (p - 0.75f) / 0.25f * 0.5f;
  } else if (p < 0.5f) {
    remapped = 0.5f + (p - 0.25f) / 0.25f * 0.5f;
    is_crescent = false;
  } else {
    remapped = (p - 0.5f) / 0.25f * 0.5f;
    is_crescent = false;
  }
  return {1.0f - remapped, is_crescent};
}

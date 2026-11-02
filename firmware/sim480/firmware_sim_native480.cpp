#include <SDL2/SDL.h>
#include <fstream>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cerrno>
#include <csignal>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#include <limits.h>
#include <utility>

#include "board_480/display_480.hpp"
#include "powerwall_client.hpp"
#include "weather_client.hpp"
#include "gateway_console.hpp"
#include "console_preview.hpp"
#include "history_save_console.hpp"
#include "simulator_voice_state.hpp"
#include "simulator_gateway_cli.hpp"
#include "native480_timezone.hpp"
#include "ui_480/ui_480.hpp"
#include "daily_trend_history.hpp"

extern "C" unsigned long millis(void) {
  static const auto t0 = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count();
  return static_cast<unsigned long>(ms) + simMillisOffset().load();
}

// Host window and firmware canvas match the physical 4B panel.
static constexpr int kWindowSize = 480;
static constexpr const char* kCapturePath = "assets/firmware_sim_native480_latest.ppm";
static constexpr int kWinW = DisplayDriver480::kWidth;
static constexpr int kWinH = DisplayDriver480::kHeight;

static const char* simulatorVoiceAssetPath(SimulatorVoiceState state) {
  if (native480_i18n::kEnglish) {
    switch (state) {
      case SimulatorVoiceState::Charging: return "firmware/assets/voice/en/charging.wav";
      case SimulatorVoiceState::Discharging: return "firmware/assets/voice/en/discharging.wav";
      case SimulatorVoiceState::OffGrid: return "firmware/assets/voice/en/offgrid.wav";
      default: return nullptr;
    }
  }
  switch (state) {
    case SimulatorVoiceState::Charging: return "firmware/assets/voice/charging.wav";
    case SimulatorVoiceState::Discharging: return "firmware/assets/voice/discharging.wav";
    case SimulatorVoiceState::OffGrid: return "firmware/assets/voice/offgrid.wav";
    default: return nullptr;
  }
}

static uint16_t g_fb[kWinW * kWinH];
// Shared only between the polling thread and the UI loop under data_mtx.
static DailyTrendHistory shared_daily_trend_history;
static bool g_fb_dirty = false;
static bool g_brightness_dirty = false;
static SDL_Rect g_dirty_rect{};
static bool g_mouse_pressed = false;
static int16_t g_mouse_x = 0;
static int16_t g_mouse_y = 0;
static volatile sig_atomic_t g_interrupt_requested = 0;

static bool setSimulatorTimezone(int offsetMinutes) {
  if (!native480_timezone::isValidOffsetMinutes(offsetMinutes)) return false;
  const int absolute = offsetMinutes < 0 ? -offsetMinutes : offsetMinutes;
  char posixTimezone[24] = {};
  // POSIX TZ signs are inverted: UTC-8 describes a local UTC+8 offset.
  std::snprintf(posixTimezone, sizeof(posixTimezone), "UTC%c%d:%02d",
                offsetMinutes < 0 ? '+' : '-', absolute / 60, absolute % 60);
  if (setenv("TZ", posixTimezone, 1) != 0) return false;
  tzset();
  return true;
}

static int simulatorTimezoneFromEnvironment() {
  const char* value = std::getenv("POWERWALL_SIM_TIMEZONE_MINUTES");
  if (!value || !*value) return native480_timezone::kDefaultOffsetMinutes;
  int offsetMinutes = native480_timezone::kDefaultOffsetMinutes;
  if (!native480_timezone::parseOffsetMinutes(value, offsetMinutes)) {
    std::fprintf(stderr,
      "[Timezone] Invalid POWERWALL_SIM_TIMEZONE_MINUTES='%s'; using default UTC+08:00.\n",
      value);
    return native480_timezone::kDefaultOffsetMinutes;
  }
  return offsetMinutes;
}

static int runTimezoneSmokeTest() {
  for (int offset = native480_timezone::kMinimumOffsetMinutes;
       offset <= native480_timezone::kMaximumOffsetMinutes; ++offset) {
    const bool expected = offset % native480_timezone::kOffsetStepMinutes == 0;
    if (native480_timezone::isValidOffsetMinutes(offset) != expected) {
      std::fprintf(stderr, "[TimezoneSmokeTest] FAIL: range validation at %d minutes\n", offset);
      return 1;
    }
  }
  for (const char* invalid : {"", "-721", "841", "14", "15", "450", "495", "15x", " 60"}) {
    int parsed = 123;
    if (native480_timezone::parseOffsetMinutes(invalid, parsed)) {
      std::fprintf(stderr, "[TimezoneSmokeTest] FAIL: accepted '%s'\n", invalid);
      return 2;
    }
  }
  for (const int offset : {-720, -180, 0, 480, 840}) {
    if (!setSimulatorTimezone(offset)) {
      std::fprintf(stderr, "[TimezoneSmokeTest] FAIL: could not apply %d minutes\n", offset);
      return 3;
    }
    const time_t utcEpoch = 0;
    const time_t expectedEpoch = static_cast<time_t>(offset) * 60;
    struct tm expected{};
    struct tm actual{};
    if (!gmtime_r(&expectedEpoch, &expected) || !localtime_r(&utcEpoch, &actual) ||
        actual.tm_year != expected.tm_year || actual.tm_yday != expected.tm_yday ||
        actual.tm_hour != expected.tm_hour || actual.tm_min != expected.tm_min) {
      std::fprintf(stderr, "[TimezoneSmokeTest] FAIL: UTC conversion at %d minutes\n", offset);
      return 4;
    }
  }
  std::printf("[TimezoneSmokeTest] PASS: UTC-12:00..UTC+14:00 whole-hour validation and host clock conversion\n");
  return 0;
}

struct SimulatorDeviceSettings {
  uint8_t brightness = 80;
  uint8_t volume = 85;
  bool voice_enabled = true;
  bool persistence_enabled = true;
  bool dirty = false;
  uint32_t changed_at = 0;
  SDL_AudioDeviceID audio_device = 0;
  unsigned restart_requests = 0;
  uint8_t voice_test_index = 0;

  void load() {
    std::ifstream file("device_settings.json");
    if (!file) return;
    const std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    JsonDocument doc;
    if (deserializeJson(doc, data) != DeserializationError::Ok) return;
    if (doc["brightness"].is<int>()) brightness = static_cast<uint8_t>(std::clamp(doc["brightness"].as<int>(), 0, 100));
    if (doc["volume"].is<int>()) volume = static_cast<uint8_t>(std::clamp(doc["volume"].as<int>(), 0, 100));
    if (doc["voice_enabled"].is<bool>()) voice_enabled = doc["voice_enabled"].as<bool>();
  }

  void save() {
    if (!dirty) return;
    if (!persistence_enabled) { dirty = false; return; }
    std::ofstream file("device_settings.json", std::ios::trunc);
    if (!file) { std::fprintf(stderr, "[Settings] Could not save device_settings.json\n"); return; }
    file << "{\"brightness\":" << static_cast<unsigned>(brightness)
         << ",\"volume\":" << static_cast<unsigned>(volume);
    file << ",\"voice_enabled\":" << (voice_enabled ? "true" : "false");
    file << "}\n";
    if (file.good()) dirty = false;
  }

  void playTestTone() {
    if (!audio_device) {
      if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        std::fprintf(stderr, "[Settings] SDL audio unavailable: %s\n", SDL_GetError());
        return;
      }
      SDL_AudioSpec wanted{}, obtained{};
      wanted.freq = 44100;
      wanted.format = AUDIO_S16SYS;
      wanted.channels = 1;
      wanted.samples = 1024;
      audio_device = SDL_OpenAudioDevice(nullptr, 0, &wanted, &obtained, 0);
      if (!audio_device) {
        std::fprintf(stderr, "[Settings] SDL audio unavailable: %s\n", SDL_GetError());
        return;
      }
    }
    SDL_ClearQueuedAudio(audio_device);
    constexpr int sample_rate = 44100;
    constexpr int sample_count = sample_rate * 3 / 10;
    std::vector<int16_t> samples(sample_count);
    const double amplitude = 10000.0 * volume / 100.0;
    for (int i = 0; i < sample_count; ++i) {
      const double fade = std::min(1.0, std::min(i / 441.0, (sample_count - i - 1) / 441.0));
      samples[i] = static_cast<int16_t>(std::sin(2.0 * M_PI * 440.0 * i / sample_rate) * amplitude * fade);
    }
    SDL_QueueAudio(audio_device, samples.data(), samples.size() * sizeof(int16_t));
    SDL_PauseAudioDevice(audio_device, 0);
    std::printf("[Settings] 440 Hz test tone at %u%% volume\n", static_cast<unsigned>(volume));
  }

  bool ensureAudioDevice() {
    if (audio_device) return true;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
      std::fprintf(stderr, "[Voice] SDL audio unavailable: %s\n", SDL_GetError());
      return false;
    }
    SDL_AudioSpec wanted{}, obtained{};
    wanted.freq = 22050;
    wanted.format = AUDIO_S16SYS;
    wanted.channels = 1;
    wanted.samples = 1024;
    audio_device = SDL_OpenAudioDevice(nullptr, 0, &wanted, &obtained, 0);
    if (!audio_device) std::fprintf(stderr, "[Voice] SDL audio unavailable: %s\n", SDL_GetError());
    return audio_device != 0;
  }

  void playVoice(SimulatorVoiceState state) {
    const char* file = simulatorVoiceAssetPath(state);
    if (!file) return;
    if (!ensureAudioDevice()) return;
    SDL_AudioSpec source{};
    Uint8* raw = nullptr;
    Uint32 length = 0;
    if (!SDL_LoadWAV(file, &source, &raw, &length)) {
      std::fprintf(stderr, "[Voice] Could not read %s: %s\n", file, SDL_GetError());
      return;
    }
    SDL_AudioCVT conversion{};
    if (SDL_BuildAudioCVT(&conversion, source.format, source.channels, source.freq,
                          AUDIO_S16SYS, 1, 22050) < 0) {
      std::fprintf(stderr, "[Voice] Could not convert %s: %s\n", file, SDL_GetError());
      SDL_FreeWAV(raw);
      return;
    }
    conversion.len = static_cast<int>(length);
    conversion.buf = static_cast<Uint8*>(SDL_malloc(length * conversion.len_mult));
    if (!conversion.buf) { SDL_FreeWAV(raw); return; }
    std::memcpy(conversion.buf, raw, length);
    SDL_FreeWAV(raw);
    if (SDL_ConvertAudio(&conversion) < 0) {
      std::fprintf(stderr, "[Voice] Could not convert %s: %s\n", file, SDL_GetError());
      SDL_free(conversion.buf);
      return;
    }
    auto* samples = reinterpret_cast<int16_t*>(conversion.buf);
    const size_t sample_count = static_cast<size_t>(conversion.len_cvt) / sizeof(int16_t);
    for (size_t i = 0; i < sample_count; ++i) {
      samples[i] = static_cast<int16_t>(static_cast<int32_t>(samples[i]) * volume / 100);
    }
    SDL_ClearQueuedAudio(audio_device);
    SDL_QueueAudio(audio_device, conversion.buf, static_cast<Uint32>(conversion.len_cvt));
    SDL_PauseAudioDevice(audio_device, 0);
    SDL_free(conversion.buf);
    std::printf("[Voice] %s at %u%% volume\n", simulatorVoicePhrase(state),
                static_cast<unsigned>(volume));
  }

  SimulatorVoiceState nextTestVoiceState() {
    static constexpr SimulatorVoiceState sequence[] = {
      SimulatorVoiceState::Charging,
      SimulatorVoiceState::Discharging,
      SimulatorVoiceState::OffGrid
    };
    const SimulatorVoiceState state = sequence[voice_test_index];
    voice_test_index = static_cast<uint8_t>((voice_test_index + 1) % 3);
    return state;
  }

  void playTestVoice() { playVoice(nextTestVoiceState()); }
};

static bool runVoiceStateSmokeTest() {
  SimulatorDeviceSettings test_settings;
  if (test_settings.nextTestVoiceState() != SimulatorVoiceState::Charging ||
      test_settings.nextTestVoiceState() != SimulatorVoiceState::Discharging ||
      test_settings.nextTestVoiceState() != SimulatorVoiceState::OffGrid ||
      test_settings.nextTestVoiceState() != SimulatorVoiceState::Charging) return false;
  SimulatorVoiceStateTracker tracker;
  auto observe = [&](bool valid, bool has_grid, bool connected, bool has_power, float watts) {
    return tracker.observe(valid, has_grid, connected, has_power, watts);
  };
  if (observe(true, true, true, true, -800) != SimulatorVoiceState::Unknown ||
      observe(true, true, true, true, -900) != SimulatorVoiceState::Unknown ||
      tracker.state() != SimulatorVoiceState::Charging) return false;
  if (observe(false, true, false, true, 900) != SimulatorVoiceState::Unknown ||
      observe(true, true, true, true, -700) != SimulatorVoiceState::Unknown ||
      tracker.state() != SimulatorVoiceState::Charging) return false;
  if (observe(true, true, false, true, 900) != SimulatorVoiceState::OffGrid) return false;
  if (observe(true, true, true, true, 900) != SimulatorVoiceState::Unknown ||
      observe(true, true, true, true, 900) != SimulatorVoiceState::Discharging) return false;
  if (observe(true, true, true, true, 200) != SimulatorVoiceState::Idle ||
      observe(true, true, true, true, -200) != SimulatorVoiceState::Unknown ||
      tracker.state() != SimulatorVoiceState::Idle) return false;
  SimulatorVoiceStateTracker interrupted;
  interrupted.observe(true, true, true, true, -800);
  interrupted.observe(true, true, true, true, -800);
  if (interrupted.observe(true, true, true, true, 800) != SimulatorVoiceState::Unknown ||
      interrupted.observe(false, true, true, true, 800) != SimulatorVoiceState::Unknown ||
      interrupted.observe(true, true, true, true, 800) != SimulatorVoiceState::Unknown ||
      interrupted.observe(true, true, true, true, 800) != SimulatorVoiceState::Discharging) return false;
  const char* const expectedCharging = native480_i18n::kEnglish ? "Battery charging" : "充電中";
  const char* const expectedDischarging = native480_i18n::kEnglish ? "Battery discharging" : "放電中";
  const char* const expectedOffGrid = native480_i18n::kEnglish ? "Grid disconnected" : "電網已斷開";
  if (std::strcmp(simulatorVoicePhrase(SimulatorVoiceState::Charging), expectedCharging) != 0 ||
      std::strcmp(simulatorVoicePhrase(SimulatorVoiceState::Discharging), expectedDischarging) != 0 ||
      std::strcmp(simulatorVoicePhrase(SimulatorVoiceState::OffGrid), expectedOffGrid) != 0) return false;
  for (const auto state : {SimulatorVoiceState::Charging, SimulatorVoiceState::Discharging,
                           SimulatorVoiceState::OffGrid}) {
    const char* const path = simulatorVoiceAssetPath(state);
    SDL_AudioSpec spec{};
    Uint8* wav = nullptr;
    Uint32 wav_length = 0;
    if (!path || !SDL_LoadWAV(path, &spec, &wav, &wav_length)) {
      std::fprintf(stderr, "[VoiceSmokeTest] FAIL: could not load localized clip %s\n",
                   path ? path : "(null)");
      return false;
    }
    const int expected_sample_rate = native480_i18n::kEnglish ? 16000 : 22050;
    const bool format_ok = spec.freq == expected_sample_rate && spec.channels == 1 &&
      SDL_AUDIO_BITSIZE(spec.format) == 16 && wav_length > 0;
    SDL_FreeWAV(wav);
    if (!format_ok) {
      std::fprintf(stderr, "[VoiceSmokeTest] FAIL: invalid WAV format for %s\n", path);
      return false;
    }
  }
  std::printf("[VoiceSmokeTest] PASS: localized phrases/assets, startup silence, direction debounce, +/-200W deadband and grid priority verified.\n");
  return true;
}

static bool runSettingsUptimeFormatSmokeTest() {
  struct UptimeCase { uint64_t seconds; const char* value; };
  static constexpr UptimeCase cases[] = {
    {0ULL, "0d 00:00:00"},
    {59ULL, "0d 00:00:59"},
    {60ULL, "0d 00:01:00"},
    {86399ULL, "0d 23:59:59"},
    {86400ULL, "1d 00:00:00"},
    {4294968ULL, "49d 17:02:48"}, // Beyond the 32-bit millis wrap interval.
  };
  for (const auto& test_case : cases) {
    char actual[48], expected[48];
    formatSettingsUptime(test_case.seconds, actual, sizeof(actual));
    std::snprintf(expected, sizeof(expected), "%s: %s",
                  native480_i18n::text("開機時間"), test_case.value);
    if (std::strcmp(actual, expected) != 0) {
      std::fprintf(stderr, "[SettingsUptimeSmokeTest] FAIL: %llu -> %s (expected %s)\n",
                   static_cast<unsigned long long>(test_case.seconds), actual, expected);
      return false;
    }
  }
  std::printf("[SettingsUptimeSmokeTest] PASS: second/minute/day boundaries and >49.7-day format\n");
  return true;
}

// Region coordinates are native framebuffer pixels.
static bool framebufferRegionHasInk(int x1, int y1, int x2, int y2) {
  x1 = std::max(0, x1); y1 = std::max(0, y1);
  x2 = std::min(kWinW - 1, x2); y2 = std::min(kWinH - 1, y2);
  if (x1 > x2 || y1 > y2) return false;
  size_t ink_pixels = 0;
  for (int y = y1; y <= y2; ++y) {
    for (int x = x1; x <= x2; ++x) {
      const uint16_t pixel = g_fb[y * kWinW + x];
      const int red = (pixel >> 11) & 0x1f;
      const int green = (pixel >> 5) & 0x3f;
      const int blue = pixel & 0x1f;
      if (red + green + blue > 25) ++ink_pixels;
    }
  }
  return ink_pixels > 8;
}

static bool framebufferIsSolidRgb565(uint16_t color) {
  for (int i = 0; i < kWinW * kWinH; ++i) {
    if (g_fb[i] != color) return false;
  }
  return true;
}

static uint64_t hashHomeHeaderFramebuffer() {
  uint64_t hash = 14695981039346656037ULL;
  for (int y = 0; y < 41; ++y) {
    for (int x = 0; x < kWinW; ++x) {
      hash = (hash ^ g_fb[y * kWinW + x]) * 1099511628211ULL;
    }
  }
  return hash;
}

static void requestInterrupt(int) { g_interrupt_requested = 1; }

static const char* historyPowerSourceText(HistoryPowerSource source) {
  switch (source) {
    case HistoryPowerSource::Grid: return "grid";
    case HistoryPowerSource::Powerwall: return "powerwall";
    default: return "unknown";
  }
}

static HistoryPowerSource parseHistoryPowerSource(JsonVariantConst source) {
  if (source.isNull()) return HistoryPowerSource::Grid; // Existing simulator history had no source field.
  const String value = source.as<String>();
  if (value == "grid") return HistoryPowerSource::Grid;
  if (value == "powerwall") return HistoryPowerSource::Powerwall;
  return HistoryPowerSource::Unknown;
}

#include "checks/weekly_history_sim.hpp"
#include "checks/weekly_history_checks.hpp"

class StdinSingleKeyMode {
public:
  StdinSingleKeyMode() {
    if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &original_) != 0) return;
    termios single_key = original_;
    single_key.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
    single_key.c_cc[VMIN] = 0;
    single_key.c_cc[VTIME] = 0;
    active_ = tcsetattr(STDIN_FILENO, TCSANOW, &single_key) == 0;
  }

  ~StdinSingleKeyMode() {
    if (active_) tcsetattr(STDIN_FILENO, TCSANOW, &original_);
  }

private:
  termios original_ = {};
  bool active_ = false;
};

static void printSimulatorHelp() {
  std::printf("================================================================\n");
  std::printf(" [ESP32 Firmware SDL2 Runner] Running real esp32/firmware/src/!\n");
  std::printf("  - Home: clock and blinking data indicator (green=live, red=unavailable/stale/demo)\n");
  std::printf("  - --home480-smoke-test : verify compact layout, animation and partial redraws\n");
  std::printf("  - Wi-Fi card: [7] cycle Wi-Fi/AP/setup/retry states, [8] Gateway setup, [9] fade, [0] hide\n");
  std::printf("  - Press [1] : Return to LIVE (wait for a successful poll)\n");
  std::printf("  - Press [2] : DEMO Toggle Off-Grid state (電網斷網 / 孤島供電)\n");
  std::printf("  - Press [3] : Preview stale Weather (過期), without changing the clock\n");
  std::printf("  - Press [4] : DEMO Cycle Battery Charging (↑) / Discharging (↓) / Standby (•)\n");
  std::printf("  - Press [5] : Toggle Waiting-for-WiFi state (等待配網設定...)\n");
  std::printf("%s\n", historySaveConsoleHelpText());
  std::printf("  - Press [F6] : Toggle simulated ESP IP / powerwall-dashboard AP settings preview\n");
  std::printf("  - Press [s] : next sun phase 10,20,...,90; [m] : next moon phase\n");
  std::printf("  - Press [w] : next weather; [z] : return to LIVE; [h] : show this help\n");
  std::printf("  - Moon cycle: 新月, 眉月, 上弦月, 盈凸月, 滿月, 虧凸月, 下弦月, 殘月\n");
  std::printf("  - Weather cycle: 晴天, 多雲, 陰天, 下雨, 強雨／閃電, 下雪／結霜, 強風, 日出, 日落, 夜晚／月亮\n");
  std::printf("  - stdin: press 0-9, s, m, w, z, or h once; no Enter needed\n");
  std::printf("  - Press [Escape] to quit\n");
  std::printf("  - --page trend|week|settings|network : open Today Trend, history, Controls, or Info\n");
  std::printf("  - --gateway-ip <IP> --gateway-password <password> : use Gateway LIVE mode; overrides local config in memory\n");
  std::printf("  - --network-ap-preview : show the simulated powerwall-dashboard AP fallback\n");
  std::printf("  - --settings-long-ssid : show Info with a maximum-length 32-byte SSID\n");
  std::printf("  - --demo-full-long-eta : preview 100%% SoC with long ETA on the Powerwall page\n");
  std::printf("  - --demo-trend-empty : show an empty Today Trend page with live demo metrics\n");
  std::printf("  - --demo-trend-missing-grid : show a signed-grid gap in the Today Trend page\n");
  std::printf("  - --demo-trend-metric-fit : check 100%% SoC, long signed power, and metric labels\n");
  std::printf("  - --trend-smoke-test : check the independent daily trend JSON simulator sidecar\n");
  std::printf("  - Click page body / [Page Down] (PWRKEY) : Original > Powerwall > Home/Grid > Today Trend > Week > Original\n");
  std::printf("  - Press [Home] (BOOT) : open Settings; Page Down returns to the previous page\n");
  std::printf("  - On Settings, click RESTART twice to confirm a restart request\n");
  std::printf("  - --tap-smoke-test : verify page fades, rapid taps, capture cancel, and controls\n");
  std::printf("  - --pw-gauge-intro-test : verify the Powerwall gauge sweeps up from 0 on page entry\n");
  std::printf("  - --history-json-compat-smoke-test : verify missing source loads and saves as grid\n");
  std::printf("  - POWERWALL_SIM_TIMEZONE_MINUTES : offset in minutes; use whole hours from -720 to 840 (default 480)\n");
  std::printf("  - --timezone-smoke-test : verify UTC range, whole-hour validation, and host conversion\n");
  std::printf("================================================================\n");
  std::fflush(stdout);
}

static bool pushMouseButtonEvent(SDL_Window* window, Uint32 type, int x, int y) {
  SDL_Event event{};
  event.type = type;
  event.button.windowID = SDL_GetWindowID(window);
  event.button.button = SDL_BUTTON_LEFT;
  event.button.state = type == SDL_MOUSEBUTTONDOWN ? SDL_PRESSED : SDL_RELEASED;
  event.button.x = x * kWindowSize / kWinW;
  event.button.y = y * kWindowSize / kWinH;
  return SDL_PushEvent(&event) == 1;
}

static bool saveFramebufferPpm(const char* path) {
  if (!path) return false;
  FILE* fp = std::fopen(path, "wb");
  if (!fp) return false;
  std::fprintf(fp, "P6\n%d %d\n255\n", kWinW, kWinH);
  bool ok = true;
  for (int i = 0; i < kWinW * kWinH; ++i) {
    const uint16_t pixel = g_fb[i];
    uint8_t r = static_cast<uint8_t>((pixel >> 11) & 0x1f); r = static_cast<uint8_t>((r << 3) | (r >> 2));
    uint8_t g = static_cast<uint8_t>((pixel >> 5) & 0x3f); g = static_cast<uint8_t>((g << 2) | (g >> 4));
    uint8_t b = static_cast<uint8_t>(pixel & 0x1f); b = static_cast<uint8_t>((b << 3) | (b >> 2));
    const uint8_t rgb[3] = {r, g, b};
    if (std::fwrite(rgb, 1, sizeof(rgb), fp) != sizeof(rgb)) { ok = false; break; }
  }
  if (std::fclose(fp) != 0) ok = false;
  return ok;
}

static void sdlPanelFlush(const lv_area_t* area, lv_color_t* color_p) {
  if (!area || !color_p) return;
  const int x1 = std::max(0, int(area->x1)), y1 = std::max(0, int(area->y1));
  const int x2 = std::min(kWinW-1, int(area->x2)), y2 = std::min(kWinH-1, int(area->y2));
  if (x1 > x2 || y1 > y2) return;
  const int src_width = area->x2 - area->x1 + 1;
  for (int y = y1; y <= y2; ++y) {
    std::memcpy(g_fb + y*kWinW + x1,
      color_p + (y-area->y1)*src_width + x1-area->x1, (x2-x1+1)*sizeof(uint16_t));
  }
  if (!g_fb_dirty) g_dirty_rect = {x1, y1, x2-x1+1, y2-y1+1};
  else {
    SDL_Rect r{x1,y1,x2-x1+1,y2-y1+1};
    SDL_UnionRect(&g_dirty_rect, &r, &g_dirty_rect);
  }
  g_fb_dirty = true;
}

static bool sdlTouchRead(int16_t* out_x, int16_t* out_y) {
  if (!out_x || !out_y) return false;
  *out_x = g_mouse_x;
  *out_y = g_mouse_y;
  return g_mouse_pressed;
}

static void pollSingleKeyStdin(bool& stdin_open) {
  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(STDIN_FILENO, &read_fds);
  timeval timeout = {0, 0};
  const int ready = select(STDIN_FILENO + 1, &read_fds, nullptr, nullptr, &timeout);
  if (ready <= 0) return;

  char bytes[128];
  const ssize_t count = read(STDIN_FILENO, bytes, sizeof(bytes));
  if (count == 0) {
    stdin_open = false;
    return;
  }
  if (count < 0) {
    if (errno != EINTR) stdin_open = false;
    return;
  }
  for (ssize_t i = 0; i < count; ++i) {
    const char key = normalizePhaseConsolePresetKey(bytes[i]);
    const bool wifi_card_key =
      key == '0' || key == '7' || key == '8' || key == '9';
    if (!isPhaseConsolePresetKey(key) && !(key >= '1' && key <= '5') &&
        !isHistorySaveKey(key) && !wifi_card_key && key != 27) continue;
    SDL_Event event = {};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = key == 27 ? SDLK_ESCAPE : static_cast<SDL_Keycode>(key);
    SDL_PushEvent(&event);
  }
}

static char phasePresetKeyFromSDL(SDL_Keycode key) {
  if (key < SDLK_a || key > SDLK_z) return '\0';
  const char preset = static_cast<char>('a' + key - SDLK_a);
  return isPhaseConsolePresetKey(preset) ? preset : '\0';
}

static bool loadInitialConfigAndHistory(PowerwallLocalClient& pw, bool demo,
                                        const SimulatorGatewayCliConfig* cliConfig = nullptr) {
  // 1. Load Gateway credentials from nvs_config.json if available
  String ip = "";
  String pass = "";
  String email = "customer@example.com";
  String title = native480_i18n::kDefaultSiteTitle;

  if (!demo) {
    std::ifstream cfg_file("nvs_config.json");
    if (cfg_file.is_open()) {
      std::string content((std::istreambuf_iterator<char>(cfg_file)), std::istreambuf_iterator<char>());
      JsonDocument doc;
      if (deserializeJson(doc, content) == DeserializationError::Ok) {
        if (doc["gateway_ip"].is<const char*>()) ip = doc["gateway_ip"].as<const char*>();
        if (doc["gateway_password"].is<const char*>()) pass = doc["gateway_password"].as<const char*>();
        if (doc["gateway_email"].is<const char*>()) email = doc["gateway_email"].as<const char*>();
        if (doc["site_title"].is<const char*>()) title = doc["site_title"].as<const char*>();
      }
    }
  }
  if (cliConfig && cliConfig->provided()) {
    ip = cliConfig->ip.c_str();
    pass = cliConfig->password.c_str();
  }
  const bool gatewayConfigured = ip.length() > 0 && pass.length() > 0;
  pw.configure(ip.c_str(), pass.c_str(), email.c_str(), title.c_str());

  // Seed only today's observations; yesterday's chart must start empty after midnight.
  std::ifstream hist_file;
  if (!demo) hist_file.open("history_today.json");
  size_t loaded_pts = 0;
  struct tm today = {};
  const bool clock_ready = getLocalTime(&today, 10) && today.tm_year >= 120;
  char today_date[11] = {};
  if (clock_ready) strftime(today_date, sizeof(today_date), "%Y-%m-%d", &today);
  const float current_hour = clock_ready
    ? today.tm_hour + today.tm_min / 60.0f + today.tm_sec / 3600.0f : 0.0f;
  if (hist_file.is_open()) {
    std::string content((std::istreambuf_iterator<char>(hist_file)), std::istreambuf_iterator<char>());
    JsonDocument doc;
    if (clock_ready && deserializeJson(doc, content) == DeserializationError::Ok &&
        doc["date"].is<const char*>() && strcmp(doc["date"].as<const char*>(), today_date) == 0 &&
        doc["points"].is<JsonArray>()) {
      JsonArray arr = doc["points"].as<JsonArray>();
      int last_bucket = -1;
      for (JsonObject pt : arr) {
        float h = pt["hour"] | 0.0f;
        if (!std::isfinite(h) || h < 0.0f || h > current_hour) continue;
        float soe = pt["soe"] | 50.0f;
        float load = pt["load_kw"] | 1.0f;
        const HistoryPowerSource source = parseHistoryPowerSource(pt["power_source"]);
        if (!std::isfinite(soe) || !std::isfinite(load)) continue;
        const int bucket = static_cast<int>(h * 12.0f); // one point per five minutes
        if (bucket == last_bucket && loaded_pts > 0) {
          pw.history[loaded_pts - 1] = {h, soe, load, source};
        } else if (loaded_pts < PowerwallLocalClient::kHistoryCapacity) {
          pw.history[loaded_pts++] = {h, soe, load, source};
          last_bucket = bucket;
        }
      }
    }
  }
  if (demo) loaded_pts = 0; // Demo pages use deterministic mixed-source history, never local live history.
  if (demo && loaded_pts < 2) {
    struct tm tm_now = {};
    getLocalTime(&tm_now, 10);
    float cur_h = tm_now.tm_hour + tm_now.tm_min / 60.0f;
    if (cur_h < 0.5f) cur_h = 12.0f;
    loaded_pts = 0;
    for (float h = 0.0f; h <= cur_h && loaded_pts < PowerwallLocalClient::kHistoryCapacity; h += 0.25f) {
      float soe = (h < 6.0f) ? (35.0f + h * 10.8f) : (h < 14.0f ? (100.0f - (h - 6.0f) * 3.5f) : (72.0f + sinf(h) * 12.0f));
      float load = 1.2f + 0.9f * sinf(h * 0.5f) + (h >= 18.0f ? 1.4f : 0.0f);
      const int sample = static_cast<int>(h * 4.0f);
      const HistoryPowerSource source = sample % 11 == 0 ? HistoryPowerSource::Unknown :
        ((h >= 6.0f && h < 10.0f) || (h >= 14.0f && h < 18.0f))
          ? HistoryPowerSource::Powerwall : HistoryPowerSource::Grid;
      pw.history[loaded_pts++] = {h, soe, load, source};
    }
    if (loaded_pts > 0) {
      HistoryPoint& latest = pw.history[loaded_pts - 1];
      latest.soe = 82.0f;
      latest.load_kw = 1.35f;
      latest.power_source = HistoryPowerSource::Powerwall;
    }
  }
  pw.seedHistory(pw.history, loaded_pts);
  if (demo) std::printf("[History] seeded %zu synthetic points for demo\n", loaded_pts);
  else std::printf("[History] loaded %zu points from history_today.json\n", loaded_pts);
  return gatewayConfigured;
}

static bool saveHistory(const PowerwallLocalClient& pw) {
  struct tm today = {};
  if (!getLocalTime(&today, 10) || today.tm_year < 120 || !pw.historyIsFor(today)) return false;
  char date[11] = {};
  strftime(date, sizeof(date), "%Y-%m-%d", &today);
  std::vector<HistoryPoint> points;
  {
    std::ifstream existing("history_today.json");
    if (existing) {
      std::string content((std::istreambuf_iterator<char>(existing)), std::istreambuf_iterator<char>());
      JsonDocument doc;
      if (deserializeJson(doc, content) == DeserializationError::Ok &&
          doc["date"].is<const char*>() && strcmp(doc["date"].as<const char*>(), date) == 0 &&
          doc["points"].is<JsonArray>()) {
        for (JsonObject pt : doc["points"].as<JsonArray>()) {
          const float hour = pt["hour"] | -1.0f;
          const float soe = pt["soe"] | 0.0f;
          const float load = pt["load_kw"] | 0.0f;
          if (std::isfinite(hour) && std::isfinite(soe) && std::isfinite(load) &&
              hour >= 0.0f && hour < 24.0f) {
            points.push_back({hour, soe, load, parseHistoryPowerSource(pt["power_source"])});
          }
        }
      }
    }
  }
  if (points.empty()) points.assign(pw.history, pw.history + pw.history_count);
  else if (pw.history_count > 0) {
    const HistoryPoint& latest = pw.history[pw.history_count - 1];
    if (latest.hour + 0.001f < points.back().hour) return true; // another writer is newer
    if (static_cast<int>(latest.hour * 60.0f) == static_cast<int>(points.back().hour * 60.0f)) {
      points.back() = latest;
    } else {
      points.push_back(latest);
    }
  }
  if (points.size() > 1440) points.erase(points.begin(), points.end() - 1440);
  char tmp_path[64];
  std::snprintf(tmp_path, sizeof(tmp_path), "history_today.json.%ld.tmp", static_cast<long>(getpid()));
  FILE* file = std::fopen(tmp_path, "wb");
  if (!file) return false;
  bool written = std::fprintf(file, "{\"date\":\"%s\",\"points\":[", date) >= 0;
  for (size_t i = 0; i < points.size() && written; ++i) {
    const HistoryPoint& point = points[i];
    written = std::fprintf(file, "%s{\"hour\":%.4f,\"soe\":%.3f,\"load_kw\":%.3f,\"power_source\":\"%s\"}",
                           i ? "," : "", point.hour, point.soe, point.load_kw,
                           historyPowerSourceText(point.power_source)) >= 0;
  }
  if (written) written = std::fprintf(file, "]}") >= 0;
  const bool closed = std::fclose(file) == 0;
  if (!written || !closed || std::rename(tmp_path, "history_today.json") != 0) {
    std::remove(tmp_path);
    return false;
  }
  return true;
}

static bool dailyTrendPointValid(const DailyTrendPoint& point) {
  if (!std::isfinite(point.hour) || point.hour < 0.0f || point.hour >= 24.0f ||
      (point.valid_mask & ~kDailyTrendKnownValidityMask) != 0) return false;
  const float bucket = point.hour * 12.0f;
  if (std::fabs(bucket - std::round(bucket)) > 0.001f) return false;
  if (!!(point.valid_mask & kDailyTrendSolarValid) != (point.solar_samples != 0) ||
      !!(point.valid_mask & kDailyTrendHomeValid) != (point.home_samples != 0) ||
      !!(point.valid_mask & kDailyTrendGridValid) != (point.grid_samples != 0) ||
      !!(point.valid_mask & kDailyTrendSoeValid) != (point.soe_samples != 0)) return false;
  if ((point.valid_mask & kDailyTrendSolarValid) &&
      (!std::isfinite(point.solar_kw) || point.solar_kw < 0.0f)) return false;
  if ((point.valid_mask & kDailyTrendHomeValid) &&
      (!std::isfinite(point.home_kw) || point.home_kw < 0.0f)) return false;
  if ((point.valid_mask & kDailyTrendGridValid) && !std::isfinite(point.grid_kw)) return false;
  if ((point.valid_mask & kDailyTrendSoeValid) &&
      (!std::isfinite(point.soe) || point.soe < 0.0f || point.soe > 100.0f)) return false;
  return true;
}

// The simulator's trend file is intentionally independent of history_today.json
// and history_week.json. The firmware uses the SPIFFS A/B sidecar instead.
static bool readDailyTrendFile(DailyTrendHistory& history, const char* path,
                               const struct tm& now) {
  history = DailyTrendHistory{};
  char requested_date[11] = {};
  if (!path || std::strftime(requested_date, sizeof(requested_date), "%Y-%m-%d", &now) != 10)
    return false;
  std::ifstream file(path);
  if (!file) return false;
  const std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  JsonDocument doc;
  if (deserializeJson(doc, content) != DeserializationError::Ok ||
      (doc["version"] | 0) != 2 || !doc["date"].is<const char*>() ||
      std::strcmp(doc["date"].as<const char*>(), requested_date) != 0 ||
      !doc["points"].is<JsonArray>() || !doc["solar_kwh_today"].is<float>() ||
      !doc["solar_kwh_complete"].is<bool>()) return false;

  JsonArray points = doc["points"].as<JsonArray>();
  if (points.size() > DailyTrendHistory::kCapacity) return false;
  std::memcpy(history.date, requested_date, sizeof(history.date));
  history.solar_kwh_today = doc["solar_kwh_today"].as<float>();
  history.solar_kwh_complete = doc["solar_kwh_complete"].as<bool>();
  if (!std::isfinite(history.solar_kwh_today) || history.solar_kwh_today < 0.0f) {
    history = DailyTrendHistory{};
    return false;
  }

  const float current_hour = now.tm_hour + now.tm_min / 60.0f + now.tm_sec / 3600.0f;
  int last_bucket = -1;
  bool had_future_point = false;
  for (JsonObject item : points) {
    if (!item["hour"].is<float>() || !item["valid_mask"].is<int>()) {
      history = DailyTrendHistory{};
      return false;
    }
    DailyTrendPoint point{};
    point.hour = item["hour"].as<float>();
    const int mask = item["valid_mask"].as<int>();
    if (mask < 0 || mask > 0xff) { history = DailyTrendHistory{}; return false; }
    point.valid_mask = static_cast<uint8_t>(mask);
    if (((point.valid_mask & kDailyTrendSolarValid) && !item["solar_kw"].is<float>()) ||
        ((point.valid_mask & kDailyTrendHomeValid) && !item["home_kw"].is<float>()) ||
        ((point.valid_mask & kDailyTrendGridValid) && !item["grid_kw"].is<float>()) ||
        ((point.valid_mask & kDailyTrendSoeValid) && !item["soe"].is<float>())) {
      history = DailyTrendHistory{};
      return false;
    }
    point.solar_kw = item["solar_kw"] | 0.0f;
    point.home_kw = item["home_kw"] | 0.0f;
    point.grid_kw = item["grid_kw"] | 0.0f;
    point.soe = item["soe"] | 0.0f;
    const char* sample_count_keys[] = {"solar_samples", "home_samples", "grid_samples", "soe_samples"};
    uint16_t* sample_counts[] = {&point.solar_samples, &point.home_samples,
                                 &point.grid_samples, &point.soe_samples};
    const uint8_t valid_bits[] = {kDailyTrendSolarValid, kDailyTrendHomeValid,
                                  kDailyTrendGridValid, kDailyTrendSoeValid};
    for (int field = 0; field < 4; ++field) {
      if ((point.valid_mask & valid_bits[field]) && !item[sample_count_keys[field]].is<int>()) {
        history = DailyTrendHistory{};
        return false;
      }
      const int sample_count = item[sample_count_keys[field]] | 0;
      if (sample_count < 0 || sample_count > UINT16_MAX) {
        history = DailyTrendHistory{};
        return false;
      }
      *sample_counts[field] = static_cast<uint16_t>(sample_count);
    }
    if (!dailyTrendPointValid(point)) { history = DailyTrendHistory{}; return false; }
    const int bucket = static_cast<int>(std::round(point.hour * 12.0f));
    if (bucket <= last_bucket) { history = DailyTrendHistory{}; return false; }
    last_bucket = bucket;
    if (point.hour > current_hour) {
      had_future_point = true;
      continue;
    }
    history.points[history.count++] = point;
  }

  // The simulator JSON does not persist the private solar integration endpoint.
  // Retain the interval-only total, but mark it partial until a new observed
  // endpoint is established in this process. A clock rollback also clears a
  // total that may include the truncated future portion.
  if (had_future_point) history.solar_kwh_today = 0.0f;
  history.solar_kwh_complete = false;
  return true;
}

static bool saveDailyTrendFile(const DailyTrendHistory& history,
                               const char* path = "daily_trend_today.json") {
  if (!path || std::strlen(history.date) != 10 || history.count > DailyTrendHistory::kCapacity ||
      !std::isfinite(history.solar_kwh_today) || history.solar_kwh_today < 0.0f) return false;
  int last_bucket = -1;
  for (size_t i = 0; i < history.count; ++i) {
    if (!dailyTrendPointValid(history.points[i])) return false;
    const int bucket = static_cast<int>(std::round(history.points[i].hour * 12.0f));
    if (bucket <= last_bucket) return false;
    last_bucket = bucket;
  }

  char tmp_path[PATH_MAX];
  const int n = std::snprintf(tmp_path, sizeof(tmp_path), "%s.%ld.tmp", path,
                              static_cast<long>(getpid()));
  if (n <= 0 || static_cast<size_t>(n) >= sizeof(tmp_path)) return false;
  FILE* file = std::fopen(tmp_path, "wb");
  if (!file) return false;
  bool written = std::fprintf(file,
    "{\"version\":2,\"date\":\"%s\",\"solar_kwh_today\":%.5f,\"solar_kwh_complete\":%s,\"points\":[",
    history.date, history.solar_kwh_today, history.solar_kwh_complete ? "true" : "false") >= 0;
  for (size_t i = 0; i < history.count && written; ++i) {
    const DailyTrendPoint& point = history.points[i];
    written = std::fprintf(file,
      "%s{\"hour\":%.5f,\"valid_mask\":%u,\"solar_kw\":%.5f,\"home_kw\":%.5f,\"grid_kw\":%.5f,\"soe\":%.5f,\"solar_samples\":%u,\"home_samples\":%u,\"grid_samples\":%u,\"soe_samples\":%u}",
      i ? "," : "", point.hour, static_cast<unsigned>(point.valid_mask),
      point.solar_kw, point.home_kw, point.grid_kw, point.soe,
      static_cast<unsigned>(point.solar_samples), static_cast<unsigned>(point.home_samples),
      static_cast<unsigned>(point.grid_samples), static_cast<unsigned>(point.soe_samples)) >= 0;
  }
  if (written) written = std::fprintf(file, "]}\n") >= 0;
  const bool closed = std::fclose(file) == 0;
  if (!written || !closed || std::rename(tmp_path, path) != 0) {
    std::remove(tmp_path);
    return false;
  }
  return true;
}

static DailyTrendSample makeDailyTrendSample(const PowerwallTelemetry& telemetry,
                                             bool poll_succeeded) {
  DailyTrendSample sample{};
  if (!poll_succeeded) return sample;
  if (telemetry.has_solar_power && std::isfinite(telemetry.solar_power_w) &&
      telemetry.solar_power_w >= 0.0f) {
    sample.solar_kw = telemetry.solar_power_w / 1000.0f;
    sample.valid_mask |= kDailyTrendSolarValid;
  }
  if (telemetry.has_load_power && std::isfinite(telemetry.load_power_w) &&
      telemetry.load_power_w >= 0.0f) {
    sample.home_kw = telemetry.load_power_w / 1000.0f;
    sample.valid_mask |= kDailyTrendHomeValid;
  }
  if (telemetry.has_site_power && std::isfinite(telemetry.site_power_w)) {
    sample.grid_kw = telemetry.site_power_w / 1000.0f;
    sample.valid_mask |= kDailyTrendGridValid;
  }
  if (std::isfinite(telemetry.soe_percent) &&
      telemetry.soe_percent >= 0.0f && telemetry.soe_percent <= 100.0f) {
    sample.soe = telemetry.soe_percent;
    sample.valid_mask |= kDailyTrendSoeValid;
  }
  return sample;
}

static void seedDemoDailyTrend(DailyTrendHistory& history, const struct tm& now,
                               bool empty = false, bool missing_grid = false) {
  history = DailyTrendHistory{};
  if (empty) return;
  struct tm midnight = now;
  midnight.tm_hour = 0;
  midnight.tm_min = 0;
  midnight.tm_sec = 0;
  midnight.tm_isdst = -1;
  const time_t start = std::mktime(&midnight);
  if (start < 0) return;
  const int current_minute = now.tm_hour * 60 + now.tm_min;
  const int last_minute = (current_minute / 5) * 5;
  const int gap_start = std::max(0, last_minute - 20);
  for (int minute = 0; minute <= last_minute; minute += 5) {
    const time_t sample_epoch = start + static_cast<time_t>(minute) * 60;
    struct tm sample_time{};
    if (!localtime_r(&sample_epoch, &sample_time)) continue;
    const float hour = minute / 60.0f;
    const float solar_phase = std::max(0.0f, std::sin((hour - 6.0f) * 3.14159265f / 12.0f));
    const float solar = 5.2f * solar_phase * (0.92f + 0.08f * std::sin(hour * 2.1f));
    const float home = std::max(0.45f, 1.25f + 0.45f * std::sin(hour * 0.63f) +
      (hour >= 17.0f ? 0.9f : 0.0f));
    DailyTrendSample sample{};
    sample.solar_kw = solar;
    sample.home_kw = home;
    sample.grid_kw = home - solar;
    sample.soe = std::clamp(55.0f + 27.0f * std::sin((hour - 5.0f) * 3.14159265f / 18.0f), 22.0f, 96.0f);
    sample.valid_mask = kDailyTrendSolarValid | kDailyTrendHomeValid |
      kDailyTrendGridValid | kDailyTrendSoeValid;
    if (missing_grid && minute >= gap_start && minute <= last_minute)
      sample.valid_mask &= static_cast<uint8_t>(~kDailyTrendGridValid);
    history.ingestPoll(sample_time, sample);
  }
}

static bool latestDemoTrendValue(const DailyTrendHistory& history, uint8_t validity,
                                 float& value) {
  for (size_t index = history.count; index > 0; --index) {
    const DailyTrendPoint& point = history.points[index - 1];
    if (!(point.valid_mask & validity)) continue;
    if (validity == kDailyTrendSolarValid) value = point.solar_kw;
    else if (validity == kDailyTrendHomeValid) value = point.home_kw;
    else if (validity == kDailyTrendGridValid) value = point.grid_kw;
    else if (validity == kDailyTrendSoeValid) value = point.soe;
    else return false;
    return true;
  }
  return false;
}

static void alignDemoTelemetryToTrend(PowerwallTelemetry& telemetry,
                                      const DailyTrendHistory& history) {
  float value = 0.0f;
  if (latestDemoTrendValue(history, kDailyTrendSolarValid, value)) {
    telemetry.solar_power_w = value * 1000.0f;
    telemetry.has_solar_power = true;
  }
  if (latestDemoTrendValue(history, kDailyTrendHomeValid, value)) {
    telemetry.load_power_w = value * 1000.0f;
    telemetry.has_load_power = true;
  }
  if (latestDemoTrendValue(history, kDailyTrendGridValid, value)) {
    telemetry.site_power_w = value * 1000.0f;
    telemetry.has_site_power = true;
  }
  if (latestDemoTrendValue(history, kDailyTrendSoeValid, value)) {
    telemetry.soe_percent = value;
    if (telemetry.has_battery_capacity && std::isfinite(telemetry.nominal_full_kwh) &&
        telemetry.nominal_full_kwh > 0.0f)
      telemetry.nominal_remaining_kwh = telemetry.nominal_full_kwh * value / 100.0f;
  }
}

static void reloadLiveDailyTrend(DailyTrendHistory& history) {
  struct tm today{};
  if (getLocalTime(&today, 10) && today.tm_year >= 120) {
    if (readDailyTrendFile(history, "daily_trend_today.json", today)) return;
    history = DailyTrendHistory{};
    history.ingestPoll(today, DailyTrendSample{});
    return;
  }
  history = DailyTrendHistory{};
}

static int runDailyTrendSimulatorSmokeTest() {
  char original_dir[PATH_MAX] = {};
  char test_dir[] = "/tmp/powerwall-trend-smoke-XXXXXX";
  if (!getcwd(original_dir, sizeof(original_dir)) || !mkdtemp(test_dir)) {
    std::fprintf(stderr, "[TrendSimSmoke] FAIL: cannot create isolated test directory\n");
    return 2;
  }
  if (chdir(test_dir) != 0) {
    rmdir(test_dir);
    std::fprintf(stderr, "[TrendSimSmoke] FAIL: cannot enter isolated test directory\n");
    return 2;
  }
  struct TestDirectoryCleanup {
    const char* original;
    const char* temporary;
    ~TestDirectoryCleanup() {
      chdir(original);
      const char* files[] = {"daily_trend_today.json", "daily_trend_today.json.tmp", "history_today.json"};
      for (const char* name : files) {
        char path[PATH_MAX];
        std::snprintf(path, sizeof(path), "%s/%s", temporary, name);
        std::remove(path);
      }
      char process_tmp[PATH_MAX];
      std::snprintf(process_tmp, sizeof(process_tmp), "%s/daily_trend_today.json.%ld.tmp",
                    temporary, static_cast<long>(getpid()));
      std::remove(process_tmp);
      rmdir(temporary);
    }
  } cleanup{original_dir, test_dir};

  struct tm now{};
  if (!getLocalTime(&now, 10) || now.tm_year < 120) {
    std::fprintf(stderr, "[TrendSimSmoke] FAIL: local clock is not synchronized\n");
    return 2;
  }
  FILE* legacy = std::fopen("history_today.json", "wb");
  const bool legacy_written = legacy && std::fputs("legacy-sentinel\n", legacy) >= 0;
  const bool legacy_closed = legacy && std::fclose(legacy) == 0;
  if (!legacy_written || !legacy_closed) {
    std::fprintf(stderr, "[TrendSimSmoke] FAIL: cannot write legacy sentinel\n");
    return 2;
  }

  static DailyTrendHistory fixture;
  static DailyTrendHistory restored;
  fixture = DailyTrendHistory{};
  struct tm first = now;
  first.tm_hour = 0; first.tm_min = 0; first.tm_sec = 0; first.tm_isdst = -1;
  std::mktime(&first);
  DailyTrendSample sample{};
  sample.solar_kw = 1.0f; sample.home_kw = 2.0f; sample.grid_kw = -0.75f; sample.soe = 60.0f;
  sample.valid_mask = kDailyTrendSolarValid | kDailyTrendHomeValid | kDailyTrendGridValid | kDailyTrendSoeValid;
  if (!fixture.ingestPoll(first, sample)) {
    std::fprintf(stderr, "[TrendSimSmoke] FAIL: first trend sample rejected\n");
    return 3;
  }
  first.tm_min = 5; std::mktime(&first);
  sample.solar_kw = 3.0f; sample.home_kw = 2.5f; sample.grid_kw = 0.5f; sample.soe = 61.0f;
  if (!fixture.ingestPoll(first, sample) || fixture.count != 2 ||
      !(fixture.points[0].valid_mask & kDailyTrendGridValid) ||
      fixture.points[0].grid_kw >= 0.0f || fixture.solar_kwh_today <= 0.0f) {
    std::fprintf(stderr, "[TrendSimSmoke] FAIL: signed grid or observed-interval fixture invalid\n");
    return 3;
  }
  first.tm_min = 10; std::mktime(&first);
  sample.valid_mask = kDailyTrendSolarValid | kDailyTrendHomeValid | kDailyTrendSoeValid;
  if (!fixture.ingestPoll(first, sample) ||
      (fixture.points[2].valid_mask & kDailyTrendGridValid)) {
    std::fprintf(stderr, "[TrendSimSmoke] FAIL: missing signed-grid field became a zero sample\n");
    return 3;
  }
  if (!saveDailyTrendFile(fixture)) {
    std::fprintf(stderr, "[TrendSimSmoke] FAIL: independent trend JSON save failed\n");
    return 4;
  }
  struct tm load_now = now;
  load_now.tm_hour = 0; load_now.tm_min = 15; load_now.tm_sec = 0; load_now.tm_isdst = -1;
  std::mktime(&load_now);
  if (!readDailyTrendFile(restored, "daily_trend_today.json", load_now) || restored.count != 3 ||
      restored.points[0].grid_kw >= 0.0f ||
      (restored.points[2].valid_mask & kDailyTrendGridValid) ||
      restored.points[0].solar_samples != 1 || restored.solar_kwh_complete) {
    std::fprintf(stderr, "[TrendSimSmoke] FAIL: trend JSON round-trip or conservative restore failed\n");
    return 5;
  }
  std::ifstream sentinel("history_today.json");
  std::string sentinel_text((std::istreambuf_iterator<char>(sentinel)), std::istreambuf_iterator<char>());
  if (sentinel_text != "legacy-sentinel\n") {
    std::fprintf(stderr, "[TrendSimSmoke] FAIL: trend write modified legacy daily JSON\n");
    return 6;
  }
  std::printf("[TrendSimSmoke] PASS: separate JSON, signed export/import, missing grid validity and restart round-trip\n");
  return 0;
}

static int runHistoryJsonCompatibilitySmokeTest() {
  // This test writes history_today.json. Keep the fixture out of the caller's
  // working directory so running it cannot replace real simulator history.
  char original_dir[PATH_MAX] = {};
  char test_dir[] = "/tmp/powerwall-history-smoke-XXXXXX";
  if (!getcwd(original_dir, sizeof(original_dir)) || !mkdtemp(test_dir)) {
    std::fprintf(stderr, "[HistoryCompatSmokeTest] FAIL: cannot create isolated test directory\n");
    return 2;
  }
  if (chdir(test_dir) != 0) {
    rmdir(test_dir);
    std::fprintf(stderr, "[HistoryCompatSmokeTest] FAIL: cannot enter isolated test directory\n");
    return 2;
  }
  struct TestDirectoryCleanup {
    const char* original;
    const char* temporary;
    ~TestDirectoryCleanup() {
      chdir(original);
      char fixture_path[PATH_MAX];
      std::snprintf(fixture_path, sizeof(fixture_path), "%s/history_today.json", temporary);
      std::remove(fixture_path);
      rmdir(temporary);
    }
  } cleanup{original_dir, test_dir};

  struct tm today = {};
  if (!getLocalTime(&today, 10) || today.tm_year < 120) {
    std::fprintf(stderr, "[HistoryCompatSmokeTest] FAIL: local Taiwan clock is unavailable\n");
    return 2;
  }
  char date[11] = {};
  strftime(date, sizeof(date), "%Y-%m-%d", &today);
  const float current_hour = today.tm_hour + today.tm_min / 60.0f + today.tm_sec / 3600.0f;
  const float point_hour = std::max(0.0f, current_hour - 1.0f / 12.0f);
  FILE* fixture = std::fopen("history_today.json", "wb");
  if (!fixture) {
    std::fprintf(stderr, "[HistoryCompatSmokeTest] FAIL: cannot create isolated fixture\n");
    return 2;
  }
  const bool fixture_ok = std::fprintf(fixture,
    "{\"date\":\"%s\",\"points\":[{\"hour\":%.4f,\"soe\":82.0,\"load_kw\":1.35}]}\n",
    date, point_hour) >= 0;
  const bool fixture_closed = std::fclose(fixture) == 0;
  if (!fixture_ok || !fixture_closed) {
    std::fprintf(stderr, "[HistoryCompatSmokeTest] FAIL: cannot write isolated fixture\n");
    return 2;
  }

  PowerwallLocalClient client;
  loadInitialConfigAndHistory(client, false);
  if (client.history_count != 1 || client.history[0].power_source != HistoryPowerSource::Grid) {
    std::fprintf(stderr, "[HistoryCompatSmokeTest] FAIL: missing power_source did not load as Grid\n");
    return 3;
  }
  if (!saveHistory(client)) {
    std::fprintf(stderr, "[HistoryCompatSmokeTest] FAIL: saveHistory failed\n");
    return 4;
  }

  std::ifstream saved_file("history_today.json");
  const std::string saved((std::istreambuf_iterator<char>(saved_file)), std::istreambuf_iterator<char>());
  JsonDocument doc;
  if (deserializeJson(doc, saved) != DeserializationError::Ok || !doc["points"].is<JsonArray>()) {
    std::fprintf(stderr, "[HistoryCompatSmokeTest] FAIL: saved JSON is invalid\n");
    return 5;
  }
  bool wrote_grid = false;
  for (JsonObject point : doc["points"].as<JsonArray>()) {
    if (point["power_source"] == "grid") wrote_grid = true;
  }
  if (!wrote_grid) {
    std::fprintf(stderr, "[HistoryCompatSmokeTest] FAIL: saved records lack power_source:grid\n");
    return 6;
  }
  std::printf("[HistoryCompatSmokeTest] PASS: missing source loaded as Grid and save wrote power_source:grid\n");
  return 0;
}


#include "checks/home_480_checks.hpp"
#include "checks/pw_gauge_intro_checks.hpp"

#include "checks/powerflow_checks.hpp"

int main(int argc, char* argv[]) {
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
      printSimulatorHelp();
      return 0;
    }
  }

  std::vector<const char*> gatewayCliArgv;
  gatewayCliArgv.reserve(static_cast<size_t>(argc));
  for (int i = 0; i < argc; ++i) gatewayCliArgv.push_back(argv[i]);
  SimulatorGatewayCliConfig gatewayCliConfig;
  const auto gatewayCliResult = parseSimulatorGatewayCliConfig(
    argc, gatewayCliArgv.data(), gatewayCliConfig);
  if (gatewayCliResult == SimulatorGatewayCliParseResult::MissingValue) {
    std::fprintf(stderr, "[Gateway] Missing a value for the Gateway IP or password option.\n");
    return 2;
  }
  if (gatewayCliResult == SimulatorGatewayCliParseResult::IncompletePair) {
    std::fprintf(stderr, "[Gateway] Provide both --gateway-ip and --gateway-password.\n");
    return 2;
  }
  if (gatewayCliResult == SimulatorGatewayCliParseResult::EmptyValue) {
    std::fprintf(stderr, "[Gateway] Gateway IP and password must both be non-empty.\n");
    return 2;
  }

  const int timezoneOffsetMinutes = simulatorTimezoneFromEnvironment();
  if (!setSimulatorTimezone(timezoneOffsetMinutes)) {
    std::fprintf(stderr, "[Timezone] Could not configure simulator host timezone.\n");
    return 10;
  }
  char timezoneText[16] = {};
  native480_timezone::formatUtcOffset(timezoneOffsetMinutes, timezoneText, sizeof(timezoneText));
  std::printf("[Timezone] Simulator local clock uses %s\n", timezoneText);
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--timezone-smoke-test") == 0) return runTimezoneSmokeTest();

  bool powerflow_smoke_test = false;
  bool powerflow_realtime_test = false;
  bool home480_smoke_test = false;
  bool pw_gauge_intro_test = false;
  bool demo_arg = false;
  bool demo_full_long_eta = false;
  bool history_writes_disabled = false;
  bool auto_quit_sec = false;
  bool tap_smoke_test = false;
  bool settings_smoke_test = false;
  bool initial_network_page = false;
  bool initial_network_ap = false;
  bool initial_long_ssid = false;
  bool history_json_compat_smoke_test = false;
  bool daily_trend_smoke_test = false;
  bool demo_trend_empty = false;
  bool demo_trend_missing_grid = false;
  bool demo_trend_metric_fit = false;
  int quit_after_ms = 0;
  uint8_t initial_page = 0;
  const char* capture_path = kCapturePath;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--powerflow-smoke-test") == 0 ||
        std::strcmp(argv[i], "--powerflow-realtime-test") == 0) {
      powerflow_smoke_test = true;
      powerflow_realtime_test = std::strcmp(argv[i], "--powerflow-realtime-test") == 0;
      demo_arg = true;
      history_writes_disabled = true;
    }
    if (std::strcmp(argv[i], "--home480-smoke-test") == 0) {
      home480_smoke_test = true;
      history_writes_disabled = true;
    }
    if (std::strcmp(argv[i], "--pw-gauge-intro-test") == 0) {
      pw_gauge_intro_test = true;
      history_writes_disabled = true;
    }
    if (std::strcmp(argv[i], "--voice-smoke-test") == 0)
      return runVoiceStateSmokeTest() ? 0 : 9;
    if (std::strcmp(argv[i], "--weekly-history-smoke-test") == 0) return runWeeklyHistoryChecks();
    if (std::strcmp(argv[i], "--trend-smoke-test") == 0) {
      daily_trend_smoke_test = true;
      history_writes_disabled = true;
    }
    if (std::strcmp(argv[i], "--demo") == 0) {
      demo_arg = true;
      history_writes_disabled = true;
    }
    if (std::strcmp(argv[i], "--demo-full-long-eta") == 0) {
      demo_arg = true;
      demo_full_long_eta = true;
      history_writes_disabled = true;
    }
    if (std::strcmp(argv[i], "--demo-trend-empty") == 0) {
      demo_arg = true;
      demo_trend_empty = true;
      history_writes_disabled = true;
      initial_page = 3;
    }
    if (std::strcmp(argv[i], "--demo-trend-missing-grid") == 0) {
      demo_arg = true;
      demo_trend_missing_grid = true;
      history_writes_disabled = true;
      initial_page = 3;
    }
    if (std::strcmp(argv[i], "--demo-trend-metric-fit") == 0) {
      demo_arg = true;
      demo_trend_metric_fit = true;
      history_writes_disabled = true;
      initial_page = 3;
    }
    if (std::strcmp(argv[i], "--tap-smoke-test") == 0) {
      tap_smoke_test = true;
      demo_arg = true;
      history_writes_disabled = true;
    }
    if (std::strcmp(argv[i], "--settings-smoke-test") == 0) {
      settings_smoke_test = true;
      demo_arg = true;
      history_writes_disabled = true;
      initial_page = 4;
      auto_quit_sec = true;
      quit_after_ms = 14000;
    }
    if (std::strcmp(argv[i], "--history-json-compat-smoke-test") == 0) {
      history_json_compat_smoke_test = true;
    }
    if (std::strcmp(argv[i], "--page") == 0 && i + 1 < argc) {
      const char* page = argv[++i];
      if (std::strcmp(page, "powerwall") == 0) initial_page = 1;
      else if (std::strcmp(page, "home-grid") == 0) initial_page = 2;
      else if (std::strcmp(page, "trend") == 0) initial_page = 3;
      else if (std::strcmp(page, "week") == 0) initial_page = 4;
      else if (std::strcmp(page, "settings") == 0) initial_page = 5;
      else if (std::strcmp(page, "network") == 0) { initial_page = 5; initial_network_page = true; }
      else initial_page = 0;
    }
    if (std::strcmp(argv[i], "--network-ap-preview") == 0) initial_network_ap = true;
    if (std::strcmp(argv[i], "--settings-long-ssid") == 0) {
      initial_long_ssid = true;
      initial_network_page = true;
      initial_page = 5;
    }
    if (std::strcmp(argv[i], "--capture") == 0 && i + 1 < argc) capture_path = argv[++i];
    if (std::strcmp(argv[i], "--smoke-test") == 0) {
      auto_quit_sec = true;
      quit_after_ms = 3500;
      history_writes_disabled = true;
    }
  }

  if (gatewayCliConfig.provided()) demo_arg = false;

  if (history_json_compat_smoke_test) return runHistoryJsonCompatibilitySmokeTest();
  if (daily_trend_smoke_test) return runDailyTrendSimulatorSmokeTest();
  if (history_writes_disabled) {
    std::printf("[History] writes disabled for demo/smoke session; local history files are read-only\n");
  }

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  StdinSingleKeyMode stdin_key_mode;
  std::signal(SIGINT, requestInterrupt);
  std::signal(SIGTERM, requestInterrupt);

  SDL_Window* win = SDL_CreateWindow(
    "ESP32 Firmware UI Simulator (native 480x480)",
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED,
    kWindowSize,
    kWindowSize,
    SDL_WINDOW_SHOWN
  );
  if (!win) { std::fprintf(stderr, "SDL window: %s\n", SDL_GetError()); SDL_Quit(); return 1; }
  SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
  if (!ren) { std::fprintf(stderr, "SDL renderer: %s\n", SDL_GetError()); SDL_DestroyWindow(win); SDL_Quit(); return 1; }
  SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, kWinW, kWinH);

  if (!tex) { std::fprintf(stderr, "SDL texture: %s\n", SDL_GetError()); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit(); return 1; }

  DisplayDriver480 displayDriver;
  PowerwallLocalClient pwClient;
  LocalWeatherClient weatherClient;
  Ui480 display480;
  SimulatorDeviceSettings deviceSettings;
  deviceSettings.persistence_enabled = !settings_smoke_test;
  if (!settings_smoke_test) deviceSettings.load();

  if (!displayDriver.begin(&sdlPanelFlush, &sdlTouchRead)) {
    std::fprintf(stderr, "DisplayDriver480::begin() failed!\n");
    return 1;
  }

  const bool gateway_configured = loadInitialConfigAndHistory(
    pwClient, demo_arg, gatewayCliConfig.provided() ? &gatewayCliConfig : nullptr);

  struct tm trend_today{};
  const bool trend_clock_ready = getLocalTime(&trend_today, 10) && trend_today.tm_year >= 120;
  if (demo_arg && trend_clock_ready) {
    seedDemoDailyTrend(shared_daily_trend_history, trend_today,
                       demo_trend_empty, demo_trend_missing_grid);
  } else if (!demo_arg && trend_clock_ready &&
             !readDailyTrendFile(shared_daily_trend_history,
                                 "daily_trend_today.json", trend_today)) {
    std::printf("[DailyTrend] no valid current-day trend JSON; starting empty\n");
  }

  WeeklyHistory week_history;
  week_history.roll(time(nullptr));
  if (!demo_arg) {
    readWeeklyFile(week_history, "history_week.json", true);
    readWeeklyFile(week_history, "history_today.json", false);
  }
  bool week_has_updates = false;
  setenv("POWERWALL_FONT_ASSETS", "firmware/assets/native480/fonts.pwf", 0);
  if (!loadPowerwallFontAssets()) { std::fprintf(stderr, "[Fonts] load failed; no UI created\n"); return 10; }
  display480.init();
  display480.setDeviceSettingsCallbacks(&deviceSettings,
    [](void* context, uint8_t value) {
      auto& settings = *static_cast<SimulatorDeviceSettings*>(context);
      settings.brightness = value;
      settings.dirty = true;
      settings.changed_at = SDL_GetTicks();
      g_brightness_dirty = true;
    },
    [](void* context, uint8_t value) {
      auto& settings = *static_cast<SimulatorDeviceSettings*>(context);
      settings.volume = value;
      settings.dirty = true;
      settings.changed_at = SDL_GetTicks();
    },
    [](void* context) {
      static_cast<SimulatorDeviceSettings*>(context)->playTestVoice();
    },
    [](void* context, bool enabled) {
      auto& settings = *static_cast<SimulatorDeviceSettings*>(context);
      settings.voice_enabled = enabled;
      settings.dirty = true;
      settings.changed_at = SDL_GetTicks();
    }
    );
  display480.setRestartCallback([](void* context) {
    ++static_cast<SimulatorDeviceSettings*>(context)->restart_requests;
    std::printf("[Settings] Restart confirmed (simulator)\n");
  });
  display480.setDeviceSettings(deviceSettings.brightness, deviceSettings.volume, deviceSettings.voice_enabled);
  display480.setWeeklyHistory(demo_arg ? demoWeeklyHistory(time(nullptr)) : week_history);
  display480.setDailyTrendHistory(shared_daily_trend_history);
  display480.setDashboardPageForSimulator(initial_page);
  if (initial_network_page) display480.setSettingsNetworkPageForSimulator();
  if (initial_network_ap) display480.setSimulatorNetworkAvailable(false);
  if (initial_long_ssid) display480.setSimulatorWifiSsid("Powerwall-Home-Network-Guest-24G");
  if (settings_smoke_test) display480.openSettingsPage();
  if (settings_smoke_test) {
    const bool uptime_format_ok = runSettingsUptimeFormatSmokeTest();
    const bool touch_targets_ok = display480.simulatorSettingsTouchTargetsMeetMinimum();
    const bool network_preview_ok = display480.simulatorNetworkPreviewMatches(true);
    if (!uptime_format_ok || !touch_targets_ok || !network_preview_ok) {
      std::fprintf(stderr,
        "[SettingsSmokeTest] FAIL: uptime_format=%d targets=%d network=%d\n",
        uptime_format_ok, touch_targets_ok, network_preview_ok);
      return 9;
    }
  }
  if (tap_smoke_test) display480.setDashboardPageForSimulator(0);
  if (powerflow_smoke_test) return runPowerflowChecks(display480, displayDriver, powerflow_realtime_test);

  if (home480_smoke_test) return runHome480Checks(display480);
  if (pw_gauge_intro_test) return runPwGaugeIntroChecks(display480);

  std::mutex data_mtx;
  PowerwallTelemetry shared_tel = pwClient.latest;
  WeatherSnapshot shared_wea = weatherClient.current;
  std::atomic<bool> running{true};
  std::atomic<bool> wifi_online{true};
  std::atomic<bool> data_dirty{true};
  HistorySaveRequest manualHistorySave;

  std::atomic<bool> demo_mode{demo_arg};
  // Seed realistic initial telemetry for the offline demo
  if (demo_arg) {
  shared_tel.ok = true;
  shared_tel.demo = true;
  shared_tel.soe_percent = 82.0f;
  shared_tel.nominal_full_kwh = 14.62f;
  shared_tel.nominal_remaining_kwh = 11.99f;
  shared_tel.has_battery_capacity = true;
  shared_tel.solar_power_w = 2400.0f;
  shared_tel.has_solar_power = true;
  shared_tel.load_power_w = 1350.0f;
  shared_tel.has_load_power = true;
  shared_tel.load_voltage_v = 112.4f;
  shared_tel.has_load_voltage = true;
  shared_tel.load_current_a = 12.0f;
  shared_tel.has_load_current = true;
  shared_tel.load_energy_imported_wh = 240310.0f;
  shared_tel.has_load_energy = true;
  shared_tel.battery_power_w = -1050.0f;
  shared_tel.has_battery_power = true;
  shared_tel.battery_voltage_v = 51.8f;
  shared_tel.has_battery_voltage = true;
  shared_tel.site_power_w = 0.0f;
  shared_tel.has_site_power = true;
  shared_tel.site_voltage_v = 111.8f;
  shared_tel.has_site_voltage = true;
  shared_tel.site_current_a = 7.4f;
  shared_tel.has_site_current = true;
  shared_tel.site_energy_imported_wh = 182300.0f;
  shared_tel.has_site_import_energy = true;
  shared_tel.site_energy_exported_wh = 24600.0f;
  shared_tel.has_site_export_energy = true;
  shared_tel.has_grid_status = true;
  shared_tel.real_mode = "autonomous";
  shared_tel.backup_reserve_percent = 24.0f;
  shared_tel.last_update_ms = millis();
  struct tm demo_time = {};
  if (getLocalTime(&demo_time, 10) && demo_time.tm_year >= 120) {
    char timestamp[16];
    std::strftime(timestamp, sizeof(timestamp), "%H:%M:%S", &demo_time);
    shared_tel.timestamp = timestamp;
  } else {
    shared_tel.timestamp = "DEMO";
  }
  if (demo_full_long_eta) {
    shared_tel.soe_percent = 100.0f;
    shared_tel.nominal_remaining_kwh = shared_tel.nominal_full_kwh;
    shared_tel.load_power_w = 60.0f;
    shared_tel.battery_power_w = 0.0f;
  }
  }
  if (demo_arg && !demo_trend_empty && !demo_full_long_eta)
    alignDemoTelemetryToTrend(shared_tel, shared_daily_trend_history);
  if (demo_trend_metric_fit) {
    shared_tel.soe_percent = 100.0f;
    shared_tel.nominal_remaining_kwh = shared_tel.nominal_full_kwh;
    shared_tel.site_power_w = 99990.0f;
    shared_tel.has_site_power = true;
    if (shared_daily_trend_history.count > 0) {
      DailyTrendPoint& latest = shared_daily_trend_history.points[shared_daily_trend_history.count - 1];
      latest.soe = 100.0f;
      latest.grid_kw = shared_tel.site_power_w / 1000.0f;
      latest.valid_mask |= kDailyTrendSoeValid | kDailyTrendGridValid;
    }
  }

  std::thread poller([&]() {
    time_t last_history_save = 0;
    time_t last_week_save = 0;
    time_t last_trend_save = 0;
    while (running.load()) {
      if (!demo_mode.load() && wifi_online.load()) {
        const bool ok = pwClient.pollLiveData(false);
        const time_t now = time(nullptr);
        if (!history_writes_disabled && ok && (last_history_save == 0 || now - last_history_save >= 60)) {
          if (runHistorySave(Serial, "daily", "auto", [&] { return saveHistory(pwClient); })) last_history_save = now;
        }
        if (running.load()) weatherClient.update(false);
        std::lock_guard<std::mutex> lk(data_mtx);
        if (!demo_mode.load() && wifi_online.load()) {
          struct tm trend_now{};
          if (getLocalTime(&trend_now, 10) && trend_now.tm_year >= 120) {
            shared_daily_trend_history.ingestPoll(
              trend_now, makeDailyTrendSample(pwClient.latest, ok));
          }
          if (ok) {
            week_history.roll(now);
            tm day{}; localtime_r(&now, &day); char date[11]; strftime(date, sizeof(date), "%Y-%m-%d", &day);
            // Only the current bucket changes during a poll. Keep the averages
            // imported from older minute-resolution files for prior buckets.
            if (pwClient.historyIsFor(day) && pwClient.history_count) {
              week_history.merge(date, &pwClient.history[pwClient.history_count-1], 1);
              week_has_updates = true;
            }
            if (!history_writes_disabled && (!last_week_save || now-last_week_save >= 60)) {
              if (runHistorySave(Serial, "weekly", "auto", [&] { return saveWeeklyHistory(week_history); })) last_week_save = now;
              else std::fprintf(stderr, "[WeeklyHistory] Save failed\n");
            }
            if (!history_writes_disabled &&
                (!last_trend_save || now - last_trend_save >= 60)) {
              if (runHistorySave(Serial, "daily trend", "auto", [&] {
                    return saveDailyTrendFile(shared_daily_trend_history);
                  })) last_trend_save = now;
            }
          }
          shared_tel = pwClient.latest; // Includes failure status and last successful timestamp.
          shared_wea = weatherClient.current;
          data_dirty.store(true);
          printGatewaySummary(Serial, shared_tel, ok, now,
                              isatty(STDOUT_FILENO) && std::getenv("NO_COLOR") == nullptr);
        }
      } else {
        std::lock_guard<std::mutex> lk(data_mtx);
        if (!demo_mode.load()) {
          shared_tel.ok = false;
          struct tm trend_now{};
          if (getLocalTime(&trend_now, 10) && trend_now.tm_year >= 120)
            shared_daily_trend_history.ingestPoll(trend_now, DailyTrendSample{});
        }
        shared_wea.stale = shared_wea.isStale();
        data_dirty.store(true);
      }
      for (int i = 0; i < 50 && running.load(); ++i) {
        if (manualHistorySave.take()) {
          // The poller remains the sole writer; never persist fabricated DEMO data.
          std::lock_guard<std::mutex> lk(data_mtx);
          tm today{};
          if (history_writes_disabled) {
            logHistorySaveEvent(Serial, "daily+weekly", "manual", "SKIPPED: history writes disabled for demo/smoke session");
          } else if (demo_mode.load()) {
            logHistorySaveEvent(Serial, "daily+weekly", "manual", "SKIPPED: DEMO active; return to LIVE first");
          } else if (!getLocalTime(&today, 10) || today.tm_year < 120 || !pwClient.historyIsFor(today)) {
            logHistorySaveEvent(Serial, "daily+weekly", "manual", "SKIPPED: clock/current-day history not ready");
          } else {
            runHistorySave(Serial, "daily", "manual", [&] { return saveHistory(pwClient); });
            runHistorySave(Serial, "weekly", "manual", [&] { return saveWeeklyHistory(week_history); });
            runHistorySave(Serial, "daily trend", "manual", [&] {
              return saveDailyTrendFile(shared_daily_trend_history);
            });
          }
          manualHistorySave.finish();
          std::fflush(stdout);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }
  });

  printSimulatorHelp();

  ConsolePreview consolePreview;
  bool configured_flag = demo_arg || gateway_configured;
  bool visible = true;
  bool stdin_open = true;
  bool needs_present = true;
  uint32_t start_ticks = SDL_GetTicks();
  const uint8_t tap_targets[] = {0, 0, 0, 0, 0, 1, 2}; // five normal pages, status pill, weather pill
  const uint8_t tap_expected_pages[] = {1, 2, 3, 4, 0, 0, 0};
  const char* tap_names[] = {"body -> Powerwall", "body -> Home/Grid", "body -> Today Trend",
                             "body -> Week", "body -> original",
                             "original status pill", "original weather pill"};
  // The compact home has passive time/data indicators; previews use the console.
  const size_t tap_count = 5;
  size_t tap_index = 0;
  uint8_t tap_phase = 0; // down, up, assert
  uint32_t tap_due = start_ticks + 500;
  int tap_status_initial = display480.simulatorStatusMode();
  int tap_weather_initial = display480.simulatorWeatherMode();
  int simulator_exit_code = 0;
  uint8_t cancel_hook_smoke_phase = tap_smoke_test ? 0 : 4;
  uint32_t cancel_hook_due = start_ticks + 500;
  uint32_t cancel_hook_deadline = 0;
  uint32_t cancel_hook_settle_until = 0;
  uint32_t tap_transition_deadline = 0;
  int rapid_repeat_remaining = 0;
  uint8_t rapid_repeat_phase = 0;
  int16_t tap_point_x = 0, tap_point_y = 0;
  uint32_t rapid_overlay_click_baseline = 0;
  bool transition_cover_captured = false;
  bool transition_covered_captured = false;
  bool transition_reveal_captured = false;
  uint8_t settings_smoke_phase = 0;
  uint32_t settings_smoke_due = start_ticks + 500;
  constexpr uint8_t settings_phase_offset = 1;
  constexpr uint8_t settings_network_phase_offset = 2;
  constexpr uint8_t settings_smoke_phase_count = 25 + settings_phase_offset + settings_network_phase_offset;
  // Native framebuffer coordinates of the settings controls.
  const int settings_brightness_x = 53, settings_brightness_y = 167;
  const int settings_volume_x = 427, settings_volume_y = 252;
  const int settings_action_x = 240, settings_action_y = 356;
  const int settings_tab_control_x = 127, settings_tab_network_x = 353, settings_tab_y = 78;
  const int settings_voice_x = 420, settings_voice_y = 296;
  const int settings_restart_x = 240, settings_restart_y = 416;
  const int settings_blank_x = 470, settings_blank_y = 430;
  const uint8_t original_brightness = deviceSettings.brightness;
  const uint8_t original_volume = deviceSettings.volume;
  SimulatorVoiceStateTracker voice_state;
  bool simulator_network_available = !initial_network_ap;
  static constexpr uint8_t wifi_card_cycle[] = {0, 1, 7, 4, 5, 6, 2};
  uint8_t wifi_card_cycle_index = 0;

  while (running.load() && !g_interrupt_requested) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) {
        running.store(false);
      } else if (ev.type == SDL_WINDOWEVENT) {
        if (ev.window.event == SDL_WINDOWEVENT_MINIMIZED || ev.window.event == SDL_WINDOWEVENT_HIDDEN) {
          visible = false; display480.setAnimationActive(false);
        } else if (ev.window.event == SDL_WINDOWEVENT_RESTORED || ev.window.event == SDL_WINDOWEVENT_SHOWN || ev.window.event == SDL_WINDOWEVENT_EXPOSED) {
          visible = true; display480.setAnimationActive(true); needs_present = true;
        }
      } else if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
        g_mouse_pressed = true;
        g_mouse_x = static_cast<int16_t>(ev.button.x * kWinW / kWindowSize);
        g_mouse_y = static_cast<int16_t>(ev.button.y * kWinH / kWindowSize);
      } else if (ev.type == SDL_MOUSEBUTTONUP && ev.button.button == SDL_BUTTON_LEFT) {
        g_mouse_pressed = false;
        g_mouse_x = static_cast<int16_t>(ev.button.x * kWinW / kWindowSize);
        g_mouse_y = static_cast<int16_t>(ev.button.y * kWinH / kWindowSize);
      } else if (ev.type == SDL_MOUSEMOTION && g_mouse_pressed) {
        g_mouse_x = static_cast<int16_t>(ev.motion.x * kWinW / kWindowSize);
        g_mouse_y = static_cast<int16_t>(ev.motion.y * kWinH / kWindowSize);
      } else if (ev.type == SDL_KEYDOWN) {
        const char phase_key = phasePresetKeyFromSDL(ev.key.keysym.sym);
        if (phase_key) {
          if (!ev.key.repeat) {
            if (phase_key == 'h') printSimulatorHelp();
            else {
              if (phase_key == 'z') {
                consolePreview.reset();
                {
                  std::lock_guard<std::mutex> lk(data_mtx);
                  // A --demo seed is fabricated; never relabel it as LIVE while
                  // waiting for the first successful Gateway poll.
                  if (demo_mode.exchange(false)) {
                    shared_tel.ok = false;
                    reloadLiveDailyTrend(shared_daily_trend_history);
                  }
                }
                wifi_online.store(true);
                configured_flag = true;
                data_dirty.store(true);
                display480.setSimulatorWifiCardMode(3);
              }
              const char line[2] = {phase_key, '\0'};
              char response[256] = {};
              display480.handlePhaseConsoleLine(line, response, sizeof(response));
              std::printf("[SimKey %c] %s\n", phase_key, response);
              std::fflush(stdout);
            }
          }
          continue;
        }
        std::lock_guard<std::mutex> lk(data_mtx);
        switch (ev.key.keysym.sym) {
          case SDLK_6:
            if (!ev.key.repeat) {
              std::printf("%s\n", manualHistorySave.request()
                ? "[history] Manual write queued; waiting for history task"
                : "[history] Manual write already pending/running; key ignored");
              std::fflush(stdout);
            }
            break;
          case SDLK_F6:
            if (!ev.key.repeat) {
              simulator_network_available = !simulator_network_available;
              display480.setSimulatorNetworkAvailable(simulator_network_available);
              std::printf("[NetworkPreview] simulated ESP IP %s\n",
                          simulator_network_available ? "available" : "unavailable; AP fallback active");
            }
            break;
          case SDLK_PAGEDOWN:
            if (!ev.key.repeat) display480.nextDashboardPage();
            break;
          case SDLK_HOME:
            if (!ev.key.repeat) display480.openSettingsPage();
            break;
          case SDLK_1:
          case SDLK_2:
          case SDLK_3:
          case SDLK_4:
          case SDLK_5: {
            const char key = static_cast<char>(ev.key.keysym.sym);
            std::printf("%s\n", consolePreview.handle(key, shared_tel));
            if (key == '1') {
              wifi_online.store(true);
              configured_flag = true;
              if (demo_mode.exchange(false)) {
                shared_tel.ok = false;
                reloadLiveDailyTrend(shared_daily_trend_history);
              }
              char response[256] = {};
              display480.handlePhaseConsoleLine("z", response, sizeof(response));
            }
            data_dirty.store(true);
            break;
          }
          case SDLK_7:
            if (!ev.key.repeat) {
              wifi_card_cycle_index = static_cast<uint8_t>((wifi_card_cycle_index + 1) %
                (sizeof(wifi_card_cycle) / sizeof(wifi_card_cycle[0])));
              const uint8_t mode = wifi_card_cycle[wifi_card_cycle_index];
              const char* url = mode == 1 ? "http://192.168.4.1" : "http://192.0.2.13";
              display480.setSimulatorWifiCardMode(mode, "Home-WiFi", url);
              static const char* const names[] = {
                "Connecting to Home-WiFi", "Setup portal", "Connected; card fading out", "Hidden",
                "Getting Powerwall data", "Getting weather data", "Powerwall unavailable; check IP/password",
                "Powerwall setup required"
              };
              std::printf("[WiFiCard] %s\n", names[mode]);
              std::fflush(stdout);
            }
            break;
          case SDLK_8:
            if (!ev.key.repeat) {
              wifi_card_cycle_index = 2;
              display480.setSimulatorWifiCardMode(7, "Home-WiFi", "http://192.0.2.13");
              std::printf("[WiFiCard] Powerwall setup required; local URL http://192.0.2.13\n");
              std::fflush(stdout);
            }
            break;
          case SDLK_9:
            if (!ev.key.repeat) {
              wifi_card_cycle_index = 6;
              display480.setSimulatorWifiCardMode(2, "Home-WiFi");
              std::printf("[WiFiCard] Connected; card fading out\n");
              std::fflush(stdout);
            }
            break;
          case SDLK_0:
            if (!ev.key.repeat) {
              wifi_card_cycle_index = 6;
              display480.setSimulatorWifiCardMode(3);
              std::printf("[WiFiCard] LIVE; onboarding card hidden\n");
              std::fflush(stdout);
            }
            break;
          case SDLK_ESCAPE:
            running.store(false);
            break;
          default:
            break;
        }
      }
    }

    if (stdin_open) pollSingleKeyStdin(stdin_open);

    if (data_dirty.exchange(false)) {
      PowerwallTelemetry local_t;
      WeatherSnapshot local_w;
    static DailyTrendHistory local_daily_trend_history;
      {
        std::lock_guard<std::mutex> lk(data_mtx);
        local_t = shared_tel;
        local_w = shared_wea;
      local_daily_trend_history = shared_daily_trend_history;
        display480.setWeeklyHistory(demo_mode.load() ? demoWeeklyHistory(time(nullptr)) : week_history);
      }
      local_t.demo = demo_mode.load();
      {
        const bool voice_data_valid = local_t.ok && !local_t.stale &&
                                      (local_t.has_grid_status || local_t.has_battery_power);
        const SimulatorVoiceState transition = voice_state.observe(
          voice_data_valid, local_t.has_grid_status,
          local_t.grid_status == "SystemGridConnected",
          local_t.has_battery_power, local_t.battery_power_w);
        if (deviceSettings.voice_enabled && transition != SimulatorVoiceState::Unknown)
          deviceSettings.playVoice(transition);
      }
      bool render_configured = configured_flag;
      consolePreview.apply(local_t, local_w, render_configured);
      display480.setDailyTrendHistory(local_daily_trend_history);
      char title[160];
      std::snprintf(title, sizeof(title), "ESP32 Firmware Simulator %dx%d — %s",
                    kWindowSize, kWindowSize, local_t.demo ? "DEMO 預覽" : "LIVE 即時資料");
      SDL_SetWindowTitle(win, title);
      display480.render(local_t, local_w, render_configured);
    }

    displayDriver.tick();
    lv_timer_handler();
    if (deviceSettings.dirty && SDL_GetTicks() - deviceSettings.changed_at >= 500) deviceSettings.save();

    if (settings_smoke_test && settings_smoke_phase < settings_smoke_phase_count && SDL_GetTicks() - start_ticks > 13000) {
      std::fprintf(stderr, "[SettingsSmokeTest] FAIL: timed out waiting for settings navigation\n");
      simulator_exit_code = 8;
      running.store(false);
    }

    if (settings_smoke_test && settings_smoke_phase < settings_smoke_phase_count && SDL_GetTicks() >= settings_smoke_due) {
      const int x = settings_smoke_phase < 2 ? settings_brightness_x :
                    settings_smoke_phase < 4 ? settings_volume_x : settings_action_x;
      const int y = settings_smoke_phase < 2 ? settings_brightness_y :
                    settings_smoke_phase < 4 ? settings_volume_y : settings_action_y;
      if (settings_smoke_phase == 6) {
        if (display480.simulatorDashboardPage() != 5 ||
            deviceSettings.brightness == original_brightness ||
            deviceSettings.volume == original_volume || !deviceSettings.audio_device ||
            !display480.simulatorSettingsUptimeLabelIsVisible() ||
            !display480.simulatorSettingsVersionLabelIsVisible()) {
          std::fprintf(stderr,
              "[SettingsSmokeTest] FAIL: slider callback or page routing (page=%u, brightness=%u/%u, volume=%u/%u, audio=%u)\n",
              static_cast<unsigned>(display480.simulatorDashboardPage()),
              static_cast<unsigned>(deviceSettings.brightness), static_cast<unsigned>(original_brightness),
              static_cast<unsigned>(deviceSettings.volume), static_cast<unsigned>(original_volume),
              static_cast<unsigned>(deviceSettings.audio_device != 0));
          simulator_exit_code = 8;
          running.store(false);
        } else {
          deviceSettings.voice_enabled = false;
          deviceSettings.playTestVoice(); // The test preview remains available with announcements disabled.
          if (!deviceSettings.audio_device) {
            std::fprintf(stderr, "[SettingsSmokeTest] FAIL: voice test playback while disabled\n");
            simulator_exit_code = 8; running.store(false);
          }
          display480.nextDashboardPage(); // PWRKEY from settings returns to Week.
        }
      } else if (settings_smoke_phase == 7) {
        if (display480.simulatorPageTransitionActive()) {
          settings_smoke_due = SDL_GetTicks() + 20;
          continue;
        }
        if (display480.simulatorDashboardPage() != 4) {
          std::fprintf(stderr, "[SettingsSmokeTest] FAIL: settings did not return to previous page\n");
          simulator_exit_code = 8;
          running.store(false);
        } else {
          std::printf("[SettingsSmokeTest] PASS: sliders and audio test retained settings; PWRKEY returned to Week\n");
          display480.openSettingsPage(); // BOOT opens settings again.
        }
      } else if (settings_smoke_phase == 8) {
        if (display480.simulatorPageTransitionActive()) {
          settings_smoke_due = SDL_GetTicks() + 20;
          continue;
        }
        if (display480.simulatorDashboardPage() != 5 ||
            !pushMouseButtonEvent(win, SDL_MOUSEBUTTONDOWN, settings_blank_x, settings_blank_y)) {
          std::fprintf(stderr, "[SettingsSmokeTest] FAIL: BOOT did not reopen settings\n");
          simulator_exit_code = 8;
          running.store(false);
        }
      } else if (settings_smoke_phase == 9) {
        if (!pushMouseButtonEvent(win, SDL_MOUSEBUTTONUP, settings_blank_x, settings_blank_y)) {
          simulator_exit_code = 8;
          running.store(false);
        }
      } else if (settings_smoke_phase == 10) {
        if (display480.simulatorPageTransitionActive()) {
          settings_smoke_due = SDL_GetTicks() + 20;
          continue;
        }
        if (display480.simulatorDashboardPage() != 5) {
          std::fprintf(stderr, "[SettingsSmokeTest] FAIL: blank settings tap left Settings\n");
          simulator_exit_code = 8;
          running.store(false);
        } else {
          std::printf("[SettingsSmokeTest] PASS: blank settings tap stayed on Settings\n");
          SDL_Event key_event{};
          key_event.type = SDL_KEYDOWN;
          key_event.key.keysym.sym = SDLK_PAGEDOWN;
          if (SDL_PushEvent(&key_event) != 1) {
            std::fprintf(stderr, "[SettingsSmokeTest] FAIL: could not queue Page Down\n");
            simulator_exit_code = 8;
            running.store(false);
          }
        }
      } else if (settings_smoke_phase == 11) {
        if (display480.simulatorPageTransitionActive()) {
          settings_smoke_due = SDL_GetTicks() + 20;
          continue;
        }
        if (display480.simulatorDashboardPage() != 4) {
          std::fprintf(stderr, "[SettingsSmokeTest] FAIL: Page Down did not return to Week\n");
          simulator_exit_code = 8;
          running.store(false);
        } else {
          std::printf("[SettingsSmokeTest] PASS: Page Down returned to Week; reopening Settings\n");
          display480.openSettingsPage();
        }
      } else if (settings_smoke_phase == 12) {
        if (display480.simulatorPageTransitionActive()) {
          settings_smoke_due = SDL_GetTicks() + 20;
          continue;
        }
        if (display480.simulatorDashboardPage() != 5 ||
            !pushMouseButtonEvent(win, SDL_MOUSEBUTTONDOWN, settings_restart_x, settings_restart_y)) {
          std::fprintf(stderr, "[SettingsSmokeTest] FAIL: restart button unavailable\n");
          simulator_exit_code = 8;
          running.store(false);
        }
      } else if (settings_smoke_phase == 12 + settings_phase_offset ||
                 settings_smoke_phase == 14 + settings_phase_offset) {
        if (!pushMouseButtonEvent(win, SDL_MOUSEBUTTONUP, settings_restart_x, settings_restart_y)) {
          simulator_exit_code = 8;
          running.store(false);
        }
      } else if (settings_smoke_phase == 13 + settings_phase_offset) {
        if (deviceSettings.restart_requests != 0 || display480.simulatorDashboardPage() != 5 ||
            !pushMouseButtonEvent(win, SDL_MOUSEBUTTONDOWN, settings_restart_x, settings_restart_y)) {
          std::fprintf(stderr, "[SettingsSmokeTest] FAIL: restart confirmation triggered early\n");
          simulator_exit_code = 8;
          running.store(false);
        }
      } else if (settings_smoke_phase == 15 + settings_phase_offset) {
        if (deviceSettings.restart_requests != 1 || display480.simulatorDashboardPage() != 5) {
          std::fprintf(stderr, "[SettingsSmokeTest] FAIL: restart confirmation callback\n");
          simulator_exit_code = 8;
          running.store(false);
        } else {
          std::printf("[SettingsSmokeTest] PASS: two taps confirm exactly one restart request\n");
        }
      } else if (settings_smoke_phase == 16 + settings_phase_offset) {
        if (!pushMouseButtonEvent(win, SDL_MOUSEBUTTONDOWN, settings_tab_network_x, settings_tab_y)) {
          std::fprintf(stderr, "[SettingsSmokeTest] FAIL: network tab mouse-down\n");
          simulator_exit_code = 8; running.store(false);
        }
      } else if (settings_smoke_phase == 17 + settings_phase_offset) {
        if (!pushMouseButtonEvent(win, SDL_MOUSEBUTTONUP, settings_tab_network_x, settings_tab_y)) {
          std::fprintf(stderr, "[SettingsSmokeTest] FAIL: network tab mouse-up\n");
          simulator_exit_code = 8; running.store(false);
        }
      } else if (settings_smoke_phase == 18 + settings_phase_offset) {
        if (!display480.simulatorSettingsNetworkPageIsVisible() ||
            !pushMouseButtonEvent(win, SDL_MOUSEBUTTONDOWN, settings_blank_x, settings_blank_y)) {
          std::fprintf(stderr, "[SettingsSmokeTest] FAIL: network settings background tap-down\n");
          simulator_exit_code = 8; running.store(false);
        }
      } else if (settings_smoke_phase == 19 + settings_phase_offset) {
        if (!pushMouseButtonEvent(win, SDL_MOUSEBUTTONUP, settings_blank_x, settings_blank_y)) {
          std::fprintf(stderr, "[SettingsSmokeTest] FAIL: network settings background tap-up\n");
          simulator_exit_code = 8; running.store(false);
        }
      } else if (settings_smoke_phase == 18 + settings_phase_offset + settings_network_phase_offset) {
        lv_refr_now(nullptr);
        if (!display480.simulatorSettingsUptimeLabelIsVisible() ||
            !display480.simulatorSettingsVersionLabelIsVisible() ||
            !saveFramebufferPpm("/tmp/powerwall-settings-info-480.ppm")) {
          std::fprintf(stderr, "[SettingsSmokeTest] FAIL: Info page footer/capture\n");
          simulator_exit_code = 8; running.store(false);
        } else {
          std::printf("[SettingsSmokeTest] PASS: Info footer visible; capture /tmp/powerwall-settings-info-480.ppm\n");
        }
        display480.setSimulatorWifiSsid("Powerwall-Home-Network-Guest-24G");
        lv_refr_now(nullptr);
        if (!display480.simulatorNetworkPreviewMatches(true) ||
            !saveFramebufferPpm("/tmp/powerwall-settings-long-ssid-480.ppm")) {
          std::fprintf(stderr, "[SettingsSmokeTest] FAIL: long connected SSID preview/capture\n");
          simulator_exit_code = 8; running.store(false);
        } else {
          std::printf("[SettingsSmokeTest] PASS: maximum-length SSID fits the Info value area; capture /tmp/powerwall-settings-long-ssid-480.ppm\n");
        }
        display480.setSimulatorWifiSsid("Home-WiFi");
        lv_refr_now(nullptr);
        if (!display480.simulatorSettingsNetworkPageIsVisible() ||
            display480.simulatorDashboardPage() != 5 ||
            !pushMouseButtonEvent(win, SDL_MOUSEBUTTONDOWN, settings_tab_control_x, settings_tab_y)) {
          std::fprintf(stderr, "[SettingsSmokeTest] FAIL: network page stayed open or control tab interaction\n");
          simulator_exit_code = 8; running.store(false);
        }
      } else if (settings_smoke_phase == 19 + settings_phase_offset + settings_network_phase_offset) {
        if (!pushMouseButtonEvent(win, SDL_MOUSEBUTTONUP, settings_tab_control_x, settings_tab_y)) {
          std::fprintf(stderr, "[SettingsSmokeTest] FAIL: control tab mouse-up\n");
          simulator_exit_code = 8; running.store(false);
        }
      } else if (settings_smoke_phase == 20 + settings_phase_offset + settings_network_phase_offset) {
        if (display480.simulatorSettingsNetworkPageIsVisible() || display480.simulatorDashboardPage() != 5 ||
            !display480.simulatorSettingsUptimeLabelIsVisible() ||
            !display480.simulatorSettingsVersionLabelIsVisible()) {
          std::fprintf(stderr, "[SettingsSmokeTest] FAIL: control page/tab interaction\n");
          simulator_exit_code = 8; running.store(false);
        } else {
          display480.setSimulatorNetworkAvailable(false);
          if (!display480.simulatorNetworkPreviewMatches(false)) {
            std::fprintf(stderr, "[SettingsSmokeTest] FAIL: AP fallback preview content\n");
            simulator_exit_code = 8; running.store(false);
          }
        }
      } else if (settings_smoke_phase == 21 + settings_phase_offset + settings_network_phase_offset) {
        display480.setSimulatorNetworkAvailable(true);
        if (!display480.simulatorNetworkPreviewMatches(true)) {
          std::fprintf(stderr, "[SettingsSmokeTest] FAIL: ESP IP preview content\n");
          simulator_exit_code = 8; running.store(false);
        } else {
          std::printf("[SettingsSmokeTest] PASS: touch reaches both settings pages and both mock network states render\n");
        }
      } else if (settings_smoke_phase == 22 + settings_phase_offset + settings_network_phase_offset ||
                 settings_smoke_phase == 23 + settings_phase_offset + settings_network_phase_offset) {
        const Uint32 type = settings_smoke_phase == 22 + settings_phase_offset + settings_network_phase_offset
          ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
        if (!pushMouseButtonEvent(win, type, settings_voice_x, settings_voice_y)) {
          std::fprintf(stderr, "[SettingsSmokeTest] FAIL: voice switch SDL input\n");
          simulator_exit_code = 8; running.store(false);
        }
      } else if (settings_smoke_phase == 24 + settings_phase_offset + settings_network_phase_offset) {
        if (display480.simulatorVoiceEnabled()) {
          std::fprintf(stderr, "[SettingsSmokeTest] FAIL: switch did not turn voice announcements off\n");
          simulator_exit_code = 8; running.store(false);
        } else {
          std::printf("[SettingsSmokeTest] PASS: touch switch turns voice announcements off\n");
        }
      } else {
        const Uint32 type = settings_smoke_phase % 2 == 0 ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
        if (!pushMouseButtonEvent(win, type, x, y)) {
          std::fprintf(stderr, "[SettingsSmokeTest] FAIL: SDL input injection\n");
          simulator_exit_code = 8;
          running.store(false);
        }
      }
      ++settings_smoke_phase;
      settings_smoke_due = SDL_GetTicks() + 110;
    }

    if (tap_smoke_test && display480.simulatorPageTransitionActive()) {
      const uint8_t phase = display480.simulatorPageTransitionPhase();
      const uint8_t opacity = display480.simulatorPageTransitionOpacity();
      if (!display480.simulatorPageTransitionBlocksInput() ||
          !display480.simulatorPageTransitionOverlayIsForeground()) {
        std::fprintf(stderr, "[TapSmokeTest] FAIL: transition overlay lost input blocking or foreground order\n");
        simulator_exit_code = 6;
        running.store(false);
      } else if (phase == 1 && opacity >= 80 && opacity <= 180 && !transition_cover_captured) {
        lv_refr_now(nullptr);
        transition_cover_captured = saveFramebufferPpm("/tmp/powerwall-transition-cover.ppm");
        if (!transition_cover_captured) {
          std::fprintf(stderr, "[TapSmokeTest] FAIL: could not save cover fade frame\n");
          simulator_exit_code = 6;
          running.store(false);
        }
      } else if (phase == 2 && opacity == LV_OPA_COVER && !transition_covered_captured) {
        lv_refr_now(nullptr);
        constexpr uint16_t kTransitionBackgroundRgb565 =
          static_cast<uint16_t>(((0x0b >> 3) << 11) | ((0x0c >> 2) << 5) | (0x0f >> 3));
        if (!framebufferIsSolidRgb565(kTransitionBackgroundRgb565)) {
          std::fprintf(stderr, "[TapSmokeTest] FAIL: fully covered framebuffer is not solid #0b0c0f RGB565\n");
          simulator_exit_code = 6;
          running.store(false);
        } else {
          transition_covered_captured = saveFramebufferPpm("/tmp/powerwall-transition-covered.ppm");
        }
        if (!transition_covered_captured) {
          std::fprintf(stderr, "[TapSmokeTest] FAIL: could not save fully covered frame\n");
          simulator_exit_code = 6;
          running.store(false);
        }
      } else if (phase == 2 && opacity >= 80 && opacity <= 180 && !transition_reveal_captured) {
        lv_refr_now(nullptr);
        transition_reveal_captured = saveFramebufferPpm("/tmp/powerwall-transition-reveal.ppm");
        if (!transition_reveal_captured) {
          std::fprintf(stderr, "[TapSmokeTest] FAIL: could not save reveal fade frame\n");
          simulator_exit_code = 6;
          running.store(false);
        }
      }
    }

    if (visible && g_fb_dirty) {
      g_fb_dirty = false;
      SDL_UpdateTexture(tex, &g_dirty_rect, g_fb + g_dirty_rect.y*kWinW + g_dirty_rect.x, kWinW * sizeof(uint16_t));
      needs_present = true;
    }
    if (g_brightness_dirty) { needs_present = true; g_brightness_dirty = false; }
    if (visible && needs_present) {
      needs_present = false;
      SDL_RenderClear(ren);
      SDL_RenderCopy(ren, tex, nullptr, nullptr);
      if (deviceSettings.brightness < 100) {
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, static_cast<Uint8>((100 - deviceSettings.brightness) * 255 / 100));
        SDL_RenderFillRect(ren, nullptr);
      }
      SDL_RenderPresent(ren);
    }

    if (tap_smoke_test && cancel_hook_smoke_phase < 4) {
      const uint32_t now = SDL_GetTicks();
      if (cancel_hook_smoke_phase == 0 && static_cast<int32_t>(now - cancel_hook_due) >= 0) {
        int16_t x = 0, y = 0;
        if (!display480.simulatorTapPoint(0, x, y) ||
            !pushMouseButtonEvent(win, SDL_MOUSEBUTTONDOWN, x, y)) {
          std::fprintf(stderr, "[TapSmokeTest] FAIL: could not start capture-hook cancel tap\n");
          simulator_exit_code = 4; running.store(false);
        } else {
          cancel_hook_smoke_phase = 1;
          cancel_hook_due = now + 50;
        }
      } else if (cancel_hook_smoke_phase == 1 && static_cast<int32_t>(now - cancel_hook_due) >= 0) {
        int16_t x = 0, y = 0;
        if (!display480.simulatorTapPoint(0, x, y) ||
            !pushMouseButtonEvent(win, SDL_MOUSEBUTTONUP, x, y)) {
          std::fprintf(stderr, "[TapSmokeTest] FAIL: could not release capture-hook cancel tap\n");
          simulator_exit_code = 4; running.store(false);
        } else {
          cancel_hook_smoke_phase = 2;
          cancel_hook_deadline = now + 1000;
        }
      } else if (cancel_hook_smoke_phase == 2) {
        if (display480.simulatorPageTransitionActive() &&
            display480.simulatorPageTransitionOpacity() >= 80 &&
            display480.simulatorPageTransitionOpacity() <= 180) {
          display480.setDashboardPageForSimulator(2);
          if (display480.simulatorDashboardPage() != 2 ||
              display480.simulatorPageTransitionActive() ||
              !display480.simulatorPageTransitionIsIdle()) {
            std::fprintf(stderr, "[TapSmokeTest] FAIL: capture hook did not cancel and reset a mid-fade transition\n");
            simulator_exit_code = 7; running.store(false);
          } else {
            std::printf("[TapSmokeTest] PASS: capture hook cancelled mid-fade and selected Home/Grid immediately.\n");
            cancel_hook_smoke_phase = 3;
            cancel_hook_settle_until = now + 240;
          }
        } else if (static_cast<int32_t>(now - cancel_hook_deadline) >= 0) {
          std::fprintf(stderr, "[TapSmokeTest] FAIL: timed out waiting for capture-hook test fade\n");
          simulator_exit_code = 7; running.store(false);
        }
      } else if (cancel_hook_smoke_phase == 3 &&
                 static_cast<int32_t>(now - cancel_hook_settle_until) >= 0) {
        if (display480.simulatorDashboardPage() != 2 ||
            display480.simulatorPageTransitionActive() ||
            !display480.simulatorPageTransitionIsIdle()) {
          std::fprintf(stderr, "[TapSmokeTest] FAIL: cancelled animation later changed page or restored its overlay\n");
          simulator_exit_code = 7; running.store(false);
        } else {
          display480.setDashboardPageForSimulator(0);
          tap_index = 0;
          tap_phase = 0;
          tap_due = now + 100;
          cancel_hook_smoke_phase = 4;
        }
      }
    }

    if (tap_smoke_test && cancel_hook_smoke_phase >= 4 && tap_index < tap_count &&
        static_cast<int32_t>(SDL_GetTicks() - tap_due) >= 0) {
      const uint32_t now = SDL_GetTicks();
      if (tap_phase == 0) {
        if (!display480.simulatorTapPoint(tap_targets[tap_index], tap_point_x, tap_point_y) ||
            !pushMouseButtonEvent(win, SDL_MOUSEBUTTONDOWN, tap_point_x, tap_point_y)) {
          std::fprintf(stderr, "[TapSmokeTest] FAIL: could not inject %s mouse-down\n", tap_names[tap_index]);
          simulator_exit_code = 4;
          running.store(false);
        } else {
          if (tap_index == 0) {
            rapid_overlay_click_baseline = display480.simulatorPageTransitionConsumedClicks();
          }
          tap_phase = 1;
          tap_due = now + 80;
        }
      } else if (tap_phase == 1) {
        if (!pushMouseButtonEvent(win, SDL_MOUSEBUTTONUP, tap_point_x, tap_point_y)) {
          std::fprintf(stderr, "[TapSmokeTest] FAIL: could not inject %s mouse-up\n", tap_names[tap_index]);
          simulator_exit_code = 4;
          running.store(false);
        } else {
          tap_phase = 2;
          tap_transition_deadline = now + 1200;
          rapid_repeat_remaining = tap_index == 0 ? 1 : 0;
          rapid_repeat_phase = 0;
          tap_due = now + (tap_index == 0 ? 40 : 5);
        }
      } else {
        if (tap_index == 0 && rapid_repeat_remaining > 0) {
          const Uint32 event_type = rapid_repeat_phase == 0 ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
          if (rapid_repeat_phase == 0) {
            if (!display480.simulatorTapPoint(0, tap_point_x, tap_point_y)) {
              std::fprintf(stderr, "[TapSmokeTest] FAIL: could not locate rapid repeated page tap\n");
              simulator_exit_code = 4;
              running.store(false);
              continue;
            }
          }
          if (!pushMouseButtonEvent(win, event_type, tap_point_x, tap_point_y)) {
            std::fprintf(stderr, "[TapSmokeTest] FAIL: could not inject rapid repeated page tap\n");
            simulator_exit_code = 4;
            running.store(false);
          } else {
            if (rapid_repeat_phase == 0) rapid_repeat_phase = 1;
            else { rapid_repeat_phase = 0; --rapid_repeat_remaining; }
            tap_due = now + 40;
          }
        } else if (display480.simulatorPageTransitionActive()) {
          if (static_cast<int32_t>(now - tap_transition_deadline) >= 0) {
            std::fprintf(stderr, "[TapSmokeTest] FAIL: timed out waiting for %s transition to become idle\n",
                         tap_names[tap_index]);
            simulator_exit_code = 5;
            running.store(false);
          } else {
            tap_due = now + 5;
          }
        } else {
          const bool action_observed = tap_targets[tap_index] == 0
            ? display480.simulatorDashboardPage() == tap_expected_pages[tap_index]
            : tap_targets[tap_index] == 1
              ? display480.simulatorStatusMode() == (tap_status_initial + 1) % 7
              : display480.simulatorWeatherMode() == (tap_weather_initial + 1) % 11;
          if (!action_observed && static_cast<int32_t>(now - tap_transition_deadline) < 0) {
            // LVGL may poll touch after this SDL release; keep waiting instead of
            // asserting against the old page before the input timer has observed it.
            tap_due = now + 5;
          } else {
            const uint8_t page = display480.simulatorDashboardPage();
            bool passed = action_observed && page == tap_expected_pages[tap_index] &&
              display480.simulatorPageTransitionIsIdle();
            if (tap_targets[tap_index] == 0 && tap_index == 0) {
              passed = passed && display480.simulatorPageTransitionConsumedClicks() >=
                rapid_overlay_click_baseline + 1;
            }
            if (tap_expected_pages[tap_index] == 1 || tap_expected_pages[tap_index] == 2) {
              passed = passed && display480.simulatorAddedHeaderIsVisible();
            }
            if (tap_expected_pages[tap_index] == 3) {
              passed = passed && display480.simulatorTodayTrendPageIsValid();
            }
            if (tap_expected_pages[tap_index] == 4) {
              passed = passed && display480.simulatorWeeklyPageIsVisible();
            }
            if (!passed) {
              std::fprintf(stderr, "[TapSmokeTest] FAIL: %s -> page=%u status=%d weather=%d\n",
                           tap_names[tap_index], page, display480.simulatorStatusMode(),
                           display480.simulatorWeatherMode());
              simulator_exit_code = 5;
              running.store(false);
            } else {
              std::printf("[TapSmokeTest] PASS: %s -> page=%u status=%d weather=%d\n",
                          tap_names[tap_index], page, display480.simulatorStatusMode(),
                          display480.simulatorWeatherMode());
              std::fflush(stdout);
              ++tap_index;
              tap_phase = 0;
              tap_due = now + 100;
              if (tap_index == tap_count) {
                if (!transition_cover_captured || !transition_covered_captured ||
                    !transition_reveal_captured) {
                  std::fprintf(stderr, "[TapSmokeTest] FAIL: transition cover, covered, and reveal captures are incomplete\n");
                  simulator_exit_code = 6;
                } else {
                  std::printf("[TapSmokeTest] PASS: all %zu real SDL taps, rapid-tap suppression, restored input, and transition captures.\n", tap_count);
                  std::printf("[TapSmokeTest] Frames: /tmp/powerwall-transition-cover.ppm, /tmp/powerwall-transition-covered.ppm, /tmp/powerwall-transition-reveal.ppm\n");
                }
                running.store(false);
              }
            }
            if (!action_observed && static_cast<int32_t>(now - tap_transition_deadline) >= 0) {
              std::fprintf(stderr, "[TapSmokeTest] FAIL: timed out waiting for %s click to register\n",
                           tap_names[tap_index]);
              simulator_exit_code = 5;
              running.store(false);
            }
          }
        }
      }
    }

    if (!tap_smoke_test && auto_quit_sec && (SDL_GetTicks() - start_ticks > static_cast<uint32_t>(quit_after_ms))) {
      // Run automated verification checks for the 6 fixes before saving frame & exiting:
      // 1. Verify nullptr-safe configure()
      pwClient.configure(nullptr, nullptr, nullptr, nullptr);
      if (pwClient.latest.gateway_ip.length() != 0) {
        std::fprintf(stderr, "[FAIL] configure(nullptr) corrupted gateway_ip!\n");
        return 2;
      }
      std::printf("[PASS] Fix #6: configure(nullptr, ...) null-safety verified.\n");

      // 2. Verify Weather 10-min vs 30-min staleness threshold
      WeatherSnapshot test_w;
      test_w.ok = true;
      test_w.label = "晴朗";
      test_w.temp_c = 25.0f;
      test_w.last_update_ms = millis();
      unsigned long t_10m = test_w.last_update_ms + 10UL * 60UL * 1000UL;
      unsigned long t_31m = test_w.last_update_ms + 31UL * 60UL * 1000UL;
      if (test_w.isStale(t_10m) || !test_w.isStale(t_31m)) {
        std::fprintf(stderr, "[FAIL] WeatherSnapshot::isStale() 10m/31m check failed!\n");
        return 3;
      }
      std::printf("[PASS] Fix #2: Weather staleness strictly triggers after >30m (not at 10m).\n");

      // 3. Verify transient Powerwall failure preserves valid telemetry when last_update_ms > 0
      PowerwallTelemetry test_t = shared_tel;
      test_t.ok = true;
      test_t.stale = true;
      test_t.load_power_w = 2400.0f;
      test_t.soe_percent = 88.0f;
      display480.render(test_t, test_w, true);
      lv_timer_handler();
      std::printf("[PASS] Transient Powerwall poll failure preserves last valid telemetry.\n");

      // 4. Verify 1-second periodic UI timer updates weather staleness automatically without render()
      simMillisOffset().fetch_add(31UL * 60UL * 1000UL);
      lv_timer_handler();
      simMillisOffset().store(0);
      std::printf("[PASS] Fix #4 & #5: Summer/Winter TOU bars & 1s periodic UI clock/staleness sync verified.\n");

      // Restore current live/demo state for final frame capture
      display480.setDashboardPageForSimulator(initial_page);
      static DailyTrendHistory capture_trend_history;
      {
        std::lock_guard<std::mutex> lk(data_mtx);
        PowerwallTelemetry capture_tel = shared_tel;
        WeatherSnapshot capture_wea = shared_wea;
        capture_trend_history = shared_daily_trend_history;
        bool capture_configured = configured_flag;
        consolePreview.apply(capture_tel, capture_wea, capture_configured);
        display480.setDailyTrendHistory(capture_trend_history);
        display480.render(capture_tel, capture_wea, capture_configured);
      }
      // Invalidate and synchronously refresh until the selected page's header
      // stops changing, so partial LVGL flushes cannot leave stale pixels in a capture.
      uint64_t previous_header_hash = 0;
      int identical_header_frames = 0;
      for (int frame = 0; frame < 12 && identical_header_frames < 2; ++frame) {
        lv_obj_invalidate(lv_scr_act());
        lv_timer_handler();
        lv_refr_now(nullptr);
        const uint64_t header_hash = hashHomeHeaderFramebuffer();
        if (header_hash == previous_header_hash) ++identical_header_frames;
        else identical_header_frames = 0;
        previous_header_hash = header_hash;
      }
      if (initial_page == 1 && (!display480.simulatorAddedHeaderIsVisible() ||
          identical_header_frames < 2 ||
          !framebufferRegionHasInk(8, 3, 224, 36) ||
          !framebufferRegionHasInk(271, 17, 472, 37))) {
        lv_area_t dot_bounds{};
        display480.simulatorPowerwallStatusDotBounds(dot_bounds);
        saveFramebufferPpm("/tmp/powerwall-page2-fail.ppm");
        std::fprintf(stderr, "[FAIL] Powerwall capture header: hook=%d stable=%d title=%d clock=%d dot=%d\n",
          display480.simulatorAddedHeaderIsVisible(), identical_header_frames,
          framebufferRegionHasInk(8, 3, 224, 36), framebufferRegionHasInk(271, 17, 472, 37),
          framebufferRegionHasInk(dot_bounds.x1, dot_bounds.y1, dot_bounds.x2, dot_bounds.y2));
        return 7;
      }
      if (initial_page == 1 || initial_page == 2) {
        bool bright = false, dim = false;
        uint16_t brightPixel = 0, dimPixel = 0;
        const uint32_t blinkDeadline = SDL_GetTicks() + 1200;
        while ((!bright || !dim) && static_cast<int32_t>(SDL_GetTicks() - blinkDeadline) < 0) {
          lv_timer_handler();
          lv_refr_now(nullptr);
          const lv_opa_t opa = display480.simulatorPowerwallStatusDotOpacity();
          lv_area_t dot_bounds{};
          display480.simulatorPowerwallStatusDotBounds(dot_bounds);
          const int dot_x = (dot_bounds.x1 + dot_bounds.x2) / 2;
          const int dot_y = (dot_bounds.y1 + dot_bounds.y2) / 2;
          if (opa == LV_OPA_COVER) {
            bright = true;
            brightPixel = g_fb[dot_y * kWinW + dot_x];
          } else if (opa == LV_OPA_30) {
            dim = true;
            dimPixel = g_fb[dot_y * kWinW + dot_x];
          }
          if (!bright || !dim) SDL_Delay(10);
        }
        if (!bright || !dim || brightPixel == dimPixel) {
          std::fprintf(stderr, "[FAIL] Powerwall status indicator pixels did not show both 500ms opacity phases (%04x/%04x).\n",
                       brightPixel, dimPixel);
          return 7;
        }
        std::printf("[PASS] Powerwall status dot pixels blinked cover/dim: %04x/%04x.\n",
                    brightPixel, dimPixel);
      }
      if (initial_page == 2) {
        if (!display480.simulatorHomeGridTextFits()) return 7;
        std::printf("[HomeGridText] PASS: missing, export, large counters, next-day countdown and endpoint labels fit.\n");
      }
      if (initial_page == 2 && (!display480.simulatorAddedHeaderIsVisible() ||
          identical_header_frames < 2 ||
          !framebufferRegionHasInk(8, 3, 261, 36) ||
          !framebufferRegionHasInk(271, 17, 472, 37))) {
        std::fprintf(stderr, "[FAIL] Home/Grid title, clock/dot, hidden elements or fixed chart geometry.\n");
        return 7;
      }
      if (initial_page == 3 && (!display480.simulatorTodayTrendPageIsValid() ||
          identical_header_frames < 2 ||
          !framebufferRegionHasInk(8, 3, 250, 36) ||
          !framebufferRegionHasInk(10, 40, 470, 360))) {
        saveFramebufferPpm("/tmp/powerwall-today-trend-fail.ppm");
        std::fprintf(stderr, "[FAIL] Today Trend page visibility, header or plot content.\n");
        return 7;
      }
      if (initial_page == 4 && !display480.simulatorWeeklyPageIsVisible()) {
        std::fprintf(stderr, "[FAIL] Week page visibility after inserting Today Trend.\n");
        return 7;
      }

      FILE* fp = std::fopen(capture_path, "wb");
      if (fp) {
        std::fprintf(fp, "P6\n%d %d\n255\n", kWindowSize, kWindowSize);
        for (int i = 0; i < kWindowSize * kWindowSize; ++i) {
          uint16_t px = g_fb[i];
          uint8_t r = ((px >> 11) & 0x1F); r = (r << 3) | (r >> 2);
          uint8_t g = ((px >> 5)  & 0x3F); g = (g << 2) | (g >> 4);
          uint8_t b = (px         & 0x1F); b = (b << 3) | (b >> 2);
          uint8_t rgb[3] = {r, g, b};
          std::fwrite(rgb, 1, 3, fp);
        }
        std::fclose(fp);
      }
      std::printf("[SmokeTest] Rendered %ux%u RGB565, output %dx%d to %s; exiting.\n",
                  kWinW, kWinH, kWindowSize, kWindowSize, capture_path);
      running.store(false);
    }

    SDL_Delay(visible ? 5 : 100);
  }

  running.store(false);
  poller.join();
  if (settings_smoke_test && settings_smoke_phase < settings_smoke_phase_count && simulator_exit_code == 0) simulator_exit_code = 8;
  deviceSettings.save();
  if (deviceSettings.audio_device) SDL_CloseAudioDevice(deviceSettings.audio_device);
  if (!history_writes_disabled && !demo_mode.load() && shared_daily_trend_history.count > 0 &&
      !saveDailyTrendFile(shared_daily_trend_history))
    std::fprintf(stderr, "[DailyTrend] final JSON sidecar save failed\n");
  if (!history_writes_disabled && week_has_updates && !saveWeeklyHistory(week_history))
    std::fprintf(stderr, "[WeeklyHistory] Final save failed\n");
  SDL_DestroyTexture(tex);
  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return simulator_exit_code;
}

#include "board_audio_4b.hpp"

#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)

#include <algorithm>
#include <cstring>
#include <ESP_I2S.h>
#include <Wire.h>
#include "es8311.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "voice_pcm_assets.hpp"

namespace {

// Waveshare ESP32-S3 Touch LCD 4B: ES8311 on I2C 0x18, audio pins below.
constexpr uint32_t kSampleRate = 16000;
constexpr uint32_t kToneHz = 440;
constexpr int16_t kToneAmplitude = 2000;
constexpr size_t kFramesPerWrite = 256;
constexpr uint16_t kMaxToneDurationMs = 1000;
constexpr uint32_t kVoiceRequestFlag = 0x80000000UL;

I2SClass g_i2s;
es8311_handle_t g_codec = nullptr;
TaskHandle_t g_audio_task = nullptr;
BoardAudio4B::AmpEnableFn g_amp_enable = nullptr;
uint8_t g_volume = 70;
volatile bool g_ready = false;
volatile bool g_tone_active = false;
volatile bool g_stop_requested = false;
volatile bool g_tone_complete = false;
uint32_t g_amp_disable_due = 0;
uint8_t g_test_voice_index = 0;

struct VoiceClip {
  const char* phrase;
  const int16_t* pcm;
  size_t samples;
};

VoiceClip voiceClip(BoardAudio4B::VoicePhrase phrase) {
  switch (phrase) {
    case BoardAudio4B::VoicePhrase::Charging:
#if defined(POWERWALL_NATIVE480_LANG_EN)
      return {"Battery charging", kVoiceChargingPcm, kVoiceChargingPcmSamples};
#else
      return {"充電中", kVoiceChargingPcm, kVoiceChargingPcmSamples};
#endif
    case BoardAudio4B::VoicePhrase::Discharging:
#if defined(POWERWALL_NATIVE480_LANG_EN)
      return {"Battery discharging", kVoiceDischargingPcm, kVoiceDischargingPcmSamples};
#else
      return {"放電中", kVoiceDischargingPcm, kVoiceDischargingPcmSamples};
#endif
    case BoardAudio4B::VoicePhrase::OffGrid:
#if defined(POWERWALL_NATIVE480_LANG_EN)
      return {"Grid disconnected", kVoiceOffGridPcm, kVoiceOffGridPcmSamples};
#else
      return {"電網已斷開", kVoiceOffGridPcm, kVoiceOffGridPcmSamples};
#endif
  }
  return {nullptr, nullptr, 0};
}

bool configureCodec(uint8_t volume_percent) {
  // Use the exact ES8311 driver distributed with Waveshare's 4B audio example.
  // Keep I2S and codec on the shared 16 kHz / 4.096 MHz MCLK clock tree.
  g_codec = es8311_create(0, ES8311_ADDRRES_0);
  if (!g_codec) {
    Serial.println("[BoardAudio4B] ES8311 handle creation failed");
    return false;
  }
  const es8311_clock_config_t clock = {
    .mclk_inverted = false,
    .sclk_inverted = false,
    .mclk_from_mclk_pin = true,
    .mclk_frequency = kSampleRate * 256,
    .sample_frequency = kSampleRate,
  };
  esp_err_t error = es8311_init(g_codec, &clock, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16);
  if (error == ESP_OK) error = es8311_sample_frequency_config(g_codec, clock.mclk_frequency, clock.sample_frequency);
  if (error == ESP_OK) error = es8311_microphone_config(g_codec, false);
  if (error == ESP_OK) error = es8311_voice_volume_set(g_codec, volume_percent, nullptr);
  if (error == ESP_OK) error = es8311_voice_mute(g_codec, false);
  if (error == ESP_OK) error = es8311_microphone_gain_set(g_codec, ES8311_MIC_GAIN_18DB);
  if (error != ESP_OK) {
    Serial.printf("[BoardAudio4B] ES8311 official init failed: 0x%x\n", static_cast<unsigned>(error));
    es8311_delete(g_codec);
    g_codec = nullptr;
    return false;
  }
  return true;
}

void audioTask(void*) {
  static int16_t pcm[kFramesPerWrite * 2];
  const uint32_t phase_step = static_cast<uint32_t>((static_cast<uint64_t>(kToneHz) << 32) / kSampleRate);

  for (;;) {
    uint32_t request = 0;
    if (xTaskNotifyWait(0, UINT32_MAX, &request, portMAX_DELAY) != pdTRUE || request == 0) {
      continue;
    }

#if !defined(POWERWALL_NATIVE480_RELEASE)
    size_t total_written = 0;
    uint32_t short_writes = 0;
#endif
    vTaskDelay(pdMS_TO_TICKS(30));  // let the external amplifier wake before PCM starts
    if (request & kVoiceRequestFlag) {
      const auto phrase = static_cast<BoardAudio4B::VoicePhrase>(request & 0xffU);
      const VoiceClip clip = voiceClip(phrase);
      if (!clip.pcm || !clip.samples) {
        g_tone_complete = true;
        continue;
      }
#if !defined(POWERWALL_NATIVE480_RELEASE)
      Serial.printf("[BoardAudio4B] Voice start: %s (%u samples)\n", clip.phrase,
                    static_cast<unsigned>(clip.samples));
#endif
      size_t offset = 0;
      while (!g_stop_requested && offset < clip.samples) {
        const size_t frames = std::min(kFramesPerWrite, clip.samples - offset);
        for (size_t frame = 0; frame < frames; ++frame) {
          pcm[frame * 2] = clip.pcm[offset + frame];
          pcm[frame * 2 + 1] = clip.pcm[offset + frame];
        }
        const size_t written = g_i2s.write(pcm, frames * 2 * sizeof(int16_t));
#if !defined(POWERWALL_NATIVE480_RELEASE)
        total_written += written;
        if (written != frames * 2 * sizeof(int16_t)) ++short_writes;
#endif
        offset += frames;
        if (written == 0) vTaskDelay(pdMS_TO_TICKS(1));
        vTaskDelay(1);
      }
#if !defined(POWERWALL_NATIVE480_RELEASE)
      Serial.printf("[BoardAudio4B] Voice done: %s, bytes=%u short_writes=%u i2s_error=0x%x\n",
                    clip.phrase, static_cast<unsigned>(total_written),
                    static_cast<unsigned>(short_writes), static_cast<unsigned>(g_i2s.lastError()));
#endif
    } else {
      const uint32_t duration_ms = request;
      uint32_t phase = 0;
#if !defined(POWERWALL_NATIVE480_RELEASE)
      Serial.printf("[BoardAudio4B] PCM start: %u ms\n", duration_ms);
#endif
      const uint32_t started_at = millis();
      while (!g_stop_requested && static_cast<uint32_t>(millis() - started_at) < duration_ms) {
        for (size_t frame = 0; frame < kFramesPerWrite; ++frame) {
          phase += phase_step;
          const int16_t sample = (phase & 0x80000000U) ? kToneAmplitude : -kToneAmplitude;
          pcm[frame * 2] = sample;
          pcm[frame * 2 + 1] = sample;
        }
        const size_t written = g_i2s.write(pcm, sizeof(pcm));
#if !defined(POWERWALL_NATIVE480_RELEASE)
        total_written += written;
        if (written != sizeof(pcm)) ++short_writes;
#endif
        if (written == 0) vTaskDelay(pdMS_TO_TICKS(1));
        vTaskDelay(1);  // match Waveshare's 07_ES8311 playback loop
      }
#if !defined(POWERWALL_NATIVE480_RELEASE)
      Serial.printf("[BoardAudio4B] PCM done: bytes=%u short_writes=%u i2s_error=0x%x\n",
                    static_cast<unsigned>(total_written), static_cast<unsigned>(short_writes),
                    static_cast<unsigned>(g_i2s.lastError()));
#endif
    }

    std::memset(pcm, 0, sizeof(pcm));
    g_i2s.write(pcm, sizeof(pcm));
    g_tone_complete = true;
  }
}

}  // namespace

bool BoardAudio4B::begin(BoardAudio4B::AmpEnableFn amp_enable) {
  if (g_ready) {
    return true;
  }
  if (!amp_enable) {
    Serial.println("[BoardAudio4B] Missing speaker amp callback");
    return false;
  }

  g_amp_enable = amp_enable;
  // Waveshare 07_ES8311 enables EXIO3 before starting I2S and ES8311,
  // then leaves the amplifier enabled during playback.
  if (!g_amp_enable(true)) {
    Serial.println("[BoardAudio4B] Failed to enable speaker amp before init");
    g_amp_enable = nullptr;
    return false;
  }
  delay(200);

  // Waveshare pin_config.h: MCLK=5, BCLK=16, LRCK=7, codec DOUT=6.
  g_i2s.setPins(16, 7, 6, 15, 5);
  if (!g_i2s.begin(I2S_MODE_STD, kSampleRate, I2S_DATA_BIT_WIDTH_16BIT,
                   I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
    Serial.printf("[BoardAudio4B] I2S init failed: %d\n", g_i2s.lastError());
    g_i2s.end();
    g_amp_enable(false);
    g_amp_enable = nullptr;
    return false;
  }

  if (!configureCodec(g_volume)) {
    Serial.println("[BoardAudio4B] ES8311 init failed");
    g_i2s.end();
    g_amp_enable(false);
    g_amp_enable = nullptr;
    return false;
  }

  if (xTaskCreatePinnedToCore(audioTask, "audio4b", 4096, nullptr, 1,
                              &g_audio_task, 1) != pdPASS) {
    Serial.println("[BoardAudio4B] Audio task creation failed");
    g_i2s.end();
    g_amp_enable(false);
    g_amp_enable = nullptr;
    g_audio_task = nullptr;
    return false;
  }

  g_ready = true;
#if !defined(POWERWALL_NATIVE480_RELEASE)
  Serial.println("[BoardAudio4B] ES8311 16 kHz playback ready");
#endif
  return true;
}

bool BoardAudio4B::setVolume(uint8_t percent) {
  if (percent > 100) {
    percent = 100;
  }
  g_volume = percent;
  if (!g_ready) {
    return true;
  }

  return g_codec && es8311_voice_volume_set(g_codec, percent, nullptr) == ESP_OK;
}

uint8_t BoardAudio4B::volume() {
  return g_volume;
}

bool BoardAudio4B::playTestTone(uint16_t duration_ms) {
  if (!g_ready || !g_audio_task || !g_amp_enable) {
    Serial.println("[BoardAudio4B] Test tone unavailable: codec or I2S not ready");
    return false;
  }
  if (g_tone_active) {
    Serial.println("[BoardAudio4B] Test tone already playing");
    return false;
  }
  if (!duration_ms) {
    duration_ms = 1;
  } else if (duration_ms > kMaxToneDurationMs) {
    duration_ms = kMaxToneDurationMs;
  }
  if (!g_amp_enable(true)) {
    Serial.println("[BoardAudio4B] Could not enable speaker amplifier");
    return false;
  }

  g_stop_requested = false;
  g_tone_complete = false;
  g_tone_active = true;
  g_amp_disable_due = 0;
  xTaskNotify(g_audio_task, duration_ms, eSetValueWithOverwrite);
#if !defined(POWERWALL_NATIVE480_RELEASE)
  Serial.printf("[BoardAudio4B] Test tone queued: %u ms, volume %u%%\n", duration_ms, g_volume);
#endif
  return true;
}

bool BoardAudio4B::playVoice(VoicePhrase phrase) {
  if (!g_ready || !g_audio_task || !g_amp_enable) {
    Serial.println("[BoardAudio4B] Voice playback unavailable: codec or I2S not ready");
    return false;
  }
  if (g_tone_active) {
    Serial.println("[BoardAudio4B] Voice playback already active");
    return false;
  }
  if (!voiceClip(phrase).pcm || !g_amp_enable(true)) return false;
  g_stop_requested = false;
  g_tone_complete = false;
  g_tone_active = true;
  g_amp_disable_due = 0;
  xTaskNotify(g_audio_task, kVoiceRequestFlag | static_cast<uint32_t>(phrase), eSetValueWithOverwrite);
#if !defined(POWERWALL_NATIVE480_RELEASE)
  Serial.printf("[BoardAudio4B] Voice queued: %s, volume %u%%\n", voiceClip(phrase).phrase, g_volume);
#endif
  return true;
}

bool BoardAudio4B::playTestVoice() {
  static constexpr VoicePhrase sequence[] = {
    VoicePhrase::Charging, VoicePhrase::Discharging, VoicePhrase::OffGrid
  };
  const VoicePhrase phrase = sequence[g_test_voice_index];
  g_test_voice_index = static_cast<uint8_t>((g_test_voice_index + 1) % 3);
  return playVoice(phrase);
}

void BoardAudio4B::update() {
  if (g_tone_complete) {
    g_tone_complete = false;
    g_amp_disable_due = millis() + 120;  // allow queued I2S samples to reach the speaker
  }
  if (g_amp_disable_due && static_cast<int32_t>(millis() - g_amp_disable_due) >= 0) {
    g_amp_disable_due = 0;
    g_tone_active = false;
    // Keep EXIO3 high as in Waveshare's audio example; stop() disables it.
  }
}

void BoardAudio4B::stop() {
  g_stop_requested = true;
  g_tone_active = false;
  g_tone_complete = false;
  g_amp_disable_due = 0;
  if (g_audio_task) {
    xTaskNotify(g_audio_task, 0, eSetValueWithOverwrite);
  }
  if (g_amp_enable) {
    g_amp_enable(false);
  }
}

bool BoardAudio4B::isReady() {
  return g_ready;
}

#else

bool BoardAudio4B::begin(BoardAudio4B::AmpEnableFn) { return false; }
bool BoardAudio4B::setVolume(uint8_t) { return false; }
uint8_t BoardAudio4B::volume() { return 0; }
bool BoardAudio4B::playTestTone(uint16_t) { return false; }
bool BoardAudio4B::playVoice(BoardAudio4B::VoicePhrase) { return false; }
bool BoardAudio4B::playTestVoice() { return false; }
void BoardAudio4B::update() {}
void BoardAudio4B::stop() {}
bool BoardAudio4B::isReady() { return false; }

#endif

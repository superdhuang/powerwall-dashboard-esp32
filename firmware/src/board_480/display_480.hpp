#pragma once
// Display & touch driver for the Waveshare ESP32-S3-Touch-LCD-4B
// (480x480 ST7701 RGB panel, GT911 touch, XCA9554 IO expander).
// Host builds (firmware sim) supply their own flush/touch callbacks.
#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>
#if defined(ESP32)
#include "sdkconfig.h"
#if !defined(CONFIG_SPIRAM_XIP_FROM_PSRAM) || !CONFIG_SPIRAM_XIP_FROM_PSRAM
#error "Waveshare S3 4B requires SDK-built PSRAM XIP support"
#endif
#if !defined(CONFIG_SPIRAM_FETCH_INSTRUCTIONS) || !CONFIG_SPIRAM_FETCH_INSTRUCTIONS
#error "Waveshare S3 4B requires instruction fetch from PSRAM"
#endif
#if !defined(CONFIG_SPIRAM_RODATA) || !CONFIG_SPIRAM_RODATA
#error "Waveshare S3 4B requires read-only data in PSRAM"
#endif
#if !defined(CONFIG_ESP32S3_DATA_CACHE_LINE_64B) || !CONFIG_ESP32S3_DATA_CACHE_LINE_64B
#error "Waveshare S3 4B requires the 64-byte data cache line for PSRAM XIP"
#endif
#include <Arduino_GFX_Library.h>
#include <esp_heap_caps.h>
#endif

class DisplayDriver480 {
public:
  static constexpr uint16_t kPanelSize = 480;
  static constexpr uint16_t kWidth = 480;
  static constexpr uint16_t kHeight = 480;
  static constexpr uint16_t kBufLines = 40;

  using PanelFlushFn = void (*)(const lv_area_t* area, lv_color_t* color_p);
  using TouchReadFn  = bool (*)(int16_t* out_x, int16_t* out_y);

  bool begin(PanelFlushFn panel_flush_fn = nullptr, TouchReadFn touch_read_fn = nullptr) {
    if (initialized_) return true;

    instance_ = this;
    if (!panel_flush_fn) {
      if (!initPanelHardware() || !panelHasValidHandle()) {
        Serial.println("[DisplayDriver480] ERROR: Panel hardware initialization failed; aborting display setup.");
        return false;
      }
    }
    if (!touch_read_fn) {
      initTouchHardware();
    }

    panel_flush_fn_ = panel_flush_fn ? panel_flush_fn : &DisplayDriver480::defaultPanelFlush;
    touch_read_fn_  = touch_read_fn  ? touch_read_fn  : &DisplayDriver480::defaultTouchRead;

    lv_init();

    const size_t buf_pixels = static_cast<size_t>(kWidth) * kBufLines;
    const size_t buf_bytes  = buf_pixels * sizeof(lv_color_t);

#if defined(ESP32)
    buf1_ = static_cast<lv_color_t*>(heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    buf2_ = static_cast<lv_color_t*>(heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#endif
    if (!buf1_) {
      buf1_ = static_cast<lv_color_t*>(malloc(buf_bytes));
    }
    if (!buf1_) {
      if (buf2_) {
        free(buf2_);
        buf2_ = nullptr;
      }
      Serial.println("[DisplayDriver480] ERROR: Failed to allocate LVGL draw buffer!");
      return false;
    }

#if LVGL_VERSION_MAJOR >= 9
    lv_tick_set_cb([]() -> uint32_t { return millis(); });
    disp_ = lv_display_create(kWidth, kHeight);
    if (!disp_) {
      Serial.println("[DisplayDriver480] ERROR: lv_display_create() failed!");
      return false;
    }
    lv_display_set_user_data(disp_, this);
    lv_display_set_flush_cb(disp_, &DisplayDriver480::flushCbV9);
    lv_display_set_buffers(disp_, buf1_, buf2_, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

    indev_ = lv_indev_create();
    lv_indev_set_type(indev_, LV_INDEV_TYPE_POINTER);
    lv_indev_set_user_data(indev_, this);
    lv_indev_set_read_cb(indev_, &DisplayDriver480::touchReadCbV9);
#else
    lv_disp_draw_buf_init(&draw_buf_, buf1_, buf2_, buf_pixels);

    lv_disp_drv_init(&disp_drv_);
    disp_drv_.hor_res   = kWidth;
    disp_drv_.ver_res   = kHeight;
    disp_drv_.flush_cb  = &DisplayDriver480::flushCbV8;
    disp_drv_.draw_buf  = &draw_buf_;
    disp_drv_.user_data = this;
    disp_ = lv_disp_drv_register(&disp_drv_);
    if (!disp_) {
      Serial.println("[DisplayDriver480] ERROR: lv_disp_drv_register() failed!");
      return false;
    }

    lv_indev_drv_init(&indev_drv_);
    indev_drv_.type      = LV_INDEV_TYPE_POINTER;
    indev_drv_.read_cb   = &DisplayDriver480::touchReadCbV8;
    indev_drv_.user_data = this;
    indev_ = lv_indev_drv_register(&indev_drv_);
#endif

    last_tick_ms_ = millis();
    initialized_ = true;
    return true;
  }

  void tick() {
#if LVGL_VERSION_MAJOR < 9 && (!defined(LV_TICK_CUSTOM) || LV_TICK_CUSTOM == 0)
    unsigned long now = millis();
    unsigned long elapsed = now - last_tick_ms_;
    if (elapsed > 0) {
      lv_tick_inc(elapsed);
      last_tick_ms_ = now;
    }
#endif
  }

  void setPanelFlushCallback(PanelFlushFn fn) { panel_flush_fn_ = fn; }
  void setTouchReadCallback(TouchReadFn fn) { touch_read_fn_ = fn; }

  bool setBrightness(uint8_t percent) {
#if defined(ESP32)
    brightness_percent_ = percent > 100 ? 100 : percent;
    if (!backlight_pwm_initialized_) return true;
    if (!backlight_pwm_ready_) return false;
    return apply4BBacklightBrightness();
#else
    (void)percent;
    return false;
#endif
  }

  bool setSpeakerAmpEnabled(bool on) {
#if defined(ESP32)
    if (!expander_ || !gfx_) return false;
    expander_->digitalWrite(kSpeakerAmpExpanderPin, on ? HIGH : LOW);
#if !defined(POWERWALL_NATIVE480_RELEASE)
    const bool verified = expander_->digitalRead(kSpeakerAmpExpanderPin) == (on ? HIGH : LOW);
    Serial.printf("[BoardAudio4B] Speaker amp %s: %s\n", on ? "on" : "off",
                  verified ? "verified" : "readback mismatch");
#endif
    return true;
#else
    (void)on;
    return false;
#endif
  }

  bool read4BPowerButton(bool& pressed) {
#if defined(ESP32)
    if (!expander_ || !gfx_) return false;
    pressed = expander_->digitalRead(kPowerKeyExpanderPin) == HIGH;
    return true;
#else
    pressed = false;
    return false;
#endif
  }

private:
  static inline DisplayDriver480* instance_ = nullptr;
  bool initialized_ = false;
  unsigned long last_tick_ms_ = 0;
  lv_color_t* buf1_ = nullptr;
  lv_color_t* buf2_ = nullptr;
  PanelFlushFn panel_flush_fn_ = nullptr;
  TouchReadFn touch_read_fn_ = nullptr;
  int16_t last_touch_x_ = 0;
  int16_t last_touch_y_ = 0;
  uint8_t gt911_addr_ = 0;

#if defined(ESP32)
  static constexpr uint8_t kBacklightPwmPin = 4;
  static constexpr uint8_t kSpeakerAmpExpanderPin = 3;
  static constexpr uint8_t kPowerKeyExpanderPin = 4;
  static constexpr uint32_t kBacklightPwmFrequencyHz = 25000;
  static constexpr uint8_t kBacklightPwmResolutionBits = 10;
  Arduino_XCA9554SWSPI* expander_ = nullptr;
  Arduino_RGB_Display* gfx_ = nullptr;
  uint8_t brightness_percent_ = 80;
  bool backlight_pwm_initialized_ = false;
  bool backlight_pwm_ready_ = false;
#endif

  bool panelHasValidHandle() const {
#if defined(ESP32)
    return gfx_ != nullptr;
#else
    return true;
#endif
  }

#if defined(ESP32)
  bool apply4BBacklightBrightness() {
    constexpr uint32_t kMaxDuty = (1U << kBacklightPwmResolutionBits) - 1U;
    const uint32_t duty = (kMaxDuty * brightness_percent_ + 50U) / 100U;
    if (!ledcWrite(kBacklightPwmPin, duty)) {
      // GPIO4 is active-low; leave the backlight off if PWM output fails.
      ledcDetach(kBacklightPwmPin);
      pinMode(kBacklightPwmPin, OUTPUT);
      digitalWrite(kBacklightPwmPin, HIGH);
      backlight_pwm_ready_ = false;
      Serial.println("[DisplayDriver480] WARN: 4B backlight PWM write failed; backlight disabled");
      return false;
    }
    return true;
  }

  void init4BBacklightPwm() {
    // The 4B backlight input is active-low. Default to off before attaching PWM.
    pinMode(kBacklightPwmPin, OUTPUT);
    digitalWrite(kBacklightPwmPin, HIGH);
    backlight_pwm_initialized_ = true;
    backlight_pwm_ready_ = ledcAttach(kBacklightPwmPin,
                                      kBacklightPwmFrequencyHz,
                                      kBacklightPwmResolutionBits);
    if (backlight_pwm_ready_) {
      backlight_pwm_ready_ = ledcOutputInvert(kBacklightPwmPin, true);
    }
    if (!backlight_pwm_ready_) {
      ledcDetach(kBacklightPwmPin);
      pinMode(kBacklightPwmPin, OUTPUT);
      digitalWrite(kBacklightPwmPin, HIGH);
      Serial.println("[DisplayDriver480] WARN: 4B backlight PWM unavailable; backlight disabled");
      return;
    }
    apply4BBacklightBrightness();
  }
#endif

  bool initPanelHardware() {
#if defined(ESP32)
    // Waveshare's 4B Arduino example: RGB data pins, ST7701 init over XCA9554,
    // and GT911 reset on the same expander.
    Wire.begin(47, 48, 400000);
    expander_ = new Arduino_XCA9554SWSPI(7, 0, 2, 1, &Wire, 0x20);
    auto* rgb = new Arduino_ESP32RGBPanel(
      17, 3, 46, 9,
      10, 11, 12, 13, 14,
      21, 8, 18, 45, 38, 39,
      40, 41, 42, 2, 1,
      // Waveshare Arduino example timing: polarity, front porch, pulse, back porch.
      1, 10, 8, 50,
      1, 10, 8, 20,
      // Keep RGB DMA bounce buffers in internal DRAM while the 480x480
      // framebuffer and LVGL draw buffers remain in PSRAM.
      0, GFX_NOT_DEFINED, false, 0, 0, 480 * 10);
    gfx_ = new Arduino_RGB_Display(480, 480, rgb, 0, true,
                                   expander_, GFX_NOT_DEFINED,
                                   st7701_type1_init_operations,
                                   sizeof(st7701_type1_init_operations));
    // Match Arduino_GFX's 12 MHz default for this Octal-PSRAM target. With
    // Waveshare's 548x518 total timing, this is about 42.3 Hz and 25% less
    // pixel/PSRAM traffic than the previous 16 MHz setting.
    if (!gfx_ || !gfx_->begin(12000000)) {
      Serial.println("[DisplayDriver480] ERROR: Waveshare 4B ST7701 init failed");
      gfx_ = nullptr;
      return false;
    }
    const size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
#if !defined(POWERWALL_NATIVE480_RELEASE)
    const size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    Serial.printf(
      "[DisplayDriver480] PSRAM XIP SDK: xip=%d fetch=%d rodata=%d cache64=%d; PSRAM total=%u free=%u; RGB bounce=%u px x2 (~%u bytes); LVGL %ux%u\n",
      CONFIG_SPIRAM_XIP_FROM_PSRAM,
      CONFIG_SPIRAM_FETCH_INSTRUCTIONS,
      CONFIG_SPIRAM_RODATA,
      CONFIG_ESP32S3_DATA_CACHE_LINE_64B,
      static_cast<unsigned>(psram_total),
      static_cast<unsigned>(psram_free),
      480U * 10U,
      static_cast<unsigned>(2U * 480U * 10U * sizeof(uint16_t)),
      static_cast<unsigned>(kWidth), static_cast<unsigned>(kHeight));
#endif
    if (psram_total == 0) {
      Serial.println("[DisplayDriver480] ERROR: PSRAM is not initialized; refusing the 4B XIP display path");
      return false;
    }
    gfx_->fillScreen(0);
    expander_->pinMode(kPowerKeyExpanderPin, INPUT);
    expander_->pinMode(kSpeakerAmpExpanderPin, OUTPUT);
    expander_->digitalWrite(kSpeakerAmpExpanderPin, LOW);
    init4BBacklightPwm();
    return true;
#else
    return true;
#endif
  }

  void initTouchHardware() {
#if defined(ESP32)
    Wire.begin(47, 48, 400000);
    expander_->pinMode(5, OUTPUT);
    expander_->pinMode(6, OUTPUT);
    expander_->digitalWrite(6, LOW);
    delay(20);
    expander_->digitalWrite(5, LOW);
    delay(20);
    expander_->digitalWrite(5, HIGH);
    delay(100);
#endif
    const uint8_t candidates[2] = {0x5D, 0x14};
    for (uint8_t addr : candidates) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        gt911_addr_ = addr;
        break;
      }
    }
  }

  static void defaultPanelFlush(const lv_area_t* area, lv_color_t* color_p) {
#if defined(ESP32)
    auto* self = instance_;
    if (!self || !self->gfx_ || !area || !color_p) return;
    self->gfx_->draw16bitRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t*>(color_p),
                                   area->x2 - area->x1 + 1, area->y2 - area->y1 + 1);
#else
    (void)area;
    (void)color_p;
#endif
  }

  static bool defaultTouchRead(int16_t* out_x, int16_t* out_y) {
    if (!instance_ || instance_->gt911_addr_ == 0 || !out_x || !out_y) {
      return false;
    }
    const uint8_t addr = instance_->gt911_addr_;
    Wire.beginTransmission(addr);
    Wire.write(0x81);
    Wire.write(0x4E);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(addr, static_cast<uint8_t>(1)) != 1) return false;

    uint8_t status = Wire.read();
    uint8_t touches = status & 0x0F;
    if ((status & 0x80) == 0 || touches == 0) {
      if (status & 0x80) {
        clearGt911Status(addr);
      }
      return false;
    }

    Wire.beginTransmission(addr);
    Wire.write(0x81);
    Wire.write(0x50);
    if (Wire.endTransmission(false) != 0) {
      clearGt911Status(addr);
      return false;
    }
    if (Wire.requestFrom(addr, static_cast<uint8_t>(4)) == 4) {
      uint8_t xl = Wire.read();
      uint8_t xh = Wire.read();
      uint8_t yl = Wire.read();
      uint8_t yh = Wire.read();
      *out_x = static_cast<int16_t>((xh << 8) | xl);
      *out_y = static_cast<int16_t>((yh << 8) | yl);
      clearGt911Status(addr);
      return true;
    }
    clearGt911Status(addr);
    return false;
  }

  static void clearGt911Status(uint8_t addr) {
    Wire.beginTransmission(addr);
    Wire.write(0x81);
    Wire.write(0x4E);
    Wire.write(0x00);
    Wire.endTransmission();
  }

#if LVGL_VERSION_MAJOR >= 9
  lv_display_t* disp_ = nullptr;
  lv_indev_t* indev_  = nullptr;

  static void flushCbV9(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    auto* self = static_cast<DisplayDriver480*>(lv_display_get_user_data(disp));
    if (self && self->panel_flush_fn_) {
      self->panel_flush_fn_(area, reinterpret_cast<lv_color_t*>(px_map));
    }
    lv_display_flush_ready(disp);
  }

  static void touchReadCbV9(lv_indev_t* indev, lv_indev_data_t* data) {
    auto* self = static_cast<DisplayDriver480*>(lv_indev_get_user_data(indev));
    int16_t x = 0, y = 0;
    if (self && self->touch_read_fn_ && self->touch_read_fn_(&x, &y)) {
      self->last_touch_x_ = x;
      self->last_touch_y_ = y;
      data->state = LV_INDEV_STATE_PRESSED;
    } else {
      data->state = LV_INDEV_STATE_RELEASED;
    }
    data->point.x = self ? self->last_touch_x_ : 0;
    data->point.y = self ? self->last_touch_y_ : 0;
  }
#else
  lv_disp_draw_buf_t draw_buf_;
  lv_disp_drv_t disp_drv_;
  lv_indev_drv_t indev_drv_;
  lv_disp_t* disp_   = nullptr;
  lv_indev_t* indev_ = nullptr;

  static void flushCbV8(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p) {
    auto* self = static_cast<DisplayDriver480*>(disp_drv->user_data);
    if (self && self->panel_flush_fn_) {
      self->panel_flush_fn_(area, color_p);
    }
    lv_disp_flush_ready(disp_drv);
  }

  static void touchReadCbV8(lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
    auto* self = static_cast<DisplayDriver480*>(indev_drv->user_data);
    int16_t x = 0, y = 0;
    if (self && self->touch_read_fn_ && self->touch_read_fn_(&x, &y)) {
      self->last_touch_x_ = x;
      self->last_touch_y_ = y;
      data->state = LV_INDEV_STATE_PR;
    } else {
      data->state = LV_INDEV_STATE_REL;
    }
    data->point.x = self ? self->last_touch_x_ : 0;
    data->point.y = self ? self->last_touch_y_ : 0;
  }
#endif
};

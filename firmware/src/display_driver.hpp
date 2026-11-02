#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>
#include "scaled_panel_flush.hpp"
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
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
#endif

#if defined(ESP32)
#include <esp_heap_caps.h>
#include "soc/soc_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#if defined(SOC_MIPI_DSI_SUPPORTED) && SOC_MIPI_DSI_SUPPORTED
#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"
#elif defined(SOC_LCD_RGB_SUPPORTED) && SOC_LCD_RGB_SUPPORTED
#include "esp_lcd_panel_rgb.h"
#endif
#endif

// Display & Touch Driver for 720x720 panels:
//  - ESP32-P4 Waveshare 4" 720x720 MIPI-DSI + GT911 I2C Touch (SDA=7, SCL=8, BL=26)
//  - ESP32-S3 Guition   4" 720x720 RGB      + GT911 I2C Touch (SDA=19, SCL=45, BL=38)
//  - ESP32-S3 Waveshare 4B 480x480 ST7701 RGB + GT911; virtual 720x720 UI scaled to panel
class DisplayDriver720 {
public:
  static constexpr uint16_t kWidth = 720;
  static constexpr uint16_t kHeight = 720;
  static constexpr uint16_t kBufLines = 40;

  using PanelFlushFn = void (*)(const lv_area_t* area, lv_color_t* color_p);
  using TouchReadFn  = bool (*)(int16_t* out_x, int16_t* out_y);

  bool begin(PanelFlushFn panel_flush_fn = nullptr, TouchReadFn touch_read_fn = nullptr) {
    if (initialized_) return true;

    instance_ = this;
    if (!panel_flush_fn) {
      if (!initPanelHardware() || !panelHasValidHandle()) {
        Serial.println("[DisplayDriver720] ERROR: Panel hardware initialization failed; aborting display setup.");
        return false;
      }
    }
    if (!touch_read_fn) {
      initTouchHardware();
    }

    panel_flush_fn_ = panel_flush_fn ? panel_flush_fn : &DisplayDriver720::defaultPanelFlush;
    touch_read_fn_  = touch_read_fn  ? touch_read_fn  : &DisplayDriver720::defaultTouchRead;

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
      Serial.println("[DisplayDriver720] ERROR: Failed to allocate LVGL draw buffer!");
      return false;
    }

#if LVGL_VERSION_MAJOR >= 9
    lv_tick_set_cb([]() -> uint32_t { return millis(); });
    disp_ = lv_display_create(kWidth, kHeight);
    if (!disp_) {
      Serial.println("[DisplayDriver720] ERROR: lv_display_create() failed!");
      return false;
    }
    lv_display_set_user_data(disp_, this);
    lv_display_set_flush_cb(disp_, &DisplayDriver720::flushCbV9);
    lv_display_set_buffers(
      disp_,
      buf1_,
      buf2_,
      buf_bytes,
      LV_DISPLAY_RENDER_MODE_PARTIAL
    );

    indev_ = lv_indev_create();
    lv_indev_set_type(indev_, LV_INDEV_TYPE_POINTER);
    lv_indev_set_user_data(indev_, this);
    lv_indev_set_read_cb(indev_, &DisplayDriver720::touchReadCbV9);
#else
    lv_disp_draw_buf_init(&draw_buf_, buf1_, buf2_, buf_pixels);

    lv_disp_drv_init(&disp_drv_);
    disp_drv_.hor_res   = kWidth;
    disp_drv_.ver_res   = kHeight;
    disp_drv_.flush_cb  = &DisplayDriver720::flushCbV8;
    disp_drv_.draw_buf  = &draw_buf_;
    disp_drv_.user_data = this;
    disp_ = lv_disp_drv_register(&disp_drv_);
    if (!disp_) {
      Serial.println("[DisplayDriver720] ERROR: lv_disp_drv_register() failed!");
      return false;
    }

    lv_indev_drv_init(&indev_drv_);
    indev_drv_.type      = LV_INDEV_TYPE_POINTER;
    indev_drv_.read_cb   = &DisplayDriver720::touchReadCbV8;
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
    uint32_t elapsed = static_cast<uint32_t>(now - last_tick_ms_);
    if (elapsed > 0) {
      lv_tick_inc(elapsed);
      last_tick_ms_ = now;
    }
#endif
  }

  void setPanelFlushCallback(PanelFlushFn fn) { panel_flush_fn_ = fn; }
  void setTouchReadCallback(TouchReadFn fn) { touch_read_fn_ = fn; }

  bool setBrightness(uint8_t percent) {
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
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
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
    if (!expander_ || !gfx_) return false;
    expander_->digitalWrite(kSpeakerAmpExpanderPin, on ? HIGH : LOW);
    const bool verified = expander_->digitalRead(kSpeakerAmpExpanderPin) == (on ? HIGH : LOW);
    Serial.printf("[BoardAudio4B] Speaker amp %s: %s\n", on ? "on" : "off",
                  verified ? "verified" : "readback mismatch");
    return true;
#else
    (void)on;
    return false;
#endif
  }

  bool read4BPowerButton(bool& pressed) {
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
    if (!expander_ || !gfx_) return false;
    pressed = expander_->digitalRead(kPowerKeyExpanderPin) == HIGH;
    return true;
#else
    pressed = false;
    return false;
#endif
  }

private:
  static inline DisplayDriver720* instance_ = nullptr;
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
  esp_lcd_panel_handle_t panel_handle_ = nullptr;
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
  static constexpr uint8_t kBacklightPwmPin = 4;
  static constexpr uint8_t kSpeakerAmpExpanderPin = 3;
  static constexpr uint8_t kPowerKeyExpanderPin = 4;
  static constexpr uint32_t kBacklightPwmFrequencyHz = 25000;
  static constexpr uint8_t kBacklightPwmResolutionBits = 10;
  Arduino_XCA9554SWSPI* expander_ = nullptr;
  Arduino_RGB_Display* gfx_ = nullptr;
  uint16_t* scaled_flush_buf_ = nullptr;
  uint8_t brightness_percent_ = 80;
  bool backlight_pwm_initialized_ = false;
  bool backlight_pwm_ready_ = false;
#endif
#if defined(SOC_MIPI_DSI_SUPPORTED) && SOC_MIPI_DSI_SUPPORTED
  esp_ldo_channel_handle_t ldo_mipi_phy_ = nullptr;
  esp_ldo_channel_handle_t ldo_vo4_      = nullptr;
  esp_lcd_dsi_bus_handle_t dsi_bus_      = nullptr;
  esp_lcd_panel_io_handle_t dbi_io_      = nullptr;
#endif
#endif

  bool panelHasValidHandle() const {
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
    return gfx_ != nullptr && scaled_flush_buf_ != nullptr;
#elif defined(ESP32)
    return panel_handle_ != nullptr;
#else
    return true;
#endif
  }

#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
  bool apply4BBacklightBrightness() {
    constexpr uint32_t kMaxDuty = (1U << kBacklightPwmResolutionBits) - 1U;
    const uint32_t duty = (kMaxDuty * brightness_percent_ + 50U) / 100U;
    if (!ledcWrite(kBacklightPwmPin, duty)) {
      // GPIO4 is active-low; leave the backlight off if PWM output fails.
      ledcDetach(kBacklightPwmPin);
      pinMode(kBacklightPwmPin, OUTPUT);
      digitalWrite(kBacklightPwmPin, HIGH);
      backlight_pwm_ready_ = false;
      Serial.println("[DisplayDriver720] WARN: 4B backlight PWM write failed; backlight disabled");
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
      Serial.println("[DisplayDriver720] WARN: 4B backlight PWM unavailable; backlight disabled");
      return;
    }
    apply4BBacklightBrightness();
  }
#endif

#if defined(ESP32) && defined(SOC_MIPI_DSI_SUPPORTED) && SOC_MIPI_DSI_SUPPORTED
  struct St7703InitCmd {
    uint8_t cmd;
    const uint8_t* data;
    size_t data_bytes;
    uint16_t delay_ms;
  };

  static void setWaveshare4BBacklight(bool on) {
    static constexpr uint8_t kBlPwmPin = 26;
    static constexpr uint8_t kBlEnPin  = 33;
    static constexpr uint32_t kBlFreq  = 5000;
    static constexpr uint8_t kBlRes    = 10;

    pinMode(kBlEnPin, OUTPUT);
    bool pwm_ready = ledcAttach(kBlPwmPin, kBlFreq, kBlRes) &&
                     ledcOutputInvert(kBlPwmPin, true);
    if (pwm_ready) {
      ledcWrite(kBlPwmPin, on ? ((1U << kBlRes) - 1U) : 0U);
      digitalWrite(kBlEnPin, on ? HIGH : LOW);
    } else {
      // Fallback: GPIO26 is active-low on Waveshare ESP32-P4-WIFI6-Touch-LCD-4B
      pinMode(kBlPwmPin, OUTPUT);
      digitalWrite(kBlPwmPin, on ? LOW : HIGH);
      digitalWrite(kBlEnPin, on ? HIGH : LOW);
    }
  }

  void cleanupDsiResources() {
    if (panel_handle_) {
      esp_lcd_panel_del(panel_handle_);
      panel_handle_ = nullptr;
    }
    if (dbi_io_) {
      esp_lcd_panel_io_del(dbi_io_);
      dbi_io_ = nullptr;
    }
    if (dsi_bus_) {
      esp_lcd_del_dsi_bus(dsi_bus_);
      dsi_bus_ = nullptr;
    }
    if (ldo_vo4_) {
      esp_ldo_release_channel(ldo_vo4_);
      ldo_vo4_ = nullptr;
    }
    if (ldo_mipi_phy_) {
      esp_ldo_release_channel(ldo_mipi_phy_);
      ldo_mipi_phy_ = nullptr;
    }
  }
#endif

  bool initPanelHardware() {
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
    // Waveshare's 4B Arduino example: RGB data pins, ST7701 init over XCA9554,
    // and GT911 reset on the same expander. Keep LVGL at 720 to match firmware_sim_480.
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
      // Keep RGB DMA bounce buffers in internal DRAM while the 480x480 LVGL
      // framebuffer and 720-to-480 scaling buffer remain in PSRAM.
      0, GFX_NOT_DEFINED, false, 0, 0, 480 * 10);
    gfx_ = new Arduino_RGB_Display(480, 480, rgb, 0, true,
                                   expander_, GFX_NOT_DEFINED,
                                   st7701_type1_init_operations,
                                   sizeof(st7701_type1_init_operations));
    // Match Arduino_GFX's 12 MHz default for this Octal-PSRAM target. With
    // Waveshare's 548x518 total timing, this is about 42.3 Hz and 25% less
    // pixel/PSRAM traffic than the previous 16 MHz setting.
    if (!gfx_ || !gfx_->begin(12000000)) {
      Serial.println("[DisplayDriver720] ERROR: Waveshare 4B ST7701 init failed");
      gfx_ = nullptr;
      return false;
    }
    const size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    const size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    Serial.printf(
      "[DisplayDriver720] PSRAM XIP SDK: xip=%d fetch=%d rodata=%d cache64=%d; PSRAM total=%u free=%u; RGB bounce=%u px x2 (~%u bytes)\n",
      CONFIG_SPIRAM_XIP_FROM_PSRAM,
      CONFIG_SPIRAM_FETCH_INSTRUCTIONS,
      CONFIG_SPIRAM_RODATA,
      CONFIG_ESP32S3_DATA_CACHE_LINE_64B,
      static_cast<unsigned>(psram_total),
      static_cast<unsigned>(psram_free),
      480U * 10U,
      static_cast<unsigned>(2U * 480U * 10U * sizeof(uint16_t)));
    if (psram_total == 0) {
      Serial.println("[DisplayDriver720] ERROR: PSRAM is not initialized; refusing the 4B XIP display path");
      return false;
    }
    scaled_flush_buf_ = static_cast<uint16_t*>(heap_caps_malloc(480 * kBufLines * sizeof(uint16_t),
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!scaled_flush_buf_) {
      Serial.println("[DisplayDriver720] ERROR: Waveshare 4B scaling buffer allocation failed");
      return false;
    }
    gfx_->fillScreen(0);
    expander_->pinMode(kPowerKeyExpanderPin, INPUT);
    expander_->pinMode(kSpeakerAmpExpanderPin, OUTPUT);
    expander_->digitalWrite(kSpeakerAmpExpanderPin, LOW);
    init4BBacklightPwm();
    return true;
#elif defined(ESP32) && defined(SOC_MIPI_DSI_SUPPORTED) && SOC_MIPI_DSI_SUPPORTED
    // Waveshare ESP32-P4-WIFI6-Touch-LCD-4B: 4" 720x720 2-lane MIPI-DSI (ST7703)
    // Backlight stays off until ST7703 DBI init + DPI stream are active.
    setWaveshare4BBacklight(false);

    // 1. Enable internal LDO VO3 (2500mV for MIPI-DSI PHY) and VO4 (3300mV per Waveshare BSP)
    esp_ldo_channel_config_t ldo_phy_cfg = {};
    ldo_phy_cfg.chan_id = 3;
    ldo_phy_cfg.voltage_mv = 2500;
    esp_err_t err = esp_ldo_acquire_channel(&ldo_phy_cfg, &ldo_mipi_phy_);
    if (err != ESP_OK) {
      Serial.printf("[DisplayDriver720] ERROR: LDO VO3 (MIPI PHY) init failed: 0x%x\n", static_cast<unsigned>(err));
      cleanupDsiResources();
      return false;
    }

    esp_ldo_channel_config_t ldo_vo4_cfg = {};
    ldo_vo4_cfg.chan_id = 4;
    ldo_vo4_cfg.voltage_mv = 3300;
    err = esp_ldo_acquire_channel(&ldo_vo4_cfg, &ldo_vo4_);
    if (err != ESP_OK) {
      Serial.printf("[DisplayDriver720] ERROR: LDO VO4 init failed: 0x%x\n", static_cast<unsigned>(err));
      cleanupDsiResources();
      return false;
    }

    // 2. Hardware reset ST7703 on GPIO27 (active-low reset per Waveshare BSP / esp_lcd_st7703)
    static constexpr int kLcdRstPin = 27;
    pinMode(kLcdRstPin, OUTPUT);
    digitalWrite(kLcdRstPin, HIGH);
    delay(5);
    digitalWrite(kLcdRstPin, LOW);
    delay(10);
    digitalWrite(kLcdRstPin, HIGH);
    delay(120);

    // 3. Create 2-lane MIPI-DSI bus (480 Mbps per lane per Waveshare 4B BSP)
    esp_lcd_dsi_bus_config_t bus_config = {};
    bus_config.bus_id = 0;
    bus_config.num_data_lanes = 2;
    bus_config.phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT;
    bus_config.lane_bit_rate_mbps = 480.0f;
    err = esp_lcd_new_dsi_bus(&bus_config, &dsi_bus_);
    if (err != ESP_OK || !dsi_bus_) {
      Serial.printf("[DisplayDriver720] ERROR: esp_lcd_new_dsi_bus failed: 0x%x\n", static_cast<unsigned>(err));
      cleanupDsiResources();
      return false;
    }

    // 4. Create MIPI-DSI DBI command interface for ST7703 register configuration
    esp_lcd_dbi_io_config_t dbi_config = {};
    dbi_config.virtual_channel = 0;
    dbi_config.lcd_cmd_bits = 8;
    dbi_config.lcd_param_bits = 8;
    err = esp_lcd_new_panel_io_dbi(dsi_bus_, &dbi_config, &dbi_io_);
    if (err != ESP_OK || !dbi_io_) {
      Serial.printf("[DisplayDriver720] ERROR: esp_lcd_new_panel_io_dbi failed: 0x%x\n", static_cast<unsigned>(err));
      cleanupDsiResources();
      return false;
    }

    // 5. Create MIPI-DSI DPI video panel (720x720 @ 38MHz RGB565, Waveshare 4B timing)
    esp_lcd_dpi_panel_config_t dpi_config = {};
    dpi_config.virtual_channel = 0;
    dpi_config.dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT;
    dpi_config.dpi_clock_freq_mhz = 38.0f;
    dpi_config.in_color_format = LCD_COLOR_FMT_RGB565;
    dpi_config.num_fbs = 1;
    dpi_config.video_timing.h_size = kWidth;
    dpi_config.video_timing.v_size = kHeight;
    dpi_config.video_timing.hsync_back_porch = 50;
    dpi_config.video_timing.hsync_pulse_width = 20;
    dpi_config.video_timing.hsync_front_porch = 50;
    dpi_config.video_timing.vsync_back_porch = 20;
    dpi_config.video_timing.vsync_pulse_width = 4;
    dpi_config.video_timing.vsync_front_porch = 20;
    // Keep DMA2D disabled so esp_lcd_panel_draw_bitmap performs a synchronous copy + cache writeback
    // before flushCbV8/flushCbV9 releases the LVGL draw buffer.
    dpi_config.flags.use_dma2d = 0;

    err = esp_lcd_new_panel_dpi(dsi_bus_, &dpi_config, &panel_handle_);
    if (err != ESP_OK || !panel_handle_) {
      Serial.printf("[DisplayDriver720] ERROR: esp_lcd_new_panel_dpi failed: 0x%x\n", static_cast<unsigned>(err));
      cleanupDsiResources();
      return false;
    }

    // 6. Send ST7703 initialization sequence + SLPOUT (0x11) + DISPON (0x29) over DBI
    static const uint8_t cmd_madctl[] = {0x00}; // RGB element order
    static const uint8_t cmd_colmod[] = {0x55}; // 16-bit RGB565
    static const uint8_t cmd_b9[] = {0xF1, 0x12, 0x83};
    static const uint8_t cmd_b1[] = {0x00, 0x00, 0x00, 0xDA, 0x80};
    static const uint8_t cmd_b2[] = {0x3C, 0x12, 0x30};
    static const uint8_t cmd_b3[] = {0x10, 0x10, 0x28, 0x28, 0x03, 0xFF, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t cmd_b4[] = {0x80};
    static const uint8_t cmd_b5[] = {0x0A, 0x0A};
    static const uint8_t cmd_b6[] = {0x97, 0x97};
    static const uint8_t cmd_b8[] = {0x26, 0x22, 0xF0, 0x13};
    static const uint8_t cmd_ba[] = {
      0x31, 0x81, 0x0F, 0xF9, 0x0E, 0x06, 0x20, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x44, 0x25, 0x00, 0x90, 0x0A, 0x00,
      0x00, 0x01, 0x4F, 0x01, 0x00, 0x00, 0x37
    };
    static const uint8_t cmd_bc[] = {0x47};
    static const uint8_t cmd_bf[] = {0x02, 0x11, 0x00};
    static const uint8_t cmd_c0[] = {0x73, 0x73, 0x50, 0x50, 0x00, 0x00, 0x12, 0x70, 0x00};
    static const uint8_t cmd_c1[] = {0x25, 0x00, 0x32, 0x32, 0x77, 0xE4, 0xFF, 0xFF, 0xCC, 0xCC, 0x77, 0x77};
    static const uint8_t cmd_c6[] = {0x82, 0x00, 0xBF, 0xFF, 0x00, 0xFF};
    static const uint8_t cmd_c7[] = {0xB8, 0x00, 0x0A, 0x10, 0x01, 0x09};
    static const uint8_t cmd_c8[] = {0x10, 0x40, 0x1E, 0x02};
    static const uint8_t cmd_cc[] = {0x0B};
    static const uint8_t cmd_e0[] = {
      0x00, 0x0B, 0x10, 0x2C, 0x3D, 0x3F, 0x42, 0x3A, 0x07, 0x0D,
      0x0F, 0x13, 0x15, 0x13, 0x14, 0x0F, 0x16, 0x00, 0x0B, 0x10,
      0x2C, 0x3D, 0x3F, 0x42, 0x3A, 0x07, 0x0D, 0x0F, 0x13, 0x15,
      0x13, 0x14, 0x0F, 0x16
    };
    static const uint8_t cmd_e3[] = {0x07, 0x07, 0x0B, 0x0B, 0x0B, 0x0B, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xC0, 0x10};
    static const uint8_t cmd_e9[] = {
      0xC8, 0x10, 0x0A, 0x00, 0x00, 0x80, 0x81, 0x12, 0x31, 0x23,
      0x4F, 0x86, 0xA0, 0x00, 0x47, 0x08, 0x00, 0x00, 0x0C, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x98, 0x02,
      0x8B, 0xAF, 0x46, 0x02, 0x88, 0x88, 0x88, 0x88, 0x88, 0x98,
      0x13, 0x8B, 0xAF, 0x57, 0x13, 0x88, 0x88, 0x88, 0x88, 0x88,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00
    };
    static const uint8_t cmd_ea[] = {
      0x97, 0x0C, 0x09, 0x09, 0x09, 0x78, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x9F, 0x31, 0x8B, 0xA8, 0x31, 0x75, 0x88, 0x88,
      0x88, 0x88, 0x88, 0x9F, 0x20, 0x8B, 0xA8, 0x20, 0x64, 0x88,
      0x88, 0x88, 0x88, 0x88, 0x23, 0x00, 0x00, 0x02, 0x71, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x40, 0x80, 0x81, 0x00, 0x00, 0x00,
      0x00
    };
    static const uint8_t cmd_ef[] = {0xFF, 0xFF, 0x01};
    static const uint8_t cmd_zero[] = {0x00};

    static const St7703InitCmd kSt7703InitSeq[] = {
      {0x36, cmd_madctl, sizeof(cmd_madctl), 0},
      {0x3A, cmd_colmod, sizeof(cmd_colmod), 0},
      {0xB9, cmd_b9, sizeof(cmd_b9), 0},
      {0xB1, cmd_b1, sizeof(cmd_b1), 0},
      {0xB2, cmd_b2, sizeof(cmd_b2), 0},
      {0xB3, cmd_b3, sizeof(cmd_b3), 0},
      {0xB4, cmd_b4, sizeof(cmd_b4), 0},
      {0xB5, cmd_b5, sizeof(cmd_b5), 0},
      {0xB6, cmd_b6, sizeof(cmd_b6), 0},
      {0xB8, cmd_b8, sizeof(cmd_b8), 0},
      {0xBA, cmd_ba, sizeof(cmd_ba), 0},
      {0xBC, cmd_bc, sizeof(cmd_bc), 0},
      {0xBF, cmd_bf, sizeof(cmd_bf), 0},
      {0xC0, cmd_c0, sizeof(cmd_c0), 0},
      {0xC1, cmd_c1, sizeof(cmd_c1), 0},
      {0xC6, cmd_c6, sizeof(cmd_c6), 0},
      {0xC7, cmd_c7, sizeof(cmd_c7), 0},
      {0xC8, cmd_c8, sizeof(cmd_c8), 0},
      {0xCC, cmd_cc, sizeof(cmd_cc), 0},
      {0xE0, cmd_e0, sizeof(cmd_e0), 0},
      {0xE3, cmd_e3, sizeof(cmd_e3), 0},
      {0xE9, cmd_e9, sizeof(cmd_e9), 0},
      {0xEA, cmd_ea, sizeof(cmd_ea), 0},
      {0xEF, cmd_ef, sizeof(cmd_ef), 0},
      {0x11, cmd_zero, sizeof(cmd_zero), 250}, // Sleep Out
      {0x29, cmd_zero, sizeof(cmd_zero), 50},  // Display On
    };

    for (size_t i = 0; i < sizeof(kSt7703InitSeq) / sizeof(kSt7703InitSeq[0]); ++i) {
      err = esp_lcd_panel_io_tx_param(
        dbi_io_,
        kSt7703InitSeq[i].cmd,
        kSt7703InitSeq[i].data,
        kSt7703InitSeq[i].data_bytes
      );
      if (err != ESP_OK) {
        Serial.printf(
          "[DisplayDriver720] ERROR: ST7703 DBI cmd 0x%02X failed: 0x%x\n",
          kSt7703InitSeq[i].cmd,
          static_cast<unsigned>(err)
        );
        cleanupDsiResources();
        return false;
      }
      if (kSt7703InitSeq[i].delay_ms > 0) {
        delay(kSt7703InitSeq[i].delay_ms);
      }
    }

    // 7. Start DPI video stream and enable active-low PWM backlight + GPIO33 enable
    err = esp_lcd_panel_init(panel_handle_);
    if (err != ESP_OK) {
      Serial.printf("[DisplayDriver720] ERROR: esp_lcd_panel_init (DPI) failed: 0x%x\n", static_cast<unsigned>(err));
      cleanupDsiResources();
      return false;
    }

    setWaveshare4BBacklight(true);
    return true;
#elif defined(ESP32) && defined(SOC_LCD_RGB_SUPPORTED) && SOC_LCD_RGB_SUPPORTED
    // Guition ESP32-S3 4" 720x720 RGB Panel
    pinMode(38, OUTPUT);
    digitalWrite(38, LOW);

    esp_lcd_rgb_panel_config_t rgb_config = {};
    rgb_config.clk_src = LCD_CLK_SRC_DEFAULT;
    rgb_config.timings.pclk_hz = 16000000;
    rgb_config.timings.h_res = kWidth;
    rgb_config.timings.v_res = kHeight;
    rgb_config.timings.hsync_pulse_width = 8;
    rgb_config.timings.hsync_back_porch = 20;
    rgb_config.timings.hsync_front_porch = 40;
    rgb_config.timings.vsync_pulse_width = 8;
    rgb_config.timings.vsync_back_porch = 12;
    rgb_config.timings.vsync_front_porch = 20;
    rgb_config.data_width = 16;
    rgb_config.bits_per_pixel = 16;
    rgb_config.num_fbs = 1;
    rgb_config.bounce_buffer_size_px = kWidth * 10;
    rgb_config.hsync_gpio_num = 16;
    rgb_config.vsync_gpio_num = 17;
    rgb_config.de_gpio_num = 18;
    rgb_config.pclk_gpio_num = 21;
    rgb_config.disp_gpio_num = -1;
    const int pins[16] = {4, 5, 6, 7, 15, 8, 20, 3, 46, 9, 10, 11, 12, 13, 14, 0};
    for (int i = 0; i < 16; ++i) {
      rgb_config.data_gpio_nums[i] = pins[i];
    }
    rgb_config.flags.fb_in_psram = 1;

    esp_err_t err = esp_lcd_new_rgb_panel(&rgb_config, &panel_handle_);
    if (err != ESP_OK || !panel_handle_) {
      Serial.printf("[DisplayDriver720] ERROR: esp_lcd_new_rgb_panel failed: 0x%x\n", static_cast<unsigned>(err));
      panel_handle_ = nullptr;
      return false;
    }
    err = esp_lcd_panel_reset(panel_handle_);
    if (err != ESP_OK) {
      Serial.printf("[DisplayDriver720] ERROR: esp_lcd_panel_reset failed: 0x%x\n", static_cast<unsigned>(err));
      esp_lcd_panel_del(panel_handle_);
      panel_handle_ = nullptr;
      return false;
    }
    err = esp_lcd_panel_init(panel_handle_);
    if (err != ESP_OK) {
      Serial.printf("[DisplayDriver720] ERROR: esp_lcd_panel_init (RGB) failed: 0x%x\n", static_cast<unsigned>(err));
      esp_lcd_panel_del(panel_handle_);
      panel_handle_ = nullptr;
      return false;
    }
    digitalWrite(38, HIGH);
    return true;
#else
    return true;
#endif
  }

  void initTouchHardware() {
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
    Wire.begin(47, 48, 400000);
    expander_->pinMode(5, OUTPUT);
    expander_->pinMode(6, OUTPUT);
    expander_->digitalWrite(6, LOW);
    delay(20);
    expander_->digitalWrite(5, LOW);
    delay(20);
    expander_->digitalWrite(5, HIGH);
    delay(100);
#elif defined(ESP32) && defined(SOC_MIPI_DSI_SUPPORTED) && SOC_MIPI_DSI_SUPPORTED
    Wire.begin(7, 8, 400000);
#elif defined(ESP32)
    Wire.begin(19, 45, 400000);
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
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
    auto* self = instance_;
    if (!self || !self->gfx_ || !self->scaled_flush_buf_ || !area || !color_p) return;
    flushScaled720To480(area->x1, area->y1, area->x2, area->y2,
      color_p, self->scaled_flush_buf_, kBufLines,
      [self](int x, int y, uint16_t* pixels, int width, int rows) {
        self->gfx_->draw16bitRGBBitmap(x, y, pixels, width, rows);
      });
#elif defined(ESP32)
    if (instance_ && instance_->panel_handle_ && area && color_p) {
      esp_err_t err = esp_lcd_panel_draw_bitmap(
        instance_->panel_handle_,
        area->x1,
        area->y1,
        area->x2 + 1,
        area->y2 + 1,
        color_p
      );
      if (err != ESP_OK) {
        Serial.printf("[DisplayDriver720] WARN: esp_lcd_panel_draw_bitmap failed: 0x%x\n", static_cast<unsigned>(err));
      }
    }
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
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
      *out_x = static_cast<int16_t>(*out_x * 3 / 2);
      *out_y = static_cast<int16_t>(*out_y * 3 / 2);
#endif
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
    auto* self = static_cast<DisplayDriver720*>(lv_display_get_user_data(disp));
    if (self && self->panel_flush_fn_) {
      self->panel_flush_fn_(area, reinterpret_cast<lv_color_t*>(px_map));
    }
    lv_display_flush_ready(disp);
  }

  static void touchReadCbV9(lv_indev_t* indev, lv_indev_data_t* data) {
    auto* self = static_cast<DisplayDriver720*>(lv_indev_get_user_data(indev));
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
    auto* self = static_cast<DisplayDriver720*>(disp_drv->user_data);
    if (self && self->panel_flush_fn_) {
      self->panel_flush_fn_(area, color_p);
    }
    lv_disp_flush_ready(disp_drv);
  }

  static void touchReadCbV8(lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
    auto* self = static_cast<DisplayDriver720*>(indev_drv->user_data);
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

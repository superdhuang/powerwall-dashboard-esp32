#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include "display_driver.hpp"
#include "wifi_manager_portal.hpp"
#include "powerwall_client.hpp"
#include "daily_history_store.hpp"
#include "weekly_history_recorder.hpp"
#include "weather_client.hpp"
#include "gateway_console.hpp"
#include "console_preview.hpp"
#include "history_save_console.hpp"
#include "ui_720x720_lvgl.hpp"
#include <Preferences.h>
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
#include "board_audio_4b.hpp"
#include "button_navigation.hpp"
#include "simulator_voice_state.hpp"
#endif

DisplayDriver720 displayDriver;
WiFiManagerPortal wifiPortal;
PowerwallLocalClient pwClient;
DailyHistoryStore historyStore;
WeeklyHistoryRecorder weeklyRecorder;
LocalWeatherClient weatherClient;
UI720x720 display720;
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
BoardAudio4B boardAudio;
static ButtonNavigation buttonNavigation;
static uint8_t deviceBrightness = 80;
static uint8_t deviceVolume = 85;
static volatile bool deviceVoiceEnabled = true;
static bool deviceSettingsDirty = false;
static unsigned long deviceSettingsChangedMs = 0;
static SimulatorVoiceStateTracker voiceStateTracker;

static void loadDeviceSettings() {
  Preferences prefs;
  if (!prefs.begin("pw_device", true)) return;
  deviceBrightness = std::min<uint8_t>(prefs.getUChar("brightness", 80), 100);
  deviceVolume = std::min<uint8_t>(prefs.getUChar("volume", 85), 100);
  deviceVoiceEnabled = prefs.getBool("voice_enabled", true);
  prefs.end();
}

static void saveDeviceSettingsIfDue(bool force = false) {
  if (!deviceSettingsDirty || (!force && millis() - deviceSettingsChangedMs < 500)) return;
  Preferences prefs;
  if (!prefs.begin("pw_device", false)) return;
  prefs.putUChar("brightness", deviceBrightness);
  prefs.putUChar("volume", deviceVolume);
  prefs.putBool("voice_enabled", deviceVoiceEnabled);
  prefs.end();
  deviceSettingsDirty = false;
}

static void onBrightnessChanged(void*, uint8_t value) {
  deviceBrightness = value;
  displayDriver.setBrightness(value);
  deviceSettingsDirty = true;
  deviceSettingsChangedMs = millis();
}

static void onVolumeChanged(void*, uint8_t value) {
  deviceVolume = value;
  boardAudio.setVolume(value);
  deviceSettingsDirty = true;
  deviceSettingsChangedMs = millis();
}

static void onTestVoice(void*) { boardAudio.playTestVoice(); }

static void onVoiceEnabledChanged(void*, bool enabled) {
  deviceVoiceEnabled = enabled;
  deviceSettingsDirty = true;
  deviceSettingsChangedMs = millis();
}

static BoardAudio4B::VoicePhrase boardVoicePhrase(SimulatorVoiceState state) {
  switch (state) {
    case SimulatorVoiceState::Charging: return BoardAudio4B::VoicePhrase::Charging;
    case SimulatorVoiceState::Discharging: return BoardAudio4B::VoicePhrase::Discharging;
    case SimulatorVoiceState::OffGrid: return BoardAudio4B::VoicePhrase::OffGrid;
    default: return BoardAudio4B::VoicePhrase::Charging;
  }
}

static void onRestart(void*) {
  bool powerPressed = false;
  if (digitalRead(0) == LOW || !displayDriver.read4BPowerButton(powerPressed) || powerPressed) {
    Serial.println("[Restart] Release both side buttons before restarting");
    return;
  }
  delay(100);
  if (digitalRead(0) == LOW || !displayDriver.read4BPowerButton(powerPressed) || powerPressed) {
    Serial.println("[Restart] Cancelled: a side button is pressed");
    return;
  }
  saveDeviceSettingsIfDue(true);
  Serial.println("[Restart] Settings button confirmed; restarting");
  Serial.flush();
  ESP.restart();
}
#endif

static SemaphoreHandle_t dataMutex = nullptr;
static TaskHandle_t networkTaskHandle = nullptr;
static PowerwallTelemetry sharedTelemetry;
static WeatherSnapshot sharedWeather;
static WeeklyHistory sharedWeeklyHistory;
static GatewayNVSConfig pendingConfig;
static bool configPending = false;
static bool sharedDataDirty = true;
static bool lastWifiConnected = false;
static bool displayReady = false;
static ConsolePreview consolePreview;
static bool previewDirty = false;
static HistorySaveRequest manualHistorySave;
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
enum class WifiStartupPhase : uint8_t {
  Connecting = 0, Portal = 1, Connected = 2, Hidden = 3,
  Powerwall = 4, Weather = 5, GatewayRetry = 6, GatewaySetup = 7
};
struct WifiPortalSnapshot {
  GatewayNVSConfig config;
  char ssid[33] = {};
  char setupUrl[48] = {};
  WifiStartupPhase phase = WifiStartupPhase::Connecting;
  bool gatewayConfigured = false;
  uint32_t configRevision = 0;
};
static QueueHandle_t wifiPortalSnapshotQueue = nullptr;
static TaskHandle_t wifiPortalTaskHandle = nullptr;
static bool wifiPortalTaskStarted = false;
static bool wifiSnapshotApplied = false;
static uint32_t appliedWifiConfigRevision = 0;
static uint8_t sharedWifiStartupPhase = 0xff;
static uint8_t lastPortalWifiPhase = 0xff;
static char lastPortalWifiSsid[33] = {};
static uint8_t lastWifiCardPhase = 0xff;
static char lastWifiCardSsid[33] = {};
static bool wifiStartupCardDismissed = false;
static uint32_t firstStartupPowerwallSuccessMs = 0;
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
static bool sharedGatewayConfigured = false;
static char lastWifiCardUrl[48] = {};
#endif
#endif

#if CONFIG_IDF_TARGET_ESP32P4
static constexpr const char* kBoardName = "ESP32-P4";
#else
static constexpr const char* kBoardName = "ESP32-S3";
#endif

static void handleSerialKey(char key) {
  if (key == 'h') {
    Serial.println(phaseConsoleHelpText());
    Serial.println(consolePreviewHelpText());
    Serial.println(historySaveConsoleHelpText());
    return;
  }
  if (isHistorySaveKey(key)) {
    Serial.println(manualHistorySave.request()
      ? "[history] Manual write queued; waiting for history task (current network poll may still be running)"
      : "[history] Manual write already pending/running; key ignored");
    return;
  }
  if (key == 'z' || key == '1') {
    consolePreview.reset();
    previewDirty = true;
    if (networkTaskHandle) xTaskNotifyGive(networkTaskHandle);
  }
  if (isConsolePreviewKey(key)) {
    // Only this UI-side copy is inspected by the preview controller.
    static PowerwallTelemetry current;
    if (dataMutex && xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      current = sharedTelemetry;
      xSemaphoreGive(dataMutex);
    }
    Serial.println(consolePreview.handle(key, current));
    previewDirty = true;
  }
  // 1 and z also clear sun/moon/weather overrides through the shared UI parser.
  if (isPhaseConsolePresetKey(key) || key == '1') {
    if (displayReady) {
      const char command[2] = {key == '1' ? 'z' : key, '\0'};
      char response[256] = {};
      display720.handlePhaseConsoleLine(command, response, sizeof(response));
      Serial.println(response);
    } else Serial.println("[phase] display unavailable");
  }
}

static void pollPhaseSerialConsole() {
  static PhaseConsoleLineBuffer input;
  char line[kPhaseConsoleMaxLineLength + 1] = {};
  while (Serial.available() > 0) {
    const char ch = static_cast<char>(Serial.read());
    if (input.empty() && (isPhaseConsolePresetKey(ch) || isConsolePreviewKey(ch) || isHistorySaveKey(ch))) {
      handleSerialKey(normalizePhaseConsolePresetKey(ch));
      continue;
    }
    const PhaseConsoleLineResult result = input.push(ch, line, sizeof(line));
    if (result == PhaseConsoleLineResult::TooLong) {
      Serial.println("[phase] input too long (max 63 characters)");
    } else if (result == PhaseConsoleLineResult::Ready) {
      if (!line[0]) continue;
      const char key = normalizePhaseConsolePresetKey(line[0]);
      if (!line[1] && (isPhaseConsolePresetKey(key) || isConsolePreviewKey(key) || isHistorySaveKey(key))) handleSerialKey(key);
      else Serial.println("[console] Unknown command; press h for help");
    }
  }
}

// Runs only on networkPollerTask, alongside all normal history access.
static void forceHistorySave() {
  tm today{};
  if (!historyStore.ready()) {
    logHistorySaveEvent(Serial, "daily+weekly", "manual", "SKIPPED: SPIFFS not mounted");
    return;
  }
  if (!getLocalTime(&today, 10) || today.tm_year < 120 || !pwClient.historyIsFor(today)) {
    logHistorySaveEvent(Serial, "daily+weekly", "manual", "SKIPPED: clock/current-day history not ready");
    return;
  }
  weeklyRecorder.update(pwClient, time(nullptr));
  if (!weeklyRecorder.ready()) {
    logHistorySaveEvent(Serial, "daily+weekly", "manual", "SKIPPED: weekly history not ready");
    return;
  }
  if (runHistorySave(Serial, "daily", "manual", [] { return historyStore.saveToday(pwClient); }))
    pwClient.markHistorySaved();
  runHistorySave(Serial, "weekly", "manual", [] { return weeklyRecorder.flush(true); });
}

#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
static bool publishWifiStartupState(WifiStartupPhase phase, bool includeTelemetry = false) {
  if (!dataMutex || xSemaphoreTake(dataMutex, pdMS_TO_TICKS(250)) != pdTRUE) return false;
  sharedWifiStartupPhase = static_cast<uint8_t>(phase);
  if (includeTelemetry) {
    sharedTelemetry = pwClient.latest;
    sharedDataDirty = true;
  }
  xSemaphoreGive(dataMutex);
  return true;
}
#endif

static void networkPollerTask(void* /*param*/) {
  uint8_t consecutiveFailures = 0;
  bool historyLoaded = false;
  bool wifiStateReported = false;
  bool previousWifiConnected = false;
  bool configLoaded = false;

  for (;;) {
    GatewayNVSConfig cfgToApply;
    bool applyNewConfig = false;
    bool gatewayConfigured = true;

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      if (configPending) {
        cfgToApply = pendingConfig;
        configPending = false;
        applyNewConfig = true;
      }
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
      gatewayConfigured = sharedGatewayConfigured;
#endif
      xSemaphoreGive(dataMutex);
    }

    if (applyNewConfig) {
      pwClient.configure(
        cfgToApply.gateway_ip,
        cfgToApply.gateway_password,
        cfgToApply.gateway_email,
        cfgToApply.site_title
      );
      consecutiveFailures = 0;
      configLoaded = true;
    }

    if (!configLoaded) {
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
      continue;
    }

    if (!historyLoaded) historyLoaded = historyStore.loadToday(pwClient);
    weeklyRecorder.update(pwClient, time(nullptr));

    bool pollSuccess = false;
    const bool wifiConnected = WiFi.status() == WL_CONNECTED;
    if (!wifiStateReported || wifiConnected != previousWifiConnected) {
      Serial.printf("[Gateway] Wi-Fi %s | %s\n", wifiConnected ? "connected" : "disconnected",
                    wifiConnected ? "starting Gateway polling" : "等待配網；Gateway polling paused (powerwall-dashboard)");
      wifiStateReported = true;
      previousWifiConnected = wifiConnected;
    }
    if (wifiConnected && !gatewayConfigured) {
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
      publishWifiStartupState(WifiStartupPhase::GatewaySetup);
#endif
      pwClient.latest.ok = false;
      pwClient.latest.stale = true;
      weatherClient.refreshState();
      Serial.println("[Gateway] 設定尚未儲存；請開啟本機設定頁輸入 Gateway IP 與密碼。");
    } else if (wifiConnected) {
      const bool forceLogin = applyNewConfig;
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
      if (!publishWifiStartupState(WifiStartupPhase::Powerwall))
        Serial.println("[Wi-Fi card] Could not publish Powerwall request phase; will retry on next poll.");
#endif
      pollSuccess = pwClient.pollLiveData(forceLogin);
      // USB serial monitors may show ANSI escapes literally; keep board logs plain.
      printGatewaySummary(Serial, pwClient.latest, pollSuccess, time(nullptr), false);
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
      if (!publishWifiStartupState(pollSuccess ? WifiStartupPhase::Weather : WifiStartupPhase::GatewayRetry, true))
        Serial.println("[Wi-Fi card] Could not publish Gateway result; will retry on next poll.");
#endif
      if (!pollSuccess) {
        Serial.printf("[Gateway] Heap internal free=%u largest=%u; PSRAM free=%u\n",
                      static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                      static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                      static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
      }
      weatherClient.update(applyNewConfig);

      if (pollSuccess) {
        consecutiveFailures = 0;
        if (pwClient.historyNeedsSave() && runHistorySave(Serial, "daily", "auto", [] {
              return historyStore.saveToday(pwClient);
            })) {
          pwClient.markHistorySaved();
        }
      } else if (consecutiveFailures < 4) {
        ++consecutiveFailures;
      }
    } else {
      pwClient.latest.ok = (pwClient.latest.last_update_ms > 0);
      pwClient.latest.stale = true;
      weatherClient.refreshState();
    }

#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
    const SimulatorVoiceState voiceTransition = voiceStateTracker.observe(
      pollSuccess, pwClient.latest.has_grid_status,
      pwClient.latest.grid_status == "SystemGridConnected",
      pwClient.latest.has_battery_power, pwClient.latest.battery_power_w);
    if (deviceVoiceEnabled && voiceTransition != SimulatorVoiceState::Unknown)
      boardAudio.playVoice(boardVoicePhrase(voiceTransition));
#endif

    weeklyRecorder.update(pwClient, time(nullptr));
    if (weeklyRecorder.needsSave() && !runHistorySave(Serial, "weekly", "auto", [] {
          return weeklyRecorder.flush();
        })) {
      Serial.println("[history] Weekly snapshot write failed; retaining RAM history and retrying next poll.");
    }

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      sharedTelemetry = pwClient.latest;
      sharedWeather   = weatherClient.current;
      weeklyRecorder.snapshot(sharedWeeklyHistory, pwClient);
      sharedDataDirty = true;
      xSemaphoreGive(dataMutex);
    }

    // Normal poll every 5s; exponential backoff (5s -> 10s -> 20s -> 30s max) on repeated failures
    unsigned long waitMs = 5000UL;
    if (!pollSuccess && consecutiveFailures > 1) {
      unsigned long backoffMs = 5000UL << (consecutiveFailures - 1);
      waitMs = (backoffMs > 30000UL) ? 30000UL : backoffMs;
    }

    // Keep the existing poll deadline/config wakeup. Manual saves are serviced
    // here without an extra network request, UI update, or concurrent writer.
    const unsigned long waitStarted = millis();
    for (;;) {
      if (manualHistorySave.take()) {
        forceHistorySave();
        manualHistorySave.finish();
      }
      const unsigned long elapsed = millis() - waitStarted;
      if (elapsed >= waitMs) break;
      const unsigned long remaining = waitMs - elapsed;
      if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(remaining < 50 ? remaining : 50))) break;
    }
  }
}

#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
static void publishWifiPortalSnapshot(const GatewayNVSConfig& config, const char* ssid,
                                      WifiStartupPhase phase, bool gatewayConfigured,
                                      uint32_t revision) {
  if (!wifiPortalSnapshotQueue) return;
  WifiPortalSnapshot snapshot{};
  snapshot.config = config;
  if (ssid && *ssid) strlcpy(snapshot.ssid, ssid, sizeof(snapshot.ssid));
  const IPAddress address = WiFi.status() == WL_CONNECTED
    ? WiFi.localIP() : (wifiPortal.captivePortalActive() ? WiFi.softAPIP() : IPAddress(0, 0, 0, 0));
  if (address != IPAddress(0, 0, 0, 0)) {
    String url = String("http://") + address.toString();
    strlcpy(snapshot.setupUrl, url.c_str(), sizeof(snapshot.setupUrl));
  }
  snapshot.phase = phase;
  snapshot.gatewayConfigured = gatewayConfigured;
  snapshot.configRevision = revision;
  xQueueOverwrite(wifiPortalSnapshotQueue, &snapshot);
}

static void wifiPortalStartupSnapshot(const GatewayNVSConfig& config, const char* ssid, void*) {
  const WifiStartupPhase phase = ssid && *ssid
    ? WifiStartupPhase::Connecting : WifiStartupPhase::Portal;
  publishWifiPortalSnapshot(config, ssid, phase, wifiPortal.gatewayConfigConfigured(), 0);
}

static WifiStartupPhase currentWifiStartupPhase(const String& savedSsid) {
  const IPAddress ip = WiFi.localIP();
  if (WiFi.status() == WL_CONNECTED && ip != IPAddress(0, 0, 0, 0) &&
      !wifiPortal.gatewayConfigConfigured()) return WifiStartupPhase::GatewaySetup;
  if (WiFi.status() == WL_CONNECTED && ip != IPAddress(0, 0, 0, 0))
    return WifiStartupPhase::Powerwall;
  if (wifiPortal.captivePortalActive() || savedSsid.isEmpty()) return WifiStartupPhase::Portal;
  return WifiStartupPhase::Connecting;
}

static const char* wifiStartupPhaseName(WifiStartupPhase phase) {
  switch (phase) {
    case WifiStartupPhase::Connecting: return "connecting";
    case WifiStartupPhase::Portal: return "portal";
    case WifiStartupPhase::Connected: return "connected";
    case WifiStartupPhase::Hidden: return "hidden";
    case WifiStartupPhase::Powerwall: return "powerwall";
    case WifiStartupPhase::Weather: return "weather";
    case WifiStartupPhase::GatewayRetry: return "gateway-retry";
    case WifiStartupPhase::GatewaySetup: return "gateway-setup";
  }
  return "unknown";
}

static void wifiPortalTask(void*) {
  wifiPortal.begin(false, wifiPortalStartupSnapshot, nullptr);

  uint32_t configRevision = 0;
  if (wifiPortal.configChanged) {
    wifiPortal.configChanged = false;
    ++configRevision;
  }
  String savedSsid = wifiPortal.savedSSID();
  WifiStartupPhase phase = currentWifiStartupPhase(savedSsid);
  publishWifiPortalSnapshot(wifiPortal.config, savedSsid.c_str(), phase,
                            wifiPortal.gatewayConfigConfigured(), configRevision);
  uint32_t publishedRevision = configRevision;
  Serial.printf("[Wi-Fi] Portal task active; phase=%s, saved SSID=%s\n",
                wifiStartupPhaseName(phase), savedSsid.length() ? savedSsid.c_str() : "(none)");

  for (;;) {
    wifiPortal.process();
    if (wifiPortal.configChanged) {
      wifiPortal.configChanged = false;
      ++configRevision;
    }
    String nextSsid = wifiPortal.savedSSID();
    WifiStartupPhase nextPhase = currentWifiStartupPhase(nextSsid);
    if (nextPhase != phase || nextSsid != savedSsid || configRevision != publishedRevision) {
      publishWifiPortalSnapshot(wifiPortal.config, nextSsid.c_str(), nextPhase,
                                wifiPortal.gatewayConfigConfigured(), configRevision);
      if (nextPhase != phase || nextSsid != savedSsid) {
        Serial.printf("[Wi-Fi] Portal state: %s, saved SSID=%s\n",
                      wifiStartupPhaseName(nextPhase), nextSsid.length() ? nextSsid.c_str() : "(none)");
      }
      phase = nextPhase;
      savedSsid = nextSsid;
      publishedRevision = configRevision;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

static void consumeWifiPortalSnapshot() {
  if (!wifiPortalSnapshotQueue) return;
  WifiPortalSnapshot snapshot{};
  if (!dataMutex || xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) != pdTRUE) return;
  const bool haveSnapshot = xQueueReceive(wifiPortalSnapshotQueue, &snapshot, 0) == pdTRUE;
  if (haveSnapshot) {
    const bool configChanged = !wifiSnapshotApplied ||
      snapshot.configRevision != appliedWifiConfigRevision;
    if (configChanged) {
      pendingConfig = snapshot.config;
      configPending = true;
      wifiSnapshotApplied = true;
      appliedWifiConfigRevision = snapshot.configRevision;
      wifiStartupCardDismissed = false;
      firstStartupPowerwallSuccessMs = 0;
      sharedTelemetry.ok = false;
      sharedTelemetry.stale = true;
      sharedTelemetry.last_update_ms = 0;
      sharedWeather.ok = false;
      sharedWeather.stale = true;
      sharedWeather.last_update_ms = 0;
      sharedGatewayConfigured = snapshot.gatewayConfigured;
    }
    sharedWifiStartupPhase = static_cast<uint8_t>(snapshot.phase);
    if (snapshot.phase == WifiStartupPhase::Connecting || snapshot.phase == WifiStartupPhase::Portal) {
      sharedWifiStartupPhase = 0xff;
      wifiStartupCardDismissed = false;
      firstStartupPowerwallSuccessMs = 0;
    }
    xSemaphoreGive(dataMutex);

    if (configChanged) {
      if (displayReady) display720.setGatewayConfigIp(snapshot.config.gateway_ip);
      if (networkTaskHandle) xTaskNotifyGive(networkTaskHandle);
      Serial.printf("[%s] Queuing Gateway configuration %s\n", kBoardName, snapshot.config.gateway_ip);
    }
    const bool phaseChanged = lastPortalWifiPhase != static_cast<uint8_t>(snapshot.phase);
    const bool ssidChanged = strncmp(lastPortalWifiSsid, snapshot.ssid, sizeof(lastPortalWifiSsid)) != 0;
    const bool urlChanged = strncmp(lastWifiCardUrl, snapshot.setupUrl, sizeof(lastWifiCardUrl)) != 0;
    if (displayReady && (phaseChanged || ssidChanged || urlChanged || configChanged)) {
      display720.setWifiStartupCardMode(static_cast<uint8_t>(snapshot.phase), snapshot.ssid,
                                        snapshot.setupUrl);
      lastWifiCardPhase = static_cast<uint8_t>(snapshot.phase);
      strlcpy(lastWifiCardSsid, snapshot.ssid, sizeof(lastWifiCardSsid));
      strlcpy(lastWifiCardUrl, snapshot.setupUrl, sizeof(lastWifiCardUrl));
    }
    lastPortalWifiPhase = static_cast<uint8_t>(snapshot.phase);
    strlcpy(lastPortalWifiSsid, snapshot.ssid, sizeof(lastPortalWifiSsid));
  } else xSemaphoreGive(dataMutex);
}

static void updateWifiStartupCardProgress() {
  if (!displayReady || wifiStartupCardDismissed || !dataMutex ||
      xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) != pdTRUE) return;
  const uint8_t phase = sharedWifiStartupPhase;
  const bool weatherReady = sharedWeather.ok && !sharedWeather.isStale(millis());
  const bool powerReady = sharedTelemetry.ok && !sharedTelemetry.stale &&
    sharedTelemetry.last_update_ms > 0;
  const uint32_t powerSuccessMs = static_cast<uint32_t>(sharedTelemetry.last_update_ms);
  xSemaphoreGive(dataMutex);
  if (phase == 0xff) return;

  if (phase == static_cast<uint8_t>(WifiStartupPhase::Weather)) {
    if (powerReady && firstStartupPowerwallSuccessMs == 0)
      firstStartupPowerwallSuccessMs = powerSuccessMs;
    if (!powerReady) {
      if (lastWifiCardPhase != phase) {
        display720.setWifiStartupCardMode(phase, lastWifiCardSsid, lastWifiCardUrl);
        lastWifiCardPhase = phase;
      }
      return;
    }
    const bool weatherWaitExpired = firstStartupPowerwallSuccessMs > 0 &&
      static_cast<uint32_t>(millis() - firstStartupPowerwallSuccessMs) >= 5000U;
    if (!weatherReady && !weatherWaitExpired) {
      if (lastWifiCardPhase != phase) {
        display720.setWifiStartupCardMode(phase, lastWifiCardSsid, lastWifiCardUrl);
        lastWifiCardPhase = phase;
      }
      return;
    }
    display720.setWifiStartupCardMode(static_cast<uint8_t>(WifiStartupPhase::Connected), lastWifiCardSsid,
                                      lastWifiCardUrl);
    lastWifiCardPhase = static_cast<uint8_t>(WifiStartupPhase::Connected);
    wifiStartupCardDismissed = true;
    Serial.printf("[Wi-Fi card] Startup data ready; fading after %s.\n",
                  weatherReady ? "weather update" : "5s weather wait");
    return;
  }

  if (lastWifiCardPhase != phase) {
    display720.setWifiStartupCardMode(phase, lastWifiCardSsid, lastWifiCardUrl);
    lastWifiCardPhase = phase;
  }
}
#endif

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("[%s 720x720] Booting Powerwall 2 Live Display...\n", kBoardName);
  auto bootStage = [](const char* stage) {
    Serial.printf("[Boot +%lu ms] %s\n", millis(), stage);
  };
  Serial.println(phaseConsoleHelpText());
  Serial.println(consolePreviewHelpText());
  Serial.println(historySaveConsoleHelpText());
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
  bootStage("Loading saved display settings");
  loadDeviceSettings();
#endif

#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
  if (!loadPowerwallFontAssets()) {
    Serial.println("[Fonts] ERROR: assets missing/corrupt; stopped before UI, no formatting. Reflash assets image.");
    while (true) delay(1000);
  }
#endif
  // 1. Initialize LVGL core, PSRAM draw buffer, display flush & touch driver before building widgets
  bootStage("Initializing display and touch");
#if defined(POWERWALL_EMULATOR_HEADLESS)
  Serial.println("[display] Emulator headless: LVGL active; LCD/touch hardware disabled");
  displayReady = displayDriver.begin(
    [](const lv_area_t*, lv_color_t*) {},
    [](int16_t*, int16_t*) -> bool { return false; });
#else
  displayReady = displayDriver.begin();
#endif
  if (!displayReady) {
    Serial.printf("[%s 720x720] ERROR: Display driver initialization failed; skipping UI creation.\n", kBoardName);
  } else {
    bootStage("Display ready; initializing audio and UI");
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
    pinMode(0, INPUT); // BOOT has a board pull-up and is active-low.
    displayDriver.setBrightness(deviceBrightness);
    if (boardAudio.begin([](bool on) -> bool { return displayDriver.setSpeakerAmpEnabled(on); }))
      boardAudio.setVolume(deviceVolume);
    display720.setDeviceSettingsCallbacks(nullptr, onBrightnessChanged, onVolumeChanged,
                                          onTestVoice, onVoiceEnabledChanged);
    display720.setRestartCallback(onRestart);
#endif
    display720.init();
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
    display720.setDeviceSettings(deviceBrightness, deviceVolume, deviceVoiceEnabled);
#endif
    display720.render(sharedTelemetry, sharedWeather, false);
    displayDriver.tick();
    lv_timer_handler();
    bootStage("UI ready");
  }

  // 2. Start tzapu/WiFiManager:
  //    - First boot (unconfigured Wi-Fi): opens SoftAP "powerwall-dashboard" (192.168.4.1) & pops up Captive Portal
  //    - Subsequent boots: auto-connects to saved Wi-Fi and keeps non-blocking web portal active for changing Powerwall IP/Password
#if defined(POWERWALL_EMULATOR_HEADLESS)
  // These are the emulator's virtual AP credentials, never the physical LAN's.
  // Connect STA directly: switching the Hosted bridge through a provisioning
  // SoftAP is unnecessary for a headless emulator and can drop its RPC events.
  Serial.println("[Wi-Fi] Connecting to esp-emu virtual AP myssid...");
  WiFi.begin("myssid", "mypassword");
  const unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 20000UL) delay(50);
#endif
  bootStage("Connecting with saved Wi-Fi settings; portal opens if unavailable");
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
  if (displayReady) display720.setWifiStartupCardMode(0, nullptr);
  wifiPortalSnapshotQueue = xQueueCreate(1, sizeof(WifiPortalSnapshot));
  if (wifiPortalSnapshotQueue &&
      xTaskCreate(wifiPortalTask, "wifi_portal", 12288, nullptr, 1,
                  &wifiPortalTaskHandle) == pdPASS) {
    wifiPortalTaskStarted = true;
    Serial.println("[Wi-Fi] S3 portal task started; main loop remains responsive during Wi-Fi connection.");
  } else {
    Serial.println("[Wi-Fi] ERROR: could not create portal queue/task; continuing UI and Gateway polling without Wi-Fi portal.");
    wifiPortalSnapshotQueue = nullptr;
    wifiPortal.loadStoredConfigForFallback();
    if (displayReady) display720.setGatewayConfigIp(wifiPortal.config.gateway_ip);
  }
#else
  wifiPortal.begin(false);
  bootStage(WiFi.status() == WL_CONNECTED ? "Wi-Fi connected" : "Wi-Fi unavailable; provisioning portal active");
#endif

  // 3. Configure NTP for Taiwan local time (UTC+8)
  bootStage("Starting clock and history storage");
  configTime(8 * 3600, 0, "pool.ntp.org", "time.google.com");

  if (!historyStore.begin()) {
    Serial.println("[history] SPIFFS mount failed; daily and weekly history will remain in RAM.");
  }

  // 4. Seed initial Gateway credentials and start background network polling task
  dataMutex = xSemaphoreCreateMutex();
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
  if (!wifiPortalTaskStarted) {
    pendingConfig = wifiPortal.config;
    configPending = true;
  }
#else
  pendingConfig = wifiPortal.config;
  configPending = true;
#endif
  lastWifiConnected = (WiFi.status() == WL_CONNECTED);

  xTaskCreate(
    networkPollerTask,
    "net_poller",
    16384,
    nullptr,
    1,
    &networkTaskHandle
  );
  bootStage("Startup complete; Gateway polling started");
}

void loop() {
  pollPhaseSerialConsole();

#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
  if (displayReady) {
    static uint32_t lastButtonPollMs = 0;
    const uint32_t now = millis();
    if (static_cast<uint32_t>(now - lastButtonPollMs) >= 20) {
      lastButtonPollMs = now;
      bool powerPressed = false;
      if (displayDriver.read4BPowerButton(powerPressed)) {
        switch (buttonNavigation.update(now, powerPressed, digitalRead(0) == LOW)) {
          case ButtonNavigation::Action::NextPage: display720.nextDashboardPage(); break;
          case ButtonNavigation::Action::Settings: display720.openSettingsPage(); break;
          case ButtonNavigation::Action::None: break;
        }
      }
    }
  }
#endif

  // Keep all LVGL operations in this main loop; the S3 portal task hands off snapshots through a queue.
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
  consumeWifiPortalSnapshot();
  updateWifiStartupCardProgress();
#else
  // Handle non-blocking tzapu/WiFiManager portal requests (allows re-configuring Gateway IP & Password anytime)
  wifiPortal.process();

  if (wifiPortal.configChanged) {
    wifiPortal.configChanged = false;
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
    display720.setGatewayConfigIp(wifiPortal.config.gateway_ip);
#endif
    Serial.printf("[%s] Queuing re-authentication for Powerwall Gateway %s...\n",
                  kBoardName, wifiPortal.config.gateway_ip);
    if (dataMutex && xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      pendingConfig = wifiPortal.config;
      configPending = true;
      xSemaphoreGive(dataMutex);
    }
    if (networkTaskHandle) {
      xTaskNotifyGive(networkTaskHandle);
    }
  }
#endif

  if (displayReady) {
    bool wifiConnected = (WiFi.status() == WL_CONNECTED);
    bool shouldRender = previewDirty || (wifiConnected != lastWifiConnected);
    lastWifiConnected = wifiConnected;

    static PowerwallTelemetry localTel;
    static WeatherSnapshot localWeather;
    static WeeklyHistory localWeeklyHistory;
    bool haveSnapshot = false;

    if (dataMutex && xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      if (sharedDataDirty || shouldRender) {
        localTel        = sharedTelemetry;
        localWeather    = sharedWeather;
        localWeeklyHistory = sharedWeeklyHistory;
        sharedDataDirty = false;
        shouldRender    = true;
        haveSnapshot    = true;
      }
      xSemaphoreGive(dataMutex);
    }

    if (shouldRender && haveSnapshot) {
      previewDirty = false;
      bool configured = wifiConnected;
      consolePreview.apply(localTel, localWeather, configured);
      display720.setWeeklyHistory(localWeeklyHistory);
      display720.render(localTel, localWeather, configured);
    }

    displayDriver.tick();
    lv_timer_handler();
  }
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
  boardAudio.update();
  saveDeviceSettingsIfDue();
#endif
  delay(5);
}

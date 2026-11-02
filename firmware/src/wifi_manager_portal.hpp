#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <Preferences.h>
#include <esp_wifi.h>
#include <cstring>
#include "gateway_config_status.hpp"
#include "native480_i18n.hpp"
#if defined(POWERWALL_NATIVE480)
#include "native480_timezone.hpp"
#endif

struct GatewayNVSConfig {
  char gateway_ip[64]       = "";
  char gateway_password[64] = "";
  char gateway_email[64]    = "customer@example.com";
#if defined(POWERWALL_NATIVE480)
#if defined(POWERWALL_NATIVE480_LANG_EN)
  char site_title[32]       = "My Home";
#else
  char site_title[32]       = "我的家";
#endif
#else
  char site_title[32]       = "我的家";
#endif
};

class WiFiManagerPortal {
public:
  using StartupCallback = void (*)(const GatewayNVSConfig&, const char*, void*);
#if defined(POWERWALL_NATIVE480)
  using TimezoneApplyCallback = void (*)(int, void*);
#endif
  GatewayNVSConfig config;
  bool configChanged = false;
#if defined(POWERWALL_NATIVE480)
  int timezoneOffsetMinutes = native480_timezone::kDefaultOffsetMinutes;
#endif

#if defined(POWERWALL_NATIVE480)
  void setTimezoneApplyCallback(TimezoneApplyCallback callback, void* context = nullptr) {
    timezoneApplyCallback_ = callback;
    timezoneApplyContext_ = context;
  }
#endif

  void begin(bool forceCaptivePortal = false, StartupCallback startupCallback = nullptr,
             void* callbackContext = nullptr) {
    loadFromNVS();
#if defined(POWERWALL_WAVESHARE_S3_LCD_4B)
    if (!WiFi.mode(WIFI_STA)) Serial.println("[Wi-Fi] Could not initialize STA mode before reading saved SSID.");
    const String savedSsid = readSavedSSID();
#else
    const String savedSsid = wm.getWiFiSSID();
#endif
    if (startupCallback) startupCallback(config, savedSsid.c_str(), callbackContext);

    wm.setDarkMode(true);
    wm.setTitle(portalLabel("Powerwall 控制面板", "Powerwall Dashboard", "Powerwall Dashboard"));
#if defined(POWERWALL_NATIVE480)
    // Keep WiFiManager actions and routes intact while localizing fixed labels.
    wm.setCustomHeadElement(portalTranslations());
#endif
    wm.setHostname("powerwall-esp32p4");

    // Custom WiFiManagerParameter fields for Local Powerwall Gateway IP & Password (created once)
    if (!params_added_) {
      param_gw_ip    = new WiFiManagerParameter("gw_ip", portalLabel("本機 Powerwall Gateway IP", "Gateway IP", "Local Powerwall Gateway IP"), config.gateway_ip, 64);
      param_gw_pass  = new WiFiManagerParameter("gw_pass", portalLabel("Powerwall Gateway 密碼（貼紙後5碼）", "Gateway password (last 5)", "Powerwall Gateway Password (貼紙後5碼)"), config.gateway_password, 64, "type='password'");
#if !defined(POWERWALL_NATIVE480)
      param_gw_email = new WiFiManagerParameter("gw_email", portalLabel("Powerwall 登入 Email", "Login email", "Powerwall Login Email"), config.gateway_email, 64);
#endif
      param_title    = new WiFiManagerParameter("site_title", portalLabel("顯示站點名稱", "Site name", "顯示站點名稱"), config.site_title, 32);
#if defined(POWERWALL_NATIVE480)
      char timezoneValue[8] = {};
      snprintf(timezoneValue, sizeof(timezoneValue), "%d", timezoneOffsetMinutes);
      param_timezone = new WiFiManagerParameter(
        "tz_min", portalLabel("時區（UTC -12:00 至 +14:00，每 1 小時）",
                               "Time zone (UTC -12:00 to +14:00, 1-hour steps)",
                               "Time zone"),
        timezoneValue, 5, "type='text' inputmode='numeric' required");
#endif

      wm.addParameter(param_gw_ip);
      wm.addParameter(param_gw_pass);
#if !defined(POWERWALL_NATIVE480)
      wm.addParameter(param_gw_email);
#endif
      wm.addParameter(param_title);
#if defined(POWERWALL_NATIVE480)
      wm.addParameter(param_timezone);
#endif
      params_added_ = true;
    } else {
      syncParamsFromConfig();
    }

    wm.setSaveParamsCallback([this]() {
      this->pullParamsAndSaveNVS();
    });
    wm.setSaveConfigCallback([this]() {
      this->pullParamsAndSaveNVS();
    });

    std::vector<const char *> menu = {"wifi", "param", "info", "sep", "restart", "exit"};
    wm.setMenu(menu);

    wm.setConfigPortalBlocking(false);
    // Bound the saved-credential attempt so an unavailable AP cannot stall boot.
    // autoConnect reads Wi-Fi credentials from ESP32 NVS, then starts the AP portal.
    wm.setConnectTimeout(12);

    if (forceCaptivePortal) {
      wm.startConfigPortal("powerwall-dashboard");
      web_portal_started_ = false;
    } else {
      bool connected = WiFi.status() == WL_CONNECTED || wm.autoConnect("powerwall-dashboard");
      if (connected) {
        // Keep non-blocking web portal active so user can change Wi-Fi or Gateway IP/Password anytime
        wm.startWebPortal();
        web_portal_started_ = true;
      } else {
        web_portal_started_ = false;
      }
    }
  }

  void process() {
    wm.process();
    if (!web_portal_started_ && WiFi.status() == WL_CONNECTED) {
      wm.startWebPortal();
      web_portal_started_ = true;
    }
  }

  String savedSSID() { return readSavedSSID(); }
  bool captivePortalActive() { return wm.getConfigPortalActive(); }
  bool gatewayConfigConfigured() const { return gateway_config_configured_; }
  void loadStoredConfigForFallback() {
    loadFromNVS();
  }

  void updateGatewayConfig(const char* ip, const char* pass, const char* email, const char* title) {
    if (ip && strlen(ip) > 0) strlcpy(config.gateway_ip, ip, sizeof(config.gateway_ip));
    if (pass && strlen(pass) > 0) strlcpy(config.gateway_password, pass, sizeof(config.gateway_password));
#if !defined(POWERWALL_NATIVE480)
    if (email && strlen(email) > 0) strlcpy(config.gateway_email, email, sizeof(config.gateway_email));
#else
    (void)email;
    strlcpy(config.gateway_email, "customer@example.com", sizeof(config.gateway_email));
#endif
    if (title && strlen(title) > 0) strlcpy(config.site_title, title, sizeof(config.site_title));
    syncParamsFromConfig();
    saveToNVS();
    configChanged = true;
  }

  void resetSettings() {
    wm.resetSettings();
    prefs.begin("pw_nvs", false);
    prefs.clear();
    prefs.end();
    strlcpy(config.gateway_ip, "", sizeof(config.gateway_ip));
    strlcpy(config.gateway_password, "", sizeof(config.gateway_password));
    strlcpy(config.gateway_email, "customer@example.com", sizeof(config.gateway_email));
    strlcpy(config.site_title, native480_i18n::kDefaultSiteTitle, sizeof(config.site_title));
#if defined(POWERWALL_NATIVE480)
    timezoneOffsetMinutes = native480_timezone::kDefaultOffsetMinutes;
    prefs.begin("pw_nvs", false);
    prefs.putInt("tz_min", timezoneOffsetMinutes);
    prefs.end();
    applyTimezone();
#endif
    gateway_config_configured_ = false;
    syncParamsFromConfig();
    configChanged = true;
  }

private:
#if defined(POWERWALL_NATIVE480)
  static const char* portalTranslations() {
#if defined(POWERWALL_NATIVE480_LANG_EN)
    static constexpr char kScript[] =
      "<script>document.addEventListener('DOMContentLoaded',function(){"
      "var w=document.createTreeWalker(document.body,NodeFilter.SHOW_TEXT),n;"
      "while(n=w.nextNode())if(n.nodeValue.trim()==='Setup')n.nodeValue=n.nodeValue.replace('Setup','Settings');"
      "var i=document.querySelector('input[name=\"tz_min\"]');if(i){var s=document.createElement('select');"
      "s.id=i.id;s.name=i.name;s.className=i.className;var v=parseInt(i.value,10);"
      "for(var m=-720;m<=840;m+=60){var a=Math.abs(m),label='UTC'+(m<0?'-':'+')+"
      "('0'+Math.floor(a/60)).slice(-2)+':'+('0'+a%60).slice(-2),o=new Option(label,''+m,m===v,m===v);s.add(o);}"
      "i.parentNode.replaceChild(s,i);}"
      "});</script>";
#else
    static constexpr char kScript[] =
      "<script>document.addEventListener('DOMContentLoaded',function(){"
      "document.documentElement.lang='zh-Hant';"
      "var t=new Map(["
      "['Configure WiFi','設定 Wi-Fi'],"
      "['Configure WiFi (No scan)','手動設定 Wi-Fi'],"
      "['Info','資訊'],['Setup','設定'],['Restart','重新啟動'],['Exit','離開'],"
      "['SSID','Wi-Fi 名稱（SSID）'],['Password','Wi-Fi 密碼'],"
      "['Show Password','顯示密碼'],['Save','儲存'],['Refresh','重新掃描'],['Back','返回'],"
      "['Connected','已連線'],['Not connected','尚未連線'],['No AP set','尚未設定 Wi-Fi'],"
      "['Saving Credentials','正在儲存 Wi-Fi 設定'],"
      "['Trying to connect ESP to network.','正在連線至 Wi-Fi 網路。'],"
      "['If it fails reconnect to AP to try again','若連線失敗，請重新連線至設定 Wi-Fi 再試一次'],"
      "['Authentication failure','驗證失敗'],['AP not found','找不到 Wi-Fi'],"
      "['Could not connect','連線失敗']]);"
      "var w=document.createTreeWalker(document.body,NodeFilter.SHOW_TEXT),n;"
      "while(n=w.nextNode()){var s=n.nodeValue,k=s.trim();if(t.has(k))n.nodeValue=s.replace(k,t.get(k));"
      "else if(/^\\s+to\\s+/.test(s))n.nodeValue=s.replace(' to ',' 至 ');"
      "else if(/^\\s+with IP\\s+/.test(s))n.nodeValue=s.replace('with IP ','IP：');}"
      "var i=document.querySelector('input[name=\"tz_min\"]');if(i){var s=document.createElement('select');"
      "s.id=i.id;s.name=i.name;s.className=i.className;var v=parseInt(i.value,10);"
      "for(var m=-720;m<=840;m+=60){var a=Math.abs(m),label='UTC'+(m<0?'-':'+')+"
      "('0'+Math.floor(a/60)).slice(-2)+':'+('0'+a%60).slice(-2),o=new Option(label,''+m,m===v,m===v);s.add(o);}"
      "i.parentNode.replaceChild(s,i);}"
      "});</script>";
#endif
    return kScript;
  }

  void applyTimezone() {
    if (timezoneApplyCallback_) timezoneApplyCallback_(timezoneOffsetMinutes, timezoneApplyContext_);
  }
#endif

  static const char* portalLabel(const char* zh, const char* en, const char* legacy) {
#if defined(POWERWALL_NATIVE480_LANG_EN)
    (void)zh; (void)legacy;
    return en;
#elif defined(POWERWALL_NATIVE480)
    (void)en; (void)legacy;
    return zh;
#else
    (void)zh; (void)en;
    return legacy;
#endif
  }

  WiFiManager wm;
  Preferences prefs;
  bool params_added_ = false;
  bool web_portal_started_ = false;
  bool ssidReadErrorLogged_ = false;
  bool gateway_config_configured_ = false;
  WiFiManagerParameter* param_gw_ip    = nullptr;
  WiFiManagerParameter* param_gw_pass  = nullptr;
#if !defined(POWERWALL_NATIVE480)
  WiFiManagerParameter* param_gw_email = nullptr;
#endif
  WiFiManagerParameter* param_title    = nullptr;
#if defined(POWERWALL_NATIVE480)
  WiFiManagerParameter* param_timezone = nullptr;
  TimezoneApplyCallback timezoneApplyCallback_ = nullptr;
  void* timezoneApplyContext_ = nullptr;
#endif

  String readSavedSSID() {
    wifi_config_t wifiConfig{};
    const esp_err_t error = esp_wifi_get_config(WIFI_IF_STA, &wifiConfig);
    if (error != ESP_OK) {
      if (!ssidReadErrorLogged_) {
        Serial.printf("[Wi-Fi] Could not read saved STA SSID (esp_wifi_get_config: %d).\n", static_cast<int>(error));
        ssidReadErrorLogged_ = true;
      }
      return String();
    }
    ssidReadErrorLogged_ = false;
    char ssid[sizeof(wifiConfig.sta.ssid) + 1] = {};
    memcpy(ssid, wifiConfig.sta.ssid, sizeof(wifiConfig.sta.ssid));
    return String(ssid);
  }

  void syncParamsFromConfig() {
    if (param_gw_ip)    param_gw_ip->setValue(config.gateway_ip, 64);
    if (param_gw_pass)  param_gw_pass->setValue(config.gateway_password, 64);
#if !defined(POWERWALL_NATIVE480)
    if (param_gw_email) param_gw_email->setValue(config.gateway_email, 64);
#endif
    if (param_title)    param_title->setValue(config.site_title, 32);
#if defined(POWERWALL_NATIVE480)
    if (param_timezone) {
      char timezoneValue[8] = {};
      snprintf(timezoneValue, sizeof(timezoneValue), "%d", timezoneOffsetMinutes);
      param_timezone->setValue(timezoneValue, 5);
    }
#endif
  }

  void loadFromNVS() {
    prefs.begin("pw_nvs", true);
    const bool ipKeyPresent = prefs.isKey("gw_ip");
    const bool passwordKeyPresent = prefs.isKey("gw_pass");
    String ip    = prefs.getString("gw_ip", "");
    String pass  = prefs.getString("gw_pass", "");
#if !defined(POWERWALL_NATIVE480)
    String email = prefs.getString("gw_email", "customer@example.com");
#endif
    String title = prefs.getString("site_title", native480_i18n::kDefaultSiteTitle);
#if defined(POWERWALL_NATIVE480)
    const bool timezoneKeyPresent = prefs.isKey("tz_min");
    int timezone = prefs.getInt("tz_min", native480_timezone::kDefaultOffsetMinutes);
#endif
    prefs.end();

    strlcpy(config.gateway_ip, ip.c_str(), sizeof(config.gateway_ip));
    strlcpy(config.gateway_password, pass.c_str(), sizeof(config.gateway_password));
#if defined(POWERWALL_NATIVE480)
    // Native480 always logs in with the supported customer account; ignore any older NVS key.
    strlcpy(config.gateway_email, "customer@example.com", sizeof(config.gateway_email));
#else
    strlcpy(config.gateway_email, email.c_str(), sizeof(config.gateway_email));
#endif
    strlcpy(config.site_title, title.c_str(), sizeof(config.site_title));
#if defined(POWERWALL_NATIVE480)
    if (!timezoneKeyPresent || !native480_timezone::isValidOffsetMinutes(timezone)) {
      timezone = native480_timezone::kDefaultOffsetMinutes;
      if (prefs.begin("pw_nvs", false)) {
        prefs.putInt("tz_min", timezone);
        prefs.end();
      }
    }
    timezoneOffsetMinutes = timezone;
#endif
    gateway_config_configured_ = hasStoredGatewayConfig(
      ipKeyPresent, passwordKeyPresent, ip.c_str(), pass.c_str());
  }

  void saveToNVS() {
    prefs.begin("pw_nvs", false);
    prefs.putString("gw_ip", config.gateway_ip);
    prefs.putString("gw_pass", config.gateway_password);
#if defined(POWERWALL_NATIVE480)
    prefs.putString("gw_email", "customer@example.com");
#else
    prefs.putString("gw_email", config.gateway_email);
#endif
    prefs.putString("site_title", config.site_title);
#if defined(POWERWALL_NATIVE480)
    prefs.putInt("tz_min", timezoneOffsetMinutes);
#endif
    prefs.end();
    gateway_config_configured_ = hasStoredGatewayConfig(
      true, true, config.gateway_ip, config.gateway_password);
  }

  void pullParamsAndSaveNVS() {
    if (param_gw_ip && strlen(param_gw_ip->getValue()) > 0) {
      strlcpy(config.gateway_ip, param_gw_ip->getValue(), sizeof(config.gateway_ip));
    }
    if (param_gw_pass && strlen(param_gw_pass->getValue()) > 0) {
      strlcpy(config.gateway_password, param_gw_pass->getValue(), sizeof(config.gateway_password));
    }
#if !defined(POWERWALL_NATIVE480)
    if (param_gw_email && strlen(param_gw_email->getValue()) > 0) {
      strlcpy(config.gateway_email, param_gw_email->getValue(), sizeof(config.gateway_email));
    }
#endif
    if (param_title && strlen(param_title->getValue()) > 0) {
      strlcpy(config.site_title, param_title->getValue(), sizeof(config.site_title));
    }
#if defined(POWERWALL_NATIVE480)
    bool timezoneChanged = false;
    if (param_timezone) {
      int requestedTimezone = timezoneOffsetMinutes;
      if (native480_timezone::parseOffsetMinutes(param_timezone->getValue(), requestedTimezone)) {
        timezoneChanged = requestedTimezone != timezoneOffsetMinutes;
        timezoneOffsetMinutes = requestedTimezone;
      } else {
        Serial.printf("[Timezone] Rejected invalid UTC offset '%s'; keeping %d minutes.\n",
                      param_timezone->getValue(), timezoneOffsetMinutes);
        syncParamsFromConfig();
      }
    }
#endif
    saveToNVS();
#if defined(POWERWALL_NATIVE480)
    if (timezoneChanged) applyTimezone();
#endif
    configChanged = true;
  }
};

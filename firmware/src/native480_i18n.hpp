#pragma once

#include <cstddef>
#include <cstring>

namespace native480_i18n {

inline constexpr const char* kManualVersion = "v0.1.1";

#if defined(POWERWALL_NATIVE480_LANG_EN)
inline constexpr bool kEnglish = true;
#else
inline constexpr bool kEnglish = false;
#endif

#if defined(POWERWALL_NATIVE480)
inline constexpr const char* kDefaultSiteTitle = kEnglish ? "My Home" : "我的家";
#else
inline constexpr const char* kDefaultSiteTitle = "我的家";
#endif

struct Phrase {
  const char* zh;
  const char* en;
};

// Long phrases come before their components where a shorter translation could
// otherwise split a label. Chinese is the source language and stays unchanged;
// the English build translates Chinese UI fragments from this table.
inline constexpr Phrase kPhrases[] = {
  {"預估可用時間 / 備援可用時間", "Est. runtime / backup"},
  {"剩餘電量 / 目前容量", "Remaining / capacity"},
  {"本機 Powerwall Gateway IP", "Gateway IP"},
  {"Powerwall Gateway 密碼", "Gateway password"},
  {"Powerwall 登入 Email", "Login email"},
  {"Powerwall 控制面板", "Powerwall Dashboard"},
  {"手機連同一 Wi-Fi 後開啟", "Join this Wi-Fi, open settings"},
  {"輸入 Gateway IP / 密碼", "Enter Gateway IP/password"},
  {"檢查 Gateway IP / 密碼", "Check Gateway IP/password"},
  {"手機同 Wi-Fi 開啟設定", "Open settings on this Wi-Fi"},
  {"等待 Wi-Fi 設定", "Waiting for Wi-Fi"},
  {"正在取得Powerwall資料", "Loading Powerwall data"},
  {"正在取得天氣資料", "Loading weather data"},
  {"Powerwall 連線失敗", "Powerwall offline"},
  {"Powerwall (備援", "Powerwall (Backup"},
  {"住家 / 電網", "HOME / GRID"},
  {"7 日用電與電量總表", "7-Day Summary"},
  {"播放語音測試", "Voice test"},
  {"顯示站點名稱", "Site name"},
  {"Powerwall Gateway IP 位址", "Powerwall Gateway IP"},
  {"Gateway IP 位址", "Gateway IP"},
  {"設定頁網址", "Settings URL"},
  {"資訊", "Info"},
  {"Wi-Fi 網路", "Wi-Fi Network"},
  {"HTTP 網址", "HTTP URL"},
  {"密碼（貼紙後 5 碼）", "Password (last 5)"},
  {"Powerwall Gateway 密碼（貼紙後5碼）", "Gateway password (last 5)"},
  {"重新啟動", "Restart"},
  {"再點", "TAP AGAIN"},
  {"等待時間同步", "Waiting for time sync"},
  {"天氣載入中", "Loading weather"},
  {"天氣載入中...", "Loading weather..."},
  {"載入中", "Loading"},
  {"容量資料不足", "No capacity"},
  {"備援資料不足", "No reserve data"},
  {"電壓/電流不足", "No meter data"},
  {"圖表不可用", "No chart"},
  {"等待樣本", "Waiting for data"},
  {"尚無紀錄", "No data yet"},
  {"已併網", "Grid connected"},
  {"電網已斷開", "Grid disconnected"},
  {"孤島供電", "Islanded"},
  {"時間電價載入中", "Loading time-of-use rates"},
  {"週間費率", "Weekday rates"},
  {"週末費率", "Weekend rates"},
  {"住家用電", "Home"},
  {"電網功率", "Grid"},
  {"電池電量", "Battery"},
  {"電池電壓", "Battery voltage"},
  {"太陽能", "Solar"},
  {"累計購電", "Import"},
  {"累計回送", "Export"},
  {"購電", "Import"}, {"回送", "Export"},
  {"累計用電", "Usage"},
  {"用電功率", "Load"},
  {"輸入功率", "Import"},
  {"回送功率", "Export"},
  {"電流", "Amps"}, {"電壓", "Volts"},
  {"功率因數", "Power factor"},
  {"預覽晴天發電", "Solar preview"},
  {"預覽日相", "Sun preview"},
  {"預覽月相", "Moon preview"},
  {"實況月相", "Current moon"},
  {"天氣預覽", "Weather preview"},
  {"目前：離峰", "Off-peak"},
  {"目前：尖峰", "Peak"},
  {"下一次切換", "Next change"},
  {"下一次尖峰", "Next peak"},
  {"下一次離峰", "Next off-peak"},
  {"下一次%s", "Next %s"},
  {"今日趨勢", "Today"},
  {"即時資料", "Live data"},
  {"模擬資料", "Simulated data"},
  {"資料過期", "Data stale"},
  {"時間同步中", "Syncing time"},
  {"狀態未知", "Unknown status"},
  {"連線重試中", "Reconnecting"},
  {"正在連接 Wi-Fi", "Connecting to Wi-Fi"},
  {"等待配網設定", "Waiting for Wi-Fi setup"},
  {"正在重試", "Retrying"},
  {"等待 IP", "IP pending"},
  {"Wi-Fi: ", "SSID: "},
  {"請設定 Powerwall", "Set up Powerwall"},
  {"Wi-Fi 已連線", "Wi-Fi connected"},
  {"連線中", "Connecting"},
  {"設定", "Settings"},
  {"控制", "Controls"},
  {"網路", "Network"},
  {"裝置設定", "Settings"},
  {"顯示亮度", "Brightness"},
  {"音量", "Volume"},
  {"語音播報", "Voice alerts"},
  {"播放測試音", "Play test tone"},
  {"模擬 ESP32 IP", "Simulator IP"},
  {"模擬 AP SSID", "Setup Wi-Fi"},
  {"預覽：", "Preview: "},
  {"預覽：高峰放電", "Preview: peak discharge"},
  {"預覽：電網充電", "Preview: grid charging"},
  {"預覽：備援底線", "Preview: reserve floor"},
  {"預覽：斷網備援", "Preview: off-grid reserve"},
  {"點擊切換", "tap to cycle"},
  {"地面紅叉叉", "red X on ground"},
  {"備援底線", "Reserve floor"},
  {"斷網備援", "Off-grid reserve"},
  {"高峰時段", "Peak"},
  {"離峰時段", "Off-peak"},
  {"尖峰", "Peak"},
  {"離峰", "Off-peak"},
  {" / 度", " / kWh"},
  {"充電中", "Charge"},
  {"放電中", "Discharge"},
  {"待機中", "Standby"},
  {"備援", "Reserve"},
  {"雲量", "Cloud cover"},
  {"過期", "stale"},
  {"天氣", "Weather"},
  {"晴天", "Clear"},
  {"晴朗", "Clear"},
  {"多雲", "Partly cloudy"},
  {"陰天", "Overcast"},
  {"下雨", "Rain"},
  {"下雪", "Snow"},
  {"夜晚", "Night"},
  {"強雨／閃電", "Storm / lightning"},
  {"下雪／結霜", "Snow / frost"},
  {"強風", "Windy"},
  {"日出", "Sunrise"},
  {"日落", "Sunset"},
  {"夜晚／月亮", "Night / moon"},
  {"自動", "Auto"},
  {"今日", "Today"},
  {"明日", "Tomorrow"},
  {"週末", "Wknd"},
  {"週間", "Wkdy"},
  {"未知", "Unknown"},
  {"台北", "Taipei"},
  {"新月", "New moon"}, {"眉月", "Crescent"},
  {"上弦月", "First quarter"}, {"盈凸月", "Waxing gibbous"},
  {"滿月", "Full moon"}, {"虧凸月", "Waning gibbous"},
  {"下弦月", "Last quarter"}, {"殘月", "Waning crescent"},
  {"貼紙後5碼", "last 5 digits on label"},
  {"POWERWALL", "POWERWALL"},
  {"Powerwall", "Powerwall"},
  {"住家", "Home"},
  {"電網", "Grid"},
  {"尖峰平均", "Peak"},
  {"離峰平均", "Off-peak"},
  {"平均", "Avg."},
  {"顯示", "DISPLAY"}, {"音效", "SOUND"},
  {"即時", "Live"}, {"示範", "DEMO"},
  {"開機時間", "Uptime"},
};

inline bool isAsciiWord(char value) {
  return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
         (value >= '0' && value <= '9') || value == '_';
}

inline const char* text(const char* source) {
  if (!source) return "";
  if (!kEnglish) return source;
  static char translated[512];
  if (source == translated) return source;
  const size_t sourceSize = strlen(source);
  if (sourceSize >= sizeof(translated)) return source;
  size_t in = 0;
  size_t out = 0;
  bool changed = false;
  while (in < sourceSize) {
    const Phrase* best = nullptr;
    size_t bestSize = 0;
    for (const Phrase& phrase : kPhrases) {
      const char* from = kEnglish ? phrase.zh : phrase.en;
      const size_t fromSize = strlen(from);
      if (fromSize <= bestSize || fromSize > sourceSize - in ||
          memcmp(source + in, from, fromSize) != 0) continue;
      const bool wordStart = isAsciiWord(from[0]);
      const bool wordEnd = isAsciiWord(from[fromSize - 1]);
      if ((wordStart && in > 0 && isAsciiWord(source[in - 1])) ||
          (wordEnd && in + fromSize < sourceSize && isAsciiWord(source[in + fromSize]))) continue;
      if (fromSize > bestSize) {
        best = &phrase;
        bestSize = fromSize;
      }
    }
    if (best) {
      const char* to = kEnglish ? best->en : best->zh;
      const size_t toSize = strlen(to);
      if (out + toSize >= sizeof(translated)) return source;
      memcpy(translated + out, to, toSize);
      out += toSize;
      in += bestSize;
      changed = true;
    } else {
      const unsigned char byte = static_cast<unsigned char>(source[in]);
      size_t width = byte < 0x80 ? 1 : (byte & 0xe0) == 0xc0 ? 2 : (byte & 0xf0) == 0xe0 ? 3 : 4;
      if (in + width > sourceSize || out + width >= sizeof(translated)) return source;
      memcpy(translated + out, source + in, width);
      in += width;
      out += width;
    }
  }
  if (!changed) return source;
  translated[out] = '\0';
  return translated;
}

}  // namespace native480_i18n

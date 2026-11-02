/**
 * ESP32-P4 720x720 Powerwall 2 Dashboard — Native C++ LVGL v9.2.2 + SDL2 Simulator
 * ================================================================================
 * - Frontend: LVGL v9.2.2 rendered in a native 720x720 SDL2 desktop window
 *   with exact 3D Godot WebGL stage backdrops, 3D wire UV shader engine,
 *   3D OffGridX ground red X mesh, 3D Powerwall2 SOE LED bar, and 24H timeline.
 *   (Camera locked at cam=[4.614, 1.92, 3.516], target=[-2, 0.302, -1])
 * - Backend:
 *   1. PowerwallLocalClient (libcurl HTTPS -> POST /api/login/Basic password->token,
 *      live polling of /api/system_status/soe, /api/meters/aggregates, etc.)
 *   2. LocalWeatherClient (libcurl HTTPS -> Open-Meteo real-time weather + astronomical
 *      and Godot Night.gd lunar phase calculation)
 *   3. Embedded tzapu/WiFiManager HTTP Server on http://localhost:8788/wifimanager
 */

#include "lvgl.h"
#include "src/display/lv_display_private.h"

extern "C" unsigned lodepng_decode32(unsigned char** out, unsigned* w, unsigned* h, const unsigned char* in, size_t insize);

#include <arpa/inet.h>
#include <curl/curl.h>
#include <fcntl.h>
#include <math.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "firmware/src/usable_eta.hpp"

#ifdef POWERWALL_SIM_320
static const int SCREEN_W = 320;
static const int SCREEN_H = 320;
#else
static const int SCREEN_W = 720;
static const int SCREEN_H = 720;
#endif
static const int STAGE_W  = 704;
static const int STAGE_H  = 440;
static const int CHART_W  = 624;
static const int CHART1_H = 56;
static const int CHART2_H = 66;

// ============================================================================
// Minimal JSON Helper Utilities
// ============================================================================
static std::string read_file_str(const std::string& path) {
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void write_file_str(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (f) f << content;
}

static std::string json_get_string(const std::string& json, const std::string& key, const std::string& def_val = "") {
    std::string pat = "\"" + key + "\"";
    size_t pos = json.find(pat);
    if (pos == std::string::npos) return def_val;
    pos = json.find(':', pos + pat.size());
    if (pos == std::string::npos) return def_val;
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) pos++;
    if (pos >= json.size() || json[pos] != '"') return def_val;
    pos++;
    std::string out;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            pos++;
        }
        out.push_back(json[pos++]);
    }
    return out;
}

static double json_get_number(const std::string& json, const std::string& key, double def_val = 0.0) {
    std::string pat = "\"" + key + "\"";
    size_t pos = json.find(pat);
    if (pos == std::string::npos) return def_val;
    pos = json.find(':', pos + pat.size());
    if (pos == std::string::npos) return def_val;
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) pos++;
    if (pos >= json.size()) return def_val;
    char* endptr = nullptr;
    double v = strtod(json.c_str() + pos, &endptr);
    if (endptr == json.c_str() + pos) return def_val;
    return v;
}

static bool json_get_bool(const std::string& json, const std::string& key, bool def_val = false) {
    std::string pat = "\"" + key + "\"";
    size_t pos = json.find(pat);
    if (pos == std::string::npos) return def_val;
    pos = json.find(':', pos + pat.size());
    if (pos == std::string::npos) return def_val;
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) pos++;
    if (pos + 4 <= json.size() && json.compare(pos, 4, "true") == 0) return true;
    if (pos + 5 <= json.size() && json.compare(pos, 5, "false") == 0) return false;
    return def_val;
}

static std::string json_get_object(const std::string& json, const std::string& key) {
    std::string pat = "\"" + key + "\"";
    size_t pos = json.find(pat);
    if (pos == std::string::npos) return "";
    pos = json.find('{', pos + pat.size());
    if (pos == std::string::npos) return "";
    int depth = 0;
    size_t start = pos;
    for (; pos < json.size(); pos++) {
        if (json[pos] == '{') depth++;
        else if (json[pos] == '}') {
            depth--;
            if (depth == 0) return json.substr(start, pos - start + 1);
        }
    }
    return "";
}

static double get_time_sec() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
}

// ============================================================================
// Global Shared State (NVS Config, Live Telemetry, Live Weather, Preview Modes)
// ============================================================================
struct NvsConfig {
    bool configured = false;
    std::string wifi_ssid = "";
    std::string wifi_password = "";
    std::string gateway_ip = "";
    std::string gateway_password = "";
    std::string gateway_email = "customer@example.com";
    std::string auth_token = "";
    std::string token_updated_at = "";
};

struct HistoryPoint {
    double hour;
    double soe;
    double load_kw;
};

struct TelemetryData {
    bool ok = false;
    std::string site_name = "我的家";
    std::string gateway_ip = "";
    std::string grid_status = "SystemGridConnected";
    std::string update_time_hms = "--:--:--";
    std::string error = "";
    double backup_reserve_pct = 24.0;
    double soe_pct = 100.0;
    double nom_full_kwh = 14.59;
    double nom_rem_kwh = 14.59;
    double solar_w = 0.0;
    double load_w = 2800.0;
    double load_v = 121.6;
    double load_a = 23.3;
    double grid_w = 2800.0;
    double grid_v = 243.2;
    double grid_a = 11.6;
    double pw_w = 0.0;
    std::vector<HistoryPoint> history;
};

struct WeatherData {
    std::string mode = "night";
    std::string label = "夜晚";
    std::string city = "台北";
    double temp_c = 25.4;
    int cloud_cover = 40;
    double rain_mm = 0.0;
    double snow_cm = 0.0;
    double wind_kmh = 8.0;
    bool is_night = true;
    std::string sunrise = "05:52";
    std::string sunset = "17:25";
    std::string moon_label = "☽ 眉月";
    int moon_illum_pct = 39;
    double godot_moon_phase = 0.39;
    bool godot_is_crescent = true;
};

static std::mutex g_mutex;
static NvsConfig g_nvs;
static TelemetryData g_telemetry;
static WeatherData g_weather;

static std::string g_state_mode = "live";    // "live", "solar", "charge", "peak", "low", "offgrid"
static std::string g_weather_mode = "auto";  // "auto", "clear", "partly", "overcast", "rain", "storm", "snow", "wind", "sunrise", "sunset", "night"

static void load_nvs_config() {
    std::string raw = read_file_str("nvs_config.json");
    if (raw.empty()) return;
    g_nvs.configured = json_get_bool(raw, "configured", false);
    g_nvs.wifi_ssid = json_get_string(raw, "wifi_ssid", "");
    g_nvs.wifi_password = json_get_string(raw, "wifi_password", "");
    g_nvs.gateway_ip = json_get_string(raw, "gateway_ip", "");
    g_nvs.gateway_password = json_get_string(raw, "gateway_password", "");
    g_nvs.gateway_email = json_get_string(raw, "gateway_email", "customer@example.com");
    g_nvs.auth_token = json_get_string(raw, "auth_token", "");
    g_nvs.token_updated_at = json_get_string(raw, "token_updated_at", "");
}

static void save_nvs_config() {
    std::ostringstream ss;
    ss << "{\n"
       << "  \"configured\": " << (g_nvs.configured ? "true" : "false") << ",\n"
       << "  \"wifi_ssid\": \"" << g_nvs.wifi_ssid << "\",\n"
       << "  \"wifi_password\": \"" << g_nvs.wifi_password << "\",\n"
       << "  \"gateway_ip\": \"" << g_nvs.gateway_ip << "\",\n"
       << "  \"gateway_password\": \"" << g_nvs.gateway_password << "\",\n"
       << "  \"gateway_email\": \"" << g_nvs.gateway_email << "\",\n"
       << "  \"auth_token\": \"" << g_nvs.auth_token << "\",\n"
       << "  \"token_updated_at\": \"" << g_nvs.token_updated_at << "\"\n"
       << "}\n";
    write_file_str("nvs_config.json", ss.str());
}

// ============================================================================
// libcurl HTTP/HTTPS Helper
// ============================================================================
struct CurlBuffer {
    std::string body;
    std::vector<std::string> cookies;
};

static size_t curl_write_cb(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    CurlBuffer* mem = (CurlBuffer*)userp;
    mem->body.append((const char*)contents, realsize);
    return realsize;
}

static size_t curl_header_cb(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t realsize = size * nitems;
    CurlBuffer* mem = (CurlBuffer*)userdata;
    std::string line(buffer, realsize);
    if (line.size() > 11 && (line.compare(0, 11, "Set-Cookie:") == 0 || line.compare(0, 11, "set-cookie:") == 0)) {
        std::string val = line.substr(11);
        while (!val.empty() && (val[0] == ' ' || val[0] == '\t')) val.erase(0, 1);
        size_t semi = val.find(';');
        if (semi != std::string::npos) val = val.substr(0, semi);
        while (!val.empty() && (val.back() == '\r' || val.back() == '\n')) val.pop_back();
        if (!val.empty()) mem->cookies.push_back(val);
    }
    return realsize;
}

static bool https_request(
    const std::string& url,
    const std::string& method,
    const std::string& post_json,
    const std::string& bearer_token,
    const std::string& cookie_hdr,
    std::string& out_body,
    std::vector<std::string>& out_cookies,
    long* out_http_code = nullptr,
    long timeout_sec = 5
) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    CurlBuffer buf;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "User-Agent: ESP32-P4-Powerwall/1.0");
    if (!post_json.empty()) {
        headers = curl_slist_append(headers, "Content-Type: application/json");
    }
    if (!bearer_token.empty()) {
        std::string auth = "Authorization: Bearer " + bearer_token;
        headers = curl_slist_append(headers, auth.c_str());
    }
    if (!cookie_hdr.empty()) {
        std::string ck = "Cookie: " + cookie_hdr;
        headers = curl_slist_append(headers, ck.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_sec);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_header_cb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &buf);

    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_json.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)post_json.size());
    }

    CURLcode res = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    if (out_http_code) *out_http_code = code;

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) return false;
    out_body = buf.body;
    out_cookies = buf.cookies;
    return (code >= 200 && code < 300);
}

// ============================================================================
// PowerwallLocalClient & LocalWeatherClient (Ported from server.py)
// ============================================================================
static std::string g_cookie_header = "";
static double g_last_login_ts = 0;
static double g_last_history_save_ts = 0;
static std::string g_history_date;

static void init_history_points(std::vector<HistoryPoint>& pts) {
    std::string raw = read_file_str("history_today.json");
    time_t now_t = time(nullptr);
    struct tm tm_now;
    localtime_r(&now_t, &tm_now);
    char today_buf[16];
    strftime(today_buf, sizeof(today_buf), "%Y-%m-%d", &tm_now);
    g_history_date = today_buf;
    pts.clear();

    if (!raw.empty() && json_get_string(raw, "date") == today_buf) {
        size_t pos = raw.find("\"points\"");
        if (pos != std::string::npos) {
            size_t arr_start = raw.find('[', pos);
            size_t arr_end = raw.rfind(']');
            if (arr_start != std::string::npos && arr_end != std::string::npos && arr_end > arr_start) {
                std::string arr = raw.substr(arr_start + 1, arr_end - arr_start - 1);
                size_t cur = 0;
                while ((cur = arr.find('{', cur)) != std::string::npos) {
                    size_t end = arr.find('}', cur);
                    if (end == std::string::npos) break;
                    std::string item = arr.substr(cur, end - cur + 1);
                    HistoryPoint hp;
                    hp.hour = json_get_number(item, "hour", 0.0);
                    hp.soe = json_get_number(item, "soe", 100.0);
                    hp.load_kw = json_get_number(item, "load_kw", 1.5);
                    const double current_hour = tm_now.tm_hour + tm_now.tm_min / 60.0 + tm_now.tm_sec / 3600.0;
                    if (std::isfinite(hp.hour) && std::isfinite(hp.soe) && std::isfinite(hp.load_kw) &&
                        hp.hour >= 0.0 && hp.hour <= current_hour) {
                        if (!pts.empty() && static_cast<int>(pts.back().hour * 60.0) == static_cast<int>(hp.hour * 60.0))
                            pts.back() = hp;
                        else if (pts.size() < 1440) pts.push_back(hp);
                    }
                    cur = end + 1;
                }
            }
        }
    }
}

static bool save_history_points(const std::vector<HistoryPoint>& pts) {
    char tmp_path[64];
    snprintf(tmp_path, sizeof(tmp_path), "history_today.json.%ld.tmp", static_cast<long>(getpid()));
    FILE* file = fopen(tmp_path, "wb");
    if (!file) return false;
    bool written = fprintf(file, "{\"date\":\"%s\",\"points\":[", g_history_date.c_str()) >= 0;
    for (size_t i = 0; i < pts.size() && written; ++i) {
        const HistoryPoint& p = pts[i];
        written = fprintf(file, "%s{\"hour\":%.4f,\"soe\":%.3f,\"load_kw\":%.3f}",
                          i ? "," : "", p.hour, p.soe, p.load_kw) >= 0;
    }
    if (written) written = fprintf(file, "]}") >= 0;
    const bool closed = fclose(file) == 0;
    if (!written || !closed || rename(tmp_path, "history_today.json") != 0) {
        remove(tmp_path);
        return false;
    }
    return true;
}

static bool powerwall_login(const std::string& host, const std::string& password, const std::string& email, std::string& out_token, std::string& out_err) {
    if (host.empty() || password.empty()) {
        out_err = "Missing gateway IP or password";
        return false;
    }
    std::string url = "https://" + host + "/api/login/Basic";
    std::string payload = "{\"username\":\"customer\",\"password\":\"" + password + "\",\"email\":\"" + email + "\",\"force_sm_off\":false}";
    std::string body;
    std::vector<std::string> cookies;
    long code = 0;
    if (!https_request(url, "POST", payload, "", "", body, cookies, &code, 6)) {
        out_err = "HTTP " + std::to_string(code) + " connecting to " + host;
        return false;
    }
    std::string tok = json_get_string(body, "token", "");
    std::string ck_joined;
    for (size_t i = 0; i < cookies.size(); i++) {
        if (i > 0) ck_joined += "; ";
        ck_joined += cookies[i];
        if (tok.empty() && cookies[i].rfind("AuthCookie=", 0) == 0) {
            tok = cookies[i].substr(11);
        }
    }
    if (tok.empty()) {
        out_err = "No token returned by Gateway";
        return false;
    }
    out_token = tok;
    g_cookie_header = ck_joined;
    g_last_login_ts = get_time_sec();
    return true;
}

static void poll_powerwall_once() {
    NvsConfig cfg;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        cfg = g_nvs;
    }
    if (!cfg.configured || cfg.gateway_ip.empty() || cfg.gateway_password.empty()) {
        return;
    }

    double now_ts = get_time_sec();
    std::string token = cfg.auth_token;
    if (token.empty() || (now_ts - g_last_login_ts > 1800)) {
        std::string new_tok, err;
        if (powerwall_login(cfg.gateway_ip, cfg.gateway_password, cfg.gateway_email, new_tok, err)) {
            token = new_tok;
            time_t t = time(nullptr);
            struct tm tm_now;
            localtime_r(&t, &tm_now);
            char buf[32];
            strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm_now);
            std::lock_guard<std::mutex> lk(g_mutex);
            g_nvs.auth_token = token;
            g_nvs.token_updated_at = buf;
            save_nvs_config();
        }
    }

    auto fetch_ep = [&](const std::string& path, std::string& out) -> bool {
        std::string url = "https://" + cfg.gateway_ip + path;
        std::vector<std::string> cks;
        long code = 0;
        if (https_request(url, "GET", "", token, g_cookie_header, out, cks, &code, 4)) return true;
        if (code == 401 || code == 403) {
            std::string new_tok, err;
            if (powerwall_login(cfg.gateway_ip, cfg.gateway_password, cfg.gateway_email, new_tok, err)) {
                token = new_tok;
                std::lock_guard<std::mutex> lk(g_mutex);
                g_nvs.auth_token = token;
                save_nvs_config();
                return https_request(url, "GET", "", token, g_cookie_header, out, cks, &code, 4);
            }
        }
        return false;
    };

    std::string soe_s, agg_s, grid_s, sys_s, site_s, op_s;
    if (!fetch_ep("/api/system_status/soe", soe_s) || !fetch_ep("/api/meters/aggregates", agg_s)) {
        return;
    }
    fetch_ep("/api/system_status/grid_status", grid_s);
    fetch_ep("/api/system_status", sys_s);
    fetch_ep("/api/site_info", site_s);
    fetch_ep("/api/operation", op_s);

    std::string site_obj = json_get_object(agg_s, "site");
    std::string load_obj = json_get_object(agg_s, "load");
    std::string solar_obj = json_get_object(agg_s, "solar");
    std::string batt_obj = json_get_object(agg_s, "battery");

    double raw_soe = json_get_number(soe_s, "percentage", 100.0);
    double nom_full_wh = json_get_number(sys_s, "nominal_full_pack_energy", 14590.0);
    double nom_rem_wh = json_get_number(sys_s, "nominal_energy_remaining", 14590.0);
    double reserve_pct = json_get_number(op_s, "backup_reserve_percent", 24.0);
    std::string site_name = json_get_string(site_s, "site_name", "我的家");
    std::string grid_status = json_get_string(grid_s, "grid_status", "SystemGridConnected");

    double load_w = json_get_number(load_obj, "instant_power", 0.0);
    double load_v = json_get_number(load_obj, "instant_average_voltage", 121.5);
    double load_a = json_get_number(load_obj, "instant_average_current", 0.0);
    double grid_w = json_get_number(site_obj, "instant_power", 0.0);
    double grid_v = json_get_number(site_obj, "instant_average_voltage", 243.0);
    double grid_a = json_get_number(site_obj, "instant_average_current", 0.0);
    double solar_w = json_get_number(solar_obj, "instant_power", 0.0);
    double pw_w = json_get_number(batt_obj, "instant_power", 0.0);

    time_t now_t = time(nullptr);
    struct tm tm_now;
    localtime_r(&now_t, &tm_now);
    char hms_buf[16];
    strftime(hms_buf, sizeof(hms_buf), "%H:%M:%S", &tm_now);
    char today_buf[11];
    strftime(today_buf, sizeof(today_buf), "%Y-%m-%d", &tm_now);
    double hour_float = tm_now.tm_hour + tm_now.tm_min / 60.0 + tm_now.tm_sec / 3600.0;

    std::lock_guard<std::mutex> lk(g_mutex);
    g_telemetry.ok = true;
    g_telemetry.site_name = site_name;
    g_telemetry.gateway_ip = cfg.gateway_ip;
    g_telemetry.grid_status = grid_status;
    g_telemetry.update_time_hms = hms_buf;
    g_telemetry.backup_reserve_pct = round(reserve_pct);
    g_telemetry.soe_pct = raw_soe;
    g_telemetry.nom_full_kwh = nom_full_wh / 1000.0;
    g_telemetry.nom_rem_kwh = nom_rem_wh / 1000.0;
    g_telemetry.load_w = load_w;
    g_telemetry.load_v = load_v;
    g_telemetry.load_a = load_a;
    g_telemetry.grid_w = grid_w;
    g_telemetry.grid_v = grid_v;
    g_telemetry.grid_a = grid_a;
    g_telemetry.solar_w = solar_w;
    g_telemetry.pw_w = pw_w;

    double load_kw = load_w / 1000.0;
    if (g_history_date != today_buf) g_last_history_save_ts = 0;
    init_history_points(g_telemetry.history);
    if (g_telemetry.history.empty() || (now_ts - g_last_history_save_ts >= 60.0)) {
        g_telemetry.history.push_back({hour_float, raw_soe, load_kw});
        g_last_history_save_ts = now_ts;
    } else {
        g_telemetry.history.back() = {hour_float, raw_soe, load_kw};
    }
    save_history_points(g_telemetry.history);
}

static void compute_moon_info(WeatherData& w) {
    double now_utc = (double)time(nullptr);
    double elapsed_days = (now_utc - 947182440.0) / 86400.0;
    double synodic = 29.530588853;
    double astro_phase = fmod(elapsed_days, synodic) / synodic;
    if (astro_phase < 0) astro_phase += 1.0;
    int illum_pct = (int)round((1.0 - cos(astro_phase * 2.0 * M_PI)) * 50.0);

    std::string mlabel;
    if (astro_phase < 0.03 || astro_phase >= 0.97) mlabel = "新月";
    else if (astro_phase < 0.22) mlabel = "眉月";
    else if (astro_phase < 0.28) mlabel = "上弦月";
    else if (astro_phase < 0.47) mlabel = "盈凸月";
    else if (astro_phase < 0.53) mlabel = "滿月";
    else if (astro_phase < 0.72) mlabel = "虧凸月";
    else if (astro_phase < 0.78) mlabel = "下弦月";
    else mlabel = "殘月";

    w.moon_illum_pct = illum_pct;
    w.moon_label = mlabel;
}

static void poll_weather_once() {
    std::string url = "https://api.open-meteo.com/v1/forecast?latitude=25.0530&longitude=121.5259"
                      "&current=temperature_2m,is_day,precipitation,rain,showers,snowfall,weather_code,cloud_cover,wind_speed_10m"
                      "&daily=sunrise,sunset&timezone=auto&forecast_days=1";
    std::string body;
    std::vector<std::string> cks;
    WeatherData w;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        w = g_weather;
    }
    compute_moon_info(w);

    if (https_request(url, "GET", "", "", "", body, cks, nullptr, 4)) {
        std::string cur = json_get_object(body, "current");
        if (!cur.empty()) {
            int wcode = (int)json_get_number(cur, "weather_code", 3);
            int is_day = (int)json_get_number(cur, "is_day", 1);
            int clouds = (int)json_get_number(cur, "cloud_cover", 50);
            double temp_c = json_get_number(cur, "temperature_2m", 25.0);
            double rain_mm = json_get_number(cur, "rain", 0.0) + json_get_number(cur, "showers", 0.0);
            double snow_cm = json_get_number(cur, "snowfall", 0.0);
            double wind_kmh = json_get_number(cur, "wind_speed_10m", 0.0);

            if (wcode == 95 || wcode == 96 || wcode == 99) { w.mode = "storm"; w.label = "強雨／閃電"; }
            else if (wcode == 71 || wcode == 73 || wcode == 75 || wcode == 77 || wcode == 85 || wcode == 86 || snow_cm > 0) { w.mode = "snow"; w.label = "下雪"; }
            else if ((wcode >= 51 && wcode <= 67) || (wcode >= 80 && wcode <= 82) || rain_mm > 0) { w.mode = "rain"; w.label = "下雨"; }
            else if (!is_day) { w.mode = "night"; w.label = "夜晚"; }
            else if (wcode == 3 || wcode == 45 || wcode == 48 || clouds >= 80) { w.mode = "overcast"; w.label = "陰天"; }
            else if (wcode == 1 || wcode == 2 || clouds >= 25) { w.mode = "partly"; w.label = "多雲"; }
            else { w.mode = "clear"; w.label = "晴朗"; }

            w.temp_c = temp_c;
            w.cloud_cover = clouds;
            w.rain_mm = rain_mm;
            w.snow_cm = snow_cm;
            w.wind_kmh = wind_kmh;
            w.is_night = (is_day == 0);
        }
    }
    std::lock_guard<std::mutex> lk(g_mutex);
    g_weather = w;
}

static void* background_poller_thread(void*) {
    poll_weather_once();
    while (true) {
        poll_powerwall_once();
        static int weather_tick = 0;
        if (++weather_tick >= 60) {
            weather_tick = 0;
            poll_weather_once();
        }
        sleep(5);
    }
    return nullptr;
}

// ============================================================================
// Embedded tzapu/WiFiManager HTTP Server (Port 8788)
// ============================================================================
static void send_http_response(int client_fd, int status, const std::string& content_type, const std::string& body) {
    std::ostringstream hdr;
    hdr << "HTTP/1.1 " << status << (status == 200 ? " OK" : " Error") << "\r\n"
        << "Content-Type: " << content_type << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Cache-Control: no-store\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Connection: close\r\n\r\n";
    std::string h = hdr.str();
    send(client_fd, h.data(), h.size(), 0);
    if (!body.empty()) {
        send(client_fd, body.data(), body.size(), 0);
    }
}

static void handle_http_client(int client_fd) {
    char buf[8192];
    ssize_t n = recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
        close(client_fd);
        return;
    }
    buf[n] = '\0';
    std::string req(buf, n);

    std::istringstream iss(req);
    std::string method, full_path;
    iss >> method >> full_path;
    std::string path = full_path;
    size_t qpos = path.find('?');
    if (qpos != std::string::npos) path = path.substr(0, qpos);

    size_t body_pos = req.find("\r\n\r\n");
    std::string req_body = (body_pos != std::string::npos) ? req.substr(body_pos + 4) : "";

    if (method == "GET" && (path == "/" || path == "/wifimanager" || path == "/generate_204" || path == "/hotspot-detect.html")) {
        std::string html = read_file_str("wifimanager.html");
        send_http_response(client_fd, 200, "text/html; charset=utf-8", html);
    } else if (method == "GET" && path == "/api/config") {
        NvsConfig cfg;
        {
            std::lock_guard<std::mutex> lk(g_mutex);
            cfg = g_nvs;
        }
        std::string preview_tok = cfg.auth_token.size() > 20
            ? (cfg.auth_token.substr(0, 12) + "..." + cfg.auth_token.substr(cfg.auth_token.size() - 8))
            : cfg.auth_token;
        std::ostringstream ss;
        ss << "{"
           << "\"configured\":" << (cfg.configured ? "true" : "false") << ","
           << "\"wifi_ssid\":\"" << cfg.wifi_ssid << "\","
           << "\"gateway_ip\":\"" << cfg.gateway_ip << "\","
           << "\"gateway_password_set\":" << (!cfg.gateway_password.empty() ? "true" : "false") << ","
           << "\"gateway_email\":\"" << cfg.gateway_email << "\","
           << "\"has_token\":" << (!cfg.auth_token.empty() ? "true" : "false") << ","
           << "\"auth_token_preview\":\"" << preview_tok << "\","
           << "\"token_updated_at\":\"" << cfg.token_updated_at << "\""
           << "}";
        send_http_response(client_fd, 200, "application/json; charset=utf-8", ss.str());
    } else if (method == "GET" && path == "/api/wifi/scan") {
        std::string scan_json = "{\"networks\":["
            "{\"ssid\":\"Home-WiFi\",\"rssi\":-42,\"secure\":true},"
            "{\"ssid\":\"Home-WiFi\",\"rssi\":-49,\"secure\":true},"
            "{\"ssid\":\"Tesla_Home_WiFi\",\"rssi\":-58,\"secure\":true},"
            "{\"ssid\":\"CHUANG-MESH\",\"rssi\":-65,\"secure\":true}"
            "]}";
        send_http_response(client_fd, 200, "application/json; charset=utf-8", scan_json);
    } else if (method == "POST" && (path == "/wifisave" || path == "/api/config")) {
        std::string ssid = json_get_string(req_body, "wifi_ssid", "");
        std::string wpass = json_get_string(req_body, "wifi_password", "");
        std::string gw_ip = json_get_string(req_body, "gateway_ip", "");
        std::string gw_pass = json_get_string(req_body, "gateway_password", "");
        std::string gw_email = json_get_string(req_body, "gateway_email", "customer@example.com");

        {
            std::lock_guard<std::mutex> lk(g_mutex);
            if (gw_pass.empty()) gw_pass = g_nvs.gateway_password;
            if (wpass.empty()) wpass = g_nvs.wifi_password;
        }

        std::string token, err;
        bool ok = powerwall_login(gw_ip, gw_pass, gw_email, token, err);
        if (!ok) {
            std::string resp = "{\"ok\":false,\"error\":\"Powerwall Login Failed: " + err + "\"}";
            send_http_response(client_fd, 400, "application/json; charset=utf-8", resp);
        } else {
            time_t t = time(nullptr);
            struct tm tm_now;
            localtime_r(&t, &tm_now);
            char buf[32];
            strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm_now);
            {
                std::lock_guard<std::mutex> lk(g_mutex);
                g_nvs.configured = true;
                g_nvs.wifi_ssid = ssid.empty() ? "Home-WiFi" : ssid;
                g_nvs.wifi_password = wpass;
                g_nvs.gateway_ip = gw_ip;
                g_nvs.gateway_password = gw_pass;
                g_nvs.gateway_email = gw_email;
                g_nvs.auth_token = token;
                g_nvs.token_updated_at = buf;
                save_nvs_config();
            }
            poll_powerwall_once();
            std::string preview_tok = token.size() > 20 ? (token.substr(0, 12) + "..." + token.substr(token.size() - 8)) : token;
            std::string resp = "{\"ok\":true,\"token_preview\":\"" + preview_tok + "\",\"gateway_ip\":\"" + gw_ip + "\"}";
            send_http_response(client_fd, 200, "application/json; charset=utf-8", resp);
        }
    } else if (method == "POST" && path == "/api/wifi/reset") {
        {
            std::lock_guard<std::mutex> lk(g_mutex);
            g_nvs.configured = false;
            g_nvs.auth_token = "";
            g_nvs.token_updated_at = "";
            save_nvs_config();
            g_telemetry.ok = false;
        }
        send_http_response(client_fd, 200, "application/json; charset=utf-8", "{\"ok\":true}");
    } else if (method == "POST" && path == "/api/preview") {
        std::string sm = json_get_string(req_body, "state", "");
        std::string wm = json_get_string(req_body, "weather", "");
        {
            std::lock_guard<std::mutex> lk(g_mutex);
            if (!sm.empty()) g_state_mode = sm;
            if (!wm.empty()) g_weather_mode = wm;
        }
        send_http_response(client_fd, 200, "application/json; charset=utf-8",
            "{\"ok\":true,\"state\":\"" + g_state_mode + "\",\"weather\":\"" + g_weather_mode + "\"}");
    } else {
        send_http_response(client_fd, 404, "text/plain", "Not Found");
    }
    close(client_fd);
}

static void* http_server_thread(void*) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) return nullptr;
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8788);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(server_fd);
        return nullptr;
    }
    listen(server_fd, 16);
    printf("📶 tzapu/WiFiManager Portal listening at http://localhost:8788/wifimanager\n");

    while (true) {
        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);
        int cfd = accept(server_fd, (struct sockaddr*)&caddr, &clen);
        if (cfd >= 0) {
            handle_http_client(cfd);
        }
    }
    return nullptr;
}

// ============================================================================
// 3D Stage Asset & Shader Engine (Exact Port of index.html 3D Shaders)
// ============================================================================
#pragma pack(push, 1)
struct OffGridXPixel {
    uint16_t x, y;
    uint8_t r, g, b;
};

struct SoePixel {
    uint16_t x, y;
    uint8_t dr, dg, db;
    float frac;
};

struct WirePixel {
    uint16_t x, y;
    uint8_t wid;
    float u_val;
    int16_t dr, dg, db;
};
#pragma pack(pop)

static std::vector<OffGridXPixel> g_offgridx_pixels;
static std::vector<SoePixel> g_soe_pixels;
static std::vector<WirePixel> g_wire_pixels;
static std::map<std::string, std::vector<uint8_t>> g_stage_images;

static void load_stage_png(const std::string& key, const std::string& base_path) {
    std::string raw_file = read_file_str(base_path + ".bgra");
    if (raw_file.size() == STAGE_W * STAGE_H * 4) {
        g_stage_images[key] = std::vector<uint8_t>((const uint8_t*)raw_file.data(), (const uint8_t*)raw_file.data() + raw_file.size());
        return;
    }
    std::string png_file = read_file_str(base_path + ".png");
    if (!png_file.empty()) {
        lv_draw_buf_t* decoded = nullptr;
        unsigned w = 0, h = 0;
        if (lodepng_decode32((unsigned char**)&decoded, &w, &h, (const unsigned char*)png_file.data(), png_file.size()) == 0 && decoded && decoded->data) {
            if ((int)w == STAGE_W && (int)h == STAGE_H) {
                const uint8_t* rgba = decoded->data;
                std::vector<uint8_t> bgra(STAGE_W * STAGE_H * 4);
                for (int i = 0; i < STAGE_W * STAGE_H; i++) {
                    bgra[i * 4 + 0] = rgba[i * 4 + 2];
                    bgra[i * 4 + 1] = rgba[i * 4 + 1];
                    bgra[i * 4 + 2] = rgba[i * 4 + 0];
                    bgra[i * 4 + 3] = 255;
                }
                g_stage_images[key] = std::move(bgra);
            }
            lv_draw_buf_destroy(decoded);
        }
    }
}

static void load_3d_binary_assets() {
    std::vector<std::string> presets = {"clear", "partly", "overcast", "rain", "storm", "snow", "wind", "sunrise", "sunset", "night"};
    for (const auto& p : presets) {
        load_stage_png(p, "assets/stage_" + p);
        if (p == "rain" || p == "storm" || p == "snow" || p == "wind") {
            load_stage_png(p + "_1", "assets/stage_" + p + "_1");
            load_stage_png(p + "_2", "assets/stage_" + p + "_2");
        }
    }

    auto read_bin = [](const char* path, auto& vec) {
        FILE* f = fopen(path, "rb");
        if (!f) return;
        uint32_t count = 0;
        if (fread(&count, sizeof(uint32_t), 1, f) == 1 && count < 100000) {
            vec.resize(count);
            fread(vec.data(), sizeof(vec[0]), count, f);
        }
        fclose(f);
    };
    read_bin("assets/offgridx_3d.bin", g_offgridx_pixels);
    read_bin("assets/soe_3d.bin", g_soe_pixels);
    read_bin("assets/wires_3d.bin", g_wire_pixels);
    printf("📦 Loaded %zu 3D weather stages, %zu wire pixels, %zu OffGridX pixels, %zu SOE pixels\n",
           g_stage_images.size(), g_wire_pixels.size(), g_offgridx_pixels.size(), g_soe_pixels.size());
}

static const float WIRE_MAX_UV[4] = {10.109375f, 3.248046875f, 3.984375f, 2.2f};
static const float WIRE_LEN[4]    = {1.8287096f, 0.6522216f,   0.8169308f, 0.4026279f};

struct WireBranch {
    bool enabled = false;
    float scale = 1.0f;
    float offset = 0.0f;
};

struct ActiveRoute {
    WireBranch branches[4]; // 0=grid, 1=load, 2=battery, 3=solar
    float total = 1.0f;
    float tail = 1.0f;
    float gap = 0.2f;
    float speed = 1.0f;
    double route_start = 0.0;
    std::string signature = "";
};

static ActiveRoute g_route;

static void update_3d_route(double solarW, double pwW, double gridW, double loadW, bool isOffGrid) {
    std::vector<int> sources;
    std::vector<int> sinks;

    if (solarW > 60.0) sources.push_back(3);
    if (pwW > 60.0) sources.push_back(2);
    else if (pwW < -60.0) sinks.push_back(2);

    if (!isOffGrid) {
        if (gridW > 60.0) sources.push_back(0);
        else if (gridW < -60.0) sinks.push_back(0);
    }
    if (loadW > 60.0) sinks.push_back(1);

    std::ostringstream sig;
    sig << (isOffGrid ? "1|" : "0|");
    for (int s : sources) sig << s << ",";
    sig << "->";
    for (int k : sinks) sig << k << ",";

    bool changed = (sig.str() != g_route.signature);
    g_route.signature = sig.str();
    if (changed) {
        g_route.route_start = get_time_sec();
    }

    for (int i = 0; i < 4; i++) g_route.branches[i].enabled = false;
    if (sources.empty() || sinks.empty()) return;

    auto wscale = [](int id) { return WIRE_LEN[id] / WIRE_MAX_UV[id]; };
    float refScale = wscale(2);

    float maxSinkLen = 0.0f;
    for (int k : sinks) maxSinkLen = std::max(maxSinkLen, WIRE_LEN[k]);

    if (sources.size() == 1) {
        int src = sources[0];
        float srcLen = WIRE_LEN[src];
        float srcScale = wscale(src);
        g_route.branches[src] = {true, -srcScale, WIRE_MAX_UV[src] * srcScale};
        for (int sink : sinks) {
            float sinkScale = wscale(sink);
            g_route.branches[sink] = {true, sinkScale, srcLen};
        }
        g_route.total = srcLen + maxSinkLen;
    } else {
        float srcLen = 0.0f;
        for (int s : sources) srcLen = std::max(srcLen, WIRE_LEN[s]);
        for (int s : sources) {
            float srcScale = wscale(s);
            g_route.branches[s] = {true, -srcScale, srcLen};
        }
        for (int sink : sinks) {
            float sinkScale = wscale(sink);
            g_route.branches[sink] = {true, sinkScale, srcLen};
        }
        g_route.total = srcLen + maxSinkLen;
    }
    g_route.tail  = 9.0f * 0.6f * refScale;
    g_route.gap   = 1.0f * refScale;
    g_route.speed = (9.0f / 2.0f) * refScale;
}

// ============================================================================
// LVGL Widget Pointers & Canvas Buffers
// ============================================================================
static uint8_t g_stage_buf[STAGE_W * STAGE_H * 4];
static uint8_t g_chart1_buf[CHART_W * CHART1_H * 4];
static uint8_t g_chart2_buf[CHART_W * CHART2_H * 4];

static lv_obj_t* g_stage_canvas = nullptr;
static lv_obj_t* g_chart1_canvas = nullptr;
static lv_obj_t* g_chart2_canvas = nullptr;

static lv_obj_t* g_lbl_site_title = nullptr;
static lv_obj_t* g_pill_status = nullptr;
static lv_obj_t* g_dot_status = nullptr;
static lv_obj_t* g_lbl_status = nullptr;
static lv_obj_t* g_pill_weather = nullptr;
static lv_obj_t* g_lbl_weather = nullptr;

static const uint8_t kChargeArrowUpMap[14 * 12] = {
    0x00, 0x00, 0x00, 0x02, 0x03, 0xa4, 0xe6, 0x2b, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x03, 0x00, 0x4d, 0xfe, 0xff, 0xb4, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x02, 0x08, 0xd1, 0xff, 0xfb, 0xff, 0x4d, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x00, 0x03, 0x00, 0x6b, 0xff, 0xfc, 0xfc, 0xff, 0xce, 0x09, 0x02, 0x01, 0x00, 0x00,
    0x01, 0x01, 0x15, 0xe3, 0xfe, 0xfd, 0xff, 0xfc, 0xff, 0x69, 0x00, 0x03, 0x00, 0x00,
    0x03, 0x00, 0x8a, 0xff, 0xfd, 0xff, 0xff, 0xfe, 0xfe, 0xe3, 0x14, 0x01, 0x02, 0x00,
    0x01, 0x27, 0xf0, 0xfd, 0xfe, 0xff, 0xff, 0xff, 0xfc, 0xff, 0x87, 0x00, 0x03, 0x00,
    0x00, 0xa2, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xfe, 0xf3, 0x24, 0x00, 0x02,
    0x40, 0xf9, 0xfd, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xa5, 0x00, 0x02,
    0xc9, 0xfc, 0xf9, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfa, 0xf9, 0xfa, 0x3b, 0x00,
    0xe5, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x51, 0x00,
    0x31, 0x69, 0x69, 0x69, 0x69, 0x69, 0x69, 0x69, 0x69, 0x68, 0x6a, 0x56, 0x05, 0x00,
};
static uint8_t g_pw_arrow_buf[14 * 12 * 4];

static lv_obj_t* g_lbl_solar_val = nullptr;
static lv_obj_t* g_lbl_home_val = nullptr;
static lv_obj_t* g_pw_val_row = nullptr;
static lv_obj_t* g_lbl_pw_kw = nullptr;
static lv_obj_t* g_lbl_pw_dot = nullptr;
static lv_obj_t* g_pw_arrow_canvas = nullptr;
static lv_obj_t* g_lbl_pw_soc = nullptr;
static lv_obj_t* g_lbl_grid_title = nullptr;
static lv_obj_t* g_lbl_grid_val = nullptr;

static lv_obj_t* g_lbl_stat_home_kw = nullptr;
static lv_obj_t* g_lbl_stat_home_va = nullptr;
static lv_obj_t* g_lbl_stat_reserve = nullptr;
static lv_obj_t* g_lbl_stat_pw_soc = nullptr;
static lv_obj_t* g_lbl_stat_pw_kwh = nullptr;
static lv_obj_t* g_lbl_stat_pw_eta = nullptr;
static lv_obj_t* g_pill_tou = nullptr;
static lv_obj_t* g_lbl_tou_pill = nullptr;
static lv_obj_t* g_lbl_tou_track = nullptr;
static lv_obj_t* g_tou_bar_we = nullptr;
static lv_obj_t* g_tou_bar_wk = nullptr;
static lv_obj_t* g_cursor_cont = nullptr;
static lv_obj_t* g_lbl_cursor_badge = nullptr;

static double g_active_solar_w = 0.0;
static double g_active_pw_w = 0.0;
static double g_active_grid_w = 2800.0;
static double g_active_load_w = 2800.0;
static double g_active_soe_pct = 100.0;
static bool   g_active_offgrid = false;
static std::string g_active_stage_key = "night";

// ============================================================================
// 2D Area Chart Software Rasterizer for LVGL Canvas (Track 1 & Track 2)
// ============================================================================
static void render_area_chart_to_buf(
    uint8_t* buf, int w, int h,
    const std::vector<HistoryPoint>& pts,
    bool is_soe,
    double max_y,
    uint8_t r_col, uint8_t g_col, uint8_t b_col,
    double reserve_pct
) {
    for (int i = 0; i < w * h; i++) {
        buf[i * 4 + 0] = 15;
        buf[i * 4 + 1] = 10;
        buf[i * 4 + 2] = 9;
        buf[i * 4 + 3] = 255;
    }

    auto blend_px = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b, float a) {
        if (x < 0 || x >= w || y < 0 || y >= h) return;
        int idx = (y * w + x) * 4;
        buf[idx + 0] = (uint8_t)(buf[idx + 0] * (1.0f - a) + b * a);
        buf[idx + 1] = (uint8_t)(buf[idx + 1] * (1.0f - a) + g * a);
        buf[idx + 2] = (uint8_t)(buf[idx + 2] * (1.0f - a) + r * a);
    };

    int mid_y = h / 2;
    for (int x = 0; x < w; x++) {
        blend_px(x, mid_y, 255, 255, 255, 0.08f);
        blend_px(x, h - 1, 255, 255, 255, 0.08f);
    }

    if (is_soe && reserve_pct > 0) {
        int ry = (int)round((1.0 - reserve_pct / 100.0) * (h - 4)) + 2;
        for (int x = 0; x < w; x++) {
            if ((x / 5) % 2 == 0) {
                blend_px(x, ry, 56, 189, 248, 0.55f);
            }
        }
    }

    if (pts.empty()) return;

    int last_px = 0, last_py = h - 2;
    for (size_t i = 0; i < pts.size(); i++) {
        double v1 = is_soe ? pts[i].soe : pts[i].load_kw;
        int x1 = std::min(w - 1, std::max(0, (int)round((pts[i].hour / 24.0) * (w - 1))));
        int y1 = std::min(h - 2, std::max(3, (int)round((1.0 - std::min(1.0, std::max(0.0, v1 / max_y))) * (h - 5)) + 3));
        last_px = x1;
        last_py = y1;

        if (i + 1 < pts.size()) {
            double v2 = is_soe ? pts[i + 1].soe : pts[i + 1].load_kw;
            int x2 = std::min(w - 1, std::max(0, (int)round((pts[i + 1].hour / 24.0) * (w - 1))));
            int y2 = std::min(h - 2, std::max(3, (int)round((1.0 - std::min(1.0, std::max(0.0, v2 / max_y))) * (h - 5)) + 3));
            if (x2 < x1) continue;
            for (int x = x1; x <= x2; x++) {
                float t = (x2 == x1) ? 0.0f : (float)(x - x1) / (float)(x2 - x1);
                int y_top = (int)round(y1 + t * (y2 - y1));
                for (int y = y_top; y < h; y++) {
                    float vfrac = 1.0f - (float)y / (float)h;
                    float alpha = is_soe ? (0.08f + 0.37f * vfrac) : (0.42f * vfrac);
                    blend_px(x, y, r_col, g_col, b_col, alpha);
                }
                blend_px(x, y_top - 1, r_col, g_col, b_col, 0.85f);
                blend_px(x, y_top,     r_col, g_col, b_col, 1.0f);
                blend_px(x, y_top + 1, r_col, g_col, b_col, 0.85f);
            }
        }
    }

    for (int dy = -5; dy <= 5; dy++) {
        for (int dx = -5; dx <= 5; dx++) {
            float d = sqrtf((float)(dx * dx + dy * dy));
            if (d <= 2.8f) {
                blend_px(last_px + dx, last_py + dy, 255, 255, 255, 1.0f);
            } else if (d <= 4.6f) {
                blend_px(last_px + dx, last_py + dy, r_col, g_col, b_col, 1.0f);
            }
        }
    }
}

// ============================================================================
// 30 FPS 3D Stage Shader Compositor (Weather + SOE LED + OffGridX + 3D Wires)
// ============================================================================
static void render_3d_stage_frame() {
    double now_sec = get_time_sec();

    std::string img_key = g_active_stage_key;
    if (img_key == "rain" || img_key == "storm" || img_key == "snow" || img_key == "wind") {
        int frame_idx = ((int)(now_sec * 5.0)) % 3;
        if (frame_idx == 1 && g_stage_images.count(img_key + "_1")) img_key += "_1";
        else if (frame_idx == 2 && g_stage_images.count(img_key + "_2")) img_key += "_2";
    }
    if (!g_stage_images.count(img_key)) img_key = "night";
    if (!g_stage_images.count(img_key)) return;
    const auto& src_bgra = g_stage_images[img_key];
    if (src_bgra.size() != STAGE_W * STAGE_H * 4) return;

    memcpy(g_stage_buf, src_bgra.data(), STAGE_W * STAGE_H * 4);

    // 1. Powerwall 2 Green LED Bar (makeSOEMaterial shader from index.html lines 504-548)
    float clamped_soe = std::max(0.0f, std::min(1.0f, (float)(g_active_soe_pct / 100.0)));
    float pulse_strength = (fabs(g_active_pw_w) > 60.0) ? 1.0f : 0.25f;
    float raw_pulse = powf(sinf((float)(now_sec / 2.0 + 0.65) * 6.2831853f) * 0.5f + 0.5f, 1.5f);
    float pulse = (1.0f - pulse_strength) + pulse_strength * raw_pulse;

    for (const auto& sp : g_soe_pixels) {
        if (sp.frac <= clamped_soe) {
            int idx = (sp.y * STAGE_W + sp.x) * 4;
            float boost = 0.65f + 0.35f * pulse;
            g_stage_buf[idx + 0] = (uint8_t)std::min(255, (int)g_stage_buf[idx + 0] + (int)(sp.db * boost));
            g_stage_buf[idx + 1] = (uint8_t)std::min(255, (int)g_stage_buf[idx + 1] + (int)(sp.dg * boost));
            g_stage_buf[idx + 2] = (uint8_t)std::min(255, (int)g_stage_buf[idx + 2] + (int)(sp.dr * boost));
        }
    }

    // 2. Off-Grid Red X on the Ground (offGridXNode in ResidentialClassic.glb)
    if (g_active_offgrid) {
        for (const auto& xp : g_offgridx_pixels) {
            int idx = (xp.y * STAGE_W + xp.x) * 4;
            g_stage_buf[idx + 0] = xp.b;
            g_stage_buf[idx + 1] = xp.g;
            g_stage_buf[idx + 2] = xp.r;
        }
    }

    // 3. 3D Powerflow Wires (makeFlowMaterial shader + cumulative route from index.html lines 458-498 & 770-869)
    float elapsed = (float)std::max(0.0, now_sec - g_route.route_start);
    float cycle = g_route.total + g_route.tail + g_route.gap;
    float head = (cycle > 0.001f) ? fmodf(elapsed * g_route.speed, cycle) : 0.0f;

    for (const auto& wp : g_wire_pixels) {
        if (wp.wid >= 4) continue;
        const auto& br = g_route.branches[wp.wid];
        if (!br.enabled) continue;
        float dist = wp.u_val * br.scale + br.offset;
        float age = head - dist;
        if (age >= 0.0f && age <= g_route.tail) {
            float t = 1.0f - age / g_route.tail;
            float wave = t * t;
            float s = wave * wave * (3.0f - 2.0f * wave);
            int idx = (wp.y * STAGE_W + wp.x) * 4;
            int nb = (int)g_stage_buf[idx + 0] + (int)roundf(wp.db * s);
            int ng = (int)g_stage_buf[idx + 1] + (int)roundf(wp.dg * s);
            int nr = (int)g_stage_buf[idx + 2] + (int)roundf(wp.dr * s);
            g_stage_buf[idx + 0] = (uint8_t)std::max(0, std::min(255, nb));
            g_stage_buf[idx + 1] = (uint8_t)std::max(0, std::min(255, ng));
            g_stage_buf[idx + 2] = (uint8_t)std::max(0, std::min(255, nr));
        }
    }

    if (g_stage_canvas) {
        lv_obj_invalidate(g_stage_canvas);
    }
}

// ============================================================================
// Synchronize UI Labels, Charts, TOU Bar, and 3D State
// ============================================================================
static void update_dashboard_ui() {
    NvsConfig cfg;
    TelemetryData tel;
    WeatherData w;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        cfg = g_nvs;
        tel = g_telemetry;
        w = g_weather;
    }

    // 1. Weather Pill & 3D Backdrop Selection
    std::string moon_str = w.moon_label + " (" + std::to_string(w.moon_illum_pct) + "%)";
    char w_buf[256];
    if (g_weather_mode == "auto") {
        if (g_state_mode == "solar") {
            g_active_stage_key = "clear";
            snprintf(w_buf, sizeof(w_buf), "預覽晴天發電 ｜ %s %.1f°C ｜ 月相 %s", w.city.c_str(), w.temp_c, moon_str.c_str());
        } else {
            g_active_stage_key = w.mode;
            std::string sun_time = w.is_night ? ("日出 " + w.sunrise) : ("日落 " + w.sunset);
            snprintf(w_buf, sizeof(w_buf), "%s：%s %.1f°C ｜ %s ｜ %s",
                     w.city.c_str(), w.label.c_str(), w.temp_c, moon_str.c_str(), sun_time.c_str());
        }
    } else {
        g_active_stage_key = g_weather_mode;
        static const std::map<std::string, std::string> preset_names = {
            {"clear", "晴天"}, {"partly", "多雲"}, {"overcast", "陰天"},
            {"rain", "下雨"}, {"storm", "強雨／閃電"}, {"snow", "下雪／結霜"},
            {"wind", "強風"}, {"sunrise", "日出"}, {"sunset", "日落"}, {"night", "夜晚／月亮"}
        };
        std::string pname = preset_names.count(g_weather_mode) ? preset_names.at(g_weather_mode) : g_weather_mode;
        snprintf(w_buf, sizeof(w_buf), "天氣預覽：%s ｜ 實況月相 %s", pname.c_str(), moon_str.c_str());
    }
    lv_label_set_text(g_lbl_weather, w_buf);

    // 2. Powerflow Mode & Telemetry Values
    double solarW = 0, pwW = 0, gridW = 0, loadW = 0, soePct = 100, reservePct = tel.backup_reserve_pct;
    bool isOffGrid = false;

    time_t now_t = time(nullptr);
    struct tm tm_now;
    localtime_r(&now_t, &tm_now);
    int month = tm_now.tm_mon + 1;
    bool isSummer = (month >= 6 && month <= 9);
    bool isWeekend = (tm_now.tm_wday == 0 || tm_now.tm_wday == 6);
    double cursorPct = ((tm_now.tm_hour + tm_now.tm_min / 60.0) / 24.0) * 100.0;
    char tou_txt[128];
    bool tou_is_peak = false;

    if (g_state_mode == "live" || g_state_mode == "live_ip") {
        if (!cfg.configured) {
            lv_label_set_text(g_lbl_status, "等待配網設定 (請於 WiFiManager 設定)");
            lv_obj_set_style_bg_color(g_dot_status, lv_color_hex(0xf59e0b), 0);
            lv_obj_set_style_text_color(g_lbl_status, lv_color_hex(0xfcd34d), 0);
            solarW = 0; pwW = 0; gridW = 0; loadW = 0; soePct = 0; isOffGrid = false;
            lv_label_set_text(g_lbl_solar_val, "-- kW");
            lv_label_set_text(g_lbl_home_val, "-- kW");
            lv_label_set_text(g_lbl_pw_kw, "-- kW");
            lv_obj_remove_flag(g_lbl_pw_dot, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(g_pw_arrow_canvas, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(g_lbl_pw_soc, "--%");
            lv_label_set_text(g_lbl_grid_title, "電網");
            lv_label_set_text(g_lbl_grid_val, "-- kW");
            lv_label_set_text(g_lbl_stat_home_kw, "-- kW");
            lv_label_set_text(g_lbl_stat_home_va, "-- V · -- A");
            lv_label_set_text(g_lbl_stat_pw_soc, "--%");
            lv_label_set_text(g_lbl_stat_pw_kwh, "-- / -- kWh");
            lv_label_set_text(g_lbl_stat_pw_eta, "· --");
        } else {
            char st_buf[96];
            if (g_state_mode == "live_ip") {
                snprintf(st_buf, sizeof(st_buf), "即時更新 (%s) %s", tel.update_time_hms.c_str(), cfg.gateway_ip.c_str());
            } else {
                snprintf(st_buf, sizeof(st_buf), "即時更新 (%s)", tel.update_time_hms.c_str());
            }
            lv_label_set_text(g_lbl_status, st_buf);
            lv_obj_set_style_bg_color(g_dot_status, lv_color_hex(0x34d399), 0);
            lv_obj_set_style_text_color(g_lbl_status, lv_color_hex(0x34d399), 0);

            solarW = tel.solar_w;
            pwW    = tel.pw_w;
            gridW  = tel.grid_w;
            loadW  = tel.load_w;
            soePct = round(tel.soe_pct);
            isOffGrid = (tel.grid_status != "SystemGridConnected");
        }

        double hourFloat = tm_now.tm_hour + tm_now.tm_min / 60.0;
        bool isWeekdayPeak = isSummer
            ? (hourFloat >= 9.0 && hourFloat < 24.0)
            : ((hourFloat >= 6.0 && hourFloat < 11.0) || (hourFloat >= 14.0 && hourFloat < 24.0));

        tou_is_peak = !isWeekend && isWeekdayPeak;
    } else if (g_state_mode == "solar") {
        lv_label_set_text(g_lbl_status, "預覽：太陽能充電 (點擊切換)");
        solarW = 5000; pwW = -3200; gridW = 0; loadW = 1800; soePct = 78; isOffGrid = false;
        isWeekend = false; cursorPct = 52.0;
    } else if (g_state_mode == "charge") {
        lv_label_set_text(g_lbl_status, "預覽：電網充電 住家+PW (點擊切換)");
        solarW = 0; pwW = -5000; gridW = 7200; loadW = 2200; soePct = 68; isOffGrid = false;
        isWeekend = false; cursorPct = 10.4;
    } else if (g_state_mode == "peak") {
        lv_label_set_text(g_lbl_status, "預覽：高峰放電 (點擊切換)");
        solarW = 0; pwW = 3200; gridW = 0; loadW = 3200; soePct = 55; isOffGrid = false;
        isWeekend = false; cursorPct = 75.0; tou_is_peak = true;
    } else if (g_state_mode == "low") {
        lv_label_set_text(g_lbl_status, "預覽：備援底線 24% (點擊切換)");
        solarW = 0; pwW = 0; gridW = 1800; loadW = 1800; soePct = 24; isOffGrid = false;
        isWeekend = false; cursorPct = 92.0; tou_is_peak = true;
    } else if (g_state_mode == "offgrid") {
        lv_label_set_text(g_lbl_status, "預覽：斷網備援 地面紅叉叉 (點擊切換)");
        solarW = 0; pwW = 2500; gridW = 0; loadW = 2500; soePct = 82; isOffGrid = true;
        isWeekend = false; cursorPct = 83.0; tou_is_peak = true;
    }

    const bool pillIsSummer = (g_state_mode == "live" || g_state_mode == "live_ip") && isSummer;
    const char* pillPrice = tou_is_peak
        ? (pillIsSummer ? "NT$5.16" : "NT$4.93")
        : (pillIsSummer ? "NT$2.06" : "NT$1.99");
    snprintf(tou_txt, sizeof(tou_txt), "%s %s / kWh",
             tou_is_peak ? "高峰時段" : "離峰時段", pillPrice);

    if ((g_state_mode != "live" && g_state_mode != "live_ip") || cfg.configured) {
        char buf[64];
        if (solarW > 60) {
            snprintf(buf, sizeof(buf), "%.1f kW", solarW / 1000.0);
            lv_obj_set_style_text_color(g_lbl_solar_val, lv_color_hex(0xffffff), 0);
        } else {
            snprintf(buf, sizeof(buf), "0 kW");
            lv_obj_set_style_text_color(g_lbl_solar_val, lv_color_hex(0xa1a1aa), 0);
        }
        lv_label_set_text(g_lbl_solar_val, buf);

        snprintf(buf, sizeof(buf), "%.1f kW", loadW / 1000.0);
        lv_label_set_text(g_lbl_home_val, buf);
        lv_label_set_text(g_lbl_stat_home_kw, buf);

        snprintf(buf, sizeof(buf), "%.1f V·%.1f A", tel.load_v, tel.load_a);
        lv_label_set_text(g_lbl_stat_home_va, buf);

        if (fabs(pwW) > 60) {
            snprintf(buf, sizeof(buf), "%.1f kW", fabs(pwW) / 1000.0);
        } else {
            snprintf(buf, sizeof(buf), "0 kW");
        }
        lv_label_set_text(g_lbl_pw_kw, buf);

        snprintf(buf, sizeof(buf), "%d%%", (int)round(soePct));
        lv_label_set_text(g_lbl_pw_soc, buf);

        if (pwW < -60 || pwW > 60) {
            bool flip_y = (pwW > 60); // 放電 (Discharging): 綠色箭頭向下; 充電 (Charging): 綠色箭頭向上
            for (int y = 0; y < 12; y++) {
                int sy = flip_y ? (11 - y) : y;
                for (int x = 0; x < 14; x++) {
                    int idx = (y * 14 + x) * 4;
                    uint8_t a = kChargeArrowUpMap[sy * 14 + x];
                    g_pw_arrow_buf[idx + 0] = 0x8E;
                    g_pw_arrow_buf[idx + 1] = 0xE5;
                    g_pw_arrow_buf[idx + 2] = 0x11;
                    g_pw_arrow_buf[idx + 3] = a;
                }
            }
            lv_obj_add_flag(g_lbl_pw_dot, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(g_pw_arrow_canvas, LV_OBJ_FLAG_HIDDEN);
            lv_obj_invalidate(g_pw_arrow_canvas);
        } else {
            lv_obj_remove_flag(g_lbl_pw_dot, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(g_pw_arrow_canvas, LV_OBJ_FLAG_HIDDEN);
        }

        snprintf(buf, sizeof(buf), "%d%%", (int)round(soePct));
        lv_label_set_text(g_lbl_stat_pw_soc, buf);

        const bool preview = g_state_mode != "live" && g_state_mode != "live_ip";
        const double fullKwh = tel.nom_full_kwh != 0.0 ? tel.nom_full_kwh : 14.6;
        const double remKwh = preview ? round(fullKwh * soePct) / 100.0 : tel.nom_rem_kwh;
        snprintf(buf, sizeof(buf), "%.2f / %.2f kWh", remKwh, fullKwh);
        lv_label_set_text(g_lbl_stat_pw_kwh, buf);
        formatUsableEta(buf, sizeof(buf), remKwh, fullKwh, reservePct, loadW / 1000.0);
        lv_label_set_text(g_lbl_stat_pw_eta, buf);

        snprintf(buf, sizeof(buf), "Powerwall (備援 %d%%)", (int)round(reservePct));
        lv_label_set_text(g_lbl_stat_reserve, buf);

        if (isOffGrid) {
            lv_label_set_text(g_lbl_grid_title, "電網斷網");
            lv_obj_set_style_text_color(g_lbl_grid_title, lv_color_hex(0xfb7185), 0);
            lv_label_set_text(g_lbl_grid_val, "孤島供電");
            lv_obj_set_style_text_color(g_lbl_grid_val, lv_color_hex(0xfb7185), 0);
        } else {
            lv_label_set_text(g_lbl_grid_title, "電網");
            lv_obj_set_style_text_color(g_lbl_grid_title, lv_color_hex(0xa1a1aa), 0);
            if (fabs(gridW) > 60) snprintf(buf, sizeof(buf), "%.1f kW", gridW / 1000.0);
            else snprintf(buf, sizeof(buf), "0 kW");
            lv_label_set_text(g_lbl_grid_val, buf);
            lv_obj_set_style_text_color(g_lbl_grid_val, lv_color_hex(0xffffff), 0);
        }
    }

    lv_obj_update_layout(lv_obj_get_parent(g_lbl_stat_pw_soc));
    lv_obj_set_x(g_lbl_stat_pw_kwh, lv_obj_get_x(g_lbl_stat_pw_soc) + lv_obj_get_width(g_lbl_stat_pw_soc) + 7);
    lv_obj_set_x(g_lbl_stat_pw_eta, lv_obj_get_x(g_lbl_stat_pw_kwh) + lv_obj_get_width(g_lbl_stat_pw_kwh) + 7);

    lv_label_set_text(g_lbl_site_title, tel.site_name.c_str());
    lv_label_set_text(g_lbl_tou_pill, tou_txt);
    if (tou_is_peak) {
        lv_obj_set_style_bg_color(g_pill_tou, lv_color_hex(0x4c0519), 0);
        lv_obj_set_style_border_color(g_pill_tou, lv_color_hex(0xf43f5e), 0);
        lv_obj_set_style_text_color(g_lbl_tou_pill, lv_color_hex(0xfecdd3), 0);
    } else {
        lv_obj_set_style_bg_color(g_pill_tou, lv_color_hex(0x064e3b), 0);
        lv_obj_set_style_border_color(g_pill_tou, lv_color_hex(0x10b981), 0);
        lv_obj_set_style_text_color(g_lbl_tou_pill, lv_color_hex(0x6ee7b7), 0);
    }

    if (isWeekend) {
        lv_label_set_text(g_lbl_tou_track, "週末費率");
        lv_obj_set_style_text_color(g_lbl_tou_track, lv_color_hex(0x34d399), 0);
        lv_obj_remove_flag(g_tou_bar_we, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_tou_bar_wk, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(g_lbl_tou_track, "週間費率");
        lv_obj_set_style_text_color(g_lbl_tou_track, lv_color_hex(0xd4d4d8), 0);
        lv_obj_add_flag(g_tou_bar_we, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g_tou_bar_wk, LV_OBJ_FLAG_HIDDEN);
    }

    int cursor_x = 66 + (int)round((cursorPct / 100.0) * CHART_W);
    lv_obj_set_pos(g_cursor_cont, cursor_x - 1, 56);

    g_active_solar_w = solarW;
    g_active_pw_w    = pwW;
    g_active_grid_w  = gridW;
    g_active_load_w  = loadW;
    g_active_soe_pct = soePct;
    g_active_offgrid = isOffGrid;
    update_3d_route(solarW, pwW, gridW, loadW, isOffGrid);

    render_area_chart_to_buf(g_chart1_buf, CHART_W, CHART1_H, tel.history, false, 6.0, 56, 189, 248, 0);
    render_area_chart_to_buf(g_chart2_buf, CHART_W, CHART2_H, tel.history, true, 100.0, 0, 230, 118, reservePct);
    if (g_chart1_canvas) lv_obj_invalidate(g_chart1_canvas);
    if (g_chart2_canvas) lv_obj_invalidate(g_chart2_canvas);
}

static void on_status_pill_clicked(lv_event_t* e) {
    (void)e;
    static const std::vector<std::string> modes = {"live", "live_ip", "solar", "charge", "peak", "low", "offgrid"};
    auto it = std::find(modes.begin(), modes.end(), g_state_mode);
    size_t idx = (it == modes.end()) ? 0 : (std::distance(modes.begin(), it) + 1) % modes.size();
    g_state_mode = modes[idx];
    update_dashboard_ui();
}

static void on_weather_pill_clicked(lv_event_t* e) {
    (void)e;
    static const std::vector<std::string> wmodes = {
        "auto", "clear", "partly", "overcast", "rain", "storm", "snow", "wind", "sunrise", "sunset", "night"
    };
    auto it = std::find(wmodes.begin(), wmodes.end(), g_weather_mode);
    size_t idx = (it == wmodes.end()) ? 0 : (std::distance(wmodes.begin(), it) + 1) % wmodes.size();
    g_weather_mode = wmodes[idx];
    update_dashboard_ui();
}

// ============================================================================
// Build Complete 720x720 LVGL UI Hierarchy
// ============================================================================
static void build_lvgl_720x720_ui() {
    const char* font_path = (access("assets/STHeiti-Medium.ttc", R_OK) == 0)
        ? "assets/STHeiti-Medium.ttc"
        : "/System/Library/Fonts/STHeiti Medium.ttc";
    lv_font_t* font_10 = lv_freetype_font_create(font_path, LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 10, LV_FREETYPE_FONT_STYLE_NORMAL);
    lv_font_t* font_11 = lv_freetype_font_create(font_path, LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 11, LV_FREETYPE_FONT_STYLE_NORMAL);
    lv_font_t* font_12 = lv_freetype_font_create(font_path, LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 12, LV_FREETYPE_FONT_STYLE_NORMAL);
    lv_font_t* font_18 = lv_freetype_font_create(font_path, LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 18, LV_FREETYPE_FONT_STYLE_BOLD);
    lv_font_t* font_20 = lv_freetype_font_create(font_path, LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 20, LV_FREETYPE_FONT_STYLE_BOLD);

    if (!font_11) font_11 = (lv_font_t*)&lv_font_montserrat_12;
    if (!font_12) font_12 = (lv_font_t*)&lv_font_montserrat_12;
    if (!font_18) font_18 = (lv_font_t*)&lv_font_montserrat_18;
    if (!font_20) font_20 = (lv_font_t*)&lv_font_montserrat_20;
    if (!font_10) font_10 = font_11;

    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0b0c0f), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // 1. TOP HERO CARD (704 x 440 at x=8, y=8)
    lv_obj_t* hero_card = lv_obj_create(scr);
    lv_obj_set_pos(hero_card, 8, 8);
    lv_obj_set_size(hero_card, 704, 440);
    lv_obj_set_style_bg_color(hero_card, lv_color_hex(0x101820), 0);
    lv_obj_set_style_border_color(hero_card, lv_color_hex(0x27272a), 0);
    lv_obj_set_style_border_width(hero_card, 1, 0);
    lv_obj_set_style_radius(hero_card, 16, 0);
    lv_obj_set_style_pad_all(hero_card, 0, 0);
    lv_obj_set_style_clip_corner(hero_card, true, 0);
    lv_obj_remove_flag(hero_card, LV_OBJ_FLAG_SCROLLABLE);

    // 3D Stage Canvas (Full-bleed 704 x 440 at y=0)
    g_stage_canvas = lv_canvas_create(hero_card);
    lv_canvas_set_buffer(g_stage_canvas, g_stage_buf, STAGE_W, STAGE_H, LV_COLOR_FORMAT_ARGB8888);
    lv_obj_set_pos(g_stage_canvas, 0, 0);
    lv_obj_set_size(g_stage_canvas, STAGE_W, STAGE_H);

    // Floating Header Overlay Inside the 3D Scene (704 x 44 at y=0, transparent)
    lv_obj_t* hdr_bar = lv_obj_create(hero_card);
    lv_obj_set_pos(hdr_bar, 0, 2);
    lv_obj_set_size(hdr_bar, 704, 40);
    lv_obj_set_style_bg_opa(hdr_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hdr_bar, 0, 0);
    lv_obj_set_style_radius(hdr_bar, 0, 0);
    lv_obj_set_style_pad_all(hdr_bar, 0, 0);
    lv_obj_remove_flag(hdr_bar, LV_OBJ_FLAG_SCROLLABLE);

    g_lbl_site_title = lv_label_create(hdr_bar);
    lv_obj_set_style_text_font(g_lbl_site_title, font_18, 0);
    lv_obj_set_style_text_color(g_lbl_site_title, lv_color_hex(0xffffff), 0);
    lv_label_set_text(g_lbl_site_title, "我的家");
    lv_obj_align(g_lbl_site_title, LV_ALIGN_LEFT_MID, 16, 0);

    g_pill_status = lv_obj_create(hdr_bar);
    lv_obj_set_size(g_pill_status, LV_SIZE_CONTENT, 24);
    lv_obj_align(g_pill_status, LV_ALIGN_LEFT_MID, 100, 0);
    lv_obj_set_style_bg_color(g_pill_status, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(g_pill_status, LV_OPA_40, 0);
    lv_obj_set_style_border_color(g_pill_status, lv_color_hex(0x059669), 0);
    lv_obj_set_style_border_width(g_pill_status, 1, 0);
    lv_obj_set_style_radius(g_pill_status, 12, 0);
    lv_obj_set_style_pad_hor(g_pill_status, 10, 0);
    lv_obj_set_style_pad_top(g_pill_status, 2, 0);
    lv_obj_set_style_pad_bottom(g_pill_status, 0, 0);
    lv_obj_set_flex_flow(g_pill_status, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_pill_status, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(g_pill_status, 6, 0);
    lv_obj_remove_flag(g_pill_status, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_pill_status, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_pill_status, on_status_pill_clicked, LV_EVENT_CLICKED, nullptr);

    g_dot_status = lv_obj_create(g_pill_status);
    lv_obj_set_size(g_dot_status, 7, 7);
    lv_obj_set_style_radius(g_dot_status, 4, 0);
    lv_obj_set_style_bg_color(g_dot_status, lv_color_hex(0x34d399), 0);
    lv_obj_set_style_border_width(g_dot_status, 0, 0);
    lv_obj_set_style_translate_y(g_dot_status, -1, 0);

    g_lbl_status = lv_label_create(g_pill_status);
    lv_obj_set_style_text_font(g_lbl_status, font_11, 0);
    lv_obj_set_style_text_color(g_lbl_status, lv_color_hex(0x34d399), 0);
    lv_label_set_text(g_lbl_status, "連線中");

    g_pill_weather = lv_obj_create(hdr_bar);
    lv_obj_set_size(g_pill_weather, LV_SIZE_CONTENT, 24);
    lv_obj_align(g_pill_weather, LV_ALIGN_RIGHT_MID, -14, 0);
    lv_obj_set_style_bg_color(g_pill_weather, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(g_pill_weather, LV_OPA_40, 0);
    lv_obj_set_style_border_color(g_pill_weather, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(g_pill_weather, 1, 0);
    lv_obj_set_style_radius(g_pill_weather, 12, 0);
    lv_obj_set_style_pad_hor(g_pill_weather, 10, 0);
    lv_obj_set_style_pad_top(g_pill_weather, 2, 0);
    lv_obj_set_style_pad_bottom(g_pill_weather, 0, 0);
    lv_obj_set_flex_flow(g_pill_weather, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_pill_weather, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(g_pill_weather, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_pill_weather, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_pill_weather, on_weather_pill_clicked, LV_EVENT_CLICKED, nullptr);

    g_lbl_weather = lv_label_create(g_pill_weather);
    lv_obj_set_style_text_font(g_lbl_weather, font_11, 0);
    lv_obj_set_style_text_color(g_lbl_weather, lv_color_hex(0xbae6fd), 0);
    lv_label_set_text(g_lbl_weather, "☽ 台北：夜晚 25.4°C / ☽ 眉月 (39%)");

    auto make_leader_line = [&](int x, int y, int h_px) {
        lv_obj_t* ln = lv_obj_create(hero_card);
        lv_obj_set_pos(ln, x, y);
        lv_obj_set_size(ln, 2, h_px);
        lv_obj_set_style_bg_color(ln, lv_color_hex(0xa1a1aa), 0);
        lv_obj_set_style_bg_opa(ln, LV_OPA_60, 0);
        lv_obj_set_style_border_width(ln, 0, 0);
        lv_obj_set_style_radius(ln, 0, 0);
    };

    // 1. SOLAR Callout (3D Anchor at x=363, y=156 on 704x440 stage)
    make_leader_line(363, 100, 56);
    lv_obj_t* lbl_s_title = lv_label_create(hero_card);
    lv_obj_set_style_text_font(lbl_s_title, font_11, 0);
    lv_obj_set_style_text_color(lbl_s_title, lv_color_hex(0xa1a1aa), 0);
    lv_label_set_text(lbl_s_title, "太陽能");
    lv_obj_set_width(lbl_s_title, 100);
    lv_obj_set_style_text_align(lbl_s_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_s_title, 363 - 50, 56);

    g_lbl_solar_val = lv_label_create(hero_card);
    lv_obj_set_style_text_font(g_lbl_solar_val, font_20, 0);
    lv_obj_set_style_text_color(g_lbl_solar_val, lv_color_hex(0xa1a1aa), 0);
    lv_label_set_text(g_lbl_solar_val, "0 kW");
    lv_obj_set_width(g_lbl_solar_val, 120);
    lv_obj_set_style_text_align(g_lbl_solar_val, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(g_lbl_solar_val, 363 - 60, 72);

    // 2. HOME Callout (3D Anchor at x=484, y=256 on 704x440 stage)
    make_leader_line(484, 176, 80);
    lv_obj_t* lbl_h_title = lv_label_create(hero_card);
    lv_obj_set_style_text_font(lbl_h_title, font_11, 0);
    lv_obj_set_style_text_color(lbl_h_title, lv_color_hex(0xa1a1aa), 0);
    lv_label_set_text(lbl_h_title, "住家");
    lv_obj_set_width(lbl_h_title, 100);
    lv_obj_set_style_text_align(lbl_h_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_h_title, 484 - 50, 132);

    g_lbl_home_val = lv_label_create(hero_card);
    lv_obj_set_style_text_font(g_lbl_home_val, font_20, 0);
    lv_obj_set_style_text_color(g_lbl_home_val, lv_color_hex(0xffffff), 0);
    lv_label_set_text(g_lbl_home_val, "2.8 kW");
    lv_obj_set_width(g_lbl_home_val, 120);
    lv_obj_set_style_text_align(g_lbl_home_val, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(g_lbl_home_val, 484 - 60, 148);

    // 3. POWERWALL Callout (3D Anchor at x=279, y=356 on 704x440 stage)
    make_leader_line(279, 356, 14);
    lv_obj_t* lbl_p_title = lv_label_create(hero_card);
    lv_obj_set_style_text_font(lbl_p_title, font_11, 0);
    lv_obj_set_style_text_color(lbl_p_title, lv_color_hex(0xa1a1aa), 0);
    lv_label_set_text(lbl_p_title, "POWERWALL");
    lv_obj_set_width(lbl_p_title, 140);
    lv_obj_set_style_text_align(lbl_p_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_p_title, 279 - 70, 374);

    g_pw_val_row = lv_obj_create(hero_card);
    lv_obj_set_size(g_pw_val_row, 180, 24);
    lv_obj_set_pos(g_pw_val_row, 279 - 90, 390);
    lv_obj_set_style_bg_opa(g_pw_val_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_pw_val_row, 0, 0);
    lv_obj_set_style_pad_all(g_pw_val_row, 0, 0);
    lv_obj_set_style_pad_column(g_pw_val_row, 5, 0);
    lv_obj_set_flex_flow(g_pw_val_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_pw_val_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(g_pw_val_row, LV_OBJ_FLAG_SCROLLABLE);

    g_lbl_pw_kw = lv_label_create(g_pw_val_row);
    lv_obj_set_style_text_font(g_lbl_pw_kw, font_20, 0);
    lv_obj_set_style_text_color(g_lbl_pw_kw, lv_color_hex(0xffffff), 0);
    lv_label_set_text(g_lbl_pw_kw, "0 kW");

    g_lbl_pw_dot = lv_obj_create(g_pw_val_row);
    lv_obj_set_size(g_lbl_pw_dot, 4, 4);
    lv_obj_set_style_radius(g_lbl_pw_dot, 2, 0);
    lv_obj_set_style_bg_color(g_lbl_pw_dot, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(g_lbl_pw_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_lbl_pw_dot, 0, 0);
    lv_obj_set_style_pad_all(g_lbl_pw_dot, 0, 0);
    lv_obj_remove_flag(g_lbl_pw_dot, LV_OBJ_FLAG_SCROLLABLE);

    g_pw_arrow_canvas = lv_canvas_create(g_pw_val_row);
    lv_canvas_set_buffer(g_pw_arrow_canvas, g_pw_arrow_buf, 14, 12, LV_COLOR_FORMAT_ARGB8888);
    lv_obj_set_size(g_pw_arrow_canvas, 14, 12);
    lv_obj_add_flag(g_pw_arrow_canvas, LV_OBJ_FLAG_HIDDEN);

    g_lbl_pw_soc = lv_label_create(g_pw_val_row);
    lv_obj_set_style_text_font(g_lbl_pw_soc, font_20, 0);
    lv_obj_set_style_text_color(g_lbl_pw_soc, lv_color_hex(0xffffff), 0);
    lv_label_set_text(g_lbl_pw_soc, "100%");

    // 4. GRID Callout (3D Anchor at x=457, y=373 on 704x440 stage)
    make_leader_line(457, 373, 10);
    g_lbl_grid_title = lv_label_create(hero_card);
    lv_obj_set_style_text_font(g_lbl_grid_title, font_11, 0);
    lv_obj_set_style_text_color(g_lbl_grid_title, lv_color_hex(0xa1a1aa), 0);
    lv_label_set_text(g_lbl_grid_title, "電網");
    lv_obj_set_width(g_lbl_grid_title, 120);
    lv_obj_set_style_text_align(g_lbl_grid_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(g_lbl_grid_title, 457 - 60, 387);

    g_lbl_grid_val = lv_label_create(hero_card);
    lv_obj_set_style_text_font(g_lbl_grid_val, font_20, 0);
    lv_obj_set_style_text_color(g_lbl_grid_val, lv_color_hex(0xffffff), 0);
    lv_label_set_text(g_lbl_grid_val, "2.8 kW");
    lv_obj_set_width(g_lbl_grid_val, 140);
    lv_obj_set_style_text_align(g_lbl_grid_val, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(g_lbl_grid_val, 457 - 70, 403);

    // 2. BOTTOM UNIFIED 24H TIMELINE CARD (704 x 256 at x=8, y=456)
    lv_obj_t* chart_card = lv_obj_create(scr);
    lv_obj_set_pos(chart_card, 8, 456);
    lv_obj_set_size(chart_card, 704, 256);
    lv_obj_set_style_bg_color(chart_card, lv_color_hex(0x13151a), 0);
    lv_obj_set_style_bg_opa(chart_card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(chart_card, lv_color_hex(0x27272a), 0);
    lv_obj_set_style_border_width(chart_card, 1, 0);
    lv_obj_set_style_radius(chart_card, 16, 0);
    lv_obj_set_style_pad_all(chart_card, 0, 0);
    lv_obj_remove_flag(chart_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl_h_stat_t = lv_label_create(chart_card);
    lv_obj_set_style_text_font(lbl_h_stat_t, font_11, 0);
    lv_obj_set_style_text_color(lbl_h_stat_t, lv_color_hex(0xa1a1aa), 0);
    lv_label_set_text(lbl_h_stat_t, "住家用量");
    lv_obj_set_pos(lbl_h_stat_t, 14, 8);

    g_lbl_stat_home_kw = lv_label_create(chart_card);
    lv_obj_set_style_text_font(g_lbl_stat_home_kw, font_18, 0);
    lv_obj_set_style_text_color(g_lbl_stat_home_kw, lv_color_hex(0x38bdf8), 0);
    lv_label_set_text(g_lbl_stat_home_kw, "2.8 kW");
    lv_obj_set_pos(g_lbl_stat_home_kw, 14, 23);

    g_lbl_stat_home_va = lv_label_create(chart_card);
    lv_obj_set_style_text_font(g_lbl_stat_home_va, font_11, 0);
    lv_obj_set_style_text_color(g_lbl_stat_home_va, lv_color_hex(0xa1a1aa), 0);
    lv_label_set_text(g_lbl_stat_home_va, "121.6 V · 23.3 A");
    lv_obj_set_pos(g_lbl_stat_home_va, 84, 27);

    lv_obj_t* vdiv = lv_obj_create(chart_card);
    lv_obj_set_pos(vdiv, 192, 12);
    lv_obj_set_size(vdiv, 1, 26);
    lv_obj_set_style_bg_color(vdiv, lv_color_hex(0x27272a), 0);
    lv_obj_set_style_border_width(vdiv, 0, 0);

    g_lbl_stat_reserve = lv_label_create(chart_card);
    lv_obj_set_style_text_font(g_lbl_stat_reserve, font_11, 0);
    lv_obj_set_style_text_color(g_lbl_stat_reserve, lv_color_hex(0xa1a1aa), 0);
    lv_label_set_text(g_lbl_stat_reserve, "Powerwall (備援 24%)");
    lv_obj_set_pos(g_lbl_stat_reserve, 206, 8);

    g_lbl_stat_pw_soc = lv_label_create(chart_card);
    lv_obj_set_style_text_font(g_lbl_stat_pw_soc, font_18, 0);
    lv_obj_set_style_text_color(g_lbl_stat_pw_soc, lv_color_hex(0x00e676), 0);
    lv_label_set_text(g_lbl_stat_pw_soc, "100%");
    lv_obj_set_pos(g_lbl_stat_pw_soc, 206, 23);

    g_lbl_stat_pw_kwh = lv_label_create(chart_card);
    lv_obj_set_style_text_font(g_lbl_stat_pw_kwh, font_11, 0);
    lv_obj_set_style_text_color(g_lbl_stat_pw_kwh, lv_color_hex(0xa1a1aa), 0);
    lv_label_set_text(g_lbl_stat_pw_kwh, "14.59 / 14.59 kWh");
    lv_obj_set_pos(g_lbl_stat_pw_kwh, 262, 27);

    g_lbl_stat_pw_eta = lv_label_create(chart_card);
    lv_obj_set_style_text_font(g_lbl_stat_pw_eta, font_11, 0);
    lv_obj_set_style_text_color(g_lbl_stat_pw_eta, lv_color_hex(0xd4d4d8), 0);
    lv_label_set_text(g_lbl_stat_pw_eta, "· --");
    lv_obj_set_pos(g_lbl_stat_pw_eta, 374, 27);

    g_pill_tou = lv_obj_create(chart_card);
    lv_obj_set_size(g_pill_tou, LV_SIZE_CONTENT, 24);
    lv_obj_align(g_pill_tou, LV_ALIGN_TOP_RIGHT, -14, 12);
    lv_obj_set_style_bg_color(g_pill_tou, lv_color_hex(0x064e3b), 0);
    lv_obj_set_style_border_color(g_pill_tou, lv_color_hex(0x10b981), 0);
    lv_obj_set_style_border_width(g_pill_tou, 1, 0);
    lv_obj_set_style_radius(g_pill_tou, 12, 0);
    lv_obj_set_style_pad_hor(g_pill_tou, 10, 0);
    lv_obj_set_style_pad_top(g_pill_tou, 2, 0);
    lv_obj_set_style_pad_bottom(g_pill_tou, 0, 0);
    lv_obj_set_flex_flow(g_pill_tou, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_pill_tou, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(g_pill_tou, LV_OBJ_FLAG_SCROLLABLE);

    g_lbl_tou_pill = lv_label_create(g_pill_tou);
    lv_obj_set_style_text_font(g_lbl_tou_pill, font_11, 0);
    lv_obj_set_style_text_color(g_lbl_tou_pill, lv_color_hex(0x6ee7b7), 0);
    lv_label_set_text(g_lbl_tou_pill, "離峰時段 NT$1.99 / kWh");

    lv_obj_t* hdiv = lv_obj_create(chart_card);
    lv_obj_set_pos(hdiv, 14, 46);
    lv_obj_set_size(hdiv, 676, 1);
    lv_obj_set_style_bg_color(hdiv, lv_color_hex(0x27272a), 0);
    lv_obj_set_style_border_width(hdiv, 0, 0);

    auto add_y_label = [&](const char* txt, int y, uint32_t col) {
        lv_obj_t* l = lv_label_create(chart_card);
        lv_obj_set_style_text_font(l, font_10, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
        lv_label_set_text(l, txt);
        lv_obj_set_width(l, 54);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(l, 6, y);
    };
    add_y_label("住家 kW", 56, 0x38bdf8);
    add_y_label("3 kW",    78, 0x71717a);
    add_y_label("0 kW",   100, 0x71717a);

    g_chart1_canvas = lv_canvas_create(chart_card);
    lv_canvas_set_buffer(g_chart1_canvas, g_chart1_buf, CHART_W, CHART1_H, LV_COLOR_FORMAT_ARGB8888);
    lv_obj_set_pos(g_chart1_canvas, 66, 56);
    lv_obj_set_size(g_chart1_canvas, CHART_W, CHART1_H);

    add_y_label("PW 100%", 116, 0x00e676);
    add_y_label("50%",     142, 0x71717a);
    add_y_label("24%",     158, 0x38bdf8);
    add_y_label("0%",      170, 0x71717a);

    g_chart2_canvas = lv_canvas_create(chart_card);
    lv_canvas_set_buffer(g_chart2_canvas, g_chart2_buf, CHART_W, CHART2_H, LV_COLOR_FORMAT_ARGB8888);
    lv_obj_set_pos(g_chart2_canvas, 66, 116);
    lv_obj_set_size(g_chart2_canvas, CHART_W, CHART2_H);

    g_lbl_tou_track = lv_label_create(chart_card);
    lv_obj_set_style_text_font(g_lbl_tou_track, font_10, 0);
    lv_obj_set_style_text_color(g_lbl_tou_track, lv_color_hex(0x34d399), 0);
    lv_label_set_text(g_lbl_tou_track, "週末費率");
    lv_obj_set_width(g_lbl_tou_track, 54);
    lv_obj_set_style_text_align(g_lbl_tou_track, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(g_lbl_tou_track, 6, 194);

    g_tou_bar_we = lv_obj_create(chart_card);
    lv_obj_set_pos(g_tou_bar_we, 66, 195);
    lv_obj_set_size(g_tou_bar_we, CHART_W, 10);
    lv_obj_set_style_bg_color(g_tou_bar_we, lv_color_hex(0x26a64c), 0);
    lv_obj_set_style_border_width(g_tou_bar_we, 0, 0);
    lv_obj_set_style_radius(g_tou_bar_we, 5, 0);

    g_tou_bar_wk = lv_obj_create(chart_card);
    lv_obj_set_pos(g_tou_bar_wk, 66, 190);
    lv_obj_set_size(g_tou_bar_wk, CHART_W, 20);
    lv_obj_set_style_bg_opa(g_tou_bar_wk, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_tou_bar_wk, 0, 0);
    lv_obj_set_style_pad_all(g_tou_bar_wk, 0, 0);
    lv_obj_add_flag(g_tou_bar_wk, LV_OBJ_FLAG_HIDDEN);

    auto make_seg = [&](int x, int w, int h_seg, uint32_t col) {
        lv_obj_t* s = lv_obj_create(g_tou_bar_wk);
        lv_obj_set_pos(s, x, (20 - h_seg) / 2);
        lv_obj_set_size(s, w, h_seg);
        lv_obj_set_style_bg_color(s, lv_color_hex(col), 0);
        lv_obj_set_style_border_width(s, 0, 0);
        lv_obj_set_style_radius(s, 3, 0);
    };
    int w0_6   = (int)round(CHART_W * 0.25);
    int w6_11  = (int)round(CHART_W * 0.2083);
    int w11_14 = (int)round(CHART_W * 0.125);
    int w14_24 = CHART_W - w0_6 - w6_11 - w11_14;
    make_seg(0, w0_6, 10, 0x26a64c);
    make_seg(w0_6, w6_11, 18, 0xb9312f);
    make_seg(w0_6 + w6_11, w11_14, 10, 0x26a64c);
    make_seg(w0_6 + w6_11 + w11_14, w14_24, 18, 0xb9312f);

    const char* ticks[6] = {"00:00", "06:00", "11:00", "14:00", "18:00", "24:00"};
    const double tick_hrs[6] = {0.0, 6.0, 11.0, 14.0, 18.0, 24.0};
    for (int i = 0; i < 6; i++) {
        lv_obj_t* tl = lv_label_create(chart_card);
        lv_obj_set_style_text_font(tl, font_10, 0);
        lv_obj_set_style_text_color(tl, lv_color_hex(0xa1a1aa), 0);
        lv_label_set_text(tl, ticks[i]);
        int tx = 66 + (int)round((tick_hrs[i] / 24.0) * (CHART_W - 28));
        lv_obj_set_pos(tl, tx, 218);
    }

    // Continuous Vertical "NOW" Cursor Line (No Badge Box)
    g_cursor_cont = lv_obj_create(chart_card);
    lv_obj_set_size(g_cursor_cont, 2, 156);
    lv_obj_set_pos(g_cursor_cont, 200, 56);
    lv_obj_set_style_bg_color(g_cursor_cont, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(g_cursor_cont, LV_OPA_90, 0);
    lv_obj_set_style_border_width(g_cursor_cont, 0, 0);
    lv_obj_set_style_radius(g_cursor_cont, 0, 0);
    lv_obj_remove_flag(g_cursor_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(g_cursor_cont, LV_OBJ_FLAG_CLICKABLE);
}

// ============================================================================
// Main Entry Point
// ============================================================================
#ifdef POWERWALL_SIM_320
#include "sim_320.hpp"
#endif
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    signal(SIGPIPE, SIG_IGN);
    curl_global_init(CURL_GLOBAL_ALL);

    lv_init();
    load_nvs_config();
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        init_history_points(g_telemetry.history);
        printf("[History] loaded %zu points from esp32/history_today.json\n", g_telemetry.history.size());
    }
    load_3d_binary_assets();

    pthread_t poll_tid;
#ifndef POWERWALL_SIM_320
    pthread_t http_tid;
    pthread_create(&http_tid, nullptr, http_server_thread, nullptr);
    pthread_detach(http_tid);
#endif

    pthread_create(&poll_tid, nullptr, background_poller_thread, nullptr);
    pthread_detach(poll_tid);

#ifndef POWERWALL_SIM_320
    if (!g_nvs.configured) {
        printf("📱 First boot (configured=false): Opening tzapu/WiFiManager Captive Portal at http://localhost:8788/wifimanager?captive=1\n");
        system("open 'http://localhost:8788/wifimanager?captive=1' >/dev/null 2>&1 &");
    }
#endif

    lv_display_t* disp = lv_sdl_window_create(SCREEN_W, SCREEN_H);
#ifdef POWERWALL_SIM_320
    lv_sdl_window_set_title(disp, "Powerwall 320x320 LVGL Simulator — 左上切換用電，右上切換天氣");
#else
    lv_sdl_window_set_title(disp, "ESP32-P4 720x720 LVGL Simulator — 我的家 Powerwall 2 [點擊左上狀態鈕切換電流/斷網叉叉 · 點擊右上天氣鈕切換日夜/天氣]");
#endif
    lv_sdl_mouse_create();
    lv_sdl_keyboard_create();

#ifdef POWERWALL_SIM_320
    g_state_mode = "solar";
    build_lvgl_320_ui();
    update_dashboard_320_ui();
    render_stage_320_frame();
#else
    build_lvgl_720x720_ui();
    update_dashboard_ui();
    render_3d_stage_frame();
#endif

    lv_timer_create([](lv_timer_t*) {
#ifdef POWERWALL_SIM_320
        render_stage_320_frame();
#else
        render_3d_stage_frame();
#endif
    }, 33, nullptr);

    lv_timer_create([](lv_timer_t* t) {
#ifdef POWERWALL_SIM_320
        update_dashboard_320_ui();
#else
        update_dashboard_ui();
#endif
        lv_display_t* d = (lv_display_t*)lv_timer_get_user_data(t);
        if (d && d->buf_act && d->buf_act->data) {
#ifdef POWERWALL_SIM_320
            FILE* fp = fopen("assets/lvgl_screen_320_latest.ppm", "wb");
#else
            FILE* fp = fopen("assets/lvgl_screen_latest.ppm", "wb");
#endif
            if (fp) {
                fprintf(fp, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
                const uint8_t* p = d->buf_act->data;
                std::vector<uint8_t> rgb(SCREEN_W * SCREEN_H * 3);
                for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
                    rgb[i * 3 + 0] = p[i * 4 + 2];
                    rgb[i * 3 + 1] = p[i * 4 + 1];
                    rgb[i * 3 + 2] = p[i * 4 + 0];
                }
                fwrite(rgb.data(), 1, rgb.size(), fp);
                fclose(fp);
            }
        }
    }, 500, disp);

    auto save_ppm = [&](const char* path) {
        if (disp && disp->buf_act && disp->buf_act->data) {
            FILE* fp = fopen(path, "wb");
            if (fp) {
                fprintf(fp, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
                const uint8_t* p = disp->buf_act->data;
                std::vector<uint8_t> rgb(SCREEN_W * SCREEN_H * 3);
                for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
                    rgb[i * 3 + 0] = p[i * 4 + 2];
                    rgb[i * 3 + 1] = p[i * 4 + 1];
                    rgb[i * 3 + 2] = p[i * 4 + 0];
                }
                fwrite(rgb.data(), 1, rgb.size(), fp);
                fclose(fp);
            }
        }
    };

    if (argc > 1 && std::string(argv[1]) == "--dump-previews") {
        auto pump_frames = [&](int n) {
            for (int i = 0; i < n; i++) {
#ifdef POWERWALL_SIM_320
                render_stage_320_frame();
#else
                render_3d_stage_frame();
#endif
                lv_obj_invalidate(lv_screen_active());
                lv_refr_now(disp);
                lv_timer_handler();
                usleep(20000);
            }
        };
        g_state_mode = "live";
        g_weather_mode = "auto";
#ifdef POWERWALL_SIM_320
        update_dashboard_320_ui();
#else
        update_dashboard_ui();
#endif
        pump_frames(8);
#ifdef POWERWALL_SIM_320
        save_ppm("assets/lvgl_screen_320_live.ppm");
#else
        save_ppm("assets/lvgl_screen_live.ppm");
#endif

        g_state_mode = "solar";
        g_weather_mode = "clear";
#ifdef POWERWALL_SIM_320
        update_dashboard_320_ui();
#else
        update_dashboard_ui();
#endif
        pump_frames(8);
#ifdef POWERWALL_SIM_320
        save_ppm("assets/lvgl_screen_320_solar_clear.ppm");
#else
        save_ppm("assets/lvgl_screen_solar_clear.ppm");
#endif

        g_state_mode = "offgrid";
        g_weather_mode = "storm";
#ifdef POWERWALL_SIM_320
        update_dashboard_320_ui();
#else
        update_dashboard_ui();
#endif
        pump_frames(8);
#ifdef POWERWALL_SIM_320
        save_ppm("assets/lvgl_screen_320_offgrid_storm.ppm");
#else
        save_ppm("assets/lvgl_screen_offgrid_storm.ppm");
#endif
        return 0;
    }

#ifdef POWERWALL_SIM_320
    printf("✅ 320x320 LVGL + SDL2 simulator running!\n");
#else
    printf("✅ ESP32-P4 720x720 LVGL + SDL2 Native Simulator running!\n");
#endif
    while (true) {
        uint32_t sleep_ms = lv_timer_handler();
        if (sleep_ms < 1) sleep_ms = 1;
        if (sleep_ms > 16) sleep_ms = 16;
        usleep(sleep_ms * 1000);
    }

    return 0;
}

import datetime
import http.server
import json
import math
import mimetypes
import os
import socket
import socketserver
import ssl
import subprocess
import threading
import time
import urllib.error
import urllib.parse
import urllib.request

ESP32_DIR = os.path.dirname(os.path.abspath(__file__))

NVS_CONFIG_PATH = os.path.join(ESP32_DIR, "nvs_config.json")
TOU_CONFIG_PATH = os.path.join(ESP32_DIR, "tou_config.json")
HISTORY_PATH = os.path.join(ESP32_DIR, "history_today.json")
INDEX_HTML_PATH = os.path.join(ESP32_DIR, "index.html")
WIFIMANAGER_HTML_PATH = os.path.join(ESP32_DIR, "wifimanager.html")
PREVIEWS_DIR = os.path.join(ESP32_DIR, "previews")
RECOVERED_DIR = os.path.join(ESP32_DIR, "recovered")

PORT = int(os.environ.get("PORT", "8788"))
BOOT_TIME = time.time()

ssl_ctx = ssl.create_default_context()
ssl_ctx.check_hostname = False
ssl_ctx.verify_mode = ssl.CERT_NONE


class NVSConfigManager:
  """Simulates ESP32 Preferences (NVS flash) for tzapu/WiFiManager + Powerwall Gateway credentials & token."""

  DEFAULT_CONFIG = {
      "configured": False,
      "wifi_ssid": "Home-WiFi",
      "wifi_password": "",
      "gateway_ip": os.environ.get("PW_HOST", ""),
      "gateway_password": os.environ.get("PW_PASSWORD", ""),
      "gateway_email": os.environ.get("PW_EMAIL", "customer@example.com"),
      "site_title": "我的家",
      "ap_ssid": "Powerwall-ESP32P4",
      "ap_ip": "192.168.4.1",
      "auth_token": "",
      "token_login_time": "",
  }

  def __init__(self, path):
    self.path = path
    self.lock = threading.Lock()
    self.data = dict(self.DEFAULT_CONFIG)
    self.load()

  def load(self):
    with self.lock:
      if os.path.exists(self.path):
        try:
          with open(self.path, "r", encoding="utf-8") as f:
            saved = json.load(f)
          if isinstance(saved, dict):
            for k, v in self.DEFAULT_CONFIG.items():
              if k == "configured":
                self.data[k] = bool(saved.get(k, v))
              else:
                self.data[k] = str(saved.get(k, v))
            self.data["updated_at"] = saved.get(
                "updated_at", datetime.datetime.now().isoformat()
            )
            return dict(self.data)
        except Exception:
          pass
      self._save_unlocked()
      return dict(self.data)

  def _save_unlocked(self):
    self.data["updated_at"] = datetime.datetime.now().isoformat()
    try:
      with open(self.path, "w", encoding="utf-8") as f:
        json.dump(self.data, f, ensure_ascii=False, indent=2)
    except Exception:
      pass

  def is_configured(self):
    with self.lock:
      return bool(self.data.get("configured", False))

  def save_token(self, token, login_time=""):
    with self.lock:
      self.data["auth_token"] = str(token or "")
      self.data["token_login_time"] = str(
          login_time or datetime.datetime.now().isoformat()
      )
      self._save_unlocked()

  def get_all(self, mask_password=False):
    with self.lock:
      out = dict(self.data)
    tok = out.get("auth_token", "")
    out["auth_token_preview"] = (
        f"{tok[:12]}...{tok[-8:]} ({len(tok)} chars)" if len(tok) > 24 else tok
    )
    if mask_password:
      pw = out.get("gateway_password", "")
      out["gateway_password_masked"] = (
          ("*" * max(0, len(pw) - 2) + pw[-2:]) if len(pw) > 2 else "*****"
      )
      out.pop("gateway_password", None)
      out.pop("wifi_password", None)
      out.pop("auth_token", None)
    return out

  def save_wifimanager_submission(self, new_fields):
    with self.lock:
      if "wifi_ssid" in new_fields and str(new_fields["wifi_ssid"]).strip():
        self.data["wifi_ssid"] = str(new_fields["wifi_ssid"]).strip()
      if (
          "wifi_password" in new_fields
          and new_fields["wifi_password"] is not None
      ):
        if str(new_fields["wifi_password"]) != "":
          self.data["wifi_password"] = str(new_fields["wifi_password"])
      if "gateway_ip" in new_fields and str(new_fields["gateway_ip"]).strip():
        self.data["gateway_ip"] = str(new_fields["gateway_ip"]).strip()
      if (
          "gateway_password" in new_fields
          and str(new_fields["gateway_password"]).strip()
      ):
        self.data["gateway_password"] = str(
            new_fields["gateway_password"]
        ).strip()
      if (
          "gateway_email" in new_fields
          and str(new_fields["gateway_email"]).strip()
      ):
        self.data["gateway_email"] = str(new_fields["gateway_email"]).strip()
      if "site_title" in new_fields and str(new_fields["site_title"]).strip():
        self.data["site_title"] = str(new_fields["site_title"]).strip()
      self.data["configured"] = True
      self._save_unlocked()
      return dict(self.data)

  def reset_to_unconfigured(self):
    """Simulates wm.resetSettings() -> clears NVS so device enters AP Captive Portal mode."""
    with self.lock:
      self.data = dict(self.DEFAULT_CONFIG)
      self.data["configured"] = False
      self.data["auth_token"] = ""
      self.data["token_login_time"] = ""
      self._save_unlocked()
      return dict(self.data)


nvs_config = NVSConfigManager(NVS_CONFIG_PATH)


class ESP32NetworkMonitor:
  """Provides real-time local IP, WiFi RSSI/quality (tzapu/WiFiManager::getRSSIasQuality), and live scan list."""

  def __init__(self):
    self.last_scan_ts = 0
    self.cached_networks = []
    self.current_rssi = -46
    self.current_channel = "36 (5GHz)"
    self.lock = threading.Lock()

  @staticmethod
  def rssi_to_quality(rssi):
    # Exact formula from tzapu/WiFiManager: WiFiManager::getRSSIasQuality(int RSSI)
    if rssi <= -100:
      return 0
    if rssi >= -50:
      return 100
    return 2 * (rssi + 100)

  @staticmethod
  def get_local_ip():
    try:
      s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
      s.settimeout(0.5)
      s.connect(("8.8.8.8", 80))
      ip = s.getsockname()[0]
      s.close()
      return ip
    except Exception:
      return "192.0.2.10"

  def scan_wifi_networks(self, force=False):
    now = time.time()
    with self.lock:
      if not force and self.cached_networks and (now - self.last_scan_ts < 30):
        return list(self.cached_networks)

    cfg = nvs_config.get_all()
    active_ssid = cfg.get("wifi_ssid") or "Home-WiFi"
    is_conf = bool(cfg.get("configured"))
    networks = []
    try:
      out = subprocess.check_output(
          ["system_profiler", "SPAirPortDataType", "-json"],
          timeout=5,
          stderr=subprocess.DEVNULL,
      )
      sp = json.loads(out)
      ifaces = sp.get("SPAirPortDataType", [{}])[0].get(
          "spairport_airport_interfaces", []
      )
      if ifaces:
        other = ifaces[0].get(
            "spairport_airport_other_local_wireless_networks", []
        )
        for idx, net in enumerate(other[:8]):
          sig_str = net.get("spairport_signal_noise", "-52 dBm / -90 dBm")
          try:
            rssi = int(sig_str.split("dBm")[0].strip())
          except Exception:
            rssi = -48 - idx * 6
          ch = net.get("spairport_network_channel", f"{1 + (idx * 5) % 11}")
          sec = net.get("spairport_security_mode", "wpa2")
          raw_name = net.get("_name", "")
          if not raw_name or raw_name == "<redacted>":
            if idx == 0:
              raw_name = active_ssid
              self.current_rssi = rssi
              self.current_channel = ch
            elif idx == 1:
              raw_name = "Home-WiFi"
            elif idx == 2:
              raw_name = "Tesla_Gateway_TEG"
            else:
              raw_name = f"Home_Mesh_{idx}"
          networks.append({
              "ssid": raw_name,
              "rssi": rssi,
              "quality": self.rssi_to_quality(rssi),
              "channel": ch,
              "encrypted": "none" not in sec.lower(),
              "connected": is_conf and (raw_name == active_ssid),
          })
    except Exception:
      pass

    if not any(n["ssid"] == active_ssid for n in networks):
      networks.insert(
          0,
          {
              "ssid": active_ssid,
              "rssi": self.current_rssi,
              "quality": self.rssi_to_quality(self.current_rssi),
              "channel": self.current_channel,
              "encrypted": True,
              "connected": is_conf,
          },
      )

    dedup = {}
    for n in networks:
      s = n["ssid"]
      if s not in dedup or n["rssi"] > dedup[s]["rssi"]:
        dedup[s] = n
    sorted_nets = sorted(dedup.values(), key=lambda x: x["rssi"], reverse=True)

    with self.lock:
      self.cached_networks = sorted_nets
      self.last_scan_ts = now
    return sorted_nets

  def get_esp32_sys_info(self, pw_client):
    cfg = nvs_config.get_all()
    is_conf = bool(cfg.get("configured", False))
    uptime_s = int(time.time() - BOOT_TIME)
    tok = pw_client.token or cfg.get("auth_token", "")
    tok_preview = (
        f"{tok[:12]}...{tok[-8:]} ({len(tok)} chars)" if len(tok) > 24 else tok
    )
    return {
        "configured": is_conf,
        "wifi_mode": "STA" if is_conf else "AP_CAPTIVE_PORTAL",
        "ap_ssid": cfg.get("ap_ssid", "Powerwall-ESP32P4"),
        "ap_ip": cfg.get("ap_ip", "192.168.4.1"),
        "chip_model": "ESP32-P4 (Dual-Core RISC-V @ 400MHz)",
        "wifi_coprocessor": "ESP32-C6 (ESP-Hosted SDIO)",
        "display": "720x720 MIPI-DSI RGB (1:1 Square)",
        "sdk_version": "ESP-IDF v5.4 / Arduino-ESP32 v3.1.1",
        "wifi_manager_repo": "https://github.com/tzapu/WiFiManager",
        "wifi_ssid": cfg.get("wifi_ssid", "") if is_conf else "",
        "local_ip": self.get_local_ip() if is_conf else "192.168.4.1",
        "rssi_dbm": self.current_rssi,
        "signal_quality_pct": self.rssi_to_quality(self.current_rssi),
        "channel": self.current_channel,
        "gateway_ip": pw_client.host,
        "gateway_auth_ok": pw_client.auth_ok if is_conf else False,
        "gateway_token_preview": tok_preview if is_conf else "",
        "gateway_token_login_time": (
            pw_client.token_login_time or cfg.get("token_login_time", "")
        ),
        "gateway_latency_ms": pw_client.last_latency_ms if is_conf else 0,
        "last_error": pw_client.last_error,
        "uptime_seconds": uptime_s,
        "free_heap_kb": 512,
        "free_psram_kb": 28640,
    }


net_monitor = ESP32NetworkMonitor()


class PowerwallLocalClient:
  """Real-time Tesla Powerwall 2 Local Gateway HTTPS client that exchanges password for a Bearer Token."""

  def __init__(self, host, password, email):
    self.host = host
    self.password = password
    self.email = email
    self.token = ""
    self.token_login_time = ""
    self.headers = {}
    self.last_login = 0
    self.auth_ok = False
    self.last_latency_ms = 0
    self.lock = threading.Lock()
    self.cached_static = {}
    self.last_static_fetch = 0
    self.latest_telemetry = None
    self.last_error = None
    self.today_str = datetime.date.today().isoformat()
    self.history_points = []
    self.last_history_record = 0
    self._init_history()

  def reconfigure(self, host, password, email=None):
    with self.lock:
      self.host = host
      self.password = password
      if email:
        self.email = email
      self.token = ""
      self.headers = {}
      self.last_login = 0
      self.cached_static = {}
      self.last_static_fetch = 0
      self.auth_ok = False
    return self.poll_live_data(force_login=True)

  def _init_history(self):
    today = datetime.date.today().isoformat()
    if os.path.exists(HISTORY_PATH):
      try:
        with open(HISTORY_PATH, "r", encoding="utf-8") as f:
          saved = json.load(f)
        if saved.get("date") == today and isinstance(saved.get("points"), list):
          self.history_points = saved["points"]
          return
      except Exception:
        pass
    self.history_points = []

  def _save_history(self):
    try:
      with open(HISTORY_PATH, "w", encoding="utf-8") as f:
        json.dump(
            {"date": self.today_str, "points": self.history_points},
            f,
            ensure_ascii=False,
        )
    except Exception:
      pass

  def _record_point(self, soe_pct, load_kw):
    now = datetime.datetime.now()
    today = now.date().isoformat()
    if today != self.today_str:
      self.today_str = today
      self.history_points = []

    now_ts = time.time()
    hour_float = round(now.hour + now.minute / 60.0 + now.second / 3600.0, 3)

    if not self.history_points or (now_ts - self.last_history_record >= 60):
      self.history_points.append({
          "hour": hour_float,
          "soe": soe_pct,
          "load_kw": load_kw,
      })
      self.last_history_record = now_ts
      self._save_history()
    else:
      self.history_points[-1] = {
          "hour": hour_float,
          "soe": soe_pct,
          "load_kw": load_kw,
      }

  def _request_json(self, path, method="GET", data=None, timeout=5):
    url = f"https://{self.host}{path}"
    req_headers = dict(self.headers)
    body = None
    if data is not None:
      body = json.dumps(data).encode("utf-8")
      req_headers["Content-Type"] = "application/json"
    req = urllib.request.Request(
        url, data=body, headers=req_headers, method=method
    )
    with urllib.request.urlopen(req, context=ssl_ctx, timeout=timeout) as resp:
      cookies = resp.headers.get_all("Set-Cookie") or []
      raw = resp.read().decode("utf-8")
      return (json.loads(raw) if raw else {}), cookies

  def login_and_acquire_token(self):
    """Exchanges customer password for a Powerwall Local Gateway Token via POST /api/login/Basic."""
    payload = {
        "username": "customer",
        "password": self.password,
        "email": self.email,
        "force_sm_off": False,
    }
    res, cookies = self._request_json(
        "/api/login/Basic", method="POST", data=payload, timeout=6
    )
    cookie_pairs = [c.split(";")[0] for c in cookies]
    cookie_header = "; ".join(cookie_pairs)

    # Extract token from response JSON or AuthCookie
    token = res.get("token", "")
    if not token:
      for cp in cookie_pairs:
        if cp.startswith("AuthCookie="):
          token = cp.split("=", 1)[1]
          break

    if not token and not cookie_header:
      raise RuntimeError("Powerwall /api/login/Basic did not return a token")

    hdrs = {}
    if cookie_header:
      hdrs["Cookie"] = cookie_header
    if token:
      hdrs["Authorization"] = f"Bearer {token}"

    self.token = token
    self.token_login_time = res.get(
        "loginTime", datetime.datetime.now().isoformat()
    )
    self.headers = hdrs
    self.last_login = time.time()
    self.auth_ok = True
    nvs_config.save_token(self.token, self.token_login_time)
    return self.token

  def ensure_login(self, force=False):
    now = time.time()
    if (
        not force
        and self.headers
        and self.token
        and (now - self.last_login < 1800)
    ):
      return
    self.login_and_acquire_token()

  def fetch_endpoint(self, path):
    try:
      return self._request_json(path, timeout=4)[0]
    except urllib.error.HTTPError as e:
      if e.code in (401, 403):
        self.login_and_acquire_token()
        return self._request_json(path, timeout=4)[0]
      raise

  def poll_live_data(self, force_login=False):
    with self.lock:
      t0 = time.time()
      try:
        self.ensure_login(force=force_login)
        now = time.time()

        if not self.cached_static or (now - self.last_static_fetch > 60):
          status = self.fetch_endpoint("/api/status")
          site_info = self.fetch_endpoint("/api/site_info")
          operation = self.fetch_endpoint("/api/operation")
          self.cached_static = {
              "status": status,
              "site_info": site_info,
              "operation": operation,
          }
          self.last_static_fetch = now

        soe = self.fetch_endpoint("/api/system_status/soe")
        aggregates = self.fetch_endpoint("/api/meters/aggregates")
        grid_status = self.fetch_endpoint("/api/system_status/grid_status")
        sys_status = self.fetch_endpoint("/api/system_status")

        site_m = aggregates.get("site", {})
        solar_m = aggregates.get("solar", {})
        battery_m = aggregates.get("battery", {})
        load_m = aggregates.get("load", {})

        nom_full_wh = sys_status.get("nominal_full_pack_energy", 13500) or 13500
        nom_rem_wh = sys_status.get("nominal_energy_remaining", 0)
        raw_soe = round(soe.get("percentage", 0), 1)
        load_kw = round(load_m.get("instant_power", 0) / 1000.0, 2)

        self._record_point(raw_soe, load_kw)
        self.last_latency_ms = int(round((time.time() - t0) * 1000))
        self.auth_ok = True

        cfg = nvs_config.get_all()
        site_name = (
            self.cached_static.get("site_info", {}).get("site_name")
            or cfg.get("site_title")
            or "我的家"
        )

        self.latest_telemetry = {
            "ok": True,
            "timestamp": datetime.datetime.now().isoformat(),
            "gateway_ip": self.host,
            "latency_ms": self.last_latency_ms,
            "site_name": site_name,
            "version": (
                self.cached_static.get("status", {}).get("version", "--")
            ),
            "operation": {
                "real_mode": (
                    self.cached_static.get("operation", {}).get(
                        "real_mode", "autonomous"
                    )
                ),
                "backup_reserve_percent": round(
                    self.cached_static.get("operation", {}).get(
                        "backup_reserve_percent", 24
                    ),
                    1,
                ),
            },
            "battery": {
                "soe_percent": raw_soe,
                "nominal_full_kwh": round(nom_full_wh / 1000.0, 2),
                "nominal_remaining_kwh": round(nom_rem_wh / 1000.0, 2),
                "instant_power_w": round(battery_m.get("instant_power", 0), 1),
                "voltage_v": round(
                    battery_m.get("instant_average_voltage", 0), 1
                ),
                "frequency_hz": round(battery_m.get("frequency", 60.0), 2),
                "current_a": round(
                    battery_m.get("instant_average_current", 0), 2
                ),
            },
            "meters": {
                "site": {
                    "power_w": round(site_m.get("instant_power", 0), 1),
                    "voltage_v": round(
                        site_m.get("instant_average_voltage", 0), 1
                    ),
                    "current_a": round(
                        site_m.get("instant_average_current", 0), 1
                    ),
                },
                "load": {
                    "power_w": round(load_m.get("instant_power", 0), 1),
                    "voltage_v": round(
                        load_m.get("instant_average_voltage", 0), 1
                    ),
                    "current_a": round(
                        load_m.get("instant_average_current", 0), 1
                    ),
                },
                "solar": {
                    "power_w": round(solar_m.get("instant_power", 0), 1),
                    "voltage_v": round(
                        solar_m.get("instant_average_voltage", 0), 1
                    ),
                    "current_a": round(
                        solar_m.get("instant_average_current", 0), 1
                    ),
                },
            },
            "grid_status": grid_status.get(
                "grid_status", "SystemGridConnected"
            ),
            "history": self.history_points[-240:],
        }
        self.last_error = None
        return self.latest_telemetry
      except Exception as e:
        self.last_error = str(e)
        self.auth_ok = False
        if self.latest_telemetry:
          fallback = dict(self.latest_telemetry)
          fallback["stale"] = True
          fallback["error"] = self.last_error
          fallback["gateway_ip"] = self.host
          return fallback
        return {
            "ok": False,
            "error": self.last_error,
            "gateway_ip": self.host,
        }


initial_cfg = nvs_config.get_all()
pw_client = PowerwallLocalClient(
    initial_cfg["gateway_ip"],
    initial_cfg["gateway_password"],
    initial_cfg["gateway_email"],
)


class LocalWeatherClient:
  """Fetches real-time local weather, sunrise/sunset, and lunar phase to drive Tesla Energy 3D Godot weather visuals."""

  def __init__(self):
    self.lat = 25.0530
    self.lon = 121.5259
    self.city = "台北"
    self.loc_resolved = False
    self.last_fetch = 0
    self.cached = {
        "mode": "night",
        "label": "夜晚",
        "temp_c": 25.4,
        "cloud_cover": 35,
        "rain_mm": 0.0,
        "snow_cm": 0.0,
        "wind_kmh": 10.0,
        "weather_code": 1,
        "is_day": 0,
        "sunrise": "05:53",
        "sunset": "17:25",
        "sun_progress": 0.0,
        "city": "台北",
    }

  def _resolve_location(self):
    if self.loc_resolved:
      return
    try:
      req = urllib.request.Request(
          "http://ip-api.com/json/", headers={"User-Agent": "Mozilla/5.0"}
      )
      with urllib.request.urlopen(req, timeout=3) as resp:
        info = json.loads(resp.read().decode("utf-8"))
        if info.get("status") == "success":
          self.lat = float(info.get("lat", self.lat))
          self.lon = float(info.get("lon", self.lon))
          city_en = info.get("city", "")
          self.city = "台北" if "Taipei" in city_en else (city_en or "本地")
          self.loc_resolved = True
    except Exception:
      self.loc_resolved = True

  @staticmethod
  def _compute_moon_info(now_utc=None):
    if now_utc is None:
      now_utc = datetime.datetime.now(datetime.timezone.utc)
    ref_new_moon = datetime.datetime(
        2000, 1, 6, 18, 14, 0, tzinfo=datetime.timezone.utc
    )
    synodic_days = 29.530588853
    elapsed_days = (now_utc - ref_new_moon).total_seconds() / 86400.0
    astro_phase = (elapsed_days % synodic_days) / synodic_days
    illum_pct = round((1.0 - math.cos(astro_phase * 2.0 * math.pi)) * 50.0)

    if astro_phase < 0.03 or astro_phase >= 0.97:
      moon_label = "🌑 新月"
    elif astro_phase < 0.22:
      moon_label = "🌒 眉月"
    elif astro_phase < 0.28:
      moon_label = "🌓 上弦月"
    elif astro_phase < 0.47:
      moon_label = "🌔 盈凸月"
    elif astro_phase < 0.53:
      moon_label = "🌕 滿月"
    elif astro_phase < 0.72:
      moon_label = "🌖 虧凸月"
    elif astro_phase < 0.78:
      moon_label = "🌗 下弦月"
    else:
      moon_label = "🌘 殘月"

    if astro_phase <= 0.25:
      remapped = (astro_phase / 0.25) * 0.5
      is_crescent = True
    elif astro_phase > 0.75:
      remapped = 0.5 + ((astro_phase - 0.75) / 0.25) * 0.5
      is_crescent = True
    elif 0.25 < astro_phase < 0.5:
      remapped = 0.5 + ((astro_phase - 0.25) / 0.25) * 0.5
      is_crescent = False
    else:
      remapped = ((astro_phase - 0.5) / 0.25) * 0.5
      is_crescent = False
    godot_phase = round(1.0 - remapped, 4)

    return {
        "astro_moon_phase": round(astro_phase, 4),
        "moon_illum_pct": illum_pct,
        "moon_label": moon_label,
        "godot_moon_phase": godot_phase,
        "godot_is_crescent": is_crescent,
    }

  def get_weather(self):
    now_ts = time.time()
    if now_ts - self.last_fetch < 600 and self.last_fetch > 0:
      return self._with_live_sun_progress(self.cached)

    self._resolve_location()
    try:
      url = (
          f"https://api.open-meteo.com/v1/forecast?latitude={self.lat}&longitude={self.lon}"
          "&current=temperature_2m,is_day,precipitation,rain,showers,snowfall,weather_code,cloud_cover,wind_speed_10m"
          "&daily=sunrise,sunset&timezone=auto&forecast_days=1"
      )
      req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
      with urllib.request.urlopen(req, timeout=4) as resp:
        raw = json.loads(resp.read().decode("utf-8"))
      cur = raw.get("current", {})
      daily = raw.get("daily", {})

      wcode = int(cur.get("weather_code", 3))
      is_day = int(cur.get("is_day", 1))
      clouds = int(cur.get("cloud_cover", 50))
      temp_c = round(float(cur.get("temperature_2m", 25.0)), 1)
      rain_mm = float(cur.get("rain", 0.0) or 0.0) + float(
          cur.get("showers", 0.0) or 0.0
      )
      snow_cm = float(cur.get("snowfall", 0.0) or 0.0)
      wind_kmh = round(float(cur.get("wind_speed_10m", 0.0) or 0.0), 1)

      sunrise_raw = (daily.get("sunrise") or ["06:00"])[0]
      sunset_raw = (daily.get("sunset") or ["18:00"])[0]
      sunrise_hm = sunrise_raw.split("T")[-1][:5]
      sunset_hm = sunset_raw.split("T")[-1][:5]

      if wcode in (95, 96, 99):
        mode, label = "storm", "強雨／閃電"
      elif wcode in (71, 73, 75, 77, 85, 86) or snow_cm > 0:
        mode, label = "snow", "下雪"
      elif (
          wcode in (51, 53, 55, 56, 57, 61, 63, 65, 66, 67, 80, 81, 82)
          or rain_mm > 0
      ):
        mode, label = "rain", "下雨"
      elif not is_day:
        mode, label = "night", "夜晚"
      elif wcode in (3, 45, 48) or clouds >= 80:
        mode, label = "overcast", "陰天"
      elif wcode in (1, 2) or clouds >= 25:
        mode, label = "partly", "多雲"
      else:
        mode, label = "clear", "晴朗"

      self.cached = {
          "mode": mode,
          "label": label,
          "temp_c": temp_c,
          "cloud_cover": clouds,
          "rain_mm": round(rain_mm, 2),
          "snow_cm": round(snow_cm, 2),
          "wind_kmh": wind_kmh,
          "weather_code": wcode,
          "is_day": is_day,
          "sunrise": sunrise_hm,
          "sunset": sunset_hm,
          "city": self.city,
      }
      self.last_fetch = now_ts
    except Exception:
      pass

    return self._with_live_sun_progress(self.cached)

  def _with_live_sun_progress(self, data):
    out = dict(data)
    moon_info = self._compute_moon_info()
    out.update(moon_info)

    try:
      now = datetime.datetime.now()
      cur_m = now.hour * 60 + now.minute
      sr_h, sr_m = [int(x) for x in out.get("sunrise", "06:00").split(":")]
      ss_h, ss_m = [int(x) for x in out.get("sunset", "18:00").split(":")]
      sr_total = sr_h * 60 + sr_m
      ss_total = ss_h * 60 + ss_m
      if ss_total > sr_total:
        prog = (cur_m - sr_total) / float(ss_total - sr_total)
        out["sun_progress"] = round(max(0.0, min(1.0, prog)), 3)
        is_night_now = (cur_m < sr_total) or (cur_m > ss_total)
      else:
        out["sun_progress"] = 0.5
        is_night_now = not bool(out.get("is_day", 1))
    except Exception:
      out["sun_progress"] = 0.5
      is_night_now = not bool(out.get("is_day", 1))

    out["is_night"] = is_night_now
    wcode = int(out.get("weather_code", 3))
    clouds = int(out.get("cloud_cover", 50))
    rain_mm = float(out.get("rain_mm", 0.0))
    snow_cm = float(out.get("snow_cm", 0.0))
    wind_kmh = float(out.get("wind_kmh", 0.0))

    if wcode in (95, 96, 99):
      rain_factor = 100
      cloud_factor = max(clouds, 95)
    elif wcode in (65, 67, 82) or rain_mm >= 4.0:
      rain_factor = 75
      cloud_factor = max(clouds, 90)
    elif (
        wcode in (51, 53, 55, 56, 57, 61, 63, 66, 80, 81)
        or rain_mm > 0.0
    ):
      rain_factor = 50
      cloud_factor = max(clouds, 80)
    else:
      rain_factor = 0
      cloud_factor = clouds

    if wcode in (71, 73, 75, 77, 85, 86) or snow_cm > 0.0:
      snow_factor = 100
      cloud_factor = max(cloud_factor, 85)
    else:
      snow_factor = 0

    if wind_kmh >= 28.0:
      wind_factor = min(90, int(round(50 + (wind_kmh - 28.0) * 1.2)))
    else:
      wind_factor = int(round(wind_kmh))

    if is_night_now:
      time_of_day = 0
    else:
      time_of_day = round(10.0 + out["sun_progress"] * 80.0, 1)

    out["godot_state"] = {
        "id": "auto",
        "label": out.get("label", "實況"),
        "cloud_factor": cloud_factor,
        "rain_factor": rain_factor,
        "snow_factor": snow_factor,
        "wind_speed": wind_factor,
        "time_of_day": time_of_day,
        "is_night": is_night_now,
        "moon_phase": out["godot_moon_phase"],
        "is_crescent": out["godot_is_crescent"],
    }
    return out


weather_client = LocalWeatherClient()


def background_poller():
  while True:
    if nvs_config.is_configured():
      pw_client.poll_live_data()
    weather_client.get_weather()
    net_monitor.scan_wifi_networks(force=False)
    time.sleep(5.0)


class ESP32SimulatorHandler(http.server.BaseHTTPRequestHandler):

  def log_message(self, format, *args):
    pass

  def _send_json(self, payload, status=200):
    raw = json.dumps(payload, ensure_ascii=False).encode("utf-8")
    self.send_response(status)
    self.send_header("Content-Type", "application/json; charset=utf-8")
    self.send_header("Cache-Control", "no-store")
    self.send_header("Access-Control-Allow-Origin", "*")
    self.send_header("Content-Length", str(len(raw)))
    self.end_headers()
    self.wfile.write(raw)

  def _serve_static_file(
      self, file_path, cache_control="public, max-age=3600", allowed_root=None
  ):
    base_dir = allowed_root or ESP32_DIR
    real_base = os.path.realpath(base_dir)
    real_target = os.path.realpath(file_path)
    if not real_target.startswith(real_base + os.sep) or not os.path.isfile(
        real_target
    ):
      self.send_error(404, "Not Found")
      return

    ext = os.path.splitext(real_target)[1].lower()
    mime_map = {
        ".js": "application/javascript; charset=utf-8",
        ".mjs": "application/javascript; charset=utf-8",
        ".json": "application/json; charset=utf-8",
        ".html": "text/html; charset=utf-8",
        ".css": "text/css; charset=utf-8",
        ".glb": "model/gltf-binary",
        ".png": "image/png",
        ".jpg": "image/jpeg",
        ".jpeg": "image/jpeg",
        ".obj": "text/plain; charset=utf-8",
        ".tscn": "text/plain; charset=utf-8",
        ".tres": "text/plain; charset=utf-8",
        ".shader": "text/plain; charset=utf-8",
        ".gd": "text/plain; charset=utf-8",
        ".md": "text/plain; charset=utf-8",
    }
    ctype = (
        mime_map.get(ext)
        or mimetypes.guess_type(real_target)[0]
        or "application/octet-stream"
    )
    try:
      with open(real_target, "rb") as f:
        content = f.read()
      self.send_response(200)
      self.send_header("Content-Type", ctype)
      self.send_header("Cache-Control", cache_control)
      self.send_header("Access-Control-Allow-Origin", "*")
      self.send_header("Content-Length", str(len(content)))
      self.end_headers()
      self.wfile.write(content)
    except Exception:
      self.send_error(500, "Internal Server Error")

  def do_OPTIONS(self):
    self.send_response(204)
    self.send_header("Access-Control-Allow-Origin", "*")
    self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
    self.send_header("Access-Control-Allow-Headers", "Content-Type")
    self.end_headers()

  def do_GET(self):
    parsed_path = urllib.parse.unquote(self.path.split("?", 1)[0])

    # OS Captive Portal Detection Routes (Android, iOS/macOS CNA, Windows)
    if parsed_path in (
        "/generate_204",
        "/gen_204",
        "/hotspot-detect.html",
        "/library/test/success.html",
        "/ncsi.txt",
        "/connecttest.txt",
        "/redirect",
    ):
      self.send_response(302)
      self.send_header("Location", "/wifimanager?captive=1")
      self.end_headers()
      return

    if parsed_path.startswith("/api/live"):
      is_conf = nvs_config.is_configured()
      if is_conf:
        data = pw_client.latest_telemetry or pw_client.poll_live_data()
      else:
        data = {
            "ok": False,
            "unconfigured": True,
            "gateway_ip": pw_client.host,
            "error": "Waiting for WiFiManager first-time configuration",
        }
      weather = weather_client.get_weather()
      try:
        with open(TOU_CONFIG_PATH, "r", encoding="utf-8") as f:
          tou_cfg = json.load(f)
      except Exception:
        tou_cfg = {}
      tou_cfg["gateway_ip"] = pw_client.host
      self._send_json({
          "telemetry": data,
          "tou_config": tou_cfg,
          "weather": weather,
          "config": nvs_config.get_all(mask_password=True),
          "esp32_sys": net_monitor.get_esp32_sys_info(pw_client),
      })
      return

    if parsed_path == "/api/config":
      self._send_json({
          "ok": True,
          "config": nvs_config.get_all(mask_password=False),
          "esp32_sys": net_monitor.get_esp32_sys_info(pw_client),
      })
      return

    if parsed_path == "/api/wifi/scan":
      qs = urllib.parse.parse_qs(
          self.path.split("?", 1)[1] if "?" in self.path else ""
      )
      force = qs.get("refresh", ["0"])[0] == "1"
      nets = net_monitor.scan_wifi_networks(force=force)
      self._send_json({
          "ok": True,
          "networks": nets,
          "esp32_sys": net_monitor.get_esp32_sys_info(pw_client),
      })
      return

    if parsed_path in ("/", "/index.html"):
      self._serve_static_file(INDEX_HTML_PATH, cache_control="no-store")
      return

    if parsed_path in (
        "/wifimanager",
        "/wifimanager.html",
        "/wifi",
        "/0wifi",
        "/param",
        "/info",
        "/setup",
    ):
      self._serve_static_file(WIFIMANAGER_HTML_PATH, cache_control="no-store")
      return

    # Serve 3D Tesla Energy assets from esp32/previews and esp32/recovered
    if parsed_path.startswith("/previews/"):
      rel = parsed_path[len("/previews/") :]
      self._serve_static_file(
          os.path.join(PREVIEWS_DIR, rel),
          cache_control="no-store",
          allowed_root=ESP32_DIR,
      )
      return

    if parsed_path.startswith("/recovered/"):
      rel = parsed_path[len("/recovered/") :]
      self._serve_static_file(
          os.path.join(RECOVERED_DIR, rel),
          allowed_root=ESP32_DIR,
      )
      return

    if parsed_path.startswith("/weather_export/"):
      rel = parsed_path[len("/weather_export/") :]
      self._serve_static_file(
          os.path.join(PREVIEWS_DIR, "weather_export", rel),
          allowed_root=ESP32_DIR,
      )
      return

    self.send_error(404, "Not Found")

  def do_POST(self):
    parsed_path = urllib.parse.unquote(self.path.split("?", 1)[0])
    length = int(self.headers.get("Content-Length", "0"))
    raw_body = self.rfile.read(length).decode("utf-8") if length > 0 else ""

    if parsed_path in ("/api/config", "/wifisave", "/paramsave"):
      ctype = self.headers.get("Content-Type", "")
      params = {}
      if "application/json" in ctype:
        try:
          params = json.loads(raw_body) if raw_body else {}
        except Exception:
          params = {}
      else:
        parsed_qs = urllib.parse.parse_qs(raw_body, keep_blank_values=True)
        for k, v in parsed_qs.items():
          params[k] = v[0] if v else ""

      normalized = {}
      if "s" in params or "wifi_ssid" in params:
        normalized["wifi_ssid"] = params.get("s") or params.get("wifi_ssid", "")
      if "p" in params or "wifi_password" in params:
        normalized["wifi_password"] = params.get("p") or params.get(
            "wifi_password", ""
        )
      if "gw_ip" in params or "gateway_ip" in params:
        normalized["gateway_ip"] = params.get("gw_ip") or params.get(
            "gateway_ip", ""
        )
      if "gw_pass" in params or "gateway_password" in params:
        normalized["gateway_password"] = params.get("gw_pass") or params.get(
            "gateway_password", ""
        )
      if "gw_email" in params or "gateway_email" in params:
        normalized["gateway_email"] = params.get("gw_email") or params.get(
            "gateway_email", ""
        )
      if "site_title" in params:
        normalized["site_title"] = params.get("site_title", "")

      updated_cfg = nvs_config.save_wifimanager_submission(normalized)

      # Sync gateway_ip into tou_config.json
      try:
        if os.path.exists(TOU_CONFIG_PATH):
          with open(TOU_CONFIG_PATH, "r", encoding="utf-8") as f:
            tou_data = json.load(f)
          tou_data["gateway_ip"] = updated_cfg["gateway_ip"]
          if updated_cfg.get("site_title"):
            tou_data["site_title"] = updated_cfg["site_title"]
          with open(TOU_CONFIG_PATH, "w", encoding="utf-8") as f:
            json.dump(tou_data, f, ensure_ascii=False, indent=2)
      except Exception:
        pass

      # Reconfigure PowerwallLocalClient -> uses password to acquire Bearer Token via /api/login/Basic
      live_res = pw_client.reconfigure(
          updated_cfg["gateway_ip"],
          updated_cfg["gateway_password"],
          updated_cfg["gateway_email"],
      )
      self._send_json({
          "ok": bool(live_res and live_res.get("ok")),
          "config": nvs_config.get_all(mask_password=False),
          "telemetry": live_res,
          "esp32_sys": net_monitor.get_esp32_sys_info(pw_client),
      })
      return

    if parsed_path == "/api/wifi/reset":
      reset_cfg = nvs_config.reset_to_unconfigured()
      pw_client.auth_ok = False
      pw_client.token = ""
      self._send_json({
          "ok": True,
          "config": reset_cfg,
          "esp32_sys": net_monitor.get_esp32_sys_info(pw_client),
      })
      return

    self.send_error(404, "Not Found")


class ThreadedHTTPServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
  daemon_threads = True
  allow_reuse_address = True


if __name__ == "__main__":
  cfg = nvs_config.get_all()
  if cfg.get("configured"):
    print(
        f"[ESP32-P4 720x720] NVS Configured -> Authenticating with Powerwall Gateway {cfg['gateway_ip']} ..."
    )
    pw_client.poll_live_data()
  else:
    print(
        f"[ESP32-P4 720x720] First Boot (Unconfigured) -> SoftAP '{cfg.get('ap_ssid')}' (192.168.4.1) Captive Portal Active!"
    )
  net_monitor.scan_wifi_networks(force=True)
  t = threading.Thread(target=background_poller, daemon=True)
  t.start()
  server = ThreadedHTTPServer(("0.0.0.0", PORT), ESP32SimulatorHandler)
  print(
      f"📟 ESP32-P4 (720x720) Display Monitor running at http://localhost:{PORT}/"
  )
  print(
      f"📶 tzapu/WiFiManager Captive Portal & Config at http://localhost:{PORT}/wifimanager"
  )
  server.serve_forever()

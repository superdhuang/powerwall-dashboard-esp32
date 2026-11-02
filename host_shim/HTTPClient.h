#pragma once
#include "Arduino.h"
#include "WiFiClientSecure.h"
#include <curl/curl.h>
#include <vector>
#include <map>
#include <algorithm>

class HTTPClient {
public:
  void setConnectTimeout(int32_t ms) { connect_timeout_ms_ = ms; }
  void setTimeout(uint16_t ms) { read_timeout_ms_ = ms; }

  bool begin(WiFiClientSecure& /*client*/, const String& url) {
    url_ = url;
    req_headers_.clear();
    resp_headers_.clear();
    resp_body_.clear();
    return !url_.empty();
  }

  void addHeader(const String& name, const String& value) {
    req_headers_.push_back(name + ": " + value);
  }

  void collectHeaders(const char* /*headerKeys*/[], const size_t /*headerKeysCount*/) {}

  int GET() {
    return perform("GET", nullptr);
  }

  int POST(const String& payload) {
    return perform("POST", payload.c_str());
  }

  String getString() const {
    return String(resp_body_);
  }

  String header(const char* name) const {
    if (!name) return String("");
    std::string target = toLower(name);
    auto it = resp_headers_.find(target);
    if (it != resp_headers_.end()) return String(it->second);
    return String("");
  }

  void end() {
    req_headers_.clear();
  }

private:
  String url_;
  int32_t connect_timeout_ms_ = 3000;
  uint16_t read_timeout_ms_ = 4000;
  std::vector<std::string> req_headers_;
  std::map<std::string, std::string> resp_headers_;
  std::string resp_body_;

  static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
  }

  static size_t writeBodyCb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
  }

  static size_t writeHeaderCb(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t total = size * nitems;
    auto* map_ptr = static_cast<std::map<std::string, std::string>*>(userdata);
    std::string line(buffer, total);
    auto colon = line.find(':');
    if (colon != std::string::npos) {
      std::string key = toLower(line.substr(0, colon));
      std::string val = line.substr(colon + 1);
      while (!val.empty() && (val.front() == ' ' || val.front() == '\t')) val.erase(val.begin());
      while (!val.empty() && (val.back() == '\r' || val.back() == '\n')) val.pop_back();
      (*map_ptr)[key] = val;
    }
    return total;
  }

  int perform(const char* method, const char* body) {
    CURL* curl = curl_easy_init();
    if (!curl) return -1;

    resp_body_.clear();
    resp_headers_.clear();

    struct curl_slist* chunk = nullptr;
    for (const auto& h : req_headers_) {
      chunk = curl_slist_append(chunk, h.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url_.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(connect_timeout_ms_));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(read_timeout_ms_));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &HTTPClient::writeBodyCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp_body_);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &HTTPClient::writeHeaderCb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &resp_headers_);

    if (chunk) {
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    }
    if (std::strcmp(method, "POST") == 0) {
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body ? body : "");
    }

    CURLcode res = curl_easy_perform(curl);
    long http_code = -1;
    if (res == CURLE_OK) {
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    }

    if (chunk) curl_slist_free_all(chunk);
    curl_easy_cleanup(curl);
    return static_cast<int>(http_code);
  }
};

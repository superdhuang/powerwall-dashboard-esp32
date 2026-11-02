#pragma once
#include "Arduino.h"
#include <map>

class Preferences {
public:
  bool begin(const char* ns, bool /*readOnly*/ = false) {
    ns_ = ns ? ns : "default";
    return true;
  }
  void end() {}
  String getString(const char* key, const char* defaultVal = "") {
    auto it = store()[ns_].find(key ? key : "");
    if (it == store()[ns_].end()) return String(defaultVal);
    return String(it->second);
  }
  size_t putString(const char* key, const char* val) {
    if (!key) return 0;
    store()[ns_][key] = val ? val : "";
    return std::strlen(val ? val : "");
  }
  size_t putString(const char* key, const String& val) {
    return putString(key, val.c_str());
  }

private:
  std::string ns_;
  static std::map<std::string, std::map<std::string, std::string>>& store() {
    static std::map<std::string, std::map<std::string, std::string>> s;
    return s;
  }
};

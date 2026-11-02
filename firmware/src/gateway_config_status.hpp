#pragma once

#include <cstring>

inline bool hasStoredGatewayConfig(bool ipKeyPresent, bool passwordKeyPresent,
                                   const char* ip, const char* password) {
  return ipKeyPresent && passwordKeyPresent && ip && ip[0] && password && password[0];
}

#pragma once

#include <string>

struct SimulatorGatewayCliConfig {
  bool has_ip = false;
  bool has_password = false;
  std::string ip;
  std::string password;

  bool provided() const { return has_ip && has_password; }
};

enum class SimulatorGatewayCliParseResult {
  None,
  Valid,
  MissingValue,
  IncompletePair,
  EmptyValue,
};

inline SimulatorGatewayCliParseResult parseSimulatorGatewayCliConfig(
    int argc, const char* const argv[], SimulatorGatewayCliConfig& config) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i] ? argv[i] : "";
    const bool ipOption = arg == "--gateway-ip" || arg.rfind("--gateway-ip=", 0) == 0;
    const bool passwordOption = arg == "--gateway-password" || arg.rfind("--gateway-password=", 0) == 0;
    if (!ipOption && !passwordOption) continue;

    const std::string prefix = ipOption ? "--gateway-ip" : "--gateway-password";
    std::string value;
    if (arg.size() > prefix.size() && arg[prefix.size()] == '=') {
      value = arg.substr(prefix.size() + 1);
    } else {
      if (i + 1 >= argc || !argv[i + 1] ||
          std::string(argv[i + 1]).rfind("--", 0) == 0)
        return SimulatorGatewayCliParseResult::MissingValue;
      value = argv[++i];
    }

    if (ipOption) {
      config.has_ip = true;
      config.ip = value;
    } else {
      config.has_password = true;
      config.password = value;
    }
  }

  if (!config.has_ip && !config.has_password) return SimulatorGatewayCliParseResult::None;
  if (!config.has_ip || !config.has_password) return SimulatorGatewayCliParseResult::IncompletePair;
  if (config.ip.empty() || config.password.empty()) return SimulatorGatewayCliParseResult::EmptyValue;
  return SimulatorGatewayCliParseResult::Valid;
}

#include "simulator_gateway_cli.hpp"

#include <cassert>

int main() {
  {
    const char* argv[] = {"simulator"};
    SimulatorGatewayCliConfig config;
    assert(parseSimulatorGatewayCliConfig(1, argv, config) == SimulatorGatewayCliParseResult::None);
  }
  {
    const char* argv[] = {"simulator", "--gateway-ip", "192.0.2.42",
                          "--gateway-password", "synthetic-test-value"};
    SimulatorGatewayCliConfig config;
    assert(parseSimulatorGatewayCliConfig(5, argv, config) == SimulatorGatewayCliParseResult::Valid);
    assert(config.provided() && config.ip == "192.0.2.42" &&
           config.password == "synthetic-test-value");
  }
  {
    const char* argv[] = {"simulator", "--gateway-ip=192.0.2.42",
                          "--gateway-password=synthetic-test-value"};
    SimulatorGatewayCliConfig config;
    assert(parseSimulatorGatewayCliConfig(3, argv, config) == SimulatorGatewayCliParseResult::Valid);
    assert(config.provided());
  }
  {
    const char* argv[] = {"simulator", "--gateway-ip", "192.0.2.42"};
    SimulatorGatewayCliConfig config;
    assert(parseSimulatorGatewayCliConfig(3, argv, config) == SimulatorGatewayCliParseResult::IncompletePair);
  }
  {
    const char* argv[] = {"simulator", "--gateway-password", "synthetic-test-value"};
    SimulatorGatewayCliConfig config;
    assert(parseSimulatorGatewayCliConfig(3, argv, config) == SimulatorGatewayCliParseResult::IncompletePair);
  }
  {
    const char* argv[] = {"simulator", "--gateway-ip"};
    SimulatorGatewayCliConfig config;
    assert(parseSimulatorGatewayCliConfig(2, argv, config) == SimulatorGatewayCliParseResult::MissingValue);
  }
  {
    const char* argv[] = {"simulator", "--gateway-ip", "--gateway-password", "synthetic-test-value"};
    SimulatorGatewayCliConfig config;
    assert(parseSimulatorGatewayCliConfig(4, argv, config) == SimulatorGatewayCliParseResult::MissingValue);
  }
  {
    const char* argv[] = {"simulator", "--gateway-ip=192.0.2.42", "--gateway-password="};
    SimulatorGatewayCliConfig config;
    assert(parseSimulatorGatewayCliConfig(3, argv, config) == SimulatorGatewayCliParseResult::EmptyValue);
  }
}

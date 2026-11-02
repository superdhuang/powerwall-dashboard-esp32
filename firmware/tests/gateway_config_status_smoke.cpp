#include "gateway_config_status.hpp"

#include <cassert>

int main() {
  // Built-in defaults must not count as a saved configuration without both keys.
  assert(!hasStoredGatewayConfig(false, false, "192.0.2.14", ""));
  assert(!hasStoredGatewayConfig(true, false, "192.0.2.14", ""));
  assert(!hasStoredGatewayConfig(false, true, "192.0.2.14", ""));

  // Existing installations remain configured when both legacy NVS keys exist.
  assert(hasStoredGatewayConfig(true, true, "192.0.2.14", "test-password"));
  assert(hasStoredGatewayConfig(true, true, "192.0.2.12", "test-password"));

  // Saved-but-incomplete fields keep the setup prompt visible.
  assert(!hasStoredGatewayConfig(true, true, "", "test-password"));
  assert(!hasStoredGatewayConfig(true, true, "192.0.2.12", ""));
  assert(!hasStoredGatewayConfig(true, true, nullptr, "test-password"));
  assert(!hasStoredGatewayConfig(true, true, "192.0.2.12", nullptr));
}

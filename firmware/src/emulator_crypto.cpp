#if defined(POWERWALL_EMULATOR_HEADLESS)

#include "sdkconfig.h"
#include "mbedtls/ecp.h"

#if !CONFIG_IDF_TARGET_ESP32P4
#error "The emulator ECC workaround is specific to ESP32-P4"
#endif

#if !defined(MBEDTLS_ECP_MUL_ALT_SOFT_FALLBACK) || !defined(MBEDTLS_ECP_VERIFY_ALT_SOFT_FALLBACK)
#error "P4 emulation requires the SDK's software ECC fallback"
#endif

// esp-emu 0.51 does not implement every ECC work mode used by this SDK.
// Select the SDK's existing software implementation in the emulator target.
// Keep normal TLS negotiation, signature/key validation, and RNG blinding.
// Physical S3/P4 builds retain the hardware accelerator and never link wrappers.
extern "C" int __wrap_ecp_mul_restartable_internal(
    mbedtls_ecp_group* group, mbedtls_ecp_point* result,
    const mbedtls_mpi* scalar, const mbedtls_ecp_point* point,
    int (*rng)(void*, unsigned char*, size_t), void* rng_context,
    mbedtls_ecp_restart_ctx* restart) {
  return ecp_mul_restartable_internal_soft(group, result, scalar, point,
                                           rng, rng_context, restart);
}

extern "C" int __wrap_mbedtls_ecp_check_pubkey(
    const mbedtls_ecp_group* group, const mbedtls_ecp_point* point) {
  return mbedtls_ecp_check_pubkey_soft(group, point);
}

#endif

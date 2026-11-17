#ifndef XP_MBEDTLS_CONFIG_H
#define XP_MBEDTLS_CONFIG_H

/*
 * Windows XP (NT 5.1) does not provide BCryptGenRandom (introduced in Vista).
 * Disable default platform entropy and supply mbedtls_hardware_poll() backed
 * by CryptoAPI CryptGenRandom() in src/platform/xp_entropy.c.
 */
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT

/* Keep TLS 1.2 & TLS 1.3 enabled; disable unused legacy server/PSA filesystem storage */
#undef MBEDTLS_PSA_ITS_FILE_C
#undef MBEDTLS_PSA_CRYPTO_STORAGE_C

/* Use MinGW-w64's conforming C99 snprintf/vsnprintf instead of _vsnprintf_s (Vista+) */
#define MBEDTLS_PLATFORM_STD_SNPRINTF  snprintf
#define MBEDTLS_PLATFORM_STD_VSNPRINTF vsnprintf

#endif /* XP_MBEDTLS_CONFIG_H */

#include <windows.h>
#include <wincrypt.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "mbedtls/entropy.h"

typedef BOOLEAN (WINAPI *RtlGenRandomFn)(PVOID RandomBuffer, ULONG RandomBufferLength);

/*
 * XP-compatible hardware/OS entropy source for mbedTLS 3.6 LTS.
 * Registered automatically when MBEDTLS_ENTROPY_HARDWARE_ALT is defined.
 */
int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen) {
    (void)data;
    if (olen == NULL || (output == NULL && len > 0)) {
        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    }
    *olen = 0;
    if (len == 0) {
        return 0;
    }

    HCRYPTPROV hProv = 0;
    if (CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        BOOL ok = CryptGenRandom(hProv, (DWORD)len, output);
        CryptReleaseContext(hProv, 0);
        if (ok) {
            *olen = len;
            return 0;
        }
    }

    HMODULE hAdvapi = GetModuleHandleA("advapi32.dll");
    if (!hAdvapi) {
        hAdvapi = LoadLibraryA("advapi32.dll");
    }
    if (hAdvapi) {
        RtlGenRandomFn gen_random =
            (RtlGenRandomFn)(void*)GetProcAddress(hAdvapi, "SystemFunction036");
        if (gen_random && gen_random(output, (ULONG)len)) {
            *olen = len;
            return 0;
        }
    }

    return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
}

/*
 * Windows XP SP3 msvcrt.dll does not export _vsnprintf_s / vsnprintf_s (Vista+).
 * mbedtls/library/platform.c compiles mbedtls_platform_win32_vsnprintf referencing
 * vsnprintf_s; provide an XP-safe shim resolved before libmsvcrt-os.a.
 */
int __cdecl vsnprintf_s(char *dst, size_t dst_size, size_t max_count, const char *fmt, va_list arg) {
    (void)max_count;
    if (!dst || dst_size == 0 || !fmt) return -1;
    int ret = vsnprintf(dst, dst_size, fmt, arg);
    if (ret < 0 || (size_t)ret >= dst_size) {
        dst[dst_size - 1] = '\0';
        return -1;
    }
    return ret;
}

int __cdecl _vsnprintf_s(char *dst, size_t dst_size, size_t max_count, const char *fmt, va_list arg) {
    return vsnprintf_s(dst, dst_size, max_count, fmt, arg);
}

#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLCHAIN="${ROOT_DIR}/cmake/toolchain-mingw-xp.cmake"
DEPS_BUILD="${ROOT_DIR}/build-deps"
DEPS_INSTALL="${DEPS_BUILD}/install"

mkdir -p "${ROOT_DIR}/third_party" "${ROOT_DIR}/resources"

if [ ! -d "${ROOT_DIR}/third_party/mbedtls/.git" ]; then
  echo "[0/2] Cloning mbedTLS 3.6 LTS..."
  git clone --depth 1 --branch mbedtls-3.6 --recurse-submodules --shallow-submodules \
    https://github.com/Mbed-TLS/mbedtls.git "${ROOT_DIR}/third_party/mbedtls"
fi

if [ ! -d "${ROOT_DIR}/third_party/curl/.git" ]; then
  echo "[0/2] Cloning curl 8.11.1..."
  git clone --depth 1 --branch curl-8_11_1 \
    https://github.com/curl/curl.git "${ROOT_DIR}/third_party/curl"
fi

if [ ! -f "${ROOT_DIR}/third_party/nlohmann/json.hpp" ]; then
  echo "[0/2] Downloading nlohmann/json.hpp..."
  mkdir -p "${ROOT_DIR}/third_party/nlohmann"
  curl -fsSL -o "${ROOT_DIR}/third_party/nlohmann/json.hpp" \
    https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
fi

if [ ! -f "${ROOT_DIR}/resources/cacert.pem" ]; then
  echo "[0/2] Downloading Mozilla cacert.pem..."
  curl -fsSL -o "${ROOT_DIR}/resources/cacert.pem" https://curl.se/ca/cacert.pem
fi

CRTDLL_FLAG=""
if i686-w64-mingw32-gcc -mcrtdll=msvcrt-os -E -x c /dev/null >/dev/null 2>&1; then
  CRTDLL_FLAG="-mcrtdll=msvcrt-os"
fi

XP_FLAGS="${CRTDLL_FLAG} -D_WIN32_WINNT=0x0501 -DWINVER=0x0501 -march=i686 -mtune=generic -ffunction-sections -fdata-sections -O2 -DNDEBUG -I${ROOT_DIR}/cmake -DMBEDTLS_USER_CONFIG_FILE=\\\"xp_mbedtls_config.h\\\""

echo "[1/2] Building mbedTLS 3.6 LTS for Windows XP..."
cmake -S "${ROOT_DIR}/third_party/mbedtls" -B "${DEPS_BUILD}/mbedtls" \
  -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}" \
  -DCMAKE_INSTALL_PREFIX="${DEPS_INSTALL}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS="${XP_FLAGS}" \
  -DENABLE_TESTING=OFF \
  -DENABLE_PROGRAMS=OFF \
  -DUSE_SHARED_MBEDTLS_LIBRARY=OFF \
  -DUSE_STATIC_MBEDTLS_LIBRARY=ON \
  -DMBEDTLS_FATAL_WARNINGS=OFF

cmake --build "${DEPS_BUILD}/mbedtls" -j"$(nproc)"
cmake --install "${DEPS_BUILD}/mbedtls"

echo "[2/2] Building libcurl 8.x (with mbedTLS) for Windows XP..."
cmake -S "${ROOT_DIR}/third_party/curl" -B "${DEPS_BUILD}/curl" \
  -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}" \
  -DCMAKE_INSTALL_PREFIX="${DEPS_INSTALL}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS="${XP_FLAGS}" \
  -DCURL_TARGET_WINDOWS_VERSION=0x0501 \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_CURL_EXE=OFF \
  -DBUILD_TESTING=OFF \
  -DBUILD_LIBCURL_DOCS=OFF \
  -DBUILD_MISC_DOCS=OFF \
  -DENABLE_CURL_MANUAL=OFF \
  -DHTTP_ONLY=ON \
  -DCURL_USE_MBEDTLS=ON \
  -DCURL_USE_PKGCONFIG=OFF \
  -DMBEDTLS_INCLUDE_DIR="${DEPS_INSTALL}/include" \
  -DMBEDTLS_LIBRARY="${DEPS_INSTALL}/lib/libmbedtls.a" \
  -DMBEDX509_LIBRARY="${DEPS_INSTALL}/lib/libmbedx509.a" \
  -DMBEDCRYPTO_LIBRARY="${DEPS_INSTALL}/lib/libmbedcrypto.a" \
  -DCURL_USE_LIBPSL=OFF \
  -DCURL_USE_LIBSSH2=OFF \
  -DUSE_LIBIDN2=OFF \
  -DUSE_WIN32_IDN=OFF \
  -DCURL_ZLIB=OFF \
  -DCURL_BROTLI=OFF \
  -DCURL_ZSTD=OFF \
  -DUSE_NGHTTP2=OFF \
  -DCURL_WINDOWS_SSPI=OFF \
  -DENABLE_UNICODE=ON

cmake --build "${DEPS_BUILD}/curl" -j"$(nproc)"
cmake --install "${DEPS_BUILD}/curl"

echo "Done! XP static dependencies installed into ${DEPS_INSTALL}"

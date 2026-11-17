cmake_minimum_required(VERSION 3.20)
project(xpagent LANGUAGES C CXX RC)

include(CheckCCompilerFlag)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(XPAGENT_DEPS_DIR "${CMAKE_SOURCE_DIR}/build-deps/install" CACHE PATH "XP prebuilt deps prefix")

if(NOT EXISTS "${XPAGENT_DEPS_DIR}/lib/libcurl.a" OR NOT EXISTS "${XPAGENT_DEPS_DIR}/lib/libmbedtls.a")
    message(FATAL_ERROR "Missing XP dependencies in ${XPAGENT_DEPS_DIR}. Run scripts/build_deps.sh first.")
endif()

# Newer mingw-w64 toolchains (e.g. Arch Linux) default to UCRT; force legacy msvcrt.dll for XP when supported.
set(XP_CRTDLL_FLAG "")
check_c_compiler_flag("-mcrtdll=msvcrt-os" HAS_MCRTDLL_MSVCRT_OS)
if(HAS_MCRTDLL_MSVCRT_OS)
    set(XP_CRTDLL_FLAG "-mcrtdll=msvcrt-os")
endif()

# Shared XP compile definitions & flags (i686 baseline, Windows XP NT 5.1)
add_library(xp_compile_opts INTERFACE)
target_compile_definitions(xp_compile_opts INTERFACE
    _WIN32_WINNT=0x0501
    WINVER=0x0501
    UNICODE
    _UNICODE
    CURL_STATICLIB
    "MBEDTLS_USER_CONFIG_FILE=\"xp_mbedtls_config.h\""
)
target_compile_options(xp_compile_opts INTERFACE
    ${XP_CRTDLL_FLAG}
    -march=i686
    -mtune=generic
    -ffunction-sections
    -fdata-sections
    -Wall
    -Wextra
    -Wno-unused-parameter
)
target_include_directories(xp_compile_opts INTERFACE
    "${CMAKE_SOURCE_DIR}/cmake/xp_compat_include"
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/resources"
    "${CMAKE_SOURCE_DIR}/cmake"
    "${CMAKE_SOURCE_DIR}/third_party"
    "${XPAGENT_DEPS_DIR}/include"
)

add_library(xp_entropy STATIC
    src/platform/xp_entropy.c
)
target_link_libraries(xp_entropy PUBLIC xp_compile_opts advapi32)

# Core non-UI library (shared between main app and unit tests)
add_library(xpagent_core STATIC
    src/platform/encoding.cpp
    src/net/http_client.cpp
    src/agent/config.cpp
    src/agent/llm_client.cpp
    src/agent/agent_loop.cpp
    src/tools/tool_registry.cpp
    src/tools/fs_tools.cpp
    src/tools/cmd_tool.cpp
    src/tools/sys_diag_tool.cpp
    src/ui/i18n.cpp
)
target_link_libraries(xpagent_core PUBLIC
    xp_compile_opts
    "${XPAGENT_DEPS_DIR}/lib/libcurl.a"
    "${XPAGENT_DEPS_DIR}/lib/libmbedtls.a"
    "${XPAGENT_DEPS_DIR}/lib/libmbedx509.a"
    "${XPAGENT_DEPS_DIR}/lib/libmbedcrypto.a"
    xp_entropy
    ws2_32
    crypt32
    advapi32
    shell32
)

# Main Windows XP GUI executable
add_executable(xpagent WIN32
    src/main.cpp
    src/ui/theme_helper.cpp
    src/ui/task_pane.cpp
    src/ui/chat_header.cpp
    src/ui/chat_view.cpp
    src/ui/approval_dialog.cpp
    src/ui/wizard_dialog.cpp
    src/ui/tray_icon.cpp
    src/ui/buddy_window.cpp
    src/ui/main_window.cpp
    resources/app.rc
)
target_link_libraries(xpagent PRIVATE
    xpagent_core
    comctl32
    comdlg32
    uxtheme
    msimg32
    gdiplus
    ole32
    shlwapi
)
target_link_options(xpagent PRIVATE
    ${XP_CRTDLL_FLAG}
    -static
    -static-libgcc
    -static-libstdc++
    -s
    -Wl,--gc-sections
    -Wl,-u,___ms_vsnprintf
    -Wl,--subsystem,windows:5.01
    -Wl,--major-os-version,5
    -Wl,--minor-os-version,1
)

# Console unit test executable
add_executable(xpagent_tests
    tests/unit_tests.cpp
)
target_link_libraries(xpagent_tests PRIVATE
    xpagent_core
)
target_link_options(xpagent_tests PRIVATE
    ${XP_CRTDLL_FLAG}
    -static
    -static-libgcc
    -static-libstdc++
    -s
    -Wl,--gc-sections
    -Wl,-u,___ms_vsnprintf
    -Wl,--subsystem,console:5.01
    -Wl,--major-os-version,5
    -Wl,--minor-os-version,1
)

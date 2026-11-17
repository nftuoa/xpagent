# XPAgent

[English](README.md) | [简体中文](README_zh-CN.md)

**XPAgent** is a native **Windows XP SP3 (32-bit x86, NT 5.1)** AI Desktop Agent written in pure C++17 + Win32 API. It connects directly to modern OpenAI-compatible HTTPS endpoints using **TLS 1.2 / 1.3**, renders a nostalgic Windows XP Luna + MSN Messenger 7.5-inspired UI, and runs as a single portable ~3.1 MB executable with zero external runtime dependencies.

## Features

- **Native TLS 1.2 / 1.3 on Stock Windows XP**
  - Bypasses XP's legacy `Schannel` (which maxes out at TLS 1.0) and outdated OS root certificate store.
  - Statically links **mbedTLS 3.6 LTS + libcurl 8.11.1** with an embedded **Mozilla CA bundle** (`cacert.pem` compiled as an `RCDATA` resource).
  - Replaces mbedTLS's Vista+ `BCryptGenRandom` entropy source with an XP-compatible `CryptGenRandom` / `RtlGenRandom` (`advapi32!SystemFunction036`) implementation (`src/platform/xp_entropy.c`).
- **Authentic Windows XP UI & Retro Chat Feel**
  - Embedded `Microsoft.Windows.Common-Controls v6.0.0.0` manifest with live `uxtheme.dll` adaptation (**Luna Blue**, **Olive Green**, **Silver**, and **Windows Classic**).
  - Left Explorer-style gradient **Task Pane** with collapsible action cards and policy toggles.
  - MSN Messenger 7.5-inspired **Chat Header** (Display Picture + presence dot), timestamped linear conversation stream (`RichEdit`), classic toolbar (`Insert File`, `Emoticons`, `Nudge!`, `Clear`, `Save Log` as `.rtf`/`.txt`), dynamic typing status bar, system tray balloons (`NIF_INFO`), and an optional GDI+ `WS_EX_LAYERED` floating desktop buddy.
  - **Bilingual UI (English / 简体中文)**: Automatically detects the Windows UI language and supports live switching from the **Language** menu or Setup Wizard.
- **Controlled Local Agent Tools**
  - `list_dir`, `read_file`, `write_file`: Full UTF-16 (`*W` Win32 API) support with workspace directory sandboxing.
  - `exec_cmd`: Executes `cmd.exe /c` via anonymous pipes with timeout protection, OEM/CP936-to-UTF-8 transcoding, and a **mandatory modal approval dialog** (protecting XP systems without UAC).
  - `get_system_info`, `list_processes`: Read-only OS, CPU, RAM, disk, and process inspection.
  - API keys are stored encrypted with XP's native DPAPI (`CryptProtectData`) when available.

## Cross-Compiling from Linux

Requirements: `i686-w64-mingw32-gcc` / `i686-w64-mingw32-g++`, `cmake` (>= 3.20), `ninja`, `python3`, `git`, `curl`.

```bash
# 1. Fetch & build static mbedTLS 3.6 LTS + libcurl 8.11.1 targeting Windows XP (NT 5.1, i686)
./scripts/build_deps.sh

# 2. Configure & build XPAgent + unit tests
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw-xp.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 3. Audit PE subsystem version (5.01) & verify zero Vista+/UCRT imports
python3 scripts/check_xp_imports.py build/xpagent.exe
python3 scripts/check_xp_imports.py build/xpagent_tests.exe

# 4. Run unit tests under Wine
WINEDEBUG=-all wine build/xpagent_tests.exe
```

Copy `build/xpagent.exe` directly to a Windows XP SP3 machine and launch—no VC++ redistributable, .NET Framework, or root certificate updates required.

## Project Structure

```text
├── cmake/
│   ├── toolchain-mingw-xp.cmake      # i686-w64-mingw32 cross toolchain
│   ├── xp_mbedtls_config.h           # Disables BCrypt platform entropy & _vsnprintf_s
│   └── xp_compat_include/            # libstdc++ UCRT guard for msvcrt-os builds
├── resources/
│   ├── app.manifest                  # ComCtl32 v6 visual styles manifest
│   ├── app.rc / resource.h           # Embeds icon, manifest, and cacert.pem
│   └── cacert.pem                    # Mozilla CA root bundle
├── scripts/
│   ├── build_deps.sh                 # Builds static mbedTLS & libcurl for XP
│   ├── check_xp_imports.py           # PE subsystem & Vista+ import auditor
│   └── gen_icon.py                   # Generates multi-size XP RGBA app.ico
├── src/
│   ├── agent/                        # Config (DPAPI), SSE LLM client, ReAct loop
│   ├── net/                          # libcurl + in-memory CA blob wrapper
│   ├── platform/                     # XP entropy/CRT shims, UTF-8/16/OEM encoding
│   ├── tools/                        # Sandboxed FS, approved cmd.exe, diagnostics
│   └── ui/                           # Win32 Luna TaskPane, ChatHeader, RichEdit, Wizard, Buddy, i18n
└── tests/
    └── unit_tests.cpp                # Encoding, sandbox, SSE, tools & entropy tests
```

## Third-Party Licenses

- [mbedTLS](https://github.com/Mbed-TLS/mbedtls): Apache-2.0 OR GPL-2.0-or-later
- [curl](https://curl.se/): curl license (MIT-like)
- [nlohmann/json](https://github.com/nlohmann/json): MIT License
- [Mozilla CA Certificate Bundle](https://curl.se/docs/caextract.html): MPL-2.0

## License

This project is licensed under the [MIT License](LICENSE).

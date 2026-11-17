# XPAgent

[English](README.md) | [简体中文](README_zh-CN.md)

**XPAgent** 是一款原生运行于 **Windows XP SP3（32 位 x86, NT 5.1）** 的 AI 桌面助手，基于纯 C++17 + Win32 API 开发。程序不依赖 Visual Studio、.NET、Python 或现代浏览器内核，内置 **TLS 1.2 / 1.3** 直连现代 OpenAI 兼容大模型 API，界面融合 Windows XP Luna 视觉样式与 MSN Messenger 7.5 复古聊天风格，编译产物为单文件绿色版（约 3.1 MB）。

## 核心特性

- **Windows XP 原生直连 TLS 1.2 / 1.3**
  - 绕过 XP 原生最高仅支持 TLS 1.0 的 `Schannel` 与过期系统根证书库。
  - 静态链接 **mbedTLS 3.6 LTS + libcurl 8.11.1**，并将 **Mozilla CA 根证书包**（`cacert.pem`）直接编译进资源段（`RCDATA`）。
  - 通过 `MBEDTLS_ENTROPY_HARDWARE_ALT` 用 XP 原生 `CryptGenRandom` / `RtlGenRandom` 替代 Vista+ 的 `BCryptGenRandom`（见 [xp_entropy.c](src/platform/xp_entropy.c)）。
- **地道 Windows XP Luna 与复古聊天体验**
  - 嵌入 `Microsoft.Windows.Common-Controls v6.0.0.0` 清单，自动跟随系统主题（**Luna 蓝 / 橄榄绿 / 银灰 / Windows 经典**）并响应 `WM_THEMECHANGED`。
  - 左侧仿 XP 资源管理器渐变折叠 **Task Pane（任务窗格）**。
  - 仿 MSN Messenger 7.5 顶部渐变联系人横幅（Display Picture 头像 + 在线状态灯）、带精确时间戳的线性消息流（`RichEdit`）、功能工具栏（插入文件、颜文字/常用语、窗口抖动 `Nudge!`、清屏、导出 `.rtf`/`.txt` 记录）、动态“正在输入...”状态栏、托盘气泡（`NIF_INFO`）及可选 GDI+ `WS_EX_LAYERED` 浮动小助手。
  - **中英双语界面**：默认自动跟随系统语言，并可在菜单栏 **语言/Lang** 或设置向导中实时切换。
- **受控本地 Agent 工具集**
  - `list_dir` / `read_file` / `write_file`：原生 UTF-16（`*W`）路径支持，严格限制写入在工作区沙箱目录内。
  - `exec_cmd`：通过匿名管道执行 `cmd.exe /c`，自动将中文 XP 的 OEM/CP936 输出转为 UTF-8，且**强制弹出模态审批窗口**（针对无 UAC 的 XP 环境提供高危拦截）。
  - `get_system_info` / `list_processes`：只读查询系统版本、CPU、内存、磁盘与运行进程。
  - API Key 优先使用 XP 原生 DPAPI（`CryptProtectData`）加密保存。

## 从 Linux 交叉编译

前置依赖：`i686-w64-mingw32-gcc` / `i686-w64-mingw32-g++`、`cmake`（>= 3.20）、`ninja`、`python3`、`git`、`curl`。

```bash
# 1. 拉取并静态编译面向 Windows XP (NT 5.1, i686) 的 mbedTLS 3.6 + libcurl
./scripts/build_deps.sh

# 2. 配置并编译 XPAgent 主程序与单元测试
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw-xp.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 3. 审计 PE 子系统版本 (5.01) 与导入表，确保无 UCRT 或 Vista+ API 泄露
python3 scripts/check_xp_imports.py build/xpagent.exe
python3 scripts/check_xp_imports.py build/xpagent_tests.exe

# 4. 在 Wine 下运行单元测试
WINEDEBUG=-all wine build/xpagent_tests.exe
```

将生成的 `build/xpagent.exe` 直接拷贝至 Windows XP SP3 即可运行，无需安装 VC++ 运行库、.NET 或更新系统根证书。

## 开源协议

本项目基于 [MIT License](LICENSE) 开源。

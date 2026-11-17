#include "tools/tool_registry.hpp"

#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <string>

#include "platform/encoding.hpp"

namespace xpagent {

namespace {

std::string ToLowerAscii(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

} // namespace

ToolExecutionResult ToolGetSystemInfo(const nlohmann::json&, const ToolContext&) {
    std::ostringstream oss;

    OSVERSIONINFOW osvi{};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    BOOL has_ver = GetVersionExW(&osvi);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    if (has_ver) {
        oss << "OS Version: Windows NT " << osvi.dwMajorVersion << "." << osvi.dwMinorVersion
            << " (Build " << osvi.dwBuildNumber << ")";
        if (osvi.szCSDVersion[0]) {
            oss << " " << WideToUtf8(osvi.szCSDVersion);
        }
        oss << "\n";
    }

    wchar_t comp_name[MAX_COMPUTERNAME_LENGTH + 1] = {0};
    DWORD comp_len = MAX_COMPUTERNAME_LENGTH + 1;
    if (GetComputerNameW(comp_name, &comp_len)) {
        oss << "Computer Name: " << WideToUtf8(comp_name) << "\n";
    }

    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    oss << "CPU Architecture: "
        << (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL ? "x86 (32-bit)" :
            si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? "x64 (64-bit)" : "Other")
        << ", Logical Processors: " << si.dwNumberOfProcessors << "\n";

    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        auto to_mb = [](DWORDLONG b) { return b / (1024ULL * 1024ULL); };
        oss << "Memory Load: " << mem.dwMemoryLoad << "% | Physical RAM: "
            << to_mb(mem.ullAvailPhys) << " MB free / "
            << to_mb(mem.ullTotalPhys) << " MB total\n";
    }

    wchar_t drives[512] = {0};
    DWORD drv_len = GetLogicalDriveStringsW(511, drives);
    if (drv_len > 0 && drv_len < 512) {
        oss << "Drives:\n";
        const wchar_t* p = drives;
        while (*p) {
            UINT type = GetDriveTypeW(p);
            ULARGE_INTEGER free_avail{}, total_bytes{}, total_free{};
            oss << "  " << WideToUtf8(p);
            if (type == DRIVE_FIXED || type == DRIVE_REMOVABLE || type == DRIVE_REMOTE) {
                if (GetDiskFreeSpaceExW(p, &free_avail, &total_bytes, &total_free)) {
                    auto to_gb = [](ULARGE_INTEGER v) {
                        return static_cast<double>(v.QuadPart) / (1024.0 * 1024.0 * 1024.0);
                    };
                    oss << std::fixed << std::setprecision(2)
                        << " [" << to_gb(free_avail) << " GB free / "
                        << to_gb(total_bytes) << " GB total]";
                }
            } else if (type == DRIVE_CDROM) {
                oss << " [CD-ROM]";
            }
            oss << "\n";
            p += wcslen(p) + 1;
        }
    }

    return {true, oss.str()};
}

ToolExecutionResult ToolListProcesses(const nlohmann::json& args, const ToolContext&) {
    std::string filter = ToLowerAscii(args.value("filter", ""));
    int max_count = args.value("max_count", 60);
    if (max_count < 1) max_count = 1;
    if (max_count > 300) max_count = 300;

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return {false, "CreateToolhelp32Snapshot failed (error " +
                           std::to_string(GetLastError()) + ")"};
    }

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (!Process32FirstW(snap, &pe)) {
        CloseHandle(snap);
        return {false, "Process32FirstW failed"};
    }

    std::ostringstream oss;
    int shown = 0;
    bool truncated = false;
    do {
        std::string exe = WideToUtf8(pe.szExeFile);
        if (!filter.empty() && ToLowerAscii(exe).find(filter) == std::string::npos) {
            continue;
        }
        if (shown >= max_count) {
            truncated = true;
            break;
        }
        oss << "PID " << pe.th32ProcessID
            << " (PPID " << pe.th32ParentProcessID
            << ", Threads " << pe.cntThreads << "): " << exe << "\n";
        ++shown;
    } while (Process32NextW(snap, &pe));

    CloseHandle(snap);
    if (shown == 0) oss << "(no matching processes)\n";
    if (truncated) oss << "... (truncated at " << max_count << " processes)\n";
    return {true, oss.str()};
}

} // namespace xpagent

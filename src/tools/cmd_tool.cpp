#include "tools/tool_registry.hpp"

#include <windows.h>

#include <string>

#include "platform/encoding.hpp"
#include "ui/i18n.hpp"

namespace xpagent {

namespace {

bool IsValidUtf8(const std::string& s) {
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        size_t extra = 0;
        if (c <= 0x7F) {
            ++i;
            continue;
        } else if ((c & 0xE0) == 0xC0) {
            if (c < 0xC2) return false;
            extra = 1;
        } else if ((c & 0xF0) == 0xE0) {
            extra = 2;
        } else if ((c & 0xF8) == 0xF0) {
            if (c > 0xF4) return false;
            extra = 3;
        } else {
            return false;
        }
        if (i + extra >= s.size()) return false;
        for (size_t j = 1; j <= extra; ++j) {
            if ((static_cast<unsigned char>(s[i + j]) & 0xC0) != 0x80) return false;
        }
        i += extra + 1;
    }
    return true;
}

std::string ConsoleBytesToUtf8(const std::string& raw) {
    if (raw.empty()) return {};
    // Pure ASCII or valid UTF-8 (e.g., chcp 65001) passes directly
    bool has_high_bit = false;
    for (unsigned char c : raw) {
        if (c >= 0x80) {
            has_high_bit = true;
            break;
        }
    }
    if (!has_high_bit || IsValidUtf8(raw)) {
        return raw;
    }
    UINT oem_cp = GetOEMCP();
    if (oem_cp == 0) oem_cp = GetACP();
    return CodePageToUtf8(raw, oem_cp);
}

} // namespace

ToolExecutionResult ToolExecCmd(const nlohmann::json& args, const ToolContext& ctx) {
    if (!ctx.allow_cmd_exec) {
        return {false, "Command execution is disabled in settings."};
    }
    if (!args.contains("command") || !args["command"].is_string()) {
        return {false, "Missing required argument: command"};
    }

    std::string command = args["command"].get<std::string>();
    std::string req_dir = args.value("working_dir", "");
    int timeout_sec = args.value("timeout_sec", 30);
    if (timeout_sec < 1) timeout_sec = 1;
    if (timeout_sec > 120) timeout_sec = 120;

    std::wstring work_dir = ResolvePathInWorkspace(ctx.workspace_dir, Utf8ToWide(req_dir));

    if (ctx.require_cmd_approval) {
        if (!ctx.request_approval) {
            return {false, "Approval handler unavailable; command execution blocked."};
        }
        std::string detail = std::string(Tr().tool_cmd_workdir) + WideToUtf8(work_dir) +
                             Tr().tool_cmd_command + command;
        if (!ctx.request_approval(Tr().tool_cmd_title, detail)) {
            return {false, "User denied command execution."};
        }
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE read_pipe = nullptr;
    HANDLE write_pipe = nullptr;
    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
        return {false, "CreatePipe failed"};
    }
    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = write_pipe;
    si.hStdError = write_pipe;
    si.hStdInput = nullptr;

    std::wstring cmdline = L"cmd.exe /c " + Utf8ToWide(command);
    PROCESS_INFORMATION pi{};

    BOOL started = CreateProcessW(
        nullptr,
        &cmdline[0],
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        work_dir.empty() ? nullptr : work_dir.c_str(),
        &si,
        &pi);

    CloseHandle(write_pipe);

    if (!started) {
        DWORD err = GetLastError();
        CloseHandle(read_pipe);
        return {false, "CreateProcessW failed (error " + std::to_string(err) + ")"};
    }

    std::string raw_output;
    const size_t kMaxOutput = 32 * 1024;
    bool truncated = false;
    bool timed_out = false;
    bool cancelled = false;
    DWORD start_tick = GetTickCount();
    DWORD timeout_ms = static_cast<DWORD>(timeout_sec) * 1000;

    char buf[4096];
    for (;;) {
        DWORD avail = 0;
        while (PeekNamedPipe(read_pipe, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
            DWORD to_read = avail < sizeof(buf) ? avail : sizeof(buf);
            DWORD got = 0;
            if (!ReadFile(read_pipe, buf, to_read, &got, nullptr) || got == 0) {
                break;
            }
            if (raw_output.size() < kMaxOutput) {
                size_t keep = kMaxOutput - raw_output.size();
                if (got <= keep) {
                    raw_output.append(buf, got);
                } else {
                    raw_output.append(buf, keep);
                    truncated = true;
                }
            } else {
                truncated = true;
            }
        }

        DWORD wait_res = WaitForSingleObject(pi.hProcess, 50);
        if (wait_res == WAIT_OBJECT_0) {
            // Drain remaining pipe data
            while (PeekNamedPipe(read_pipe, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
                DWORD to_read = avail < sizeof(buf) ? avail : sizeof(buf);
                DWORD got = 0;
                if (!ReadFile(read_pipe, buf, to_read, &got, nullptr) || got == 0) break;
                if (raw_output.size() < kMaxOutput) {
                    size_t keep = kMaxOutput - raw_output.size();
                    raw_output.append(buf, got < keep ? got : keep);
                    if (got > keep) truncated = true;
                }
            }
            break;
        }

        if (ctx.is_cancelled && ctx.is_cancelled()) {
            cancelled = true;
            TerminateProcess(pi.hProcess, 1);
            break;
        }
        if (GetTickCount() - start_tick >= timeout_ms) {
            timed_out = true;
            TerminateProcess(pi.hProcess, 1);
            break;
        }
    }

    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(read_pipe);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    std::string utf8_out = ConsoleBytesToUtf8(raw_output);
    std::string result = "ExitCode: " + std::to_string(exit_code);
    if (timed_out) result += " (TIMED OUT)";
    if (cancelled) result += " (CANCELLED)";
    result += "\n";
    if (!utf8_out.empty()) {
        result += utf8_out;
        if (utf8_out.back() != '\n') result.push_back('\n');
    } else {
        result += "(no output)\n";
    }
    if (truncated) {
        result += "... (output truncated at 32 KB)\n";
    }

    return {!timed_out && !cancelled, result};
}

} // namespace xpagent

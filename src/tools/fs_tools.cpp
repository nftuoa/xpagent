#include "tools/tool_registry.hpp"

#include <windows.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "platform/encoding.hpp"
#include "ui/i18n.hpp"

namespace xpagent {

namespace {

std::wstring NormalizeSlashesAndTrim(std::wstring p) {
    for (wchar_t& ch : p) {
        if (ch == L'/') ch = L'\\';
    }
    while (p.size() > 3 && p.back() == L'\\') {
        p.pop_back();
    }
    return p;
}

std::wstring CanonicalizePath(const std::wstring& path) {
    if (path.empty()) return {};
    wchar_t buf[MAX_PATH * 2] = {0};
    DWORD len = GetFullPathNameW(path.c_str(), static_cast<DWORD>(sizeof(buf) / sizeof(wchar_t)),
                                 buf, nullptr);
    if (len == 0 || len >= sizeof(buf) / sizeof(wchar_t)) {
        return NormalizeSlashesAndTrim(path);
    }
    return NormalizeSlashesAndTrim(std::wstring(buf, len));
}

bool IsAbsoluteWindowsPath(const std::wstring& p) {
    if (p.size() >= 2 && p[1] == L':') return true;
    if (p.size() >= 2 && ((p[0] == L'\\' && p[1] == L'\\') || (p[0] == L'/' && p[1] == L'/')))
        return true;
    if (!p.empty() && (p[0] == L'\\' || p[0] == L'/')) return true;
    return false;
}

bool EnsureDirectoryRecursive(const std::wstring& dir) {
    if (dir.empty()) return false;
    DWORD attr = GetFileAttributesW(dir.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return true;
    }
    size_t pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos && pos > 2) {
        EnsureDirectoryRecursive(dir.substr(0, pos));
    }
    return CreateDirectoryW(dir.c_str(), nullptr) != FALSE ||
           GetLastError() == ERROR_ALREADY_EXISTS;
}

} // namespace

std::wstring ResolvePathInWorkspace(const std::wstring& workspace_dir, const std::wstring& input_path) {
    std::wstring base = CanonicalizePath(workspace_dir);
    if (input_path.empty() || input_path == L".") {
        return base;
    }
    std::wstring clean = input_path;
    for (wchar_t& ch : clean) {
        if (ch == L'/') ch = L'\\';
    }
    if (IsAbsoluteWindowsPath(clean)) {
        return CanonicalizePath(clean);
    }
    if (!base.empty() && base.back() != L'\\') {
        base.push_back(L'\\');
    }
    return CanonicalizePath(base + clean);
}

bool IsPathInsideWorkspace(const std::wstring& workspace_dir, const std::wstring& target_path) {
    std::wstring base = CanonicalizePath(workspace_dir);
    std::wstring target = CanonicalizePath(target_path);
    if (base.empty() || target.empty()) return false;

    if (target.size() < base.size()) return false;
    if (_wcsnicmp(target.c_str(), base.c_str(), base.size()) != 0) {
        return false;
    }
    if (target.size() == base.size()) return true;
    return base.back() == L'\\' || target[base.size()] == L'\\';
}

ToolExecutionResult ToolListDir(const nlohmann::json& args, const ToolContext& ctx) {
    if (!ctx.allow_file_read) {
        return {false, "File read permission is disabled in settings."};
    }

    std::string req_path = args.value("path", "");
    int max_entries = args.value("max_entries", 100);
    if (max_entries < 1) max_entries = 1;
    if (max_entries > 500) max_entries = 500;

    std::wstring dir = ResolvePathInWorkspace(ctx.workspace_dir, Utf8ToWide(req_path));
    std::wstring pattern = dir;
    if (!pattern.empty() && pattern.back() != L'\\') pattern.push_back(L'\\');
    pattern += L"*";

    WIN32_FIND_DATAW fd{};
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        return {false, "Cannot open directory: " + WideToUtf8(dir) +
                           " (error " + std::to_string(GetLastError()) + ")"};
    }

    std::ostringstream oss;
    oss << "Directory: " << WideToUtf8(dir) << "\n";
    int count = 0;
    bool truncated = false;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) {
            continue;
        }
        if (count >= max_entries) {
            truncated = true;
            break;
        }
        bool is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        unsigned long long sz = (static_cast<unsigned long long>(fd.nFileSizeHigh) << 32) |
                                fd.nFileSizeLow;
        oss << (is_dir ? "[DIR]  " : "[FILE] ") << WideToUtf8(fd.cFileName);
        if (!is_dir) {
            oss << " (" << sz << " bytes)";
        }
        oss << "\n";
        ++count;
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    if (count == 0) oss << "(empty)\n";
    if (truncated) oss << "... (truncated at " << max_entries << " entries)\n";
    return {true, oss.str()};
}

ToolExecutionResult ToolReadFile(const nlohmann::json& args, const ToolContext& ctx) {
    if (!ctx.allow_file_read) {
        return {false, "File read permission is disabled in settings."};
    }
    if (!args.contains("path") || !args["path"].is_string()) {
        return {false, "Missing required argument: path"};
    }

    std::wstring path = ResolvePathInWorkspace(ctx.workspace_dir, Utf8ToWide(args["path"].get<std::string>()));
    int start_line = args.value("start_line", 1);
    int end_line = args.value("end_line", 200);
    if (start_line < 1) start_line = 1;
    if (end_line < start_line) end_line = start_line + 199;

    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return {false, "Failed to open file: " + WideToUtf8(path) +
                           " (error " + std::to_string(GetLastError()) + ")"};
    }

    LARGE_INTEGER file_sz{};
    if (!GetFileSizeEx(h, &file_sz) || file_sz.QuadPart > 2LL * 1024 * 1024) {
        CloseHandle(h);
        return {false, "File too large (> 2 MB) or unreadable: " + WideToUtf8(path)};
    }

    std::string raw(static_cast<size_t>(file_sz.QuadPart), '\0');
    DWORD total_read = 0;
    while (total_read < raw.size()) {
        DWORD chunk = 0;
        if (!ReadFile(h, &raw[total_read], static_cast<DWORD>(raw.size() - total_read), &chunk, nullptr)) {
            CloseHandle(h);
            return {false, "ReadFile failed"};
        }
        if (chunk == 0) break;
        total_read += chunk;
    }
    CloseHandle(h);
    raw.resize(total_read);

    // Strip UTF-8 BOM if present
    if (raw.size() >= 3 && static_cast<unsigned char>(raw[0]) == 0xEF &&
        static_cast<unsigned char>(raw[1]) == 0xBB &&
        static_cast<unsigned char>(raw[2]) == 0xBF) {
        raw.erase(0, 3);
    }

    std::ostringstream out;
    out << "File: " << WideToUtf8(path) << "\n";
    int line_no = 1;
    size_t pos = 0;
    size_t emitted_bytes = 0;
    const size_t kMaxOutputBytes = 24 * 1024;
    bool hit_limit = false;

    while (pos <= raw.size()) {
        size_t nl = raw.find('\n', pos);
        std::string line = (nl == std::string::npos)
                               ? raw.substr(pos)
                               : raw.substr(pos, nl - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line_no >= start_line && line_no <= end_line) {
            std::string formatted = std::to_string(line_no) + ": " + line + "\n";
            if (emitted_bytes + formatted.size() > kMaxOutputBytes) {
                hit_limit = true;
                break;
            }
            out << formatted;
            emitted_bytes += formatted.size();
        }
        if (nl == std::string::npos || line_no >= end_line) break;
        pos = nl + 1;
        ++line_no;
    }

    if (hit_limit) out << "... (output truncated at 24 KB)\n";
    return {true, out.str()};
}

ToolExecutionResult ToolWriteFile(const nlohmann::json& args, const ToolContext& ctx) {
    if (!ctx.allow_file_write) {
        return {false, "File write permission is disabled in settings."};
    }
    if (!args.contains("path") || !args["path"].is_string() ||
        !args.contains("content") || !args["content"].is_string()) {
        return {false, "Missing required arguments: path, content"};
    }

    std::wstring path = ResolvePathInWorkspace(ctx.workspace_dir, Utf8ToWide(args["path"].get<std::string>()));
    if (!IsPathInsideWorkspace(ctx.workspace_dir, path)) {
        return {false, "Sandbox blocked write outside workspace: " + WideToUtf8(path)};
    }

    bool append = args.value("append", false);
    const std::string& content = args["content"].get_ref<const std::string&>();

    if (ctx.require_write_approval && ctx.request_approval) {
        std::string detail = std::string(append ? Tr().tool_write_append : Tr().tool_write_overwrite) +
                             WideToUtf8(path) + Tr().tool_write_bytes + std::to_string(content.size());
        if (!ctx.request_approval(Tr().tool_write_title, detail)) {
            return {false, "User denied file write operation."};
        }
    }

    size_t slash = path.find_last_of(L'\\');
    if (slash != std::wstring::npos) {
        EnsureDirectoryRecursive(path.substr(0, slash));
    }

    HANDLE h = CreateFileW(
        path.c_str(),
        append ? FILE_APPEND_DATA : GENERIC_WRITE,
        0,
        nullptr,
        append ? OPEN_ALWAYS : CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return {false, "Failed to open file for writing: " + WideToUtf8(path) +
                           " (error " + std::to_string(GetLastError()) + ")"};
    }

    DWORD written = 0;
    BOOL ok = WriteFile(h, content.data(), static_cast<DWORD>(content.size()), &written, nullptr);
    CloseHandle(h);
    if (!ok || written != content.size()) {
        return {false, "WriteFile failed (error " + std::to_string(GetLastError()) + ")"};
    }

    return {true, "Wrote " + std::to_string(written) + " bytes to " + WideToUtf8(path)};
}

} // namespace xpagent

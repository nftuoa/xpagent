#include "agent/config.hpp"

#include <windows.h>
#include <shlobj.h>
#include <wincrypt.h>

#include <fstream>
#include <sstream>
#include <vector>

#include <nlohmann/json.hpp>

#include "platform/encoding.hpp"
#include "ui/i18n.hpp"

namespace xpagent {

namespace {

std::string Base64Encode(const unsigned char* data, size_t len) {
    static const char kTbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        unsigned int b = static_cast<unsigned int>(data[i]) << 16;
        if (i + 1 < len) b |= static_cast<unsigned int>(data[i + 1]) << 8;
        if (i + 2 < len) b |= static_cast<unsigned int>(data[i + 2]);
        out.push_back(kTbl[(b >> 18) & 0x3F]);
        out.push_back(kTbl[(b >> 12) & 0x3F]);
        out.push_back((i + 1 < len) ? kTbl[(b >> 6) & 0x3F] : '=');
        out.push_back((i + 2 < len) ? kTbl[b & 0x3F] : '=');
    }
    return out;
}

std::vector<unsigned char> Base64Decode(const std::string& in) {
    static const int kDec[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    std::vector<unsigned char> out;
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (c == '=') break;
        int d = kDec[c];
        if (d == -1) continue;
        val = (val << 6) + d;
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<unsigned char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

std::string ProtectSecret(const std::string& plain) {
    if (plain.empty()) return {};
    DATA_BLOB in_blob{};
    in_blob.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plain.data()));
    in_blob.cbData = static_cast<DWORD>(plain.size());
    DATA_BLOB out_blob{};
    if (CryptProtectData(&in_blob, L"XPAgentApiKey", nullptr, nullptr, nullptr,
                         CRYPTPROTECT_UI_FORBIDDEN, &out_blob)) {
        std::string encoded = Base64Encode(out_blob.pbData, out_blob.cbData);
        LocalFree(out_blob.pbData);
        return encoded;
    }
    return {};
}

std::string UnprotectSecret(const std::string& b64) {
    if (b64.empty()) return {};
    std::vector<unsigned char> raw = Base64Decode(b64);
    if (raw.empty()) return {};
    DATA_BLOB in_blob{};
    in_blob.pbData = raw.data();
    in_blob.cbData = static_cast<DWORD>(raw.size());
    DATA_BLOB out_blob{};
    if (CryptUnprotectData(&in_blob, nullptr, nullptr, nullptr, nullptr,
                           CRYPTPROTECT_UI_FORBIDDEN, &out_blob)) {
        std::string plain(reinterpret_cast<const char*>(out_blob.pbData), out_blob.cbData);
        LocalFree(out_blob.pbData);
        return plain;
    }
    return {};
}

std::string ReadFileUtf8(const std::wstring& path) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return {};
    DWORD sz = GetFileSize(h, nullptr);
    std::string out;
    if (sz != INVALID_FILE_SIZE && sz > 0 && sz < 1024 * 1024) {
        out.resize(sz);
        DWORD read = 0;
        if (!ReadFile(h, &out[0], sz, &read, nullptr)) {
            out.clear();
        } else {
            out.resize(read);
        }
    }
    CloseHandle(h);
    return out;
}

bool WriteFileUtf8(const std::wstring& path, const std::string& data) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    BOOL ok = WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
    CloseHandle(h);
    return ok && written == data.size();
}

} // namespace

std::wstring GetConfigDirPath() {
    wchar_t appdata[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appdata))) {
        std::wstring dir = std::wstring(appdata) + L"\\XPAgent";
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir;
    }
    return L".";
}

std::wstring GetConfigFilePath() {
    return GetConfigDirPath() + L"\\config.json";
}

std::wstring GetDefaultWorkspaceDir() {
    wchar_t docs[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, 0, docs))) {
        std::wstring dir = std::wstring(docs) + L"\\XPAgentWorkspace";
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir;
    }
    return L"C:\\XPAgentWorkspace";
}

bool LoadAgentConfig(AgentConfig& out_cfg) {
    out_cfg.workspace_dir = GetDefaultWorkspaceDir();
    SetCurrentUiLang(ResolveUiLang(out_cfg.ui_lang));
    out_cfg.system_prompt = Tr().default_system_prompt;

    std::string raw = ReadFileUtf8(GetConfigFilePath());
    if (raw.empty()) {
        return false;
    }

    try {
        auto j = nlohmann::json::parse(raw);
        if (j.contains("ui_lang") && j["ui_lang"].is_string()) {
            out_cfg.ui_lang = j["ui_lang"].get<std::string>();
            SetCurrentUiLang(ResolveUiLang(out_cfg.ui_lang));
            out_cfg.system_prompt = Tr().default_system_prompt;
        }
        if (j.contains("base_url") && j["base_url"].is_string())
            out_cfg.base_url = j["base_url"].get<std::string>();
        if (j.contains("model") && j["model"].is_string())
            out_cfg.model = j["model"].get<std::string>();
        if (j.contains("proxy_url") && j["proxy_url"].is_string())
            out_cfg.proxy_url = j["proxy_url"].get<std::string>();
        if (j.contains("workspace_dir") && j["workspace_dir"].is_string()) {
            std::wstring ws = Utf8ToWide(j["workspace_dir"].get<std::string>());
            if (!ws.empty()) out_cfg.workspace_dir = ws;
        }
        if (j.contains("system_prompt") && j["system_prompt"].is_string()) {
            std::string sp = j["system_prompt"].get<std::string>();
            if (!sp.empty()) out_cfg.system_prompt = sp;
        }

        if (j.contains("api_key_dpapi") && j["api_key_dpapi"].is_string()) {
            out_cfg.api_key = UnprotectSecret(j["api_key_dpapi"].get<std::string>());
        }
        if (out_cfg.api_key.empty() && j.contains("api_key") && j["api_key"].is_string()) {
            out_cfg.api_key = j["api_key"].get<std::string>();
        }

        auto read_bool = [&](const char* k, bool& dst) {
            if (j.contains(k) && j[k].is_boolean()) dst = j[k].get<bool>();
        };
        read_bool("allow_file_read", out_cfg.allow_file_read);
        read_bool("allow_file_write", out_cfg.allow_file_write);
        read_bool("allow_cmd_exec", out_cfg.allow_cmd_exec);
        read_bool("require_cmd_approval", out_cfg.require_cmd_approval);
        read_bool("require_write_approval", out_cfg.require_write_approval);
        read_bool("enable_tray_balloons", out_cfg.enable_tray_balloons);
        read_bool("enable_buddy", out_cfg.enable_buddy);
        read_bool("enable_sounds", out_cfg.enable_sounds);
        read_bool("configured", out_cfg.configured);

        if (j.contains("max_agent_steps") && j["max_agent_steps"].is_number_integer()) {
            int steps = j["max_agent_steps"].get<int>();
            if (steps >= 1 && steps <= 32) out_cfg.max_agent_steps = steps;
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool SaveAgentConfig(const AgentConfig& cfg) {
    nlohmann::json j;
    j["ui_lang"] = cfg.ui_lang;
    j["base_url"] = cfg.base_url;
    j["model"] = cfg.model;
    j["proxy_url"] = cfg.proxy_url;
    j["workspace_dir"] = WideToUtf8(cfg.workspace_dir);
    j["system_prompt"] = cfg.system_prompt;

    std::string protected_key = ProtectSecret(cfg.api_key);
    if (!protected_key.empty()) {
        j["api_key_dpapi"] = protected_key;
    } else if (!cfg.api_key.empty()) {
        // Fallback if DPAPI unavailable (e.g. certain stripped Wine/PE setups)
        j["api_key"] = cfg.api_key;
    }

    j["allow_file_read"] = cfg.allow_file_read;
    j["allow_file_write"] = cfg.allow_file_write;
    j["allow_cmd_exec"] = cfg.allow_cmd_exec;
    j["require_cmd_approval"] = cfg.require_cmd_approval;
    j["require_write_approval"] = cfg.require_write_approval;
    j["enable_tray_balloons"] = cfg.enable_tray_balloons;
    j["enable_buddy"] = cfg.enable_buddy;
    j["enable_sounds"] = cfg.enable_sounds;
    j["max_agent_steps"] = cfg.max_agent_steps;
    j["configured"] = cfg.configured;

    return WriteFileUtf8(GetConfigFilePath(), j.dump(2));
}

} // namespace xpagent

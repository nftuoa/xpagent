#pragma once

#include <string>

namespace xpagent {

struct AgentConfig {
    std::string base_url = "https://api.openai.com/v1";
    std::string api_key;
    std::string model = "gpt-4o-mini";
    std::string proxy_url;
    std::wstring workspace_dir;
    std::string ui_lang = "auto"; // "auto", "en", "zh"
    std::string system_prompt;

    bool allow_file_read = true;
    bool allow_file_write = true;
    bool allow_cmd_exec = true;
    bool require_cmd_approval = true;
    bool require_write_approval = false;
    bool enable_tray_balloons = true;
    bool enable_buddy = true;
    bool enable_sounds = true;
    int max_agent_steps = 12;
    bool configured = false;
};

std::wstring GetConfigDirPath();
std::wstring GetConfigFilePath();
std::wstring GetDefaultWorkspaceDir();

bool LoadAgentConfig(AgentConfig& out_cfg);
bool SaveAgentConfig(const AgentConfig& cfg);

} // namespace xpagent

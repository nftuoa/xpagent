#pragma once

#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace xpagent {

using ApprovalPromptFn = std::function<bool(const std::string& title, const std::string& detail)>;
using ToolCancelFn = std::function<bool()>;

struct ToolContext {
    std::wstring workspace_dir;
    bool allow_file_read = true;
    bool allow_file_write = true;
    bool allow_cmd_exec = true;
    bool require_cmd_approval = true;
    bool require_write_approval = false;
    ApprovalPromptFn request_approval;
    ToolCancelFn is_cancelled;
};

struct ToolExecutionResult {
    bool ok = false;
    std::string content;
};

class ToolRegistry {
public:
    static nlohmann::json BuildOpenAIToolsSchema(const ToolContext& ctx);
    static ToolExecutionResult Execute(
        const std::string& name,
        const nlohmann::json& args,
        const ToolContext& ctx);
};

// Individual tool implementations
ToolExecutionResult ToolListDir(const nlohmann::json& args, const ToolContext& ctx);
ToolExecutionResult ToolReadFile(const nlohmann::json& args, const ToolContext& ctx);
ToolExecutionResult ToolWriteFile(const nlohmann::json& args, const ToolContext& ctx);
ToolExecutionResult ToolExecCmd(const nlohmann::json& args, const ToolContext& ctx);
ToolExecutionResult ToolGetSystemInfo(const nlohmann::json& args, const ToolContext& ctx);
ToolExecutionResult ToolListProcesses(const nlohmann::json& args, const ToolContext& ctx);

// Path helpers exposed for unit testing
std::wstring ResolvePathInWorkspace(const std::wstring& workspace_dir, const std::wstring& input_path);
bool IsPathInsideWorkspace(const std::wstring& workspace_dir, const std::wstring& target_path);

} // namespace xpagent

#include "tools/tool_registry.hpp"

namespace xpagent {

namespace {

nlohmann::json MakeToolDef(
    const char* name,
    const char* description,
    nlohmann::json properties,
    std::vector<std::string> required) {
    return {
        {"type", "function"},
        {"function", {
            {"name", name},
            {"description", description},
            {"parameters", {
                {"type", "object"},
                {"properties", std::move(properties)},
                {"required", std::move(required)}
            }}
        }}
    };
}

} // namespace

nlohmann::json ToolRegistry::BuildOpenAIToolsSchema(const ToolContext& ctx) {
    nlohmann::json arr = nlohmann::json::array();

    if (ctx.allow_file_read) {
        arr.push_back(MakeToolDef(
            "list_dir",
            "List files and subdirectories in a directory (relative to workspace or absolute path).",
            {
                {"path", {{"type", "string"}, {"description", "Directory path; empty or '.' means workspace root."}}},
                {"max_entries", {{"type", "integer"}, {"description", "Max entries to return (default 100)."}}}
            },
            {}));

        arr.push_back(MakeToolDef(
            "read_file",
            "Read text content of a file with optional 1-based line range.",
            {
                {"path", {{"type", "string"}, {"description", "File path (relative to workspace or absolute)."}}},
                {"start_line", {{"type", "integer"}, {"description", "1-based start line (default 1)."}}},
                {"end_line", {{"type", "integer"}, {"description", "1-based inclusive end line (default 200)."}}}
            },
            {"path"}));
    }

    if (ctx.allow_file_write) {
        arr.push_back(MakeToolDef(
            "write_file",
            "Write or append UTF-8 text to a file strictly inside the workspace directory.",
            {
                {"path", {{"type", "string"}, {"description", "Target file path inside workspace."}}},
                {"content", {{"type", "string"}, {"description", "Text content to write."}}},
                {"append", {{"type", "boolean"}, {"description", "Append instead of overwrite (default false)."}}}
            },
            {"path", "content"}));
    }

    if (ctx.allow_cmd_exec) {
        arr.push_back(MakeToolDef(
            "exec_cmd",
            "Execute a Windows cmd.exe command (requires user approval before execution).",
            {
                {"command", {{"type", "string"}, {"description", "Command line to run via cmd.exe /c."}}},
                {"working_dir", {{"type", "string"}, {"description", "Working directory (default workspace root)."}}},
                {"timeout_sec", {{"type", "integer"}, {"description", "Timeout in seconds (default 30, max 120)."}}}
            },
            {"command"}));
    }

    arr.push_back(MakeToolDef(
        "get_system_info",
        "Get read-only Windows XP system info: OS version, CPU, RAM, and disk drives.",
        nlohmann::json::object(),
        {}));

    arr.push_back(MakeToolDef(
        "list_processes",
        "List running Windows processes (PID and executable name).",
        {
            {"filter", {{"type", "string"}, {"description", "Optional case-insensitive substring filter."}}},
            {"max_count", {{"type", "integer"}, {"description", "Max rows to return (default 60)."}}}
        },
        {}));

    return arr;
}

ToolExecutionResult ToolRegistry::Execute(
    const std::string& name,
    const nlohmann::json& args,
    const ToolContext& ctx) {
    if (name == "list_dir") return ToolListDir(args, ctx);
    if (name == "read_file") return ToolReadFile(args, ctx);
    if (name == "write_file") return ToolWriteFile(args, ctx);
    if (name == "exec_cmd") return ToolExecCmd(args, ctx);
    if (name == "get_system_info") return ToolGetSystemInfo(args, ctx);
    if (name == "list_processes") return ToolListProcesses(args, ctx);
    return {false, "Unknown tool: " + name};
}

} // namespace xpagent

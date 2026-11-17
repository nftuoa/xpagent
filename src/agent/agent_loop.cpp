#include "agent/agent_loop.hpp"

#include <utility>

#include "agent/llm_client.hpp"
#include "tools/tool_registry.hpp"

namespace xpagent {

namespace {

struct WorkerParams {
    AgentSession* self;
    HWND hwnd;
    AgentConfig cfg;
    std::string user_text;
};

void PostHeapString(HWND hwnd, UINT msg, const std::string& text) {
    if (!hwnd || !IsWindow(hwnd)) return;
    auto* ptr = new std::string(text);
    if (!PostMessageW(hwnd, msg, 0, reinterpret_cast<LPARAM>(ptr))) {
        delete ptr;
    }
}

} // namespace

AgentSession::AgentSession(const HttpClient& http) : http_(http) {}

AgentSession::~AgentSession() {
    RequestCancel();
    WaitUntilIdle();
}

void AgentSession::ClearHistory() {
    ScopedLock lock(history_cs_);
    history_ = nlohmann::json::array();
}

bool AgentSession::IsRunning() const {
    return InterlockedCompareExchange(const_cast<LONG*>(&running_), 0, 0) != 0;
}

void AgentSession::RequestCancel() {
    InterlockedExchange(&cancel_flag_, 1);
}

void AgentSession::WaitUntilIdle() {
    if (worker_thread_) {
        WaitForSingleObject(worker_thread_, INFINITE);
        CloseHandle(worker_thread_);
        worker_thread_ = nullptr;
    }
}

bool AgentSession::StartTurn(HWND target_hwnd, const AgentConfig& cfg, const std::string& user_text) {
    if (IsRunning()) return false;
    WaitUntilIdle();

    InterlockedExchange(&cancel_flag_, 0);
    InterlockedExchange(&running_, 1);

    auto* params = new WorkerParams{this, target_hwnd, cfg, user_text};
    DWORD tid = 0;
    worker_thread_ = CreateThread(nullptr, 0, WorkerThreadProc, params, 0, &tid);
    if (!worker_thread_) {
        delete params;
        InterlockedExchange(&running_, 0);
        return false;
    }
    return true;
}

DWORD WINAPI AgentSession::WorkerThreadProc(LPVOID param) {
    auto* p = static_cast<WorkerParams*>(param);
    AgentSession* self = p->self;
    HWND hwnd = p->hwnd;
    AgentConfig cfg = std::move(p->cfg);
    std::string user_text = std::move(p->user_text);
    delete p;

    self->RunWorker(hwnd, std::move(cfg), std::move(user_text));
    InterlockedExchange(&self->running_, 0);
    return 0;
}

void AgentSession::RunWorker(HWND target_hwnd, AgentConfig cfg, std::string user_text) {
    nlohmann::json local_messages;
    {
        ScopedLock lock(history_cs_);
        if (history_.empty()) {
            history_.push_back({{"role", "system"}, {"content", cfg.system_prompt}});
        } else if (history_[0].value("role", "") == "system") {
            history_[0]["content"] = cfg.system_prompt;
        }
        history_.push_back({{"role", "user"}, {"content", user_text}});
        local_messages = history_;
    }

    auto is_cancelled = [this]() -> bool {
        return InterlockedCompareExchange(&cancel_flag_, 0, 0) != 0;
    };

    ToolContext tool_ctx;
    tool_ctx.workspace_dir = cfg.workspace_dir;
    tool_ctx.allow_file_read = cfg.allow_file_read;
    tool_ctx.allow_file_write = cfg.allow_file_write;
    tool_ctx.allow_cmd_exec = cfg.allow_cmd_exec;
    tool_ctx.require_cmd_approval = cfg.require_cmd_approval;
    tool_ctx.require_write_approval = cfg.require_write_approval;
    tool_ctx.is_cancelled = is_cancelled;
    tool_ctx.request_approval = [target_hwnd, &is_cancelled](
                                    const std::string& title, const std::string& detail) -> bool {
        if (is_cancelled() || !target_hwnd || !IsWindow(target_hwnd)) return false;
        ApprovalRequest req;
        req.title = title;
        req.detail = detail;
        // Synchronous SendMessageW keeps `req` alive until modal dialog finishes on UI thread
        SendMessageW(target_hwnd, WM_AGENT_APPROVAL_REQ, 0, reinterpret_cast<LPARAM>(&req));
        return req.approved && !is_cancelled();
    };

    LlmClient llm(http_);
    nlohmann::json tools_schema = ToolRegistry::BuildOpenAIToolsSchema(tool_ctx);

    for (int step = 0; step < cfg.max_agent_steps; ++step) {
        if (is_cancelled()) {
            PostHeapString(target_hwnd, WM_AGENT_TURN_ERROR, "任务已由用户取消。");
            return;
        }

        LlmTurnResult res = llm.ChatCompletionStream(
            cfg,
            local_messages,
            tools_schema,
            [target_hwnd](const std::string& delta) {
                PostHeapString(target_hwnd, WM_AGENT_STREAM_TOKEN, delta);
            },
            is_cancelled);

        if (res.cancelled) {
            PostHeapString(target_hwnd, WM_AGENT_TURN_ERROR, "生成已中断。");
            return;
        }
        if (!res.ok) {
            PostHeapString(target_hwnd, WM_AGENT_TURN_ERROR, res.error);
            return;
        }

        // Record assistant message
        nlohmann::json assistant_msg = {{"role", "assistant"}};
        if (!res.assistant_text.empty()) {
            assistant_msg["content"] = res.assistant_text;
        } else {
            assistant_msg["content"] = nullptr;
        }

        if (!res.tool_calls.empty()) {
            nlohmann::json tc_arr = nlohmann::json::array();
            for (size_t i = 0; i < res.tool_calls.size(); ++i) {
                const auto& tc = res.tool_calls[i];
                std::string tc_id = tc.id.empty() ? ("call_" + std::to_string(step) + "_" + std::to_string(i)) : tc.id;
                tc_arr.push_back({
                    {"id", tc_id},
                    {"type", "function"},
                    {"function", {
                        {"name", tc.name},
                        {"arguments", tc.arguments_json.empty() ? "{}" : tc.arguments_json}
                    }}
                });
            }
            assistant_msg["tool_calls"] = tc_arr;
        }

        local_messages.push_back(assistant_msg);

        if (res.tool_calls.empty()) {
            ScopedLock lock(history_cs_);
            history_ = std::move(local_messages);
            PostHeapString(target_hwnd, WM_AGENT_TURN_DONE, "");
            return;
        }

        // Execute requested tools
        for (size_t i = 0; i < res.tool_calls.size(); ++i) {
            if (is_cancelled()) {
                PostHeapString(target_hwnd, WM_AGENT_TURN_ERROR, "任务已取消。");
                return;
            }
            const auto& tc = res.tool_calls[i];
            std::string tc_id = assistant_msg["tool_calls"][i]["id"].get<std::string>();

            PostHeapString(target_hwnd, WM_AGENT_TOOL_STATUS,
                           "正在调用工具: " + tc.name + " " + tc.arguments_json);

            nlohmann::json parsed_args = nlohmann::json::object();
            try {
                if (!tc.arguments_json.empty()) {
                    parsed_args = nlohmann::json::parse(tc.arguments_json);
                }
            } catch (...) {
                parsed_args = nlohmann::json::object();
            }

            ToolExecutionResult exec_res = ToolRegistry::Execute(tc.name, parsed_args, tool_ctx);

            std::string preview = exec_res.content;
            if (preview.size() > 600) {
                preview = preview.substr(0, 600) + "\n... (已折叠更多输出)";
            }
            PostHeapString(target_hwnd, WM_AGENT_TOOL_RESULT,
                           "[" + tc.name + (exec_res.ok ? " 成功" : " 失败") + "]\n" + preview);

            local_messages.push_back({
                {"role", "tool"},
                {"tool_call_id", tc_id},
                {"content", exec_res.content}
            });
        }
    }

    ScopedLock lock(history_cs_);
    history_ = std::move(local_messages);
    PostHeapString(target_hwnd, WM_AGENT_TURN_DONE, "已达到单轮最大工具调用步数上限。");
}

} // namespace xpagent

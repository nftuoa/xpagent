#pragma once

#include <windows.h>

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "agent/config.hpp"
#include "net/http_client.hpp"
#include "platform/win_thread.hpp"

namespace xpagent {

// Custom Win32 messages posted from worker thread to main UI window
constexpr UINT WM_AGENT_STREAM_TOKEN  = WM_APP + 101; // LPARAM: std::string* (heap)
constexpr UINT WM_AGENT_TOOL_STATUS   = WM_APP + 102; // LPARAM: std::string* (heap)
constexpr UINT WM_AGENT_TOOL_RESULT   = WM_APP + 103; // LPARAM: std::string* (heap)
constexpr UINT WM_AGENT_APPROVAL_REQ  = WM_APP + 104; // LPARAM: ApprovalRequest* (stack on worker)
constexpr UINT WM_AGENT_TURN_DONE     = WM_APP + 105; // LPARAM: std::string* (optional summary)
constexpr UINT WM_AGENT_TURN_ERROR    = WM_APP + 106; // LPARAM: std::string* (error text)

struct ApprovalRequest {
    std::string title;
    std::string detail;
    bool approved = false;
};

class AgentSession {
public:
    explicit AgentSession(const HttpClient& http);
    ~AgentSession();

    AgentSession(const AgentSession&) = delete;
    AgentSession& operator=(const AgentSession&) = delete;

    void ClearHistory();
    bool IsRunning() const;

    // Starts an asynchronous turn on a Win32 worker thread.
    // Returns false if a turn is already running.
    bool StartTurn(HWND target_hwnd, const AgentConfig& cfg, const std::string& user_text);
    void RequestCancel();
    void WaitUntilIdle();

private:
    static DWORD WINAPI WorkerThreadProc(LPVOID param);
    void RunWorker(HWND target_hwnd, AgentConfig cfg, std::string user_text);

    const HttpClient& http_;
    mutable CriticalSection history_cs_;
    nlohmann::json history_ = nlohmann::json::array();

    HANDLE worker_thread_ = nullptr;
    volatile LONG running_ = 0;
    volatile LONG cancel_flag_ = 0;
};

} // namespace xpagent

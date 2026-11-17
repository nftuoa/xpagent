#pragma once

#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "agent/config.hpp"
#include "net/http_client.hpp"

namespace xpagent {

struct ToolCallSpec {
    std::string id;
    std::string name;
    std::string arguments_json;
};

struct LlmTurnResult {
    bool ok = false;
    bool cancelled = false;
    std::string assistant_text;
    std::vector<ToolCallSpec> tool_calls;
    std::string error;
};

// Reusable SSE accumulator for OpenAI-compatible streaming responses (unit-testable)
class SseChatAccumulator {
public:
    using TokenCallback = std::function<void(const std::string& delta)>;

    explicit SseChatAccumulator(TokenCallback on_token = nullptr);

    // Feed arbitrary raw HTTP chunk bytes; returns false only if aborted internally.
    void Feed(const char* data, size_t len);
    void Finish();

    const std::string& Text() const { return text_; }
    const std::vector<ToolCallSpec>& ToolCalls() const { return tool_calls_; }
    const std::string& RawBuffer() const { return raw_all_; }
    bool SawSseEvent() const { return saw_sse_; }
    bool Done() const { return done_; }

private:
    void ProcessLine(const std::string& line);
    void ProcessJsonPayload(const std::string& payload);

    TokenCallback on_token_;
    std::string line_buf_;
    std::string raw_all_;
    std::string text_;
    std::vector<ToolCallSpec> tool_calls_;
    bool saw_sse_ = false;
    bool done_ = false;
};

class LlmClient {
public:
    explicit LlmClient(const HttpClient& http);

    LlmTurnResult ChatCompletionStream(
        const AgentConfig& cfg,
        const nlohmann::json& messages,
        const nlohmann::json& tools,
        SseChatAccumulator::TokenCallback on_token,
        CancelCheckCallback cancel_check) const;

    // Quick connectivity & auth probe (GET /models or tiny non-stream chat)
    bool TestConnection(const AgentConfig& cfg, std::string& out_message) const;

private:
    const HttpClient& http_;
};

std::string NormalizeEndpointUrl(const std::string& base_url, const std::string& suffix);

} // namespace xpagent

#include "agent/llm_client.hpp"

#include <utility>

namespace xpagent {

std::string NormalizeEndpointUrl(const std::string& base_url, const std::string& suffix) {
    std::string base = base_url;
    while (!base.empty() && base.back() == '/') {
        base.pop_back();
    }
    if (suffix.empty()) return base;
    if (suffix.front() == '/') return base + suffix;
    return base + "/" + suffix;
}

SseChatAccumulator::SseChatAccumulator(TokenCallback on_token)
    : on_token_(std::move(on_token)) {}

void SseChatAccumulator::Feed(const char* data, size_t len) {
    if (!data || len == 0) return;
    if (raw_all_.size() < 64 * 1024) {
        raw_all_.append(data, len);
    }
    line_buf_.append(data, len);

    size_t start = 0;
    for (;;) {
        size_t nl = line_buf_.find('\n', start);
        if (nl == std::string::npos) break;
        std::string line = line_buf_.substr(start, nl - start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        ProcessLine(line);
        start = nl + 1;
    }
    if (start > 0) {
        line_buf_.erase(0, start);
    }
}

void SseChatAccumulator::Finish() {
    if (!line_buf_.empty()) {
        std::string tail = std::move(line_buf_);
        line_buf_.clear();
        if (!tail.empty() && tail.back() == '\r') tail.pop_back();
        ProcessLine(tail);
    }

    // Fallback: if server returned a complete non-SSE JSON object
    if (!saw_sse_ && !raw_all_.empty()) {
        try {
            auto j = nlohmann::json::parse(raw_all_);
            if (j.contains("choices") && j["choices"].is_array() && !j["choices"].empty()) {
                const auto& msg = j["choices"][0]["message"];
                if (msg.contains("content") && msg["content"].is_string()) {
                    std::string c = msg["content"].get<std::string>();
                    text_ += c;
                    if (on_token_ && !c.empty()) on_token_(c);
                }
                if (msg.contains("tool_calls") && msg["tool_calls"].is_array()) {
                    for (const auto& tc : msg["tool_calls"]) {
                        ToolCallSpec spec;
                        spec.id = tc.value("id", "");
                        if (tc.contains("function")) {
                            spec.name = tc["function"].value("name", "");
                            spec.arguments_json = tc["function"].value("arguments", "{}");
                        }
                        if (!spec.name.empty()) {
                            tool_calls_.push_back(std::move(spec));
                        }
                    }
                }
                done_ = true;
            }
        } catch (...) {
        }
    }
}

void SseChatAccumulator::ProcessLine(const std::string& line) {
    if (line.empty() || line[0] == ':') return;
    if (line.rfind("data:", 0) == 0) {
        saw_sse_ = true;
        std::string payload = line.substr(5);
        if (!payload.empty() && payload[0] == ' ') {
            payload.erase(0, 1);
        }
        if (payload == "[DONE]") {
            done_ = true;
            return;
        }
        ProcessJsonPayload(payload);
    }
}

void SseChatAccumulator::ProcessJsonPayload(const std::string& payload) {
    try {
        auto j = nlohmann::json::parse(payload);
        if (!j.contains("choices") || !j["choices"].is_array() || j["choices"].empty()) {
            return;
        }
        const auto& choice = j["choices"][0];
        if (!choice.contains("delta") || !choice["delta"].is_object()) {
            return;
        }
        const auto& delta = choice["delta"];

        if (delta.contains("content") && delta["content"].is_string()) {
            std::string chunk = delta["content"].get<std::string>();
            if (!chunk.empty()) {
                text_ += chunk;
                if (on_token_) on_token_(chunk);
            }
        }

        if (delta.contains("tool_calls") && delta["tool_calls"].is_array()) {
            for (const auto& tc : delta["tool_calls"]) {
                size_t idx = tc.value("index", static_cast<size_t>(0));
                if (idx >= tool_calls_.size()) {
                    tool_calls_.resize(idx + 1);
                }
                ToolCallSpec& dst = tool_calls_[idx];
                if (tc.contains("id") && tc["id"].is_string()) {
                    dst.id += tc["id"].get<std::string>();
                }
                if (tc.contains("function") && tc["function"].is_object()) {
                    const auto& fn = tc["function"];
                    if (fn.contains("name") && fn["name"].is_string()) {
                        dst.name += fn["name"].get<std::string>();
                    }
                    if (fn.contains("arguments") && fn["arguments"].is_string()) {
                        dst.arguments_json += fn["arguments"].get<std::string>();
                    }
                }
            }
        }
    } catch (...) {
        // Ignore malformed partial SSE line
    }
}

LlmClient::LlmClient(const HttpClient& http) : http_(http) {}

LlmTurnResult LlmClient::ChatCompletionStream(
    const AgentConfig& cfg,
    const nlohmann::json& messages,
    const nlohmann::json& tools,
    SseChatAccumulator::TokenCallback on_token,
    CancelCheckCallback cancel_check) const {

    LlmTurnResult out;
    if (cfg.base_url.empty()) {
        out.error = "API Base URL is empty";
        return out;
    }

    nlohmann::json req;
    req["model"] = cfg.model;
    req["messages"] = messages;
    req["stream"] = true;
    if (tools.is_array() && !tools.empty()) {
        req["tools"] = tools;
        req["tool_choice"] = "auto";
    }

    std::vector<std::string> headers = {
        "Content-Type: application/json",
        "Accept: text/event-stream"
    };
    if (!cfg.api_key.empty()) {
        headers.push_back("Authorization: Bearer " + cfg.api_key);
    }

    SseChatAccumulator acc(std::move(on_token));
    std::string url = NormalizeEndpointUrl(cfg.base_url, "/chat/completions");

    HttpResponse http_resp = http_.PostJsonStream(
        url,
        headers,
        req.dump(),
        cfg.proxy_url,
        20,
        [&](const char* data, size_t len) {
            acc.Feed(data, len);
            return true;
        },
        std::move(cancel_check));

    acc.Finish();

    if (http_resp.cancelled) {
        out.cancelled = true;
        out.assistant_text = acc.Text();
        out.error = "已取消生成";
        return out;
    }

    if (!http_resp.error.empty()) {
        out.error = "网络/TLS 错误: " + http_resp.error;
        return out;
    }

    if (http_resp.status_code < 200 || http_resp.status_code >= 300) {
        std::string msg = "HTTP " + std::to_string(http_resp.status_code);
        try {
            auto ej = nlohmann::json::parse(acc.RawBuffer());
            if (ej.contains("error") && ej["error"].contains("message")) {
                msg += ": " + ej["error"]["message"].get<std::string>();
            } else if (!acc.RawBuffer().empty()) {
                msg += ": " + acc.RawBuffer().substr(0, 300);
            }
        } catch (...) {
            if (!acc.RawBuffer().empty()) {
                msg += ": " + acc.RawBuffer().substr(0, 300);
            }
        }
        out.error = msg;
        return out;
    }

    out.ok = true;
    out.assistant_text = acc.Text();
    out.tool_calls = acc.ToolCalls();
    return out;
}

bool LlmClient::TestConnection(const AgentConfig& cfg, std::string& out_message) const {
    if (cfg.base_url.empty()) {
        out_message = "请填写 API Base URL";
        return false;
    }
    std::vector<std::string> headers;
    if (!cfg.api_key.empty()) {
        headers.push_back("Authorization: Bearer " + cfg.api_key);
    }
    std::string url = NormalizeEndpointUrl(cfg.base_url, "/models");
    HttpResponse resp = http_.Get(url, headers, cfg.proxy_url, 15);
    if (!resp.error.empty()) {
        out_message = "连接失败: " + resp.error;
        return false;
    }
    if (resp.status_code >= 200 && resp.status_code < 300) {
        out_message = "TLS 握手与 API 鉴权成功 (HTTP " + std::to_string(resp.status_code) + ")";
        return true;
    }
    out_message = "TLS 连接成功，但服务器返回 HTTP " + std::to_string(resp.status_code);
    if (!resp.body.empty()) {
        out_message += " (" + resp.body.substr(0, 160) + ")";
    }
    return false;
}

} // namespace xpagent

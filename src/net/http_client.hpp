#pragma once

#include <functional>
#include <string>
#include <vector>

namespace xpagent {

struct HttpResponse {
    long status_code = 0;
    std::string body;
    std::string error;
    bool cancelled = false;
};

using StreamChunkCallback = std::function<bool(const char* data, size_t len)>;
using CancelCheckCallback = std::function<bool()>;

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    // Global libcurl init/cleanup (call once on main thread)
    static void GlobalInit();
    static void GlobalCleanup();

    // Load PEM CA bundle from embedded RCDATA resource ID (0 = disable embedded lookup)
    void SetEmbeddedCaResource(int resource_id);

    HttpResponse PostJson(
        const std::string& url,
        const std::vector<std::string>& headers,
        const std::string& json_body,
        const std::string& proxy_url,
        long timeout_sec,
        CancelCheckCallback cancel_check = nullptr) const;

    HttpResponse PostJsonStream(
        const std::string& url,
        const std::vector<std::string>& headers,
        const std::string& json_body,
        const std::string& proxy_url,
        long connect_timeout_sec,
        StreamChunkCallback on_chunk,
        CancelCheckCallback cancel_check = nullptr) const;

    HttpResponse Get(
        const std::string& url,
        const std::vector<std::string>& headers,
        const std::string& proxy_url,
        long timeout_sec) const;

private:
    const void* ca_data_ = nullptr;
    size_t ca_size_ = 0;

    HttpResponse PerformRequest(
        const std::string& url,
        const std::string& method,
        const std::vector<std::string>& headers,
        const std::string& body,
        const std::string& proxy_url,
        long connect_timeout_sec,
        long total_timeout_sec,
        StreamChunkCallback on_chunk,
        CancelCheckCallback cancel_check) const;
};

} // namespace xpagent

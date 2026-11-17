#include "net/http_client.hpp"

#include <windows.h>
#include <curl/curl.h>

namespace xpagent {

namespace {

struct WriteContext {
    std::string* body_out = nullptr;
    StreamChunkCallback stream_cb;
    bool aborted = false;
};

size_t CurlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    size_t total = size * nmemb;
    auto* ctx = static_cast<WriteContext*>(userdata);
    if (!ctx) return 0;

    if (ctx->stream_cb) {
        if (!ctx->stream_cb(ptr, total)) {
            ctx->aborted = true;
            return 0; // Signals CURLE_WRITE_ERROR to stop transfer cleanly
        }
    }
    if (ctx->body_out) {
        ctx->body_out->append(ptr, total);
    }
    return total;
}

int CurlProgressCallback(void* clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    auto* cancel_cb = static_cast<CancelCheckCallback*>(clientp);
    if (cancel_cb && *cancel_cb && (*cancel_cb)()) {
        return 1; // Abort transfer
    }
    return 0;
}

} // namespace

void HttpClient::GlobalInit() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void HttpClient::GlobalCleanup() {
    curl_global_cleanup();
}

HttpClient::HttpClient() = default;
HttpClient::~HttpClient() = default;

void HttpClient::SetEmbeddedCaResource(int resource_id) {
    ca_data_ = nullptr;
    ca_size_ = 0;
    if (resource_id <= 0) return;

    HMODULE hMod = GetModuleHandleW(nullptr);
    HRSRC hRes = FindResourceW(hMod, MAKEINTRESOURCEW(resource_id), RT_RCDATA);
    if (!hRes) return;
    HGLOBAL hMem = LoadResource(hMod, hRes);
    if (!hMem) return;
    DWORD sz = SizeofResource(hMod, hRes);
    const void* ptr = LockResource(hMem);
    if (ptr && sz > 0) {
        ca_data_ = ptr;
        ca_size_ = static_cast<size_t>(sz);
    }
}

HttpResponse HttpClient::PostJson(
    const std::string& url,
    const std::vector<std::string>& headers,
    const std::string& json_body,
    const std::string& proxy_url,
    long timeout_sec,
    CancelCheckCallback cancel_check) const {
    return PerformRequest(
        url, "POST", headers, json_body, proxy_url,
        15, timeout_sec, nullptr, std::move(cancel_check));
}

HttpResponse HttpClient::PostJsonStream(
    const std::string& url,
    const std::vector<std::string>& headers,
    const std::string& json_body,
    const std::string& proxy_url,
    long connect_timeout_sec,
    StreamChunkCallback on_chunk,
    CancelCheckCallback cancel_check) const {
    return PerformRequest(
        url, "POST", headers, json_body, proxy_url,
        connect_timeout_sec, 0 /* no total timeout for long SSE streams */,
        std::move(on_chunk), std::move(cancel_check));
}

HttpResponse HttpClient::Get(
    const std::string& url,
    const std::vector<std::string>& headers,
    const std::string& proxy_url,
    long timeout_sec) const {
    return PerformRequest(
        url, "GET", headers, "", proxy_url,
        15, timeout_sec, nullptr, nullptr);
}

HttpResponse HttpClient::PerformRequest(
    const std::string& url,
    const std::string& method,
    const std::vector<std::string>& headers,
    const std::string& body,
    const std::string& proxy_url,
    long connect_timeout_sec,
    long total_timeout_sec,
    StreamChunkCallback on_chunk,
    CancelCheckCallback cancel_check) const {

    HttpResponse resp;
    CURL* curl = curl_easy_init();
    if (!curl) {
        resp.error = "Failed to initialize libcurl easy handle";
        return resp;
    }

    struct curl_slist* header_list = nullptr;
    for (const auto& h : headers) {
        header_list = curl_slist_append(header_list, h.c_str());
    }

    WriteContext write_ctx;
    if (on_chunk) {
        write_ctx.stream_cb = std::move(on_chunk);
    } else {
        write_ctx.body_out = &resp.body;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "XPAgent/1.0 (Windows NT 5.1)");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connect_timeout_sec);
    if (total_timeout_sec > 0) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, total_timeout_sec);
    }
    // Stall detection: abort if < 1 byte/sec for 60s
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);

    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    }

    if (header_list) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    }

    if (!proxy_url.empty()) {
        curl_easy_setopt(curl, CURLOPT_PROXY, proxy_url.c_str());
    }

    // TLS verification using embedded Mozilla CA bundle
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    if (ca_data_ && ca_size_ > 0) {
        struct curl_blob blob{};
        blob.data = const_cast<void*>(ca_data_);
        blob.len = ca_size_;
        blob.flags = CURL_BLOB_NOCOPY;
        curl_easy_setopt(curl, CURLOPT_CAINFO_BLOB, &blob);
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_ctx);

    if (cancel_check) {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, CurlProgressCallback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &cancel_check);
    }

    // For non-2xx responses while streaming, collect response body too if stream_cb is absent
    // Note: if stream_cb is set and HTTP status >= 400, caller inspects status_code + error chunks.
    std::string error_body;
    if (on_chunk) {
        // Keep a fallback buffer in case server returns JSON error instead of SSE
        // Note: stream_cb will receive raw bytes and can handle non-SSE lines directly.
    }

    char errbuf[CURL_ERROR_SIZE] = {0};
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status_code);

    if (res == CURLE_ABORTED_BY_CALLBACK || write_ctx.aborted) {
        resp.cancelled = true;
        resp.error = "Request cancelled by user";
    } else if (res != CURLE_OK) {
        resp.error = errbuf[0] ? std::string(errbuf) : std::string(curl_easy_strerror(res));
    }

    if (header_list) {
        curl_slist_free_all(header_list);
    }
    curl_easy_cleanup(curl);
    return resp;
}

} // namespace xpagent

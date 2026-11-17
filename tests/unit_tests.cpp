#include <windows.h>

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "agent/llm_client.hpp"
#include "net/http_client.hpp"
#include "platform/encoding.hpp"
#include "tools/tool_registry.hpp"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

extern "C" int mbedtls_hardware_poll(void* data, unsigned char* output, size_t len, size_t* olen);

namespace {

int g_failures = 0;

void Check(bool cond, const char* expr, int line) {
    if (!cond) {
        std::cerr << "[FAIL] line " << line << ": " << expr << "\n";
        ++g_failures;
    }
}

#define EXPECT_TRUE(x) Check((x), #x, __LINE__)

void TestEncodingRoundTrip() {
    std::string utf8 = "Windows XP 中文路径测试_123";
    std::wstring wide = xpagent::Utf8ToWide(utf8);
    EXPECT_TRUE(!wide.empty());
    EXPECT_TRUE(xpagent::WideToUtf8(wide) == utf8);

    std::string cp936 = xpagent::Utf8ToCodePage("测试GBK", 936);
    if (!cp936.empty()) {
        EXPECT_TRUE(xpagent::CodePageToUtf8(cp936, 936) == "测试GBK");
    }
}

void TestWorkspaceSandbox() {
    std::wstring ws = L"C:\\Documents and Settings\\User\\My Documents\\XPAgentWorkspace";
    std::wstring inside = xpagent::ResolvePathInWorkspace(ws, L"subdir\\notes.txt");
    EXPECT_TRUE(xpagent::IsPathInsideWorkspace(ws, inside));

    std::wstring traversal = xpagent::ResolvePathInWorkspace(ws, L"..\\..\\secret.txt");
    EXPECT_TRUE(!xpagent::IsPathInsideWorkspace(ws, traversal));

    std::wstring prefix_spoof = L"C:\\Documents and Settings\\User\\My Documents\\XPAgentWorkspaceEvil\\a.txt";
    EXPECT_TRUE(!xpagent::IsPathInsideWorkspace(ws, prefix_spoof));
}

void TestSseAccumulator() {
    std::string streamed;
    xpagent::SseChatAccumulator acc([&](const std::string& delta) { streamed += delta; });

    const char* chunk1 =
        "data: {\"choices\":[{\"delta\":{\"content\":\"你好\"}}]}\n"
        "data: {\"choices\":[{\"delta\":{\"conte";
    const char* chunk2 =
        "nt\":\" XP\"}}]}\r\n"
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_1\","
        "\"function\":{\"name\":\"get_system_info\",\"arguments\":\"{}\"}}]}}]}\n"
        "data: [DONE]\n";

    acc.Feed(chunk1, strlen(chunk1));
    acc.Feed(chunk2, strlen(chunk2));
    acc.Finish();

    EXPECT_TRUE(acc.Done());
    EXPECT_TRUE(acc.Text() == "你好 XP");
    EXPECT_TRUE(streamed == "你好 XP");
    EXPECT_TRUE(acc.ToolCalls().size() == 1);
    EXPECT_TRUE(acc.ToolCalls()[0].name == "get_system_info");
    EXPECT_TRUE(acc.ToolCalls()[0].arguments_json == "{}");
}

void TestSystemTools() {
    xpagent::ToolContext ctx;
    ctx.workspace_dir = L".";

    auto sys = xpagent::ToolGetSystemInfo(nlohmann::json::object(), ctx);
    EXPECT_TRUE(sys.ok);
    EXPECT_TRUE(sys.content.find("OS Version:") != std::string::npos);
    EXPECT_TRUE(sys.content.find("Memory Load:") != std::string::npos);

    auto procs = xpagent::ToolListProcesses(nlohmann::json::object(), ctx);
    EXPECT_TRUE(procs.ok);
    EXPECT_TRUE(procs.content.find("PID ") != std::string::npos);

    auto blocked = xpagent::ToolWriteFile(
        {{"path", "C:\\Windows\\system32\\evil.txt"}, {"content", "nope"}}, ctx);
    EXPECT_TRUE(!blocked.ok);
    EXPECT_TRUE(blocked.content.find("Sandbox blocked") != std::string::npos);
}

void TestXpEntropy() {
    unsigned char buf1[32] = {0};
    unsigned char buf2[32] = {0};
    size_t olen1 = 0, olen2 = 0;
    EXPECT_TRUE(mbedtls_hardware_poll(nullptr, buf1, sizeof(buf1), &olen1) == 0);
    EXPECT_TRUE(mbedtls_hardware_poll(nullptr, buf2, sizeof(buf2), &olen2) == 0);
    EXPECT_TRUE(olen1 == sizeof(buf1));
    EXPECT_TRUE(olen2 == sizeof(buf2));
    bool diff = false;
    for (size_t i = 0; i < sizeof(buf1); ++i) {
        if (buf1[i] != buf2[i]) diff = true;
    }
    EXPECT_TRUE(diff);

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&drbg);
    const unsigned char pers[] = "xpagent_test";
    int rc = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy, pers, sizeof(pers) - 1);
    EXPECT_TRUE(rc == 0);
    unsigned char rnd[16] = {0};
    EXPECT_TRUE(mbedtls_ctr_drbg_random(&drbg, rnd, sizeof(rnd)) == 0);
    mbedtls_ctr_drbg_free(&drbg);
    mbedtls_entropy_free(&entropy);

    xpagent::HttpClient::GlobalInit();
    xpagent::HttpClient http;
    xpagent::HttpClient::GlobalCleanup();
}

} // namespace

int main() {
    TestEncodingRoundTrip();
    TestWorkspaceSandbox();
    TestSseAccumulator();
    TestSystemTools();
    TestXpEntropy();

    if (g_failures == 0) {
        std::cout << "[PASS] All XPAgent unit tests passed.\n";
        return 0;
    }
    std::cerr << "[FAIL] " << g_failures << " test(s) failed.\n";
    return 1;
}

#include <windows.h>
#include <commctrl.h>
#include <objbase.h>
#include <objidl.h>
#include <gdiplus.h>

#include <cstdio>

#include "net/http_client.hpp"
#include "resource.h"
#include "ui/i18n.hpp"
#include "ui/main_window.hpp"

namespace {

LONG WINAPI XpUnhandledExceptionFilter(EXCEPTION_POINTERS* ep) {
    DWORD code = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0;
    PVOID addr = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionAddress : nullptr;

    wchar_t tmp[MAX_PATH] = {0};
    if (GetTempPathW(MAX_PATH, tmp) > 0) {
        std::wstring log_path = std::wstring(tmp) + L"xpagent_crash.log";
        HANDLE h = CreateFileW(log_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            char msg[256];
            int n = snprintf(msg, sizeof(msg),
                             "XPAgent crashed: ExceptionCode=0x%08lX Address=%p\r\n",
                             static_cast<unsigned long>(code), addr);
            if (n > 0) {
                DWORD written = 0;
                WriteFile(h, msg, static_cast<DWORD>(n), &written, nullptr);
            }
            CloseHandle(h);
        }
    }

    wchar_t box[256];
    _snwprintf(box, 255,
               L"XPAgent 遇到未捕获异常并即将退出。\r\n"
               L"异常代码: 0x%08lX\r\n异常地址: %p\r\n"
               L"日志已写入 %%TEMP%%\\xpagent_crash.log",
               static_cast<unsigned long>(code), addr);
    box[255] = L'\0';
    MessageBoxW(nullptr, box, L"XPAgent 崩溃报告", MB_ICONERROR | MB_OK);
    return EXCEPTION_EXECUTE_HANDLER;
}

} // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    SetUnhandledExceptionFilter(XpUnhandledExceptionFilter);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    ULONG_PTR gdiplus_token = 0;
    Gdiplus::GdiplusStartupInput gdiplus_input;
    bool has_gdiplus = (Gdiplus::GdiplusStartup(&gdiplus_token, &gdiplus_input, nullptr) == Gdiplus::Ok);

    xpagent::HttpClient::GlobalInit();
    xpagent::HttpClient http;
    http.SetEmbeddedCaResource(IDR_CACERT_PEM);

    int exit_code = 0;
    {
        xpagent::MainWindow main_win(hInstance, http);
        if (!main_win.Create(nCmdShow)) {
            MessageBoxW(nullptr, xpagent::Tr().msg_init_err, L"XPAgent", MB_ICONERROR);
            exit_code = 1;
        } else {
            MSG msg{};
            while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            exit_code = static_cast<int>(msg.wParam);
        }
    }

    xpagent::HttpClient::GlobalCleanup();
    if (has_gdiplus && gdiplus_token) Gdiplus::GdiplusShutdown(gdiplus_token);
    CoUninitialize();
    return exit_code;
}

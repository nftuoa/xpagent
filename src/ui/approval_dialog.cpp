#include "ui/approval_dialog.hpp"

#include "platform/encoding.hpp"
#include "ui/i18n.hpp"
#include "ui/theme_helper.hpp"

namespace xpagent {

namespace {

constexpr wchar_t kApprovalClass[] = L"XPAgentApprovalDialogClass";
constexpr int IDC_BTN_ALLOW = 301;
constexpr int IDC_BTN_DENY  = 302;

struct ApprovalState {
    bool finished = false;
    bool approved = false;
    std::wstring heading;
    std::wstring detail;
    HFONT font_normal = nullptr;
    HFONT font_bold = nullptr;
    HFONT font_mono = nullptr;
};

LRESULT CALLBACK ApprovalWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<ApprovalState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_NCCREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            st = static_cast<ApprovalState*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
            return TRUE;
        }
        case WM_CREATE: {
            st->font_normal = CreateXpUiFont(9, false);
            st->font_bold = CreateXpUiFont(10, true);
            st->font_mono = CreateXpMonoFont(9);

            HWND hIcon = CreateWindowExW(
                0, L"STATIC", nullptr,
                WS_CHILD | WS_VISIBLE | SS_ICON,
                18, 16, 32, 32,
                hwnd, nullptr, nullptr, nullptr);
            SendMessageW(hIcon, STM_SETICON,
                         reinterpret_cast<WPARAM>(LoadIconW(nullptr, IDI_WARNING)), 0);

            HWND hHead = CreateWindowExW(
                0, L"STATIC", st->heading.c_str(),
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                62, 20, 410, 26,
                hwnd, nullptr, nullptr, nullptr);
            SendMessageW(hHead, WM_SETFONT, reinterpret_cast<WPARAM>(st->font_bold), TRUE);

            HWND hDesc = CreateWindowExW(
                0, L"STATIC",
                Tr().appr_desc,
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                18, 56, 454, 20,
                hwnd, nullptr, nullptr, nullptr);
            SendMessageW(hDesc, WM_SETFONT, reinterpret_cast<WPARAM>(st->font_normal), TRUE);

            HWND hEdit = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", st->detail.c_str(),
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                18, 82, 454, 155,
                hwnd, nullptr, nullptr, nullptr);
            SendMessageW(hEdit, WM_SETFONT, reinterpret_cast<WPARAM>(st->font_mono), TRUE);

            HWND hAllow = CreateWindowExW(
                0, L"BUTTON", Tr().appr_btn_allow,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                268, 250, 98, 26,
                hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BTN_ALLOW)), nullptr, nullptr);
            SendMessageW(hAllow, WM_SETFONT, reinterpret_cast<WPARAM>(st->font_normal), TRUE);

            HWND hDeny = CreateWindowExW(
                0, L"BUTTON", Tr().appr_btn_deny,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                374, 250, 98, 26,
                hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BTN_DENY)), nullptr, nullptr);
            SendMessageW(hDeny, WM_SETFONT, reinterpret_cast<WPARAM>(st->font_normal), TRUE);
            SetFocus(hDeny);
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wp);
            if (id == IDC_BTN_ALLOW) {
                st->approved = true;
                st->finished = true;
                DestroyWindow(hwnd);
                return 0;
            }
            if (id == IDC_BTN_DENY || id == IDCANCEL) {
                st->approved = false;
                st->finished = true;
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            if (st) {
                st->approved = false;
                st->finished = true;
            }
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (st) {
                if (st->font_normal) DeleteObject(st->font_normal);
                if (st->font_bold) DeleteObject(st->font_bold);
                if (st->font_mono) DeleteObject(st->font_mono);
            }
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace

bool ShowApprovalDialog(
    HWND parent,
    HINSTANCE hinst,
    const std::string& title_utf8,
    const std::string& detail_utf8) {

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ApprovalWndProc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = kApprovalClass;
    RegisterClassExW(&wc);

    ApprovalState st;
    st.heading = Utf8ToWide(title_utf8);
    st.detail = Utf8ToWide(detail_utf8);

    int w = 496, h = 320;
    RECT prc{0, 0, 800, 600};
    if (parent && IsWindow(parent)) {
        GetWindowRect(parent, &prc);
    } else {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &prc, 0);
    }
    int x = prc.left + ((prc.right - prc.left) - w) / 2;
    int y = prc.top + ((prc.bottom - prc.top) - h) / 2;

    if (parent && IsWindow(parent)) {
        EnableWindow(parent, FALSE);
    }

    HWND dlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        kApprovalClass,
        Tr().appr_window_title,
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, w, h,
        parent,
        nullptr,
        hinst,
        &st);

    if (dlg) {
        MSG msg{};
        while (!st.finished && GetMessageW(&msg, nullptr, 0, 0) > 0) {
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
                SendMessageW(dlg, WM_CLOSE, 0, 0);
                continue;
            }
            if (!IsDialogMessageW(dlg, &msg)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
    }

    if (parent && IsWindow(parent)) {
        EnableWindow(parent, TRUE);
        SetForegroundWindow(parent);
    }
    return st.approved;
}

} // namespace xpagent

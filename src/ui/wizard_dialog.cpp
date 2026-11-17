#include "ui/wizard_dialog.hpp"

#include <shlobj.h>

#include <string>

#include "agent/llm_client.hpp"
#include "platform/encoding.hpp"
#include "ui/i18n.hpp"
#include "ui/theme_helper.hpp"

namespace xpagent {

namespace {

constexpr wchar_t kWizardClass[] = L"XPAgentWizardDialogClass";
constexpr UINT WM_WIZARD_TEST_DONE = WM_APP + 201;

constexpr int IDC_WIZ_BASE_URL      = 401;
constexpr int IDC_WIZ_API_KEY       = 402;
constexpr int IDC_WIZ_MODEL         = 403;
constexpr int IDC_WIZ_PROXY         = 404;
constexpr int IDC_WIZ_WORKSPACE     = 405;
constexpr int IDC_WIZ_BROWSE        = 406;
constexpr int IDC_WIZ_SYS_PROMPT    = 407;
constexpr int IDC_WIZ_LANG          = 408;
constexpr int IDC_WIZ_TEST          = 409;
constexpr int IDC_WIZ_STATUS        = 410;
constexpr int IDC_WIZ_SAVE          = 411;
constexpr int IDC_WIZ_CANCEL        = 412;

struct WizardState {
    bool finished = false;
    bool saved = false;
    bool testing = false;
    const HttpClient* http = nullptr;
    AgentConfig* cfg = nullptr;
    XpPalette palette;
    HFONT font_normal = nullptr;
    HFONT font_bold = nullptr;

    HWND h_base_url = nullptr;
    HWND h_api_key = nullptr;
    HWND h_model = nullptr;
    HWND h_proxy = nullptr;
    HWND h_workspace = nullptr;
    HWND h_sys_prompt = nullptr;
    HWND h_lang_combo = nullptr;
    HWND h_test_btn = nullptr;
    HWND h_status = nullptr;
};

struct TestThreadParam {
    HWND hwnd;
    const HttpClient* http;
    AgentConfig cfg;
};

struct TestResultMsg {
    bool ok;
    std::string message;
};

DWORD WINAPI TestConnectionThread(LPVOID lp) {
    auto* p = static_cast<TestThreadParam*>(lp);
    LlmClient llm(*p->http);
    std::string msg;
    bool ok = llm.TestConnection(p->cfg, msg);
    auto* res = new TestResultMsg{ok, msg};
    if (!PostMessageW(p->hwnd, WM_WIZARD_TEST_DONE, 0, reinterpret_cast<LPARAM>(res))) {
        delete res;
    }
    delete p;
    return 0;
}

std::wstring GetEditTextW(HWND h) {
    int len = GetWindowTextLengthW(h);
    if (len <= 0) return {};
    std::wstring buf(static_cast<size_t>(len) + 1, L'\0');
    GetWindowTextW(h, &buf[0], len + 1);
    buf.resize(static_cast<size_t>(len));
    return buf;
}

void ReadControlsIntoConfig(const WizardState& st, AgentConfig& out) {
    out.base_url = WideToUtf8(GetEditTextW(st.h_base_url));
    out.api_key = WideToUtf8(GetEditTextW(st.h_api_key));
    out.model = WideToUtf8(GetEditTextW(st.h_model));
    out.proxy_url = WideToUtf8(GetEditTextW(st.h_proxy));
    std::wstring ws = GetEditTextW(st.h_workspace);
    if (!ws.empty()) out.workspace_dir = ws;
    std::wstring sp = GetEditTextW(st.h_sys_prompt);
    if (!sp.empty()) out.system_prompt = WideToUtf8(sp);

    LRESULT sel = SendMessageW(st.h_lang_combo, CB_GETCURSEL, 0, 0);
    if (sel == 1) out.ui_lang = "en";
    else if (sel == 2) out.ui_lang = "zh";
    else out.ui_lang = "auto";
}

LRESULT CALLBACK WizardWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<WizardState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_NCCREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            st = static_cast<WizardState*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
            return TRUE;
        }
        case WM_CREATE: {
            const auto& s = Tr();
            st->palette = QueryCurrentXpPalette();
            st->font_normal = CreateXpUiFont(9, false);
            st->font_bold = CreateXpUiFont(10, true);

            auto make_label = [&](const wchar_t* text, int x, int y, int w, int h) {
                HWND hw = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT,
                                          x, y, w, h, hwnd, nullptr, nullptr, nullptr);
                SendMessageW(hw, WM_SETFONT, reinterpret_cast<WPARAM>(st->font_normal), TRUE);
                return hw;
            };
            auto make_edit = [&](const wchar_t* text, int id, int x, int y, int w, int h, DWORD extra = 0) {
                HWND hw = CreateWindowExW(
                    WS_EX_CLIENTEDGE, L"EDIT", text,
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | extra,
                    x, y, w, h, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
                SendMessageW(hw, WM_SETFONT, reinterpret_cast<WPARAM>(st->font_normal), TRUE);
                return hw;
            };

            int y = 76;
            make_label(s.wiz_lbl_base_url, 22, y + 3, 118, 20);
            st->h_base_url = make_edit(Utf8ToWide(st->cfg->base_url).c_str(),
                                       IDC_WIZ_BASE_URL, 144, y, 360, 23);

            y += 32;
            make_label(s.wiz_lbl_api_key, 22, y + 3, 118, 20);
            st->h_api_key = make_edit(Utf8ToWide(st->cfg->api_key).c_str(),
                                      IDC_WIZ_API_KEY, 144, y, 360, 23, ES_PASSWORD);

            y += 32;
            make_label(s.wiz_lbl_model, 22, y + 3, 118, 20);
            st->h_model = make_edit(Utf8ToWide(st->cfg->model).c_str(),
                                    IDC_WIZ_MODEL, 144, y, 360, 23);

            y += 32;
            make_label(s.wiz_lbl_proxy, 22, y + 3, 118, 20);
            st->h_proxy = make_edit(Utf8ToWide(st->cfg->proxy_url).c_str(),
                                    IDC_WIZ_PROXY, 144, y, 360, 23);

            y += 32;
            make_label(s.wiz_lbl_workspace, 22, y + 3, 118, 20);
            st->h_workspace = make_edit(st->cfg->workspace_dir.c_str(),
                                        IDC_WIZ_WORKSPACE, 144, y, 275, 23);
            HWND hBrowse = CreateWindowExW(
                0, L"BUTTON", s.wiz_btn_browse,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                426, y - 1, 78, 25, hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_WIZ_BROWSE)), nullptr, nullptr);
            SendMessageW(hBrowse, WM_SETFONT, reinterpret_cast<WPARAM>(st->font_normal), TRUE);

            y += 32;
            make_label(s.wiz_lbl_lang, 22, y + 3, 118, 20);
            st->h_lang_combo = CreateWindowExW(
                0, L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                144, y, 200, 120, hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_WIZ_LANG)), nullptr, nullptr);
            SendMessageW(st->h_lang_combo, WM_SETFONT, reinterpret_cast<WPARAM>(st->font_normal), TRUE);
            SendMessageW(st->h_lang_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s.wiz_lang_auto));
            SendMessageW(st->h_lang_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s.wiz_lang_en));
            SendMessageW(st->h_lang_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s.wiz_lang_zh));
            int cur_sel = 0;
            if (st->cfg->ui_lang == "en" || st->cfg->ui_lang == "en-US") cur_sel = 1;
            else if (st->cfg->ui_lang == "zh" || st->cfg->ui_lang == "zh-CN") cur_sel = 2;
            SendMessageW(st->h_lang_combo, CB_SETCURSEL, cur_sel, 0);

            y += 34;
            make_label(s.wiz_lbl_sys_prompt, 22, y + 4, 118, 20);
            st->h_sys_prompt = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", Utf8ToWide(st->cfg->system_prompt).c_str(),
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL,
                144, y, 360, 62, hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_WIZ_SYS_PROMPT)), nullptr, nullptr);
            SendMessageW(st->h_sys_prompt, WM_SETFONT, reinterpret_cast<WPARAM>(st->font_normal), TRUE);

            y += 74;
            st->h_test_btn = CreateWindowExW(
                0, L"BUTTON", s.wiz_btn_test,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                22, y, 148, 26, hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_WIZ_TEST)), nullptr, nullptr);
            SendMessageW(st->h_test_btn, WM_SETFONT, reinterpret_cast<WPARAM>(st->font_normal), TRUE);

            st->h_status = make_label(s.wiz_status_default, 180, y + 4, 325, 36);

            HWND hSave = CreateWindowExW(
                0, L"BUTTON", s.wiz_btn_save,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                296, 424, 110, 26, hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_WIZ_SAVE)), nullptr, nullptr);
            SendMessageW(hSave, WM_SETFONT, reinterpret_cast<WPARAM>(st->font_normal), TRUE);

            HWND hCancel = CreateWindowExW(
                0, L"BUTTON", s.wiz_btn_cancel,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                414, 424, 90, 26, hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_WIZ_CANCEL)), nullptr, nullptr);
            SendMessageW(hCancel, WM_SETFONT, reinterpret_cast<WPARAM>(st->font_normal), TRUE);
            return 0;
        }
        case WM_PAINT: {
            const auto& s = Tr();
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc{};
            GetClientRect(hwnd, &rc);

            RECT banner{0, 0, rc.right, 62};
            FillHorizontalGradient(hdc, banner, RGB(255, 255, 255), st->palette.card_header_right);
            RECT line{0, 62, rc.right, 64};
            HBRUSH acc = CreateSolidBrush(st->palette.accent_bar);
            FillRect(hdc, &line, acc);
            DeleteObject(acc);

            RECT bot_line{14, 410, rc.right - 14, 412};
            DrawEdge(hdc, &bot_line, EDGE_ETCHED, BF_TOP);

            SetBkMode(hdc, TRANSPARENT);
            HGDIOBJ old_font = SelectObject(hdc, st->font_bold);
            SetTextColor(hdc, st->palette.card_header_text);
            RECT title_rc{20, 10, rc.right - 20, 32};
            DrawTextW(hdc, s.wiz_banner_title, -1, &title_rc, DT_LEFT | DT_SINGLELINE);

            SelectObject(hdc, st->font_normal);
            SetTextColor(hdc, RGB(70, 70, 70));
            RECT sub_rc{20, 34, rc.right - 20, 56};
            DrawTextW(hdc, s.wiz_banner_sub, -1, &sub_rc, DT_LEFT | DT_SINGLELINE);
            SelectObject(hdc, old_font);

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wp);
            if (id == IDC_WIZ_BROWSE) {
                std::wstring picked = BrowseForFolderXp(hwnd, Tr().browse_workspace_title);
                if (!picked.empty()) {
                    SetWindowTextW(st->h_workspace, picked.c_str());
                }
                return 0;
            }
            if (id == IDC_WIZ_TEST) {
                if (st->testing) return 0;
                st->testing = true;
                EnableWindow(st->h_test_btn, FALSE);
                SetWindowTextW(st->h_status, Tr().wiz_status_testing);

                auto* param = new TestThreadParam{hwnd, st->http, *st->cfg};
                ReadControlsIntoConfig(*st, param->cfg);
                DWORD tid = 0;
                HANDLE ht = CreateThread(nullptr, 0, TestConnectionThread, param, 0, &tid);
                if (ht) {
                    CloseHandle(ht);
                } else {
                    delete param;
                    st->testing = false;
                    EnableWindow(st->h_test_btn, TRUE);
                    SetWindowTextW(st->h_status, Tr().wiz_status_thread_err);
                }
                return 0;
            }
            if (id == IDC_WIZ_SAVE) {
                ReadControlsIntoConfig(*st, *st->cfg);
                SetCurrentUiLang(ResolveUiLang(st->cfg->ui_lang));
                st->cfg->configured = true;
                SaveAgentConfig(*st->cfg);
                st->saved = true;
                st->finished = true;
                DestroyWindow(hwnd);
                return 0;
            }
            if (id == IDC_WIZ_CANCEL || id == IDCANCEL) {
                st->saved = false;
                st->finished = true;
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }
        case WM_WIZARD_TEST_DONE: {
            auto* res = reinterpret_cast<TestResultMsg*>(lp);
            if (res) {
                st->testing = false;
                EnableWindow(st->h_test_btn, TRUE);
                SetWindowTextW(st->h_status, Utf8ToWide(res->message).c_str());
                delete res;
            }
            return 0;
        }
        case WM_CLOSE:
            if (st) {
                st->saved = false;
                st->finished = true;
            }
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (st) {
                if (st->font_normal) DeleteObject(st->font_normal);
                if (st->font_bold) DeleteObject(st->font_bold);
            }
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace

std::wstring BrowseForFolderXp(HWND parent, const std::wstring& title) {
    BROWSEINFOW bi{};
    bi.hwndOwner = parent;
    bi.lpszTitle = title.c_str();
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_EDITBOX;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return {};
    wchar_t path[MAX_PATH] = {0};
    std::wstring result;
    if (SHGetPathFromIDListW(pidl, path)) {
        result = path;
    }
    CoTaskMemFree(pidl);
    return result;
}

bool ShowSetupWizardDialog(
    HWND parent,
    HINSTANCE hinst,
    const HttpClient& http,
    AgentConfig& in_out_cfg) {

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WizardWndProc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = kWizardClass;
    RegisterClassExW(&wc);

    WizardState st;
    st.http = &http;
    st.cfg = &in_out_cfg;

    int w = 534, h = 494;
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
        kWizardClass,
        Tr().wiz_window_title,
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
    return st.saved;
}

} // namespace xpagent

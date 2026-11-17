#include "ui/main_window.hpp"

#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>

#include <memory>

#include "platform/encoding.hpp"
#include "resource.h"
#include "ui/approval_dialog.hpp"
#include "ui/wizard_dialog.hpp"

namespace xpagent {

namespace {

constexpr wchar_t kMainWindowClass[] = L"XPAgentMainWindowClass";
constexpr int IDC_INPUT_EDIT = 501;
constexpr int IDC_SEND_BTN   = 502;
constexpr int IDC_STOP_BTN   = 503;
constexpr int IDC_STATUS_BAR = 504;

constexpr int kTaskPaneWidth    = 228;
constexpr int kChatHeaderHeight = 58;
constexpr int kToolbarHeight    = 30;
constexpr int kInputAreaHeight  = 84;

constexpr UINT_PTR kTypingTimerId = 101;
constexpr UINT_PTR kNudgeTimerId  = 102;

constexpr int IDM_EMOJI_BASE = 4000;
constexpr int kQuickInsertCount = 9;

} // namespace

MainWindow::MainWindow(HINSTANCE hinst, HttpClient& http)
    : hinst_(hinst), http_(http), session_(http) {
    LoadAgentConfig(cfg_);
    palette_ = QueryCurrentXpPalette();
}

MainWindow::~MainWindow() {
    if (ui_font_) DeleteObject(ui_font_);
    if (input_panel_brush_) DeleteObject(input_panel_brush_);
}

bool MainWindow::Create(int nCmdShow) {
    app_icon_ = LoadIconW(hinst_, MAKEINTRESOURCEW(IDI_APP_ICON));
    if (!app_icon_) app_icon_ = LoadIconW(nullptr, IDI_APPLICATION);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hinst_;
    wc.hIcon = app_icon_;
    wc.hIconSm = app_icon_;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = kMainWindowClass;
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(
        0,
        kMainWindowClass,
        Tr().app_title,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 960, 660,
        nullptr,
        nullptr,
        hinst_,
        this);

    if (!hwnd_) return false;

    ShowWindow(hwnd_, nCmdShow);
    UpdateWindow(hwnd_);

    if (!cfg_.configured) {
        std::string prev_default = Tr().default_system_prompt;
        if (ShowSetupWizardDialog(hwnd_, hinst_, http_, cfg_)) {
            ApplyLanguage(ResolveUiLang(cfg_.ui_lang));
            if (cfg_.system_prompt == prev_default) {
                cfg_.system_prompt = Tr().default_system_prompt;
                SaveAgentConfig(cfg_);
            }
            chat_view_.AppendSystemNotice(Tr().chat_setup_done);
        } else {
            chat_view_.AppendSystemNotice(Tr().chat_setup_pending);
        }
    }
    return true;
}

void MainWindow::CreateMenus() {
    const auto& s = Tr();
    HMENU old_menu = GetMenu(hwnd_);
    HMENU menubar = CreateMenu();

    HMENU file_menu = CreatePopupMenu();
    AppendMenuW(file_menu, MF_STRING, IDM_FILE_WIZARD, s.menu_file_wizard);
    AppendMenuW(file_menu, MF_STRING, IDM_FILE_WORKSPACE, s.menu_file_workspace);
    AppendMenuW(file_menu, MF_STRING, IDC_TB_SAVE_CHAT, s.menu_file_save_log);
    AppendMenuW(file_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file_menu, MF_STRING, IDM_FILE_EXIT, s.menu_file_exit);
    AppendMenuW(menubar, MF_POPUP, reinterpret_cast<UINT_PTR>(file_menu), s.menu_file);

    HMENU chat_menu = CreatePopupMenu();
    AppendMenuW(chat_menu, MF_STRING, IDM_CHAT_NEW, s.menu_chat_new);
    AppendMenuW(chat_menu, MF_STRING, IDM_CHAT_STOP, s.menu_chat_stop);
    AppendMenuW(chat_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(chat_menu, MF_STRING, IDC_TB_NUDGE, s.menu_chat_nudge);
    AppendMenuW(menubar, MF_POPUP, reinterpret_cast<UINT_PTR>(chat_menu), s.menu_chat);

    HMENU tools_menu = CreatePopupMenu();
    AppendMenuW(tools_menu, MF_STRING, IDM_TOOLS_SYSINFO, s.menu_tools_sysinfo);
    AppendMenuW(tools_menu, MF_STRING, IDM_TOOLS_TOGGLE_BUDDY, s.menu_tools_buddy);
    AppendMenuW(menubar, MF_POPUP, reinterpret_cast<UINT_PTR>(tools_menu), s.menu_tools);

    HMENU lang_menu = CreatePopupMenu();
    UiLang cur = GetCurrentUiLang();
    AppendMenuW(lang_menu, MF_STRING | (cur == UiLang::EnUS ? MF_CHECKED : 0), IDM_LANG_EN, s.menu_lang_en);
    AppendMenuW(lang_menu, MF_STRING | (cur == UiLang::ZhCN ? MF_CHECKED : 0), IDM_LANG_ZH, s.menu_lang_zh);
    AppendMenuW(menubar, MF_POPUP, reinterpret_cast<UINT_PTR>(lang_menu), s.menu_lang);

    HMENU help_menu = CreatePopupMenu();
    AppendMenuW(help_menu, MF_STRING, IDM_HELP_ABOUT, s.menu_help_about);
    AppendMenuW(menubar, MF_POPUP, reinterpret_cast<UINT_PTR>(help_menu), s.menu_help);

    SetMenu(hwnd_, menubar);
    if (old_menu) DestroyMenu(old_menu);
}

void MainWindow::ApplyLanguage(UiLang lang) {
    SetCurrentUiLang(lang);
    const auto& s = Tr();

    SetWindowTextW(hwnd_, s.app_title);
    CreateMenus();
    DrawMenuBar(hwnd_);

    task_pane_.RefreshLanguage();
    chat_header_.RefreshLanguage();
    buddy_.RefreshLanguage();

    SetWindowTextW(h_tb_insert_, s.tb_insert_file);
    SetWindowTextW(h_tb_emoji_, s.tb_emoticons);
    SetWindowTextW(h_tb_nudge_, s.tb_nudge);
    SetWindowTextW(h_tb_clear_, s.tb_clear);
    SetWindowTextW(h_tb_save_, s.tb_save);
    SetWindowTextW(h_send_btn_, s.btn_send);
    SetWindowTextW(h_stop_btn_, s.btn_stop);

    RefreshHeader(session_.IsRunning() ? ChatHeader::Presence::Typing : ChatHeader::Presence::Online);
    RefreshStatusBar();
}

void MainWindow::CreateChildControls() {
    const auto& s = Tr();
    ui_font_ = CreateXpUiFont(9, false);
    input_panel_brush_ = CreateSolidBrush(palette_.input_panel_bg);

    task_pane_.Create(hwnd_, hinst_, 0, 0, kTaskPaneWidth, 500);
    chat_header_.Create(hwnd_, hinst_, kTaskPaneWidth, 0, 600, kChatHeaderHeight);
    chat_view_.Create(hwnd_, hinst_, kTaskPaneWidth, kChatHeaderHeight, 600, 360);

    auto make_tb_btn = [&](const wchar_t* label, int id) {
        HWND b = CreateWindowExW(
            0, L"BUTTON", label,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            0, 0, 82, 24,
            hwnd_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            hinst_, nullptr);
        SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(ui_font_), TRUE);
        return b;
    };

    h_tb_insert_ = make_tb_btn(s.tb_insert_file, IDC_TB_INSERT_FILE);
    h_tb_emoji_  = make_tb_btn(s.tb_emoticons, IDC_TB_EMOTICONS);
    h_tb_nudge_  = make_tb_btn(s.tb_nudge, IDC_TB_NUDGE);
    h_tb_clear_  = make_tb_btn(s.tb_clear, IDC_TB_CLEAR_CHAT);
    h_tb_save_   = make_tb_btn(s.tb_save, IDC_TB_SAVE_CHAT);

    h_input_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
            ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
        0, 0, 400, 64,
        hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_INPUT_EDIT)),
        hinst_,
        nullptr);
    SendMessageW(h_input_, WM_SETFONT, reinterpret_cast<WPARAM>(ui_font_), TRUE);
    orig_input_proc_ = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(h_input_, GWLP_WNDPROC));
    SetWindowLongPtrW(h_input_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    SetWindowLongPtrW(h_input_, GWLP_WNDPROC,
                      reinterpret_cast<LONG_PTR>(InputEditSubclassProc));

    h_send_btn_ = CreateWindowExW(
        0, L"BUTTON", s.btn_send,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        0, 0, 88, 30,
        hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SEND_BTN)),
        hinst_,
        nullptr);
    SendMessageW(h_send_btn_, WM_SETFONT, reinterpret_cast<WPARAM>(ui_font_), TRUE);

    h_stop_btn_ = CreateWindowExW(
        0, L"BUTTON", s.btn_stop,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        0, 0, 88, 28,
        hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_STOP_BTN)),
        hinst_,
        nullptr);
    SendMessageW(h_stop_btn_, WM_SETFONT, reinterpret_cast<WPARAM>(ui_font_), TRUE);
    EnableWindow(h_stop_btn_, FALSE);

    h_status_ = CreateWindowExW(
        0, STATUSCLASSNAMEW, L"",
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_STATUS_BAR)),
        hinst_,
        nullptr);

    tray_icon_.Create(hwnd_, app_icon_, s.tray_tooltip);
    buddy_.Create(hwnd_, hinst_);
    buddy_.SetVisible(cfg_.enable_buddy);

    SyncTaskPaneStates();
    RefreshHeader(ChatHeader::Presence::Online);
    LayoutChildren();
    RefreshStatusBar();

    chat_view_.AppendSystemNotice(s.chat_welcome_notice);
    chat_view_.AppendBanner(s.chat_welcome_banner);
}

void MainWindow::LayoutChildren() {
    if (!hwnd_ || !h_status_) return;

    SendMessageW(h_status_, WM_SIZE, 0, 0);
    RECT st_rc{};
    GetWindowRect(h_status_, &st_rc);
    int status_h = st_rc.bottom - st_rc.top;

    RECT rc{};
    GetClientRect(hwnd_, &rc);
    int cw = rc.right - rc.left;
    int ch = rc.bottom - rc.top - status_h;
    if (ch < 220) ch = 220;

    int pane_w = (cw > 540) ? kTaskPaneWidth : 0;
    if (pane_w > 0) {
        ShowWindow(task_pane_.Handle(), SW_SHOW);
        task_pane_.Resize(0, 0, pane_w, ch);
    } else {
        ShowWindow(task_pane_.Handle(), SW_HIDE);
    }

    int right_x = pane_w;
    int right_w = cw - pane_w;

    chat_header_.Resize(right_x, 0, right_w, kChatHeaderHeight);

    int chat_h = ch - kChatHeaderHeight - kToolbarHeight - kInputAreaHeight;
    if (chat_h < 60) chat_h = 60;
    chat_view_.Resize(right_x, kChatHeaderHeight, right_w, chat_h);

    int tb_y = kChatHeaderHeight + chat_h + 3;
    int x = right_x + 8;
    MoveWindow(h_tb_insert_, x, tb_y, 84, 24, TRUE); x += 90;
    MoveWindow(h_tb_emoji_,  x, tb_y, 88, 24, TRUE); x += 94;
    MoveWindow(h_tb_nudge_,  x, tb_y, 66, 24, TRUE); x += 72;
    MoveWindow(h_tb_clear_,  x, tb_y, 54, 24, TRUE);
    MoveWindow(h_tb_save_, right_x + right_w - 90, tb_y, 82, 24, TRUE);

    int pad = 10;
    int btn_w = 88;
    int edit_w = right_w - pad * 3 - btn_w;
    if (edit_w < 80) edit_w = 80;
    int input_y = kChatHeaderHeight + chat_h + kToolbarHeight + 8;
    int input_h = kInputAreaHeight - 16;

    MoveWindow(h_input_, right_x + pad, input_y, edit_w, input_h, TRUE);
    MoveWindow(h_send_btn_, right_x + pad * 2 + edit_w, input_y, btn_w, 30, TRUE);
    MoveWindow(h_stop_btn_, right_x + pad * 2 + edit_w, input_y + 36, btn_w, 28, TRUE);
}

void MainWindow::RefreshHeader(ChatHeader::Presence presence) {
    std::wstring model_w = Utf8ToWide(cfg_.model);
    std::wstring sub = std::wstring(Tr().hdr_workspace_prefix) + cfg_.workspace_dir;
    chat_header_.SetInfo(model_w, sub, presence);
}

void MainWindow::RefreshStatusBar() {
    if (!h_status_) return;
    const auto& s = Tr();
    int parts[3] = {260, 500, -1};
    SendMessageW(h_status_, SB_SETPARTS, 3, reinterpret_cast<LPARAM>(parts));

    std::wstring s0 = s.status_ready;
    if (session_.IsRunning()) {
        static const wchar_t* kDots[] = {L"", L".", L"..", L"..."};
        s0 = std::wstring(s.status_typing_prefix) + kDots[typing_dots_ % 4];
    }
    std::wstring s1 = std::wstring(s.status_model_prefix) + Utf8ToWide(cfg_.model);
    std::wstring s2 = std::wstring(s.status_ws_prefix) + cfg_.workspace_dir;

    SendMessageW(h_status_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(s0.c_str()));
    SendMessageW(h_status_, SB_SETTEXTW, 1, reinterpret_cast<LPARAM>(s1.c_str()));
    SendMessageW(h_status_, SB_SETTEXTW, 2, reinterpret_cast<LPARAM>(s2.c_str()));
}

void MainWindow::SyncTaskPaneStates() {
    task_pane_.SetToggleState(IDC_TASK_TOGGLE_WRITE, cfg_.allow_file_write);
    task_pane_.SetToggleState(IDC_TASK_TOGGLE_CMD, cfg_.require_cmd_approval);
    task_pane_.SetToggleState(IDC_TASK_TOGGLE_BUDDY, cfg_.enable_buddy);
    task_pane_.SetToggleState(IDC_TASK_TOGGLE_SOUND, cfg_.enable_sounds);
}

void MainWindow::PlayEventBeep(UINT beep_type) const {
    if (cfg_.enable_sounds) {
        MessageBeep(beep_type);
    }
}

void MainWindow::TriggerWindowNudge(bool announce) {
    if (announce) {
        chat_view_.AppendSystemNotice(Tr().chat_nudge_notice);
    }
    PlayEventBeep(MB_ICONASTERISK);
    if (!IsIconic(hwnd_) && nudge_step_ == 0) {
        GetWindowRect(hwnd_, &nudge_origin_);
        nudge_step_ = 1;
        SetTimer(hwnd_, kNudgeTimerId, 28, nullptr);
    }
}

void MainWindow::SetRunningState(bool running) {
    EnableWindow(h_send_btn_, running ? FALSE : TRUE);
    EnableWindow(h_stop_btn_, running ? TRUE : FALSE);
    buddy_.SetState(running ? BuddyWindow::State::Thinking : BuddyWindow::State::Idle);
    RefreshHeader(running ? ChatHeader::Presence::Typing : ChatHeader::Presence::Online);

    if (running) {
        typing_dots_ = 1;
        SetTimer(hwnd_, kTypingTimerId, 420, nullptr);
    } else {
        KillTimer(hwnd_, kTypingTimerId);
    }
    RefreshStatusBar();
}

void MainWindow::AppendInputText(const std::wstring& text) {
    if (!h_input_ || text.empty()) return;
    DWORD start = 0, end = 0;
    SendMessageW(h_input_, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
    SendMessageW(h_input_, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(text.c_str()));
    SetFocus(h_input_);
}

void MainWindow::OnInsertFileClicked() {
    wchar_t file_buf[MAX_PATH] = {0};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFilter = Tr().open_file_filter;
    ofn.lpstrFile = file_buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = cfg_.workspace_dir.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&ofn)) {
        AppendInputText(std::wstring(Tr().open_file_prefix) + file_buf + L" ");
    }
}

void MainWindow::OnEmoticonsClicked() {
    const auto& s = Tr();
    const wchar_t* labels[kQuickInsertCount] = {
        s.qi_smile, s.qi_happy, s.qi_frustrated, s.qi_thumbsup, s.qi_cheer,
        nullptr, s.qi_list_dir, s.qi_sysinfo, s.qi_procs
    };

    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    for (int i = 0; i < kQuickInsertCount; ++i) {
        if (!labels[i]) {
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        } else {
            AppendMenuW(menu, MF_STRING, IDM_EMOJI_BASE + i, labels[i]);
        }
    }
    RECT rc{};
    GetWindowRect(h_tb_emoji_, &rc);
    TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_TOPALIGN, rc.left, rc.bottom, 0, hwnd_, nullptr);
    DestroyMenu(menu);
}

void MainWindow::OnSaveChatClicked() {
    wchar_t file_buf[MAX_PATH] = L"XPAgent_ChatLog.rtf";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFilter = Tr().save_log_filter;
    ofn.lpstrFile = file_buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"rtf";
    ofn.lpstrInitialDir = cfg_.workspace_dir.c_str();
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetSaveFileNameW(&ofn)) {
        if (chat_view_.SaveToFile(file_buf)) {
            chat_view_.AppendSystemNotice(std::wstring(Tr().chat_saved_prefix) + file_buf);
        } else {
            MessageBoxW(hwnd_, Tr().chat_save_failed, L"XPAgent", MB_ICONERROR);
        }
    }
}

void MainWindow::SendPromptText(const std::string& utf8_prompt) {
    if (utf8_prompt.empty()) return;
    if (session_.IsRunning()) {
        MessageBoxW(hwnd_, Tr().msg_busy, L"XPAgent", MB_ICONINFORMATION);
        return;
    }
    if (cfg_.base_url.empty()) {
        if (ShowSetupWizardDialog(hwnd_, hinst_, http_, cfg_)) {
            ApplyLanguage(ResolveUiLang(cfg_.ui_lang));
        }
        return;
    }

    PlayEventBeep(MB_OK);
    chat_view_.AppendUserMessage(utf8_prompt);
    chat_view_.BeginAssistantTurn(cfg_.model);
    SetRunningState(true);

    if (!session_.StartTurn(hwnd_, cfg_, utf8_prompt)) {
        chat_view_.AppendError(WideToUtf8(Tr().msg_start_worker_err));
        SetRunningState(false);
    }
}

void MainWindow::OnSendClicked() {
    int len = GetWindowTextLengthW(h_input_);
    if (len <= 0) return;
    std::wstring wtext(static_cast<size_t>(len) + 1, L'\0');
    GetWindowTextW(h_input_, &wtext[0], len + 1);
    wtext.resize(static_cast<size_t>(len));

    size_t first = wtext.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) return;
    size_t last = wtext.find_last_not_of(L" \t\r\n");
    wtext = wtext.substr(first, last - first + 1);

    SetWindowTextW(h_input_, L"");
    SendPromptText(WideToUtf8(wtext));
}

void MainWindow::OnStopClicked() {
    session_.RequestCancel();
}

void MainWindow::ShowAboutDialog() {
    MessageBoxW(hwnd_, Tr().about_body, Tr().about_title, MB_ICONINFORMATION | MB_OK);
}

LRESULT CALLBACK MainWindow::InputEditSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_KEYDOWN && wp == VK_RETURN && (GetKeyState(VK_SHIFT) & 0x8000) == 0) {
        if (self) self->OnSendClicked();
        return 0;
    }
    if (self && self->orig_input_proc_) {
        return CallWindowProcW(self->orig_input_proc_, hwnd, msg, wp, lp);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = static_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) return self->HandleMessage(msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT MainWindow::HandleMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            CreateMenus();
            CreateChildControls();
            return 0;

        case WM_SIZE:
            LayoutChildren();
            return 0;

        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
            mmi->ptMinTrackSize.x = 680;
            mmi->ptMinTrackSize.y = 480;
            return 0;
        }

        case WM_TIMER:
            if (wp == kTypingTimerId) {
                ++typing_dots_;
                RefreshStatusBar();
                return 0;
            }
            if (wp == kNudgeTimerId) {
                static const int kOffsets[8][2] = {
                    {-6, -4}, {6, 4}, {-5, 3}, {5, -3},
                    {-4, -2}, {4, 2}, {-2, 1}, {0, 0}
                };
                if (nudge_step_ >= 1 && nudge_step_ <= 8) {
                    int idx = nudge_step_ - 1;
                    SetWindowPos(hwnd_, nullptr,
                                 nudge_origin_.left + kOffsets[idx][0],
                                 nudge_origin_.top + kOffsets[idx][1],
                                 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                    ++nudge_step_;
                } else {
                    KillTimer(hwnd_, kNudgeTimerId);
                    nudge_step_ = 0;
                }
                return 0;
            }
            break;

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
            SetBkMode(reinterpret_cast<HDC>(wp), TRANSPARENT);
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd_, &ps);
            if (h_status_ && input_panel_brush_) {
                RECT rc{};
                GetClientRect(hwnd_, &rc);
                RECT st_rc{};
                GetWindowRect(h_status_, &st_rc);
                int status_h = st_rc.bottom - st_rc.top;
                int pane_w = (rc.right > 540) ? kTaskPaneWidth : 0;
                int panel_top = rc.bottom - status_h - kInputAreaHeight - kToolbarHeight;
                RECT lower_bg = {pane_w, panel_top, rc.right, rc.bottom - status_h};
                FillRect(hdc, &lower_bg, input_panel_brush_);
                DrawEdge(hdc, &lower_bg, EDGE_ETCHED, BF_TOP);
                RECT tb_sep = {pane_w, panel_top + kToolbarHeight, rc.right, panel_top + kToolbarHeight + 2};
                DrawEdge(hdc, &tb_sep, EDGE_ETCHED, BF_TOP);
            }
            EndPaint(hwnd_, &ps);
            return 0;
        }

        case WM_THEMECHANGED: {
            palette_ = QueryCurrentXpPalette();
            task_pane_.SetPalette(palette_);
            chat_header_.SetPalette(palette_);
            if (input_panel_brush_) DeleteObject(input_panel_brush_);
            input_panel_brush_ = CreateSolidBrush(palette_.input_panel_bg);
            InvalidateRect(hwnd_, nullptr, TRUE);
            return 0;
        }

        case WM_NOTIFY:
            if (chat_view_.HandleNotify(reinterpret_cast<NMHDR*>(lp))) {
                return 0;
            }
            break;

        case WM_COMMAND: {
            int id = LOWORD(wp);
            if (id >= IDM_EMOJI_BASE && id < IDM_EMOJI_BASE + kQuickInsertCount) {
                const auto& s = Tr();
                const wchar_t* texts[kQuickInsertCount] = {
                    L":-) ", L"^_^ ", L">_< ", L"(y) ", L"\\o/ ",
                    nullptr, s.qi_list_dir_prompt, s.qi_sysinfo_prompt, s.qi_procs_prompt
                };
                const wchar_t* txt = texts[id - IDM_EMOJI_BASE];
                if (txt) AppendInputText(txt);
                return 0;
            }
            switch (id) {
                case IDC_SEND_BTN:
                    OnSendClicked();
                    return 0;
                case IDC_STOP_BTN:
                case IDM_CHAT_STOP:
                    OnStopClicked();
                    return 0;
                case IDC_TB_INSERT_FILE:
                    OnInsertFileClicked();
                    return 0;
                case IDC_TB_EMOTICONS:
                    OnEmoticonsClicked();
                    return 0;
                case IDC_TB_NUDGE:
                    TriggerWindowNudge(true);
                    return 0;
                case IDC_HDR_SAVE_LOG:
                case IDC_TB_SAVE_CHAT:
                    OnSaveChatClicked();
                    return 0;
                case IDM_LANG_EN:
                    cfg_.ui_lang = "en";
                    SaveAgentConfig(cfg_);
                    ApplyLanguage(UiLang::EnUS);
                    return 0;
                case IDM_LANG_ZH:
                    cfg_.ui_lang = "zh";
                    SaveAgentConfig(cfg_);
                    ApplyLanguage(UiLang::ZhCN);
                    return 0;
                case IDM_FILE_WIZARD:
                case IDC_TASK_WIZARD:
                case IDC_HDR_WIZARD:
                    if (ShowSetupWizardDialog(hwnd_, hinst_, http_, cfg_)) {
                        ApplyLanguage(ResolveUiLang(cfg_.ui_lang));
                    }
                    return 0;
                case IDM_FILE_WORKSPACE:
                case IDC_TASK_WORKSPACE: {
                    std::wstring picked = BrowseForFolderXp(hwnd_, Tr().browse_workspace_title);
                    if (!picked.empty()) {
                        cfg_.workspace_dir = picked;
                        SaveAgentConfig(cfg_);
                        RefreshHeader(ChatHeader::Presence::Online);
                        RefreshStatusBar();
                    }
                    return 0;
                }
                case IDM_CHAT_NEW:
                case IDC_TASK_NEW_CHAT:
                case IDC_TB_CLEAR_CHAT:
                    if (!session_.IsRunning()) {
                        session_.ClearHistory();
                        chat_view_.Clear();
                        chat_view_.AppendSystemNotice(Tr().chat_new_session);
                    }
                    return 0;
                case IDM_TOOLS_SYSINFO:
                case IDC_TASK_SYS_CHECK:
                    SendPromptText(Tr().quick_sys_check_prompt);
                    return 0;
                case IDC_TASK_PROC_CHECK:
                    SendPromptText(Tr().quick_proc_check_prompt);
                    return 0;
                case IDC_TASK_TOGGLE_WRITE:
                    cfg_.allow_file_write = (HIWORD(wp) != 0);
                    SaveAgentConfig(cfg_);
                    SyncTaskPaneStates();
                    return 0;
                case IDC_TASK_TOGGLE_CMD:
                    cfg_.require_cmd_approval = (HIWORD(wp) != 0);
                    SaveAgentConfig(cfg_);
                    SyncTaskPaneStates();
                    return 0;
                case IDC_TASK_TOGGLE_BUDDY:
                case IDM_TOOLS_TOGGLE_BUDDY:
                case IDM_TRAY_BUDDY:
                    cfg_.enable_buddy = (id == IDC_TASK_TOGGLE_BUDDY) ? (HIWORD(wp) != 0) : !cfg_.enable_buddy;
                    buddy_.SetVisible(cfg_.enable_buddy);
                    SaveAgentConfig(cfg_);
                    SyncTaskPaneStates();
                    return 0;
                case IDC_TASK_TOGGLE_SOUND:
                    cfg_.enable_sounds = (HIWORD(wp) != 0);
                    SaveAgentConfig(cfg_);
                    SyncTaskPaneStates();
                    return 0;
                case IDM_TRAY_SHOW:
                    ShowWindow(hwnd_, SW_RESTORE);
                    SetForegroundWindow(hwnd_);
                    return 0;
                case IDM_HELP_ABOUT:
                    ShowAboutDialog();
                    return 0;
                case IDM_FILE_EXIT:
                case IDM_TRAY_EXIT:
                    DestroyWindow(hwnd_);
                    return 0;
            }
            break;
        }

        case WM_XPAGENT_TRAY:
            if (lp == WM_LBUTTONDBLCLK) {
                ShowWindow(hwnd_, SW_RESTORE);
                SetForegroundWindow(hwnd_);
            } else if (lp == WM_RBUTTONUP) {
                tray_icon_.ShowContextMenu(hwnd_, buddy_.IsVisible());
            }
            return 0;

        case WM_AGENT_STREAM_TOKEN: {
            std::unique_ptr<std::string> ptr(reinterpret_cast<std::string*>(lp));
            if (ptr) chat_view_.AppendAssistantDelta(*ptr);
            return 0;
        }

        case WM_AGENT_TOOL_STATUS: {
            std::unique_ptr<std::string> ptr(reinterpret_cast<std::string*>(lp));
            if (ptr) chat_view_.AppendToolStatus(*ptr);
            return 0;
        }

        case WM_AGENT_TOOL_RESULT: {
            std::unique_ptr<std::string> ptr(reinterpret_cast<std::string*>(lp));
            if (ptr) chat_view_.AppendToolResult(*ptr);
            return 0;
        }

        case WM_AGENT_APPROVAL_REQ: {
            auto* req = reinterpret_cast<ApprovalRequest*>(lp);
            if (req) {
                buddy_.SetState(BuddyWindow::State::NeedsApproval);
                RefreshHeader(ChatHeader::Presence::NeedsApproval);
                PlayEventBeep(MB_ICONEXCLAMATION);
                if (cfg_.enable_tray_balloons && IsIconic(hwnd_)) {
                    tray_icon_.ShowBalloon(Tr().tray_need_approval,
                                           Utf8ToWide(req->title), NIIF_WARNING);
                }
                req->approved = ShowApprovalDialog(hwnd_, hinst_, req->title, req->detail);
                buddy_.SetState(BuddyWindow::State::Thinking);
                RefreshHeader(ChatHeader::Presence::Typing);
            }
            return 0;
        }

        case WM_AGENT_TURN_DONE: {
            std::unique_ptr<std::string> ptr(reinterpret_cast<std::string*>(lp));
            chat_view_.EndAssistantTurn();
            if (ptr && !ptr->empty()) {
                chat_view_.AppendSystemNotice(Utf8ToWide(*ptr));
            }
            SetRunningState(false);
            PlayEventBeep(MB_ICONASTERISK);
            if (cfg_.enable_tray_balloons && (!IsWindowVisible(hwnd_) || IsIconic(hwnd_))) {
                tray_icon_.ShowBalloon(L"XPAgent", Tr().tray_done, NIIF_INFO);
            }
            return 0;
        }

        case WM_AGENT_TURN_ERROR: {
            std::unique_ptr<std::string> ptr(reinterpret_cast<std::string*>(lp));
            chat_view_.EndAssistantTurn();
            if (ptr) chat_view_.AppendError(*ptr);
            SetRunningState(false);
            PlayEventBeep(MB_ICONHAND);
            if (cfg_.enable_tray_balloons && (!IsWindowVisible(hwnd_) || IsIconic(hwnd_))) {
                tray_icon_.ShowBalloon(Tr().tray_error,
                                       ptr ? Utf8ToWide(*ptr) : Tr().tray_error_fallback, NIIF_ERROR);
            }
            return 0;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd_);
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd_, kTypingTimerId);
            KillTimer(hwnd_, kNudgeTimerId);
            session_.RequestCancel();
            session_.WaitUntilIdle();
            buddy_.Destroy();
            tray_icon_.Destroy();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

} // namespace xpagent

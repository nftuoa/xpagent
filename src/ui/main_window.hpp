#pragma once

#include <windows.h>

#include <memory>
#include <string>

#include "agent/agent_loop.hpp"
#include "agent/config.hpp"
#include "net/http_client.hpp"
#include "ui/buddy_window.hpp"
#include "ui/chat_header.hpp"
#include "ui/chat_view.hpp"
#include "ui/i18n.hpp"
#include "ui/task_pane.hpp"
#include "ui/theme_helper.hpp"
#include "ui/tray_icon.hpp"

namespace xpagent {

class MainWindow {
public:
    MainWindow(HINSTANCE hinst, HttpClient& http);
    ~MainWindow();

    bool Create(int nCmdShow);
    HWND Handle() const { return hwnd_; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    static LRESULT CALLBACK InputEditSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT HandleMessage(UINT msg, WPARAM wp, LPARAM lp);

    void CreateMenus();
    void CreateChildControls();
    void LayoutChildren();
    void RefreshStatusBar();
    void RefreshHeader(ChatHeader::Presence presence);
    void ApplyLanguage(UiLang lang);
    void SyncTaskPaneStates();
    void SendPromptText(const std::string& utf8_prompt);
    void OnSendClicked();
    void OnStopClicked();
    void OnInsertFileClicked();
    void OnEmoticonsClicked();
    void OnSaveChatClicked();
    void TriggerWindowNudge(bool announce);
    void PlayEventBeep(UINT beep_type) const;
    void AppendInputText(const std::wstring& text);
    void SetRunningState(bool running);
    void ShowAboutDialog();

    HINSTANCE hinst_ = nullptr;
    HWND hwnd_ = nullptr;
    HttpClient& http_;
    AgentConfig cfg_;
    AgentSession session_;

    XpPalette palette_;
    HICON app_icon_ = nullptr;
    HFONT ui_font_ = nullptr;
    HBRUSH input_panel_brush_ = nullptr;

    XpTaskPane task_pane_;
    ChatHeader chat_header_;
    ChatView chat_view_;
    TrayIcon tray_icon_;
    BuddyWindow buddy_;

    HWND h_tb_insert_ = nullptr;
    HWND h_tb_emoji_ = nullptr;
    HWND h_tb_nudge_ = nullptr;
    HWND h_tb_clear_ = nullptr;
    HWND h_tb_save_ = nullptr;

    HWND h_input_ = nullptr;
    HWND h_send_btn_ = nullptr;
    HWND h_stop_btn_ = nullptr;
    HWND h_status_ = nullptr;
    WNDPROC orig_input_proc_ = nullptr;

    int typing_dots_ = 0;
    int nudge_step_ = 0;
    RECT nudge_origin_{};
};

} // namespace xpagent

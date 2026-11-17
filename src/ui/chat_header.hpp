#pragma once

#include <windows.h>

#include <string>

#include "ui/theme_helper.hpp"

namespace xpagent {

// MSN Messenger 7.5 / QQ 2005 style top gradient header bar with Display Picture,
// online status dot, model nickname, personal status line, and quick action buttons.
class ChatHeader {
public:
    enum class Presence {
        Online,
        Typing,
        NeedsApproval
    };

    ChatHeader();
    ~ChatHeader();

    bool Create(HWND parent, HINSTANCE hinst, int x, int y, int w, int h);
    HWND Handle() const { return hwnd_; }
    void Resize(int x, int y, int w, int h);
    void SetPalette(const XpPalette& palette);
    void SetInfo(const std::wstring& model_name, const std::wstring& subtitle, Presence presence);
    void RefreshLanguage();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT HandleMessage(UINT msg, WPARAM wp, LPARAM lp);
    void LayoutButtons();
    void Paint(HDC hdc);

    HWND hwnd_ = nullptr;
    HWND parent_ = nullptr;
    HWND btn_wizard_ = nullptr;
    HWND btn_save_ = nullptr;

    XpPalette palette_;
    HFONT font_title_ = nullptr;
    HFONT font_sub_ = nullptr;
    HFONT font_btn_ = nullptr;

    std::wstring model_name_ = L"XPAgent";
    std::wstring subtitle_ = L"TLS 1.3 直连就绪";
    Presence presence_ = Presence::Online;
};

} // namespace xpagent

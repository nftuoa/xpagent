#include "ui/chat_header.hpp"

#include <objidl.h>
#include <gdiplus.h>

#include "resource.h"
#include "ui/i18n.hpp"

namespace xpagent {

namespace {

constexpr wchar_t kChatHeaderClass[] = L"XPAgentChatHeaderClass";

} // namespace

ChatHeader::ChatHeader() {
    palette_ = QueryCurrentXpPalette();
}

ChatHeader::~ChatHeader() {
    if (font_title_) DeleteObject(font_title_);
    if (font_sub_) DeleteObject(font_sub_);
    if (font_btn_) DeleteObject(font_btn_);
}

bool ChatHeader::Create(HWND parent, HINSTANCE hinst, int x, int y, int w, int h) {
    parent_ = parent;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kChatHeaderClass;
    RegisterClassExW(&wc);

    font_title_ = CreateXpUiFont(10, true);
    font_sub_ = CreateXpUiFont(8, false);
    font_btn_ = CreateXpUiFont(9, false);

    hwnd_ = CreateWindowExW(
        0, kChatHeaderClass, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        x, y, w, h, parent, nullptr, hinst, this);
    if (!hwnd_) return false;

    btn_wizard_ = CreateWindowExW(
        0, L"BUTTON", Tr().hdr_btn_settings,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        0, 0, 68, 24, hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_HDR_WIZARD)), hinst, nullptr);
    SendMessageW(btn_wizard_, WM_SETFONT, reinterpret_cast<WPARAM>(font_btn_), TRUE);

    btn_save_ = CreateWindowExW(
        0, L"BUTTON", Tr().hdr_btn_save,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        0, 0, 74, 24, hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_HDR_SAVE_LOG)), hinst, nullptr);
    SendMessageW(btn_save_, WM_SETFONT, reinterpret_cast<WPARAM>(font_btn_), TRUE);

    LayoutButtons();
    return true;
}

void ChatHeader::Resize(int x, int y, int w, int h) {
    if (!hwnd_) return;
    MoveWindow(hwnd_, x, y, w, h, TRUE);
    LayoutButtons();
}

void ChatHeader::LayoutButtons() {
    if (!hwnd_) return;
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    int w = rc.right - rc.left;
    int y = (rc.bottom - 24) / 2;
    if (btn_save_) MoveWindow(btn_save_, w - 84, y, 74, 24, TRUE);
    if (btn_wizard_) MoveWindow(btn_wizard_, w - 158, y, 68, 24, TRUE);
}

void ChatHeader::SetPalette(const XpPalette& palette) {
    palette_ = palette;
    if (hwnd_) InvalidateRect(hwnd_, nullptr, TRUE);
}

void ChatHeader::SetInfo(const std::wstring& model_name, const std::wstring& subtitle, Presence presence) {
    model_name_ = model_name.empty() ? L"XPAgent" : model_name;
    subtitle_ = subtitle;
    presence_ = presence;
    if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
}

void ChatHeader::RefreshLanguage() {
    if (btn_wizard_) SetWindowTextW(btn_wizard_, Tr().hdr_btn_settings);
    if (btn_save_) SetWindowTextW(btn_save_, Tr().hdr_btn_save);
    if (hwnd_) InvalidateRect(hwnd_, nullptr, TRUE);
}

void ChatHeader::Paint(HDC hdc) {
    RECT rc{};
    GetClientRect(hwnd_, &rc);

    HDC memdc = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HGDIOBJ old_bmp = SelectObject(memdc, bmp);

    // Classic MSN 7.5 soft horizontal+vertical header wash
    RECT grad_rc = {0, 0, rc.right, rc.bottom - 2};
    FillHorizontalGradient(memdc, grad_rc, palette_.card_body_bg, RGB(255, 255, 255));

    RECT sep = {0, rc.bottom - 2, rc.right, rc.bottom};
    HBRUSH sep_br = CreateSolidBrush(palette_.accent_bar);
    FillRect(memdc, &sep, sep_br);
    DeleteObject(sep_br);

    // Draw MSN Display Picture frame + online status indicator with GDI+
    {
        Gdiplus::Graphics g(memdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

        // Outer white frame shadow + glossy avatar tile
        Gdiplus::SolidBrush shadow_br(Gdiplus::Color(45, 0, 0, 0));
        g.FillRectangle(&shadow_br, 11.0f, 8.0f, 44.0f, 44.0f);

        Gdiplus::RectF dp_rect(9.0f, 6.0f, 44.0f, 44.0f);
        Gdiplus::LinearGradientBrush dp_bg(
            dp_rect,
            Gdiplus::Color(255, 198, 224, 255),
            Gdiplus::Color(255, 62, 128, 226),
            Gdiplus::LinearGradientModeVertical);
        g.FillRectangle(&dp_bg, dp_rect);

        // Cute MSN-style stylized figure silhouette + glossy shine
        Gdiplus::SolidBrush head_br(Gdiplus::Color(245, 255, 255, 255));
        g.FillEllipse(&head_br, 24.0f, 13.0f, 14.0f, 14.0f);
        g.FillPie(&head_br, 17.0f, 29.0f, 28.0f, 22.0f, 180.0f, 180.0f);

        Gdiplus::Pen frame_pen(Gdiplus::Color(255, 255, 255, 255), 2.0f);
        g.DrawRectangle(&frame_pen, dp_rect);
        Gdiplus::Pen outer_pen(Gdiplus::Color(160, 88, 120, 176), 1.0f);
        g.DrawRectangle(&outer_pen, 8.5f, 5.5f, 45.0f, 45.0f);

        // Status orb at bottom-right of Display Picture
        Gdiplus::Color st_col(255, 56, 188, 56);
        if (presence_ == Presence::Typing) {
            st_col = Gdiplus::Color(255, 245, 172, 24);
        } else if (presence_ == Presence::NeedsApproval) {
            st_col = Gdiplus::Color(255, 224, 54, 48);
        }
        Gdiplus::SolidBrush st_br(st_col);
        Gdiplus::Pen st_rim(Gdiplus::Color(255, 255, 255, 255), 1.5f);
        g.FillEllipse(&st_br, 44.0f, 40.0f, 11.0f, 11.0f);
        g.DrawEllipse(&st_rim, 44.0f, 40.0f, 11.0f, 11.0f);
    }

    SetBkMode(memdc, TRANSPARENT);
    HGDIOBJ old_font = SelectObject(memdc, font_title_);
    SetTextColor(memdc, palette_.card_header_text);

    std::wstring status_tag = Tr().hdr_online;
    if (presence_ == Presence::Typing) status_tag = Tr().hdr_typing;
    else if (presence_ == Presence::NeedsApproval) status_tag = Tr().hdr_approval;

    std::wstring headline = L"XPAgent · " + model_name_ + status_tag;
    RECT title_rc = {64, 8, rc.right - 168, 28};
    DrawTextW(memdc, headline.c_str(), -1, &title_rc,
              DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);

    SelectObject(memdc, font_sub_);
    SetTextColor(memdc, RGB(88, 100, 122));
    RECT sub_rc = {64, 29, rc.right - 168, 49};
    DrawTextW(memdc, subtitle_.c_str(), -1, &sub_rc,
              DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);

    SelectObject(memdc, old_font);
    BitBlt(hdc, 0, 0, rc.right, rc.bottom, memdc, 0, 0, SRCCOPY);
    SelectObject(memdc, old_bmp);
    DeleteObject(bmp);
    DeleteDC(memdc);
}

LRESULT CALLBACK ChatHeader::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    ChatHeader* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = static_cast<ChatHeader*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<ChatHeader*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) return self->HandleMessage(msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT ChatHeader::HandleMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd_, &ps);
            Paint(hdc);
            EndPaint(hwnd_, &ps);
            return 0;
        }
        case WM_COMMAND:
            if (parent_) {
                return SendMessageW(parent_, WM_COMMAND, wp, lp);
            }
            break;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

} // namespace xpagent

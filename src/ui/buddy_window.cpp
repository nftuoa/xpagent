#include "ui/buddy_window.hpp"

#include <objidl.h>
#include <gdiplus.h>

#include "resource.h"
#include "ui/i18n.hpp"

namespace xpagent {

namespace {

constexpr wchar_t kBuddyClass[] = L"XPAgentBuddyClass";
constexpr UINT_PTR kBuddyTimerId = 1;
constexpr int kBuddyWidth = 112;
constexpr int kBuddyHeight = 104;

} // namespace

BuddyWindow::~BuddyWindow() {
    Destroy();
}

bool BuddyWindow::Create(HWND main_hwnd, HINSTANCE hinst) {
    main_hwnd_ = main_hwnd;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
    wc.lpszClassName = kBuddyClass;
    RegisterClassExW(&wc);

    RECT wa{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int x = wa.right - kBuddyWidth - 28;
    int y = wa.bottom - kBuddyHeight - 28;

    hwnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        kBuddyClass,
        L"XPAgent Buddy",
        WS_POPUP,
        x, y, kBuddyWidth, kBuddyHeight,
        main_hwnd,
        nullptr,
        hinst,
        this);

    if (!hwnd_) return false;
    caption_ = Tr().buddy_idle;
    SetTimer(hwnd_, kBuddyTimerId, 180, nullptr);
    RenderLayered();
    return true;
}

void BuddyWindow::Destroy() {
    if (hwnd_) {
        KillTimer(hwnd_, kBuddyTimerId);
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void BuddyWindow::SetVisible(bool visible) {
    visible_ = visible;
    if (hwnd_) {
        ShowWindow(hwnd_, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
        if (visible) RenderLayered();
    }
}

bool BuddyWindow::IsVisible() const {
    return visible_ && hwnd_ && IsWindowVisible(hwnd_);
}

void BuddyWindow::SetState(State state, const std::wstring& caption) {
    state_ = state;
    if (!caption.empty()) {
        caption_ = caption;
    } else {
        switch (state_) {
            case State::Idle: caption_ = Tr().buddy_idle; break;
            case State::Thinking: caption_ = Tr().buddy_thinking; break;
            case State::NeedsApproval: caption_ = Tr().buddy_approval; break;
        }
    }
    if (hwnd_ && visible_) RenderLayered();
}

void BuddyWindow::RefreshLanguage() {
    SetState(state_);
}

void BuddyWindow::RenderLayered() {
    if (!hwnd_) return;

    HDC screen_dc = GetDC(nullptr);
    HDC mem_dc = CreateCompatibleDC(screen_dc);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = kBuddyWidth;
    bmi.bmiHeader.biHeight = -kBuddyHeight; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(mem_dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ old_bmp = SelectObject(mem_dc, dib);

    {
        Gdiplus::Graphics g(mem_dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));

        // Top speech pill
        Gdiplus::SolidBrush pill_bg(Gdiplus::Color(235, 255, 255, 225)); // XP tooltip cream yellow
        Gdiplus::Pen pill_border(Gdiplus::Color(220, 90, 110, 150), 1.0f);
        Gdiplus::RectF pill(4.0f, 2.0f, kBuddyWidth - 8.0f, 24.0f);
        g.FillRectangle(&pill_bg, pill);
        g.DrawRectangle(&pill_border, pill);

        Gdiplus::Font font(L"Tahoma", 8.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
        Gdiplus::SolidBrush txt_brush(Gdiplus::Color(255, 28, 52, 110));
        Gdiplus::StringFormat sf;
        sf.SetAlignment(Gdiplus::StringAlignmentCenter);
        sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        g.DrawString(caption_.c_str(), -1, &font, pill, &sf, &txt_brush);

        // Soft drop shadow
        float bob = (state_ == State::Thinking) ? ((anim_tick_ % 4 < 2) ? -2.0f : 2.0f) : 0.0f;
        Gdiplus::SolidBrush shadow_brush(Gdiplus::Color(65, 0, 0, 0));
        g.FillEllipse(&shadow_brush, 30.0f, 88.0f, 52.0f, 10.0f);

        // Orb body (XP Blue idle, Emerald thinking, Amber approval)
        Gdiplus::Color top_col(255, 88, 166, 255);
        Gdiplus::Color bot_col(255, 24, 84, 198);
        if (state_ == State::Thinking) {
            top_col = Gdiplus::Color(255, 118, 224, 128);
            bot_col = Gdiplus::Color(255, 36, 138, 48);
        } else if (state_ == State::NeedsApproval) {
            top_col = Gdiplus::Color(255, 255, 206, 92);
            bot_col = Gdiplus::Color(255, 214, 118, 16);
        }

        Gdiplus::RectF body(28.0f, 32.0f + bob, 56.0f, 56.0f);
        Gdiplus::LinearGradientBrush orb_brush(
            body, top_col, bot_col, Gdiplus::LinearGradientModeForwardDiagonal);
        g.FillEllipse(&orb_brush, body);

        Gdiplus::Pen rim_pen(Gdiplus::Color(235, 255, 255, 255), 1.6f);
        g.DrawEllipse(&rim_pen, body);

        // Top gloss highlight
        Gdiplus::SolidBrush gloss(Gdiplus::Color(110, 255, 255, 255));
        g.FillEllipse(&gloss, 36.0f, 36.0f + bob, 32.0f, 16.0f);

        // Cute eyes (blink occasionally)
        bool blink = (state_ == State::Idle) && ((anim_tick_ % 22) == 0);
        Gdiplus::SolidBrush eye_brush(Gdiplus::Color(255, 255, 255, 255));
        if (blink) {
            Gdiplus::Pen blink_pen(Gdiplus::Color(255, 255, 255, 255), 2.0f);
            g.DrawLine(&blink_pen, 44.0f, 59.0f + bob, 50.0f, 59.0f + bob);
            g.DrawLine(&blink_pen, 62.0f, 59.0f + bob, 68.0f, 59.0f + bob);
        } else {
            g.FillEllipse(&eye_brush, 44.0f, 54.0f + bob, 6.0f, 9.0f);
            g.FillEllipse(&eye_brush, 62.0f, 54.0f + bob, 6.0f, 9.0f);
        }
    }

    POINT pt_src{0, 0};
    SIZE wnd_size{kBuddyWidth, kBuddyHeight};
    BLENDFUNCTION bf{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(hwnd_, screen_dc, nullptr, &wnd_size, mem_dc, &pt_src, 0, &bf, ULW_ALPHA);

    SelectObject(mem_dc, old_bmp);
    DeleteObject(dib);
    DeleteDC(mem_dc);
    ReleaseDC(nullptr, screen_dc);
}

LRESULT CALLBACK BuddyWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    BuddyWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = static_cast<BuddyWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<BuddyWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) return self->HandleMessage(msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT BuddyWindow::HandleMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_TIMER:
            if (wp == kBuddyTimerId && visible_) {
                ++anim_tick_;
                if (state_ == State::Thinking || (anim_tick_ % 22) <= 1) {
                    RenderLayered();
                }
            }
            return 0;
        case WM_LBUTTONDOWN: {
            dragging_ = true;
            SetCapture(hwnd_);
            POINT cursor{};
            GetCursorPos(&cursor);
            RECT wr{};
            GetWindowRect(hwnd_, &wr);
            drag_offset_.x = cursor.x - wr.left;
            drag_offset_.y = cursor.y - wr.top;
            return 0;
        }
        case WM_MOUSEMOVE:
            if (dragging_) {
                POINT cursor{};
                GetCursorPos(&cursor);
                SetWindowPos(hwnd_, nullptr,
                             cursor.x - drag_offset_.x,
                             cursor.y - drag_offset_.y,
                             0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
            return 0;
        case WM_LBUTTONUP:
            if (dragging_) {
                dragging_ = false;
                ReleaseCapture();
            }
            return 0;
        case WM_LBUTTONDBLCLK:
            if (main_hwnd_) {
                ShowWindow(main_hwnd_, SW_RESTORE);
                SetForegroundWindow(main_hwnd_);
            }
            return 0;
        case WM_RBUTTONUP:
            if (main_hwnd_) {
                PostMessageW(main_hwnd_, WM_COMMAND, IDM_TRAY_BUDDY, 0);
            }
            return 0;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

} // namespace xpagent

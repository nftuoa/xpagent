#include "ui/task_pane.hpp"

#include "resource.h"
#include "ui/i18n.hpp"

namespace xpagent {

namespace {

constexpr wchar_t kTaskPaneClass[] = L"XPAgentTaskPaneClass";

} // namespace

XpTaskPane::XpTaskPane() {
    palette_ = QueryCurrentXpPalette();
    RebuildDefaultGroups();
}

XpTaskPane::~XpTaskPane() {
    if (font_normal_) DeleteObject(font_normal_);
    if (font_bold_) DeleteObject(font_bold_);
    if (font_hover_) DeleteObject(font_hover_);
}

void XpTaskPane::RebuildDefaultGroups() {
    const auto& s = Tr();
    bool exp0 = groups_.size() > 0 ? groups_[0].expanded : true;
    bool exp1 = groups_.size() > 1 ? groups_[1].expanded : true;
    bool exp2 = groups_.size() > 2 ? groups_[2].expanded : true;

    groups_.clear();

    TaskPaneGroup g1;
    g1.title = s.tp_group_common;
    g1.expanded = exp0;
    g1.items.push_back({IDC_TASK_NEW_CHAT, s.tp_new_chat, false, false, {}});
    g1.items.push_back({IDC_TASK_WORKSPACE, s.tp_workspace, false, false, {}});
    g1.items.push_back({IDC_TASK_WIZARD, s.tp_wizard, false, false, {}});
    groups_.push_back(std::move(g1));

    TaskPaneGroup g2;
    g2.title = s.tp_group_diag;
    g2.expanded = exp1;
    g2.items.push_back({IDC_TASK_SYS_CHECK, s.tp_sys_check, false, false, {}});
    g2.items.push_back({IDC_TASK_PROC_CHECK, s.tp_proc_check, false, false, {}});
    groups_.push_back(std::move(g2));

    TaskPaneGroup g3;
    g3.title = s.tp_group_policy;
    g3.expanded = exp2;
    g3.items.push_back({IDC_TASK_TOGGLE_WRITE, s.tp_allow_write, true, true, {}});
    g3.items.push_back({IDC_TASK_TOGGLE_CMD, s.tp_cmd_approval, true, true, {}});
    g3.items.push_back({IDC_TASK_TOGGLE_BUDDY, s.tp_show_buddy, true, true, {}});
    g3.items.push_back({IDC_TASK_TOGGLE_SOUND, s.tp_enable_sounds, true, true, {}});
    groups_.push_back(std::move(g3));
}

void XpTaskPane::RefreshLanguage() {
    std::vector<std::pair<int, bool>> states;
    for (const auto& g : groups_) {
        for (const auto& item : g.items) {
            if (item.is_toggle) states.push_back({item.command_id, item.checked});
        }
    }
    RebuildDefaultGroups();
    for (const auto& kv : states) {
        SetToggleState(kv.first, kv.second);
    }
    LayoutItems();
    if (hwnd_) InvalidateRect(hwnd_, nullptr, TRUE);
}

bool XpTaskPane::Create(HWND parent, HINSTANCE hinst, int x, int y, int w, int h) {
    parent_ = parent;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kTaskPaneClass;
    RegisterClassExW(&wc);

    font_normal_ = CreateXpUiFont(9, false, false);
    font_bold_ = CreateXpUiFont(9, true, false);
    font_hover_ = CreateXpUiFont(9, false, true);

    hwnd_ = CreateWindowExW(
        0,
        kTaskPaneClass,
        L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        x, y, w, h,
        parent,
        nullptr,
        hinst,
        this);

    LayoutItems();
    return hwnd_ != nullptr;
}

void XpTaskPane::SetPalette(const XpPalette& palette) {
    palette_ = palette;
    if (hwnd_) InvalidateRect(hwnd_, nullptr, TRUE);
}

void XpTaskPane::SetToggleState(int command_id, bool checked) {
    for (auto& g : groups_) {
        for (auto& item : g.items) {
            if (item.command_id == command_id && item.is_toggle) {
                item.checked = checked;
            }
        }
    }
    if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
}

void XpTaskPane::Resize(int x, int y, int w, int h) {
    if (!hwnd_) return;
    MoveWindow(hwnd_, x, y, w, h, TRUE);
    LayoutItems();
}

void XpTaskPane::LayoutItems() {
    if (!hwnd_) return;
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    int w = rc.right - rc.left;
    int margin = 12;
    int y = 12;

    for (auto& g : groups_) {
        g.header_rect = {margin, y, w - margin, y + 24};
        y += 24;
        if (g.expanded) {
            y += 6;
            for (auto& item : g.items) {
                item.hit_rect = {margin + 8, y, w - margin - 8, y + 22};
                y += 24;
            }
            y += 8;
        } else {
            for (auto& item : g.items) {
                item.hit_rect = {0, 0, 0, 0};
            }
        }
        y += 12;
    }
}

int XpTaskPane::HitTest(POINT pt, bool& out_is_header) const {
    out_is_header = false;
    int global_item = 0;
    for (size_t gi = 0; gi < groups_.size(); ++gi) {
        if (PtInRect(&groups_[gi].header_rect, pt)) {
            out_is_header = true;
            return static_cast<int>(gi);
        }
        if (groups_[gi].expanded) {
            for (const auto& item : groups_[gi].items) {
                if (PtInRect(&item.hit_rect, pt)) {
                    return global_item;
                }
                ++global_item;
            }
        } else {
            global_item += static_cast<int>(groups_[gi].items.size());
        }
    }
    return -1;
}

void XpTaskPane::Paint(HDC hdc) {
    RECT rc{};
    GetClientRect(hwnd_, &rc);

    // Double buffer to prevent flicker
    HDC memdc = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HGDIOBJ old_bmp = SelectObject(memdc, bmp);

    FillVerticalGradient(memdc, rc, palette_.pane_top, palette_.pane_bottom);
    SetBkMode(memdc, TRANSPARENT);

    int margin = 12;
    int global_item = 0;

    for (size_t gi = 0; gi < groups_.size(); ++gi) {
        const auto& g = groups_[gi];
        // Card header with subtle XP rounded top look
        RECT hdr = g.header_rect;
        FillHorizontalGradient(memdc, hdr, palette_.card_header_left, palette_.card_header_right);

        HPEN border_pen = CreatePen(PS_SOLID, 1, palette_.card_border);
        HGDIOBJ old_pen = SelectObject(memdc, border_pen);
        HGDIOBJ old_brush = SelectObject(memdc, GetStockObject(NULL_BRUSH));

        // Draw card body first if expanded
        if (g.expanded && !g.items.empty()) {
            int body_bottom = g.items.back().hit_rect.bottom + 8;
            RECT body_rc = {hdr.left, hdr.bottom, hdr.right, body_bottom};
            HBRUSH body_brush = CreateSolidBrush(palette_.card_body_bg);
            FillRect(memdc, &body_rc, body_brush);
            DeleteObject(body_brush);
            Rectangle(memdc, body_rc.left, body_rc.top - 1, body_rc.right, body_rc.bottom);
        }

        // Header title text + chevron
        HGDIOBJ old_font = SelectObject(memdc, font_bold_);
        SetTextColor(memdc, palette_.card_header_text);
        RECT txt_rc = {hdr.left + 10, hdr.top, hdr.right - 24, hdr.bottom};
        DrawTextW(memdc, g.title.c_str(), -1, &txt_rc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

        const wchar_t* chev = g.expanded ? L"▲" : L"▼";
        RECT chev_rc = {hdr.right - 24, hdr.top, hdr.right - 6, hdr.bottom};
        DrawTextW(memdc, chev, -1, &chev_rc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);

        // Items
        if (g.expanded) {
            for (const auto& item : g.items) {
                bool hovered = (hover_index_ == global_item);
                SelectObject(memdc, hovered ? font_hover_ : font_normal_);
                SetTextColor(memdc, hovered ? palette_.link_hover : palette_.link_normal);

                RECT ir = item.hit_rect;
                if (item.is_toggle) {
                    RECT box = {ir.left, ir.top + 4, ir.left + 13, ir.top + 17};
                    DrawFrameControl(memdc, &box, DFC_BUTTON,
                                     DFCS_BUTTONCHECK | (item.checked ? DFCS_CHECKED : 0));
                    RECT label_rc = {ir.left + 18, ir.top, ir.right, ir.bottom};
                    DrawTextW(memdc, item.label.c_str(), -1, &label_rc,
                              DT_SINGLELINE | DT_VCENTER | DT_LEFT);
                } else {
                    // Bullet dot
                    RECT bullet_rc = {ir.left, ir.top, ir.left + 12, ir.bottom};
                    DrawTextW(memdc, L"•", -1, &bullet_rc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
                    RECT label_rc = {ir.left + 12, ir.top, ir.right, ir.bottom};
                    DrawTextW(memdc, item.label.c_str(), -1, &label_rc,
                              DT_SINGLELINE | DT_VCENTER | DT_LEFT);
                }
                ++global_item;
            }
        } else {
            global_item += static_cast<int>(g.items.size());
        }

        SelectObject(memdc, old_font);
        SelectObject(memdc, old_brush);
        SelectObject(memdc, old_pen);
        DeleteObject(border_pen);
        (void)margin;
    }

    BitBlt(hdc, 0, 0, rc.right, rc.bottom, memdc, 0, 0, SRCCOPY);
    SelectObject(memdc, old_bmp);
    DeleteObject(bmp);
    DeleteDC(memdc);
}

LRESULT CALLBACK XpTaskPane::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    XpTaskPane* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = static_cast<XpTaskPane*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<XpTaskPane*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) return self->HandleMessage(msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT XpTaskPane::HandleMessage(UINT msg, WPARAM wp, LPARAM lp) {
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
        case WM_MOUSEMOVE: {
            if (!tracking_mouse_) {
                TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd_, 0};
                TrackMouseEvent(&tme);
                tracking_mouse_ = true;
            }
            POINT pt{static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp))};
            bool is_hdr = false;
            int idx = HitTest(pt, is_hdr);
            int new_hover = is_hdr ? (-2 - idx) : idx;
            if (new_hover != hover_index_) {
                hover_index_ = new_hover;
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            SetCursor(LoadCursorW(nullptr, (idx >= 0) ? IDC_HAND : IDC_ARROW));
            return 0;
        }
        case WM_MOUSELEAVE:
            tracking_mouse_ = false;
            if (hover_index_ != -1) {
                hover_index_ = -1;
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONUP: {
            POINT pt{static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp))};
            bool is_hdr = false;
            int idx = HitTest(pt, is_hdr);
            if (is_hdr && idx >= 0 && idx < static_cast<int>(groups_.size())) {
                groups_[idx].expanded = !groups_[idx].expanded;
                LayoutItems();
                InvalidateRect(hwnd_, nullptr, FALSE);
            } else if (!is_hdr && idx >= 0) {
                int cur = 0;
                for (auto& g : groups_) {
                    for (auto& item : g.items) {
                        if (cur == idx) {
                            if (item.is_toggle) {
                                item.checked = !item.checked;
                                InvalidateRect(hwnd_, nullptr, FALSE);
                            }
                            if (parent_) {
                                SendMessageW(parent_, WM_COMMAND,
                                             MAKEWPARAM(item.command_id, item.checked ? 1 : 0),
                                             reinterpret_cast<LPARAM>(hwnd_));
                            }
                            return 0;
                        }
                        ++cur;
                    }
                }
            }
            return 0;
        }
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

} // namespace xpagent

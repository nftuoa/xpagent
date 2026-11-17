#include "ui/tray_icon.hpp"

#include <shellapi.h>
#include <cwchar>

#include "resource.h"
#include "ui/i18n.hpp"

namespace xpagent {

TrayIcon::~TrayIcon() {
    Destroy();
}

bool TrayIcon::Create(HWND owner, HICON icon, const std::wstring& tooltip) {
    owner_ = owner;
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = owner;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_XPAGENT_TRAY;
    nid.hIcon = icon;
    wcsncpy(nid.szTip, tooltip.c_str(), 127);

    added_ = Shell_NotifyIconW(NIM_ADD, &nid) != FALSE;
    return added_;
}

void TrayIcon::Destroy() {
    if (!added_ || !owner_) return;
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = owner_;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    added_ = false;
}

void TrayIcon::ShowBalloon(const std::wstring& title, const std::wstring& text, DWORD info_flags) {
    if (!added_ || !owner_) return;
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = owner_;
    nid.uID = 1;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = info_flags;
    nid.uTimeout = 4000;
    wcsncpy(nid.szInfoTitle, title.c_str(), 63);
    wcsncpy(nid.szInfo, text.c_str(), 255);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void TrayIcon::ShowContextMenu(HWND owner, bool buddy_visible) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, IDM_TRAY_SHOW, Tr().tray_show);
    AppendMenuW(menu, MF_STRING | (buddy_visible ? MF_CHECKED : 0),
                IDM_TRAY_BUDDY, Tr().tray_buddy);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_TRAY_EXIT, Tr().tray_exit);

    POINT pt{};
    GetCursorPos(&pt);
    SetForegroundWindow(owner);
    TrackPopupMenu(menu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, owner, nullptr);
    PostMessageW(owner, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

} // namespace xpagent

#pragma once

#include <windows.h>

#include <string>

namespace xpagent {

constexpr UINT WM_XPAGENT_TRAY = WM_APP + 150;

class TrayIcon {
public:
    TrayIcon() = default;
    ~TrayIcon();

    bool Create(HWND owner, HICON icon, const std::wstring& tooltip);
    void Destroy();
    void ShowBalloon(const std::wstring& title, const std::wstring& text, DWORD info_flags = NIIF_INFO);
    void ShowContextMenu(HWND owner, bool buddy_visible);

private:
    HWND owner_ = nullptr;
    bool added_ = false;
};

} // namespace xpagent

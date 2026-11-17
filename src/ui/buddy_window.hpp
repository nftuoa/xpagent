#pragma once

#include <windows.h>

#include <string>

namespace xpagent {

// Optional Windows XP floating desktop assistant rendered with GDI+ + UpdateLayeredWindow.
class BuddyWindow {
public:
    enum class State {
        Idle,
        Thinking,
        NeedsApproval
    };

    BuddyWindow() = default;
    ~BuddyWindow();

    bool Create(HWND main_hwnd, HINSTANCE hinst);
    void Destroy();
    void SetVisible(bool visible);
    bool IsVisible() const;
    void SetState(State state, const std::wstring& caption = L"");
    void RefreshLanguage();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT HandleMessage(UINT msg, WPARAM wp, LPARAM lp);

    void RenderLayered();

    HWND hwnd_ = nullptr;
    HWND main_hwnd_ = nullptr;
    State state_ = State::Idle;
    std::wstring caption_;
    int anim_tick_ = 0;
    bool visible_ = false;
    bool dragging_ = false;
    POINT drag_offset_{};
};

} // namespace xpagent

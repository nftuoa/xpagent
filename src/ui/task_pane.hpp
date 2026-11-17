#pragma once

#include <windows.h>

#include <string>
#include <vector>

#include "ui/theme_helper.hpp"

namespace xpagent {

struct TaskPaneItem {
    int command_id = 0;
    std::wstring label;
    bool is_toggle = false;
    bool checked = false;
    RECT hit_rect{};
};

struct TaskPaneGroup {
    std::wstring title;
    bool expanded = true;
    RECT header_rect{};
    std::vector<TaskPaneItem> items;
};

class XpTaskPane {
public:
    XpTaskPane();
    ~XpTaskPane();

    bool Create(HWND parent, HINSTANCE hinst, int x, int y, int w, int h);
    HWND Handle() const { return hwnd_; }
    void SetPalette(const XpPalette& palette);
    void SetToggleState(int command_id, bool checked);
    void RefreshLanguage();
    void Resize(int x, int y, int w, int h);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT HandleMessage(UINT msg, WPARAM wp, LPARAM lp);

    void RebuildDefaultGroups();
    void LayoutItems();
    void Paint(HDC hdc);
    int HitTest(POINT pt, bool& out_is_header) const;

    HWND hwnd_ = nullptr;
    HWND parent_ = nullptr;
    XpPalette palette_;
    HFONT font_normal_ = nullptr;
    HFONT font_bold_ = nullptr;
    HFONT font_hover_ = nullptr;
    std::vector<TaskPaneGroup> groups_;
    int hover_index_ = -1; // >= 0: item global idx; < -1: header (-2 - group_idx)
    bool tracking_mouse_ = false;
};

} // namespace xpagent

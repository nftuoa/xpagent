#pragma once

#include <windows.h>

namespace xpagent {

enum class XpThemeStyle {
    LunaBlue,
    LunaOlive,
    LunaSilver,
    Classic
};

struct XpPalette {
    XpThemeStyle style = XpThemeStyle::LunaBlue;

    // Task Pane gradient background
    COLORREF pane_top = RGB(123, 162, 231);
    COLORREF pane_bottom = RGB(99, 117, 214);

    // Card header gradient & body
    COLORREF card_header_left = RGB(255, 255, 255);
    COLORREF card_header_right = RGB(198, 211, 247);
    COLORREF card_header_text = RGB(33, 93, 198);
    COLORREF card_body_bg = RGB(214, 223, 247);
    COLORREF card_border = RGB(255, 255, 255);

    // Links & accents
    COLORREF link_normal = RGB(33, 93, 198);
    COLORREF link_hover = RGB(66, 142, 255);
    COLORREF accent_bar = RGB(49, 106, 197);
    COLORREF chat_bg = RGB(255, 255, 255);
    COLORREF input_panel_bg = RGB(236, 233, 216);
    COLORREF wizard_banner_bg = RGB(255, 255, 255);
};

XpPalette QueryCurrentXpPalette();

void FillVerticalGradient(HDC hdc, const RECT& rc, COLORREF top, COLORREF bottom);
void FillHorizontalGradient(HDC hdc, const RECT& rc, COLORREF left, COLORREF right);

HFONT CreateXpUiFont(int point_size = 9, bool bold = false, bool underline = false);
HFONT CreateXpMonoFont(int point_size = 9);

} // namespace xpagent

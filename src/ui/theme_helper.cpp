#include "ui/theme_helper.hpp"

#include <uxtheme.h>
#include <cwchar>

namespace xpagent {

namespace {

bool ContainsCaseInsensitive(const wchar_t* haystack, const wchar_t* needle) {
    if (!haystack || !needle) return false;
    wchar_t lower_h[260] = {0};
    wcsncpy(lower_h, haystack, 259);
    _wcslwr(lower_h);
    return wcsstr(lower_h, needle) != nullptr;
}

} // namespace

XpPalette QueryCurrentXpPalette() {
    XpPalette p;
    if (!IsThemeActive()) {
        p.style = XpThemeStyle::Classic;
        p.pane_top = GetSysColor(COLOR_3DFACE);
        p.pane_bottom = GetSysColor(COLOR_3DSHADOW);
        p.card_header_left = GetSysColor(COLOR_ACTIVECAPTION);
        p.card_header_right = GetSysColor(COLOR_GRADIENTACTIVECAPTION);
        p.card_header_text = GetSysColor(COLOR_CAPTIONTEXT);
        p.card_body_bg = GetSysColor(COLOR_WINDOW);
        p.card_border = GetSysColor(COLOR_3DSHADOW);
        p.link_normal = GetSysColor(COLOR_HIGHLIGHT);
        p.link_hover = RGB(0, 102, 204);
        p.accent_bar = GetSysColor(COLOR_HIGHLIGHT);
        p.input_panel_bg = GetSysColor(COLOR_3DFACE);
        return p;
    }

    wchar_t theme_file[MAX_PATH] = {0};
    wchar_t color_scheme[128] = {0};
    if (SUCCEEDED(GetCurrentThemeName(theme_file, MAX_PATH, color_scheme, 128, nullptr, 0))) {
        if (ContainsCaseInsensitive(color_scheme, L"homestead")) {
            p.style = XpThemeStyle::LunaOlive;
            p.pane_top = RGB(165, 189, 132);
            p.pane_bottom = RGB(141, 164, 108);
            p.card_header_left = RGB(255, 255, 255);
            p.card_header_right = RGB(216, 226, 186);
            p.card_header_text = RGB(86, 102, 45);
            p.card_body_bg = RGB(246, 246, 236);
            p.link_normal = RGB(86, 102, 45);
            p.link_hover = RGB(114, 146, 29);
            p.accent_bar = RGB(130, 152, 86);
            p.input_panel_bg = RGB(236, 233, 216);
            return p;
        }
        if (ContainsCaseInsensitive(color_scheme, L"metallic")) {
            p.style = XpThemeStyle::LunaSilver;
            p.pane_top = RGB(196, 200, 212);
            p.pane_bottom = RGB(164, 168, 185);
            p.card_header_left = RGB(255, 255, 255);
            p.card_header_right = RGB(214, 215, 224);
            p.card_header_text = RGB(63, 61, 61);
            p.card_body_bg = RGB(240, 241, 245);
            p.link_normal = RGB(51, 51, 92);
            p.link_hover = RGB(0, 102, 204);
            p.accent_bar = RGB(124, 124, 148);
            p.input_panel_bg = RGB(224, 225, 230);
            return p;
        }
    }

    // Default: Windows XP Luna Blue
    p.style = XpThemeStyle::LunaBlue;
    return p;
}

void FillVerticalGradient(HDC hdc, const RECT& rc, COLORREF top, COLORREF bottom) {
    TRIVERTEX v[2]{};
    v[0].x = rc.left;
    v[0].y = rc.top;
    v[0].Red = static_cast<COLOR16>(GetRValue(top) << 8);
    v[0].Green = static_cast<COLOR16>(GetGValue(top) << 8);
    v[0].Blue = static_cast<COLOR16>(GetBValue(top) << 8);
    v[0].Alpha = 0xFF00;

    v[1].x = rc.right;
    v[1].y = rc.bottom;
    v[1].Red = static_cast<COLOR16>(GetRValue(bottom) << 8);
    v[1].Green = static_cast<COLOR16>(GetGValue(bottom) << 8);
    v[1].Blue = static_cast<COLOR16>(GetBValue(bottom) << 8);
    v[1].Alpha = 0xFF00;

    GRADIENT_RECT g{0, 1};
    GradientFill(hdc, v, 2, &g, 1, GRADIENT_FILL_RECT_V);
}

void FillHorizontalGradient(HDC hdc, const RECT& rc, COLORREF left, COLORREF right) {
    TRIVERTEX v[2]{};
    v[0].x = rc.left;
    v[0].y = rc.top;
    v[0].Red = static_cast<COLOR16>(GetRValue(left) << 8);
    v[0].Green = static_cast<COLOR16>(GetGValue(left) << 8);
    v[0].Blue = static_cast<COLOR16>(GetBValue(left) << 8);
    v[0].Alpha = 0xFF00;

    v[1].x = rc.right;
    v[1].y = rc.bottom;
    v[1].Red = static_cast<COLOR16>(GetRValue(right) << 8);
    v[1].Green = static_cast<COLOR16>(GetGValue(right) << 8);
    v[1].Blue = static_cast<COLOR16>(GetBValue(right) << 8);
    v[1].Alpha = 0xFF00;

    GRADIENT_RECT g{0, 1};
    GradientFill(hdc, v, 2, &g, 1, GRADIENT_FILL_RECT_H);
}

HFONT CreateXpUiFont(int point_size, bool bold, bool underline) {
    HDC hdc = GetDC(nullptr);
    int log_pixels = hdc ? GetDeviceCaps(hdc, LOGPIXELSY) : 96;
    if (hdc) ReleaseDC(nullptr, hdc);

    LOGFONTW lf{};
    NONCLIENTMETRICSW ncm{};
    ncm.cbSize = sizeof(NONCLIENTMETRICSW);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0)) {
        lf = ncm.lfMessageFont;
    } else {
        wcscpy(lf.lfFaceName, L"Tahoma");
    }

    lf.lfHeight = -MulDiv(point_size, log_pixels, 72);
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    lf.lfUnderline = underline ? TRUE : FALSE;
    lf.lfQuality = DEFAULT_QUALITY;
    return CreateFontIndirectW(&lf);
}

HFONT CreateXpMonoFont(int point_size) {
    HDC hdc = GetDC(nullptr);
    int log_pixels = hdc ? GetDeviceCaps(hdc, LOGPIXELSY) : 96;
    if (hdc) ReleaseDC(nullptr, hdc);

    LOGFONTW lf{};
    lf.lfHeight = -MulDiv(point_size, log_pixels, 72);
    lf.lfWeight = FW_NORMAL;
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    wcscpy(lf.lfFaceName, L"Lucida Console");
    return CreateFontIndirectW(&lf);
}

} // namespace xpagent

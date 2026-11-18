#pragma once
#include <windows.h>
#include "UiScale.h"

// Small vector icons use the current control color and remain legible when disabled.
enum class UiIcon {
    None,
    Play,
    Stop,
    Close,
    Restart,
    Folder,
    Add,
    Trash,
    Save,
    Search,
    Console,
    Home,
    Settings,
    Users,
    Plugin,
    Backup,
    Clock,
    Image,
    Help,
    File,
    Send
};
// Microsoft's MDL2 glyph library ships with supported Windows versions.
inline void drawUiIcon(HDC dc, UiIcon icon, int x, int y, COLORREF color) {
    // Glyph font for the current UI scale; recreated only if the DPI changes.
    static HFONT glyphFont = nullptr;
    static int glyphDpi = 0;
    if (glyphDpi != UiDpi) {
        if (glyphFont)
            DeleteObject(glyphFont);
        glyphDpi = UiDpi;
        glyphFont = CreateFontW(-S(17), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe MDL2 Assets");
    }
    constexpr wchar_t glyphs[]{0,      0xE768, 0xE71A, 0xE711, 0xE72C, 0xE8B7, 0xE710,
                               0xE74D, 0xE74E, 0xE721, 0xE756, 0xE80F, 0xE713, 0xE716,
                               0xEA86, 0xE8F1, 0xE823, 0xE91B, 0xE897, 0xE8A5, 0xE724};
    auto i = static_cast<unsigned>(icon);
    if (!i || i >= sizeof(glyphs) / sizeof(glyphs[0]))
        return;
    int saved = SaveDC(dc);
    SelectObject(dc, glyphFont);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    RECT r{x, y - S(1), x + S(18), y + S(18)};
    DrawTextW(dc, &glyphs[i], 1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    RestoreDC(dc, saved);
}

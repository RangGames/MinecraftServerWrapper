#pragma once
#include <windows.h>
#include <string>
struct MessagePalette {
    COLORREF background, panel, border, text, muted, accent;
    bool light;
};
int showThemedMessage(HWND owner, const std::wstring &message, const std::wstring &title, UINT flags,
                      HFONT font, HFONT heading, const MessagePalette &palette, bool english);

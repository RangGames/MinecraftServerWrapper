#pragma once
#include <windows.h>
#include <algorithm>
#include "UiScale.h"
// Keep native single-line EDIT behavior; center its client area rather than drawing text ourselves.
inline void centerEditClient(HWND h, WPARAM full, LPARAM data) {
    auto &r = full ? reinterpret_cast<NCCALCSIZE_PARAMS *>(data)->rgrc[0] : *reinterpret_cast<RECT *>(data);
    HDC dc = GetDC(h);
    auto font = reinterpret_cast<HFONT>(SendMessageW(h, WM_GETFONT, 0, 0));
    auto old = SelectObject(dc, font ? font : GetStockObject(DEFAULT_GUI_FONT));
    TEXTMETRICW metrics{};
    GetTextMetricsW(dc, &metrics);
    SelectObject(dc, old);
    ReleaseDC(h, dc);
    int available = r.bottom - r.top, height = std::min(static_cast<int>(metrics.tmHeight), available);
    r.top += std::max(0, (available - height) / 2);
    r.bottom = r.top + height;
}
// underline: optional 2px bottom stroke (Fluent focus indicator); CLR_INVALID to omit.
inline void paintEditFrame(HWND h, COLORREF background, COLORREF border, COLORREF underline = CLR_INVALID) {
    HDC dc = GetWindowDC(h);
    RECT window{}, client{};
    GetWindowRect(h, &window);
    GetClientRect(h, &client);
    POINT origin{};
    ClientToScreen(h, &origin);
    OffsetRect(&client, origin.x - window.left, origin.y - window.top);
    int saved = SaveDC(dc);
    ExcludeClipRect(dc, client.left, client.top, client.right, client.bottom);
    RECT r{0, 0, window.right - window.left, window.bottom - window.top};
    auto brush = CreateSolidBrush(background);
    FillRect(dc, &r, brush);
    DeleteObject(brush);
    brush = CreateSolidBrush(border);
    FrameRect(dc, &r, brush);
    DeleteObject(brush);
    if (underline != CLR_INVALID) {
        RECT line{r.left, r.bottom - S(2), r.right, r.bottom};
        brush = CreateSolidBrush(underline);
        FillRect(dc, &line, brush);
        DeleteObject(brush);
    }
    RestoreDC(dc, saved);
    ReleaseDC(h, dc);
}
inline void refreshEditFrame(HWND h) {
    SetWindowPos(h, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    RedrawWindow(h, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME);
}

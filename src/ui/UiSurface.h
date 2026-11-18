#pragma once
#include <windows.h>
#include <algorithm>
#include <objidl.h>
#include <gdiplus.h>
inline void roundedSurface(HDC dc, RECT r, COLORREF background, COLORREF outline, int radius = 10) {
    if (r.right <= r.left || r.bottom <= r.top)
        return;
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    auto color = [](COLORREF c) { return Gdiplus::Color(255, GetRValue(c), GetGValue(c), GetBValue(c)); };
    Gdiplus::GraphicsPath path;
    float x = r.left + 0.5f, y = r.top + 0.5f, w = r.right - r.left - 1.0f, h = r.bottom - r.top - 1.0f,
          d = static_cast<float>(radius * 2);
    d = std::min(d, std::min(w, h));
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d, y, d, d, 270, 90);
    path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    path.AddArc(x, y + h - d, d, d, 90, 90);
    path.CloseFigure();
    Gdiplus::SolidBrush brush(color(background));
    g.FillPath(&brush, &path);
    if (outline != background) {
        Gdiplus::Pen pen(color(outline), 1.0f);
        g.DrawPath(&pen, &path);
    }
}

#pragma once
#include <windows.h>
#include <algorithm>
#include <objidl.h>
#include <gdiplus.h>
// Rounded rectangle with an anti-aliased outline. Large areas are filled with plain GDI and only the
// corners go through GDI+, which keeps full-window repaints (cards, content layer) cheap.
inline void roundedSurface(HDC dc, RECT r, COLORREF background, COLORREF outline, int radius = 10) {
    if (r.right <= r.left || r.bottom <= r.top)
        return;
    int width = r.right - r.left, height = r.bottom - r.top;
    radius = std::max(0, std::min(radius, std::min(width, height) / 2));
    // Interior cross: everything except the four corner squares.
    SetDCBrushColor(dc, background);
    auto brush = static_cast<HBRUSH>(GetStockObject(DC_BRUSH));
    RECT horizontal{r.left, r.top + radius, r.right, r.bottom - radius};
    RECT vertical{r.left + radius, r.top, r.right - radius, r.bottom};
    FillRect(dc, &horizontal, brush);
    FillRect(dc, &vertical, brush);
    if (radius == 0 && outline == background)
        return;
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    auto color = [](COLORREF c) { return Gdiplus::Color(255, GetRValue(c), GetGValue(c), GetBValue(c)); };
    Gdiplus::GraphicsPath path;
    float x = r.left + 0.5f, y = r.top + 0.5f, w = width - 1.0f, h = height - 1.0f,
          d = static_cast<float>(radius * 2);
    d = std::max(1.0f, std::min(d, std::min(w, h)));
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d, y, d, d, 270, 90);
    path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    path.AddArc(x, y + h - d, d, d, 90, 90);
    path.CloseFigure();
    if (radius > 0) {
        // Fill only the corner squares with anti-aliasing.
        Gdiplus::Region corners(Gdiplus::Rect(r.left, r.top, radius, radius));
        corners.Union(Gdiplus::Rect(r.right - radius, r.top, radius, radius));
        corners.Union(Gdiplus::Rect(r.left, r.bottom - radius, radius, radius));
        corners.Union(Gdiplus::Rect(r.right - radius, r.bottom - radius, radius, radius));
        g.SetClip(&corners);
        Gdiplus::SolidBrush fillBrush(color(background));
        g.FillPath(&fillBrush, &path);
        g.ResetClip();
    }
    if (outline != background) {
        Gdiplus::Pen pen(color(outline), 1.0f);
        g.DrawPath(&pen, &path);
    }
}

#include "App.h"
#include "UiInput.h"
#include "UiSurface.h"
namespace wrapper {
void App::drawCheckbox(HWND hwnd, HDC dc) {
    int saved = SaveDC(dc);
    RECT rect{};
    GetClientRect(hwnd, &rect);
    fill(dc, rect, backdrop(hwnd));
    bool checked = SendMessageW(hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED,
         enabled = IsWindowEnabled(hwnd) != FALSE;
    int y = (rect.bottom - 20) / 2;
    // Fluent check box: rounded square, accent fill when checked, neutral stroke otherwise.
    COLORREF fillColor = checked ? (enabled ? Accent : Muted) : Field;
    COLORREF stroke = checked ? fillColor : GetFocus() == hwnd ? Accent : enabled ? Muted : Border;
    roundedSurface(dc, {1, y, 21, y + 20}, fillColor, stroke, 4);
    if (checked) {
        HPEN tick = CreatePen(PS_SOLID, 2, OnAccent);
        auto oldPen = SelectObject(dc, tick);
        MoveToEx(dc, 6, y + 10, nullptr);
        LineTo(dc, 9, y + 13);
        LineTo(dc, 16, y + 6);
        SelectObject(dc, oldPen);
        DeleteObject(tick);
    }
    auto value = text(hwnd);
    rect.left = 30;
    rect.right -= 4;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, enabled ? Foreground : Muted);
    SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(hwnd, WM_GETFONT, 0, 0)));
    DrawTextW(dc, value.c_str(), -1, &rect, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    RestoreDC(dc, saved);
}

LRESULT CALLBACK App::inputProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR data) {
    auto *app = reinterpret_cast<App *>(data);
    // Classify the control once; this procedure runs for every message of ~100 controls.
    enum Kind : INT_PTR { Other = 1, Checkbox, SingleLineEdit, Combo };
    auto kind = reinterpret_cast<INT_PTR>(GetPropW(hwnd, L"WrapperKind"));
    if (!kind) {
        wchar_t className[32]{};
        GetClassNameW(hwnd, className, 32);
        auto style = GetWindowLongPtrW(hwnd, GWL_STYLE);
        kind = _wcsicmp(className, L"BUTTON") == 0 && (style & BS_TYPEMASK) == BS_AUTOCHECKBOX ? Checkbox
               : _wcsicmp(className, L"EDIT") == 0 && !(style & ES_MULTILINE)                ? SingleLineEdit
               : _wcsicmp(className, L"COMBOBOX") == 0                                       ? Combo
                                                                                             : Other;
        SetPropW(hwnd, L"WrapperKind", reinterpret_cast<HANDLE>(kind));
    }
    bool checkbox = kind == Checkbox, edit = kind == SingleLineEdit;
    if (edit && msg == WM_NCCALCSIZE) {
        auto result = DefSubclassProc(hwnd, msg, w, l);
        centerEditClient(hwnd, w, l);
        return result;
    }
    if (edit && msg == WM_NCPAINT) {
        bool focused = GetFocus() == hwnd;
        paintEditFrame(hwnd, Field, focused ? Accent : Border, focused ? Accent : CLR_INVALID);
        return 0;
    }
    if (checkbox && msg == WM_ERASEBKGND)
        return 1;
    if (checkbox && msg == WM_PAINT) {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);
        app->drawCheckbox(hwnd, dc);
        EndPaint(hwnd, &paint);
        return 0;
    }
    if (checkbox && (msg == WM_PRINT || msg == WM_PRINTCLIENT)) {
        app->drawCheckbox(hwnd, reinterpret_cast<HDC>(w));
        return 0;
    }
    if (msg == WM_MOUSEMOVE) {
        if (!GetPropW(hwnd, L"Hover")) {
            SetPropW(hwnd, L"Hover", reinterpret_cast<HANDLE>(1));
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&track);
    }
    if (msg == WM_MOUSELEAVE)
        RemovePropW(hwnd, L"Hover");
    if (msg == WM_NCDESTROY) {
        RemovePropW(hwnd, L"Hover");
        RemovePropW(hwnd, L"WrapperKind");
    }
    if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS || msg == WM_MOUSELEAVE || msg == WM_ENABLE)
        InvalidateRect(hwnd, nullptr, FALSE);
    auto result = DefSubclassProc(hwnd, msg, w, l);
    if (checkbox && (msg == BM_SETCHECK || msg == WM_LBUTTONUP || msg == WM_KEYUP))
        InvalidateRect(hwnd, nullptr, FALSE);
    if (edit && msg == WM_SIZE)
        SendMessageW(hwnd, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(10, 10));
    if (edit && msg == WM_SETFONT)
        refreshEditFrame(hwnd);
    if (edit && (msg == WM_SETFOCUS || msg == WM_KILLFOCUS || msg == WM_ENABLE))
        RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME);
    if (msg == WM_PAINT && kind == Combo) {
        {
            HDC dc = GetDC(hwnd);
            RECT rect{};
            GetClientRect(hwnd, &rect);
            bool focused = GetFocus() == hwnd;
            HBRUSH border = CreateSolidBrush(focused ? Accent : Border);
            FrameRect(dc, &rect, border);
            DeleteObject(border);
            if (focused) {
                RECT underline{rect.left, rect.bottom - 2, rect.right, rect.bottom};
                fill(dc, underline, Accent);
            }
            RECT arrow{rect.right - 32, rect.top + 1, rect.right - 1, rect.bottom - (focused ? 2 : 1)};
            fill(dc, arrow, Field);
            HPEN pen = CreatePen(PS_SOLID, 1, IsWindowEnabled(hwnd) ? Muted : Border);
            auto old = SelectObject(dc, pen);
            int x = (arrow.left + arrow.right) / 2, y = (arrow.top + arrow.bottom) / 2;
            MoveToEx(dc, x - 4, y - 2, nullptr);
            LineTo(dc, x, y + 2);
            LineTo(dc, x + 5, y - 3);
            SelectObject(dc, old);
            DeleteObject(pen);
            ReleaseDC(hwnd, dc);
        }
    }
    (void)app;
    return result;
}
} // namespace wrapper

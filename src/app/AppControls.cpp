#include "App.h"
#include "UiInput.h"
namespace wrapper {
void App::drawCheckbox(HWND hwnd, HDC dc) {
    int saved = SaveDC(dc);
    RECT rect{};
    GetClientRect(hwnd, &rect);
    fill(dc, rect, Surface);
    bool checked = SendMessageW(hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED,
         enabled = IsWindowEnabled(hwnd) != FALSE;
    int y = (rect.bottom - 18) / 2;
    RECT box{2, y, 20, y + 18};
    HBRUSH brush = CreateSolidBrush(checked && enabled ? Accent : Panel);
    HPEN pen = CreatePen(PS_SOLID, 1, GetFocus() == hwnd ? Accent : Border);
    auto oldBrush = SelectObject(dc, brush);
    auto oldPen = SelectObject(dc, pen);
    RoundRect(dc, box.left, box.top, box.right, box.bottom, 5, 5);
    if (checked) {
        HPEN tick = CreatePen(PS_SOLID, 2, enabled ? RGB(255, 255, 255) : Muted);
        SelectObject(dc, tick);
        MoveToEx(dc, 6, y + 9, nullptr);
        LineTo(dc, 10, y + 13);
        LineTo(dc, 17, y + 5);
        SelectObject(dc, pen);
        DeleteObject(tick);
    }
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
    auto value = text(hwnd);
    rect.left = 28;
    rect.right -= 4;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, enabled ? Foreground : Muted);
    SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(hwnd, WM_GETFONT, 0, 0)));
    DrawTextW(dc, value.c_str(), -1, &rect, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    RestoreDC(dc, saved);
}

LRESULT CALLBACK App::inputProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR data) {
    auto *app = reinterpret_cast<App *>(data);
    wchar_t className[32]{};
    GetClassNameW(hwnd, className, 32);
    bool checkbox = _wcsicmp(className, L"BUTTON") == 0 &&
                    (GetWindowLongPtrW(hwnd, GWL_STYLE) & BS_TYPEMASK) == BS_AUTOCHECKBOX;
    bool edit = _wcsicmp(className, L"EDIT") == 0 && !(GetWindowLongPtrW(hwnd, GWL_STYLE) & ES_MULTILINE);
    if (edit && msg == WM_NCCALCSIZE) {
        auto result = DefSubclassProc(hwnd, msg, w, l);
        centerEditClient(hwnd, w, l);
        return result;
    }
    if (edit && msg == WM_NCPAINT) {
        paintEditFrame(hwnd, Panel, GetFocus() == hwnd ? Accent : Border);
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
    if (msg == WM_NCDESTROY)
        RemovePropW(hwnd, L"Hover");
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
    if (msg == WM_PAINT) {
        wchar_t type[32]{};
        GetClassNameW(hwnd, type, 32);
        if (_wcsicmp(type, L"COMBOBOX") == 0) {
            HDC dc = GetDC(hwnd);
            RECT rect{};
            GetClientRect(hwnd, &rect);
            HBRUSH border = CreateSolidBrush(GetFocus() == hwnd ? Accent : Border);
            FrameRect(dc, &rect, border);
            DeleteObject(border);
            if (_wcsicmp(type, L"COMBOBOX") == 0) {
                RECT arrow{rect.right - 26, rect.top + 1, rect.right - 1, rect.bottom - 1};
                fill(dc, arrow, Surface);
                HPEN pen = CreatePen(PS_SOLID, 1, IsWindowEnabled(hwnd) ? Foreground : Muted);
                auto old = SelectObject(dc, pen);
                int x = (arrow.left + arrow.right) / 2, y = (arrow.top + arrow.bottom) / 2;
                MoveToEx(dc, x - 4, y - 2, nullptr);
                LineTo(dc, x, y + 2);
                LineTo(dc, x + 4, y - 2);
                SelectObject(dc, old);
                DeleteObject(pen);
            }
            ReleaseDC(hwnd, dc);
        }
    }
    (void)app;
    return result;
}
} // namespace wrapper

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
    int box = S(20), y = (rect.bottom - box) / 2;
    // Fluent check box: rounded square, accent fill when checked, neutral stroke otherwise.
    COLORREF fillColor = checked ? (enabled ? Accent : Muted) : Field;
    COLORREF stroke = checked ? fillColor : GetFocus() == hwnd ? Accent : enabled ? Muted : Border;
    roundedSurface(dc, {S(1), y, S(1) + box, y + box}, fillColor, stroke, S(4));
    if (checked) {
        HPEN tick = CreatePen(PS_SOLID, S(2), OnAccent);
        auto oldPen = SelectObject(dc, tick);
        MoveToEx(dc, S(6), y + S(10), nullptr);
        LineTo(dc, S(9), y + S(13));
        LineTo(dc, S(16), y + S(6));
        SelectObject(dc, oldPen);
        DeleteObject(tick);
    }
    auto value = text(hwnd);
    rect.left = S(30);
    rect.right -= 4;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, enabled ? Foreground : Muted);
    SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(hwnd, WM_GETFONT, 0, 0)));
    DrawTextW(dc, value.c_str(), -1, &rect, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    RestoreDC(dc, saved);
}

void App::drawCombo(HWND hwnd, HDC target) {
    RECT rect{};
    GetClientRect(hwnd, &rect);
    int width = std::max(1L, rect.right), height = std::max(1L, rect.bottom);
    HDC dc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, width, height);
    auto oldBitmap = SelectObject(dc, bitmap);
    bool enabled = IsWindowEnabled(hwnd) != FALSE, focused = GetFocus() == hwnd,
         open = SendMessageW(hwnd, CB_GETDROPPEDSTATE, 0, 0) != FALSE,
         hover = GetPropW(hwnd, L"Hover") != nullptr;
    COLORREF base = backdrop(hwnd);
    fill(dc, rect, base);
    COLORREF face = !enabled ? blend(Field, base, 0.5) : (hover || open) && !focused ? ControlHover : Field;
    roundedSurface(dc, rect, face, focused || open ? Accent : Border, S(5));
    if (focused || open) {
        RECT underline{rect.left + S(3), rect.bottom - S(2), rect.right - S(3), rect.bottom};
        fill(dc, underline, Accent);
    }
    // Selected item text.
    auto index = SendMessageW(hwnd, CB_GETCURSEL, 0, 0);
    if (index != CB_ERR) {
        auto length = SendMessageW(hwnd, CB_GETLBTEXTLEN, index, 0);
        if (length > 0 && length < 32768) {
            std::wstring label(static_cast<size_t>(length) + 1, 0);
            SendMessageW(hwnd, CB_GETLBTEXT, index, reinterpret_cast<LPARAM>(label.data()));
            label.resize(static_cast<size_t>(length));
            RECT textRect{rect.left + S(10), rect.top, rect.right - S(34), rect.bottom};
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, enabled ? Foreground : Muted);
            auto font = reinterpret_cast<HFONT>(SendMessageW(hwnd, WM_GETFONT, 0, 0));
            auto oldFont = SelectObject(dc, font ? font : this->font);
            DrawTextW(dc, label.c_str(), -1, &textRect, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            SelectObject(dc, oldFont);
        }
    }
    // Chevron.
    HPEN pen = CreatePen(PS_SOLID, std::max(1, S(1)), enabled ? Muted : Border);
    auto oldPen = SelectObject(dc, pen);
    int x = rect.right - S(18), y = (rect.top + rect.bottom) / 2;
    MoveToEx(dc, x - S(4), y - S(2), nullptr);
    LineTo(dc, x, y + S(2));
    LineTo(dc, x + S(5), y - S(3));
    SelectObject(dc, oldPen);
    DeleteObject(pen);
    BitBlt(target, 0, 0, width, height, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
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
    bool checkbox = kind == Checkbox, edit = kind == SingleLineEdit, combo = kind == Combo;
    // Closed drop-down lists are painted entirely here. Letting the native control paint first and
    // drawing over it showed the classic frame and arrow for a moment (page switches, clicks, focus).
    if (combo && msg == WM_ERASEBKGND)
        return 1;
    if (combo && (msg == WM_PAINT || msg == WM_PRINTCLIENT)) {
        PAINTSTRUCT paint{};
        HDC target = msg == WM_PAINT ? BeginPaint(hwnd, &paint) : reinterpret_cast<HDC>(w);
        app->drawCombo(hwnd, target);
        if (msg == WM_PAINT)
            EndPaint(hwnd, &paint);
        return 0;
    }
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
        SendMessageW(hwnd, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(S(10), S(10)));
    if (edit && msg == WM_SETFONT)
        refreshEditFrame(hwnd);
    if (edit && (msg == WM_SETFOCUS || msg == WM_KILLFOCUS || msg == WM_ENABLE))
        RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME);
    // The native control repaints itself directly (not via WM_PAINT) on clicks, keys, selection and
    // focus changes; repaint ours immediately so the classic look never shows.
    if (combo && (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP || msg == WM_KEYDOWN || msg == WM_KEYUP ||
                  msg == WM_SETFOCUS || msg == WM_KILLFOCUS || msg == WM_ENABLE || msg == WM_CAPTURECHANGED ||
                  msg == CB_SETCURSEL || msg == WM_SETTEXT || msg == CB_SHOWDROPDOWN || msg == WM_MOUSELEAVE))
        RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    (void)app;
    return result;
}
} // namespace wrapper

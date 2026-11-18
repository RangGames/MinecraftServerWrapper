#include "UiMessageBox.h"
#include "Localization.h"
#include "UiScale.h"
#include "UiSurface.h"
#include <commctrl.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <algorithm>
namespace {
struct Message {
    std::wstring body, title;
    UINT flags;
    HFONT font = nullptr, heading = nullptr;
    MessagePalette colors;
    bool english, eula = false;
    HBRUSH background = nullptr;
    ~Message() {
        DeleteObject(font);
        DeleteObject(heading);
        DeleteObject(background);
    }
    std::wstring tr(const std::wstring &value) const { return localize(value, english); }
    bool confirm() const { return (flags & MB_TYPEMASK) == MB_YESNO; }
};
void fillBox(HDC dc, RECT rect, COLORREF color) {
    SetDCBrushColor(dc, color);
    FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}
COLORREF contrastText(COLORREF background) {
    return GetRValue(background) * 299 + GetGValue(background) * 587 + GetBValue(background) * 114 > 155000
               ? RGB(20, 25, 30)
               : RGB(255, 255, 255);
}
INT_PTR CALLBACK messageProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    auto *data = reinterpret_cast<Message *>(GetWindowLongPtrW(hwnd, DWLP_USER));
    if (msg == WM_INITDIALOG) {
        data = reinterpret_cast<Message *>(l);
        SetWindowLongPtrW(hwnd, DWLP_USER, l);
        SetWindowTextW(hwnd, data->title.c_str());
        auto add = [&](const wchar_t *type, const std::wstring &value, DWORD style, int id, int x, int y,
                       int width, int height) {
            HWND child = CreateWindowExW(0, type, value.c_str(), WS_CHILD | WS_VISIBLE | style, x, y, width,
                                         height, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                         GetModuleHandleW(nullptr), nullptr);
            SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(id == 10 ? data->heading : data->font),
                         TRUE);
            SetWindowTheme(child, L"", L"");
            return child;
        };
        MONITORINFO monitor{};
        monitor.cbSize = sizeof(monitor);
        GetMonitorInfoW(MonitorFromWindow(GetWindow(hwnd, GW_OWNER), MONITOR_DEFAULTTONEAREST), &monitor);
        int width = std::clamp<LONG>(monitor.rcWork.right - monitor.rcWork.left - S(40), S(360), S(640));
        HDC dc = GetDC(hwnd);
        auto previous = SelectObject(dc, data->font);
        RECT measure{0, 0, width - S(112), 0};
        DrawTextW(dc, data->body.c_str(), -1, &measure, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
        SelectObject(dc, previous);
        ReleaseDC(hwnd, dc);
        int bodyHeight = std::clamp<LONG>(
            measure.bottom + S(12), S(70), std::max<LONG>(S(70), monitor.rcWork.bottom - monitor.rcWork.top - S(240)));
        int height = bodyHeight + S(150);
        RECT frame{0, 0, width, height};
        AdjustWindowRectEx(&frame, GetWindowLongW(hwnd, GWL_STYLE), FALSE, GetWindowLongW(hwnd, GWL_EXSTYLE));
        RECT owner{};
        GetWindowRect(GetWindow(hwnd, GW_OWNER), &owner);
        int outerW = frame.right - frame.left, outerH = frame.bottom - frame.top;
        int x = std::clamp((owner.left + owner.right - outerW) / 2, monitor.rcWork.left,
                           std::max(monitor.rcWork.left, monitor.rcWork.right - outerW));
        int y = std::clamp((owner.top + owner.bottom - outerH) / 2, monitor.rcWork.top,
                           std::max(monitor.rcWork.top, monitor.rcWork.bottom - outerH));
        SetWindowPos(hwnd, nullptr, x, y, outerW, outerH, SWP_NOZORDER);
        auto heading = data->tr(data->confirm() ? L"확인" : (data->flags & MB_ICONERROR) ? L"오류" : L"안내");
        add(L"STATIC", heading, 0, 10, S(72), S(22), width - S(96), S(30));
        add(L"EDIT", data->body,
            ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | (measure.bottom > bodyHeight ? WS_VSCROLL : 0), 11,
            S(72), S(66), width - S(100), bodyHeight);
        int buttonY = height - S(52);
        if (data->eula)
            add(L"BUTTON", data->tr(L"EULA 열기"), WS_TABSTOP | BS_OWNERDRAW, 8, S(24), buttonY, S(120), S(34));
        if (data->confirm()) {
            add(L"BUTTON", data->tr(L"확인"), WS_TABSTOP | BS_OWNERDRAW, IDYES, width - S(260), buttonY,
                S(112), S(34));
            add(L"BUTTON", data->tr(L"취소"), WS_TABSTOP | BS_OWNERDRAW, IDNO, width - S(136), buttonY, S(112), S(34));
        } else
            add(L"BUTTON", data->english ? L"OK" : L"확인", WS_TABSTOP | BS_OWNERDRAW, IDOK, width - S(136),
                buttonY, S(112), S(34));
        int defaultId = data->confirm() ? IDNO : IDOK;
        SendMessageW(hwnd, DM_SETDEFID, defaultId, 0);
        SetFocus(GetDlgItem(hwnd, defaultId));
        BOOL dark = !data->colors.light;
        DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
        DwmSetWindowAttribute(hwnd, 35, &data->colors.background, sizeof(COLORREF));
        DwmSetWindowAttribute(hwnd, 36, &data->colors.text, sizeof(COLORREF));
        return FALSE;
    }
    if (!data)
        return FALSE;
    switch (msg) {
    case WM_COMMAND: {
        int id = LOWORD(w);
        if (id == 8) {
            ShellExecuteW(hwnd, L"open", L"https://www.minecraft.net/eula", nullptr, nullptr, SW_SHOWNORMAL);
            return TRUE;
        }
        if (id == IDYES || id == IDNO || id == IDOK || id == IDCANCEL) {
            EndDialog(hwnd, id == IDCANCEL ? (data->confirm() ? IDNO : IDOK) : id);
            return TRUE;
        }
        break;
    }
    case WM_CLOSE:
        EndDialog(hwnd, data->confirm() ? IDNO : IDOK);
        return TRUE;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(w);
        SetTextColor(dc, data->colors.text);
        SetBkColor(dc, data->colors.background);
        return reinterpret_cast<INT_PTR>(data->background);
    }
    case WM_DRAWITEM: {
        auto &item = *reinterpret_cast<DRAWITEMSTRUCT *>(l);
        HDC dc = item.hDC;
        int saved = SaveDC(dc);
        fillBox(dc, item.rcItem, data->colors.panel);
        bool focus = (item.itemState & ODS_FOCUS) != 0, pressed = (item.itemState & ODS_SELECTED) != 0;
        // Fluent dialog buttons: the affirmative action uses the accent color.
        bool primary = item.CtlID == IDOK || item.CtlID == IDYES;
        COLORREF background = primary ? data->colors.accent : data->colors.background;
        if (pressed)
            background = RGB((GetRValue(background) * 4 + GetRValue(data->colors.panel)) / 5,
                             (GetGValue(background) * 4 + GetGValue(data->colors.panel)) / 5,
                             (GetBValue(background) * 4 + GetBValue(data->colors.panel)) / 5);
        roundedSurface(dc, item.rcItem, background,
                       focus ? data->colors.text : primary ? background : data->colors.border, S(6));
        wchar_t label[128]{};
        GetWindowTextW(item.hwndItem, label, 128);
        RECT rect = item.rcItem;
        SelectObject(dc, data->font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, contrastText(background));
        DrawTextW(dc, label, -1, &rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
        RestoreDC(dc, saved);
        return TRUE;
    }
    case WM_ERASEBKGND:
        return TRUE;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);
        RECT rect{};
        GetClientRect(hwnd, &rect);
        fillBox(dc, rect, data->colors.background);
        RECT bottom = rect;
        bottom.top = bottom.bottom - S(72);
        fillBox(dc, bottom, data->colors.panel);
        COLORREF color = (data->flags & MB_ICONERROR) ? RGB(222, 85, 85) : data->colors.accent;
        HBRUSH brush = CreateSolidBrush(color);
        auto oldBrush = SelectObject(dc, brush);
        auto oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
        Ellipse(dc, S(24), S(22), S(56), S(54));
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(brush);
        RECT icon{S(24), S(22), S(56), S(54)};
        SelectObject(dc, data->heading);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, contrastText(color));
        DrawTextW(dc,
                  data->confirm()                ? L"?"
                  : (data->flags & MB_ICONERROR) ? L"!"
                                                 : L"i",
                  -1, &icon, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        EndPaint(hwnd, &paint);
        return TRUE;
    }
    }
    return FALSE;
}
} // namespace
int showThemedMessage(HWND owner, const std::wstring &body, const std::wstring &title, UINT flags, HFONT font,
                      HFONT heading, const MessagePalette &palette, bool english) {
    Message data;
    data.body = body;
    data.title = title;
    data.flags = flags;
    data.colors = palette;
    data.english = english;
    data.eula = body.find(L"https://www.minecraft.net/eula") != std::wstring::npos;
    LOGFONTW value{};
    GetObjectW(font, sizeof(value), &value);
    data.font = CreateFontIndirectW(&value);
    GetObjectW(heading, sizeof(value), &value);
    data.heading = CreateFontIndirectW(&value);
    data.background = CreateSolidBrush(palette.background);
    struct alignas(DWORD) Template {
        DLGTEMPLATE dialog{};
        WORD menu = 0, cls = 0, title = 0;
    } layout;
    layout.dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | WS_CLIPCHILDREN;
    layout.dialog.dwExtendedStyle = WS_EX_CONTROLPARENT;
    layout.dialog.cx = 380;
    layout.dialog.cy = 180;
    HWND root = owner;
    while (HWND parent = GetWindow(root, GW_OWNER))
        root = parent;
    bool disableRoot = root && root != owner && IsWindowEnabled(root);
    if (disableRoot)
        EnableWindow(root, FALSE);
    auto result = DialogBoxIndirectParamW(GetModuleHandleW(nullptr), &layout.dialog, owner, messageProc,
                                          reinterpret_cast<LPARAM>(&data));
    if (disableRoot && IsWindow(root))
        EnableWindow(root, TRUE);
    if (result == -1 && IsWindow(owner))
        return MessageBoxW(owner, body.c_str(), title.c_str(), flags);
    return result == -1 ? ((flags & MB_TYPEMASK) == MB_YESNO ? IDNO : IDOK) : static_cast<int>(result);
}

#include "App.h"
namespace wrapper {
namespace {
void paintScrollbars(HWND hwnd, HDC supplied = nullptr) {
    if (!supplied && !IsWindowVisible(hwnd))
        return;
    RECT window{};
    GetWindowRect(hwnd, &window);
    HDC dc = supplied ? supplied : GetWindowDC(hwnd);
    if (!dc)
        return;
    for (LONG object : {OBJID_VSCROLL, OBJID_HSCROLL}) {
        SCROLLBARINFO info{};
        info.cbSize = sizeof(info);
        if (!GetScrollBarInfo(hwnd, object, &info) ||
            (info.rgstate[0] & (STATE_SYSTEM_INVISIBLE | STATE_SYSTEM_OFFSCREEN)))
            continue;
        RECT track = info.rcScrollBar;
        OffsetRect(&track, -window.left, -window.top);
        fill(dc, track, Panel);
        if (info.xyThumbBottom <= info.xyThumbTop)
            continue;
        RECT thumb = track;
        if (object == OBJID_VSCROLL) {
            thumb.left += 4;
            thumb.right -= 4;
            thumb.top += info.xyThumbTop;
            thumb.bottom = track.top + info.xyThumbBottom;
        } else {
            thumb.top += 4;
            thumb.bottom -= 4;
            thumb.left += info.xyThumbTop;
            thumb.right = track.left + info.xyThumbBottom;
        }
        COLORREF color = (info.rgstate[3] & STATE_SYSTEM_PRESSED)
                             ? Accent
                             : blend(Panel, Muted, 0.55);
        HBRUSH brush = CreateSolidBrush(color);
        auto oldBrush = SelectObject(dc, brush);
        auto oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
        RoundRect(dc, thumb.left, thumb.top, thumb.right, thumb.bottom, 8, 8);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(brush);
    }
    if (!supplied)
        ReleaseDC(hwnd, dc);
}
} // namespace
LRESULT CALLBACK App::scrollProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR data) {
    auto *app = reinterpret_cast<App *>(data);
    if (msg == WM_DROPFILES && GetDlgCtrlID(hwnd) == PluginList)
        return SendMessageW(app->window, msg, w, l);
    auto result = DefSubclassProc(hwnd, msg, w, l);
    if ((msg == WM_PAINT || msg == WM_PRINT) && GetDlgCtrlID(hwnd) == PlayerFaces::ControlId &&
        app->faces.count() == 0) {
        HDC dc = msg == WM_PRINT ? reinterpret_cast<HDC>(w) : GetDC(hwnd);
        RECT r{};
        GetClientRect(hwnd, &r);
        fill(dc, r, Panel);
        r.left += 18;
        r.right -= 18;
        r.top += 28;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, Muted);
        SelectObject(dc, app->font);
        auto value = app->tr(L"접속 중인 플레이어가 없습니다.");
        DrawTextW(dc, value.c_str(), -1, &r, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
        if (msg != WM_PRINT)
            ReleaseDC(hwnd, dc);
    }
    int id = GetDlgCtrlID(hwnd);
    if ((msg == WM_PAINT || msg == WM_PRINT) && (id == PluginList || id == BackupList || id == TaskList) &&
        ListView_GetItemCount(hwnd) == 0) {
        HDC dc = msg == WM_PRINT ? reinterpret_cast<HDC>(w) : GetDC(hwnd);
        int saved = SaveDC(dc);
        RECT r{}, header{};
        GetClientRect(hwnd, &r);
        GetWindowRect(ListView_GetHeader(hwnd), &header);
        r.top = header.bottom - header.top + 12;
        r.left += 20;
        r.right -= 20;
        r.bottom -= 20;
        fill(dc, r, Panel);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, Muted);
        SelectObject(dc, app->font);
        auto value =
            app->tr(id == PluginList   ? L"플러그인이 없습니다. JAR을 끌어 놓거나 가져오기로 추가하세요."
                    : id == BackupList ? L"아직 백업이 없습니다. 저장 위치를 선택하고 첫 백업을 만드세요."
                                       : L"예약 작업이 없습니다. 동작과 간격을 선택해 작업을 추가하세요.");
        DrawTextW(dc, value.c_str(), -1, &r,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        RestoreDC(dc, saved);
        if (msg != WM_PRINT)
            ReleaseDC(hwnd, dc);
    }
    if (msg == WM_PAINT && GetDlgCtrlID(hwnd) == Console && GetPropW(hwnd, L"WrapperTailPending")) {
        RemovePropW(hwnd, L"WrapperTailPending");
        if (app->autoScroll) {
            app->scrollEnd();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
    }
    switch (msg) {
    case WM_PRINT:
        paintScrollbars(hwnd, reinterpret_cast<HDC>(w));
        break;
    case WM_NCPAINT:
    case WM_PAINT:
    case WM_VSCROLL:
    case WM_HSCROLL:
    case WM_MOUSEWHEEL:
    case WM_SIZE:
    case EM_SETSCROLLPOS:
    case EM_SCROLLCARET:
        paintScrollbars(hwnd);
        break;
    }
    return result;
}
} // namespace wrapper

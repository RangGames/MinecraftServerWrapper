#include "App.h"

namespace wrapper {

void App::scrollEnd() {
    HWND log = control(Console);
    CHARRANGE end{LONG_MAX, LONG_MAX};
    SendMessageW(log, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&end));
    SendMessageW(log, EM_SCROLLCARET, 0, 0);
    SendMessageW(log, WM_VSCROLL, SB_BOTTOM, 0);
    SendMessageW(log, EM_LINESCROLL, 0, SendMessageW(log, EM_GETLINECOUNT, 0, 0));
}

void App::createConsole(bool preserve) {
    DWORD begin = 0, end = 0;
    bool focused = false;
    if (preserve) {
        focused = GetFocus() == control(Console);
        SendMessageW(control(Console), EM_GETSEL, reinterpret_cast<WPARAM>(&begin),
                     reinterpret_cast<LPARAM>(&end));
        placedRects.erase(control(Console));
        DestroyWindow(control(Console));
    }
    DWORD style = WS_TABSTOP | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL;
    if (!wrapping)
        style |= ES_AUTOHSCROLL | WS_HSCROLL;
    HWND log = add(MSFTEDIT_CLASS, L"", style, Console);
    SetWindowSubclass(log, scrollProc, 3, reinterpret_cast<DWORD_PTR>(this));
    SendMessageW(log, EM_SETBKGNDCOLOR, 0, Panel);
    SendMessageW(log, EM_SETUNDOLIMIT, 0, 0);
    SendMessageW(log, EM_SETTARGETDEVICE, 0, wrapping ? 0 : 1);
    SendMessageW(log, EM_SETLIMITTEXT, 2000000, 0);
    styleFont(Console, mono);
    HDC dc = GetDC(log);
    auto previous = SelectObject(dc, mono);
    TEXTMETRICW metrics{};
    GetTextMetricsW(dc, &metrics);
    int tab = MulDiv(metrics.tmAveCharWidth * 4, 1440, GetDeviceCaps(dc, LOGPIXELSX));
    SelectObject(dc, previous);
    ReleaseDC(log, dc);
    PARAFORMAT2 paragraph{};
    paragraph.cbSize = sizeof(paragraph);
    paragraph.dwMask = PFM_TABSTOPS;
    paragraph.cTabCount = 32;
    for (int i = 0; i < 32; ++i)
        paragraph.rgxTabs[i] = (i + 1) * tab;
    SendMessageW(log, EM_SETREADONLY, FALSE, 0);
    SendMessageW(log, EM_SETPARAFORMAT, 0, reinterpret_cast<LPARAM>(&paragraph));
    SendMessageW(log, EM_SETREADONLY, TRUE, 0);
    SendMessageW(log, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(8, 8));
    if (preserve) {
        restoreConsole();
        SendMessageW(log, EM_SETSEL, begin, end);
        if (autoScroll)
            scrollEnd();
        if (focused)
            SetFocus(log);
        layout();
    }
}

void App::restoreConsole() {
    if ((page != 0 && page != 1) || IsIconic(window)) {
        consoleDirty = true;
        return;
    }
    consoleDirty = false;
    HWND log = control(Console);
    // WM_SETREDRAW(TRUE) adds WS_VISIBLE. Never pair it on a hidden control.
    const bool pauseRedraw = (GetWindowLongPtrW(log, GWL_STYLE) & WS_VISIBLE) != 0;
    if (pauseRedraw)
        SendMessageW(log, WM_SETREDRAW, FALSE, 0);
    SendMessageW(log, EM_SETREADONLY, FALSE, 0);
    SetWindowTextW(log, L"");
    for (const auto &span : current().spans) {
        auto content = current().log.substr(span.start, span.length);
        appendConsoleRun(content, span.color);
    }
    SendMessageW(log, EM_SETREADONLY, TRUE, 0);
    if (pauseRedraw)
        SendMessageW(log, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(log, nullptr, FALSE);
}

void App::appendConsoleRun(const std::wstring &value, COLORREF color) {
    HWND log = control(Console);
    bool wasReadOnly = (GetWindowLongPtrW(log, GWL_STYLE) & ES_READONLY) != 0;
    if (wasReadOnly)
        SendMessageW(log, EM_SETREADONLY, FALSE, 0);
    CHARRANGE range{LONG_MAX, LONG_MAX};
    SendMessageW(log, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));
    SendMessageW(log, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&range));
    LONG start = range.cpMin;
    SendMessageW(log, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(value.c_str()));
    SendMessageW(log, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&range));
    LONG end = range.cpMax;
    CHARRANGE inserted{start, end};
    SendMessageW(log, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&inserted));
    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_COLOR;
    format.crTextColor = color == ConsoleDefaultColor ? Foreground : color;
    SendMessageW(log, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
    range = {end, end};
    SendMessageW(log, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));
    if (wasReadOnly)
        SendMessageW(log, EM_SETREADONLY, TRUE, 0);
}

void App::findNext() {
    auto needle = text(control(SearchText));
    if (needle.empty())
        return;
    CHARRANGE selection{};
    SendMessageW(control(Console), EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    FINDTEXTEXW find{};
    find.chrg = {selection.cpMax, -1};
    find.lpstrText = needle.data();
    auto position = SendMessageW(control(Console), EM_FINDTEXTEXW, FR_DOWN, reinterpret_cast<LPARAM>(&find));
    if (position < 0) {
        find.chrg = {0, -1};
        position = SendMessageW(control(Console), EM_FINDTEXTEXW, FR_DOWN, reinterpret_cast<LPARAM>(&find));
    }
    if (position < 0) {
        message(L"검색 결과가 없습니다.", L"찾기", MB_OK | MB_ICONINFORMATION);
        return;
    }
    SendMessageW(control(Console), EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&find.chrgText));
    SendMessageW(control(Console), EM_SCROLLCARET, 0, 0);
    SetFocus(control(Console));
}

} // namespace wrapper

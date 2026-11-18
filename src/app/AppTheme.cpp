#include "App.h"
namespace wrapper {
void App::applyTheme() {
    static bool applying = false;
    if (applying)
        return;
    applying = true;
    DWORD systemLight = 0, size = sizeof(systemLight);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                 L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &systemLight, &size);
    lightTheme = theme == 1 || (theme == 2 && systemLight != 0);
    // Windows 11 (Fluent) neutrals with the default Windows accent blue.
    if (lightTheme) {
        Background = RGB(243, 243, 243), Surface = RGB(249, 249, 249), Panel = RGB(255, 255, 255);
        Border = RGB(214, 214, 214), CardStroke = RGB(229, 229, 229);
        Foreground = RGB(27, 27, 27), Muted = RGB(96, 96, 96);
        Field = RGB(255, 255, 255), Control = RGB(251, 251, 251), ControlHover = RGB(240, 240, 240);
        Subtle = RGB(234, 234, 234);
        Accent = RGB(0, 95, 184), OnAccent = RGB(255, 255, 255), Danger = RGB(196, 43, 28);
        Success = RGB(15, 123, 15), Caution = RGB(157, 93, 0);
        Blue = RGB(0, 95, 184), Purple = RGB(107, 79, 187), Green = RGB(15, 123, 15);
    } else {
        Background = RGB(32, 32, 32), Surface = RGB(39, 39, 39), Panel = RGB(45, 45, 45);
        Border = RGB(62, 62, 62), CardStroke = RGB(53, 53, 53);
        Foreground = RGB(240, 240, 240), Muted = RGB(168, 168, 168);
        Field = RGB(52, 52, 52), Control = RGB(55, 55, 55), ControlHover = RGB(63, 63, 63);
        Subtle = RGB(58, 58, 58);
        Accent = RGB(96, 205, 255), OnAccent = RGB(0, 0, 0), Danger = RGB(255, 153, 164);
        Success = RGB(108, 203, 95), Caution = RGB(252, 185, 0);
        Blue = RGB(96, 205, 255), Purple = RGB(180, 160, 255), Green = RGB(108, 203, 95);
    }
    for (auto brush : {bgBrush, surfaceBrush, panelBrush, fieldBrush})
        if (brush)
            DeleteObject(brush);
    bgBrush = CreateSolidBrush(Background);
    surfaceBrush = CreateSolidBrush(Surface);
    panelBrush = CreateSolidBrush(Panel);
    fieldBrush = CreateSolidBrush(Field);
    statusColors();
    if (themeApplied != static_cast<int>(lightTheme)) {
        themeApplied = static_cast<int>(lightTheme);
        for (auto &server : servers)
            loadThumbnail(*server);
    }
    faces.setColors(Panel, Subtle, Foreground, Muted, Accent);
    for (int id : {PluginList, BackupList, TaskList}) {
        ListView_SetBkColor(control(id), Panel);
        ListView_SetTextBkColor(control(id), Panel);
        ListView_SetTextColor(control(id), Foreground);
    }
    SendMessageW(control(Console), EM_SETBKGNDCOLOR, 0, Panel);
    if (control(Console)) {
        restoreConsole();
        if (autoScroll)
            scrollEnd();
    }
    BOOL dark = !lightTheme;
    DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));
    DwmSetWindowAttribute(window, 35, &Background, sizeof(Background));
    DwmSetWindowAttribute(window, 36, &Foreground, sizeof(Foreground));
    DWORD corners = 2; // DWMWCP_ROUND on Windows 11; ignored by earlier versions.
    DwmSetWindowAttribute(window, 33, &corners, sizeof(corners));
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE | RDW_FRAME);
    if (groupWindow) {
        DwmSetWindowAttribute(groupWindow, 20, &dark, sizeof(dark));
        DwmSetWindowAttribute(groupWindow, 35, &Background, sizeof(Background));
        DwmSetWindowAttribute(groupWindow, 36, &Foreground, sizeof(Foreground));
        RedrawWindow(groupWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE | RDW_FRAME);
    }
    applying = false;
}

// Status pill colors follow the selected server's state: online green, transitional amber, offline neutral.
void App::statusColors() {
    COLORREF tone = Muted;
    if (!servers.empty()) {
        auto &server = current();
        if (server.recoverAt || (server.process.running() && (server.process.stopping() || !server.ready)))
            tone = Caution;
        else if (server.process.running())
            tone = Success;
    }
    COLORREF fill = tone == Muted ? Subtle : blend(Panel, tone, lightTheme ? 0.14 : 0.22);
    if (fill == statusFill && tone == statusText && statusBrush)
        return;
    statusFill = fill;
    statusText = tone;
    if (statusBrush)
        DeleteObject(statusBrush);
    statusBrush = CreateSolidBrush(statusFill);
    if (window) {
        InvalidateRect(window, &statusPill, FALSE);
        InvalidateRect(control(Status), nullptr, TRUE);
    }
}

// The color behind a child control: sidebar, a card, or the content layer.
COLORREF App::backdrop(HWND child) const {
    if (GetParent(child) != window)
        return Surface;
    RECT r{};
    GetWindowRect(child, &r);
    MapWindowPoints(nullptr, window, reinterpret_cast<POINT *>(&r), 2);
    if (r.left < SidebarWidth)
        return Background;
    POINT center{(r.left + r.right) / 2, (r.top + r.bottom) / 2};
    for (const auto &card : cards)
        if (PtInRect(&card, center))
            return Panel;
    return Surface;
}
} // namespace wrapper

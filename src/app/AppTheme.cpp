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
    Background = lightTheme ? RGB(241, 241, 240) : RGB(19, 19, 19);
    Surface = lightTheme ? RGB(248, 248, 247) : RGB(26, 26, 26);
    Panel = lightTheme ? RGB(255, 255, 255) : RGB(33, 33, 33);
    Border = lightTheme ? RGB(216, 216, 212) : RGB(58, 58, 56);
    Foreground = lightTheme ? RGB(35, 35, 33) : RGB(235, 235, 231);
    Muted = lightTheme ? RGB(105, 105, 101) : RGB(165, 165, 158);
    Accent = lightTheme ? RGB(91, 113, 30) : RGB(190, 207, 119);
    for (auto brush : {bgBrush, surfaceBrush, panelBrush})
        if (brush)
            DeleteObject(brush);
    bgBrush = CreateSolidBrush(Background);
    surfaceBrush = CreateSolidBrush(Surface);
    panelBrush = CreateSolidBrush(Panel);
    faces.setTheme(lightTheme);
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
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE);
    if (groupWindow) {
        DwmSetWindowAttribute(groupWindow, 20, &dark, sizeof(dark));
        RedrawWindow(groupWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE);
    }
    applying = false;
}
} // namespace wrapper

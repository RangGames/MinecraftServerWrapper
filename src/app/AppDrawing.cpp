#include "App.h"
#include "UiSurface.h"

namespace wrapper {

void App::paintGraph(HDC dc, RECT rect) {
    fill(dc, rect, Panel);
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, small);
    const auto &history = current().samples;
    const COLORREF colors[]{Purple, Blue, RGB(255, 102, 102)};
    const wchar_t *labels[]{L"CPU", L"RAM", L"PLAYERS"};
    for (int chart = 0; chart < 3; ++chart) {
        RECT band{rect.left + 14, rect.top + chart * (rect.bottom - rect.top) / 3 + 8, rect.right - 14,
                  rect.top + (chart + 1) * (rect.bottom - rect.top) / 3 - 8};
        SetTextColor(dc, Muted);
        auto chartLabel = tr(labels[chart]);
        TextOutW(dc, band.left, band.top, chartLabel.c_str(), static_cast<int>(chartLabel.size()));
        int yTop = band.top + 22, yBottom = band.bottom - 20;
        HPEN grid = CreatePen(PS_SOLID, 1, Border);
        auto old = SelectObject(dc, grid);
        MoveToEx(dc, band.left, yBottom, nullptr);
        LineTo(dc, band.right, yBottom);
        MoveToEx(dc, band.left, (yTop + yBottom) / 2, nullptr);
        LineTo(dc, band.right, (yTop + yBottom) / 2);
        SelectObject(dc, old);
        DeleteObject(grid);
        double maximum = chart == 0 ? 100 : chart == 1 ? static_cast<double>(current().memory) : 10;
        for (const auto &sample : history)
            maximum = std::max(maximum, (chart == 1   ? sample.ram
                                         : chart == 2 ? sample.players
                                                      : sample.cpu) *
                                            1.1);
        if (!history.empty()) {
            std::vector<POINT> points;
            for (size_t i = 0; i < history.size(); ++i) {
                double value = chart == 0 ? history[i].cpu : chart == 1 ? history[i].ram : history[i].players;
                int x =
                    band.right - static_cast<int>((history.size() - 1 - i) * (band.right - band.left) / 119);
                int y = yBottom -
                        static_cast<int>(std::clamp(value / maximum, 0.0, 1.0) * std::max(1, yBottom - yTop));
                points.push_back({x, y});
            }
            HPEN line = CreatePen(PS_SOLID, 2, colors[chart]);
            auto previous = SelectObject(dc, line);
            if (points.size() > 1)
                Polyline(dc, points.data(), static_cast<int>(points.size()));
            else {
                MoveToEx(dc, points.front().x - 2, points.front().y, nullptr);
                LineTo(dc, points.front().x, points.front().y);
            }
            SelectObject(dc, previous);
            DeleteObject(line);
        }
        std::wstring value = chart == 0   ? decimal(current().process.cpuPercent()) + L" % CPU"
                             : chart == 1 ? decimal(current().process.memoryMB(), 0) + L" MB RAM"
                                          : std::to_wstring(current().players.size()) + L" PLAYERS";
        SetTextColor(dc, colors[chart]);
        RECT label{band.left, yBottom + 3, band.right, band.bottom + 2};
        DrawTextW(dc, value.c_str(), -1, &label, DT_RIGHT | DT_SINGLELINE);
    }
}

LRESULT CALLBACK App::graphProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR data) {
    auto *app = reinterpret_cast<App *>(data);
    if (msg == WM_PAINT) {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);
        RECT rect{};
        GetClientRect(hwnd, &rect);
        app->paintGraph(dc, rect);
        EndPaint(hwnd, &paint);
        return 0;
    }
    if (msg == WM_ERASEBKGND)
        return 1;
    return DefSubclassProc(hwnd, msg, w, l);
}

LRESULT CALLBACK App::headerProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR data) {
    auto *app = reinterpret_cast<App *>(data);
    if (msg == WM_PAINT) {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);
        RECT all{};
        GetClientRect(hwnd, &all);
        fill(dc, all, Surface);
        SetTextColor(dc, Foreground);
        SetBkMode(dc, TRANSPARENT);
        SelectObject(dc, app->font);
        for (int i = 0; i < Header_GetItemCount(hwnd); ++i) {
            wchar_t buffer[256]{};
            HDITEMW item{};
            item.mask = HDI_TEXT;
            item.pszText = buffer;
            item.cchTextMax = 256;
            SendMessageW(hwnd, HDM_GETITEMW, i, reinterpret_cast<LPARAM>(&item));
            RECT rect{};
            Header_GetItemRect(hwnd, i, &rect);
            rect.left += 10;
            DrawTextW(dc, buffer, -1, &rect, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
        EndPaint(hwnd, &paint);
        return 0;
    }
    return DefSubclassProc(hwnd, msg, w, l);
}

UiIcon App::iconFor(int id) const {
    if (id >= DashboardTab && id <= SettingsTab) {
        constexpr UiIcon icons[]{UiIcon::Home,   UiIcon::Console, UiIcon::Plugin,  UiIcon::Users,
                                 UiIcon::Backup, UiIcon::Clock,   UiIcon::Settings};
        return icons[id - DashboardTab];
    }
    switch (id) {
    case GroupManager:
    case GroupManageSettings:
        return UiIcon::Users;
    case ManagerCreate:
        return UiIcon::Add;
    case ManagerRename:
        return UiIcon::File;
    case ManagerClose:
        return UiIcon::Close;
    case ManagerStart:
    case Start:
    case TaskRun:
        return UiIcon::Play;
    case ManagerStop:
    case Stop:
        return UiIcon::Stop;
    case Kill:
        return UiIcon::Close;
    case Restart:
    case PluginRefresh:
        return UiIcon::Restart;
    case Folder:
    case PluginFolder:
    case BackupFolder:
    case BackupChoose:
    case ChooseServerFolder:
        return UiIcon::Folder;
    case AddServer:
    case TaskAdd:
    case PluginImport:
        return UiIcon::Add;
    case ManagerDelete:
    case PresetDelete:
    case RemoveServer:
    case Clear:
    case TaskDelete:
        return UiIcon::Trash;
    case ManagerApply:
    case PresetSave:
    case SaveSettings:
    case TaskUpdate:
    case ExportLog:
        return UiIcon::Save;
    case BrowseJava:
    case BrowseJar:
    case FindNext:
        return UiIcon::Search;
    case RailHome:
        return UiIcon::Home;
    case RailConsole:
        return UiIcon::Console;
    case FontChoose:
    case SettingsAppearance:
    case SettingsProperties:
    case SettingsLaunch:
    case SettingsGeneral:
    case RailSettings:
    case MenuTools:
        return UiIcon::Settings;
    case MenuFile:
        return UiIcon::File;
    case MenuServer:
        return UiIcon::Home;
    case MenuHelp:
    case EulaLink:
        return UiIcon::Help;
    case ChooseImage:
    case ResetImage:
        return UiIcon::Image;
    case BackupNow:
        return UiIcon::Backup;
    case Send:
        return UiIcon::Send;
    case TaskToggle:
        return UiIcon::Clock;
    case PluginToggle:
        return UiIcon::Plugin;
    case PlayerKick:
    case PlayerBan:
    case PlayerOp:
    case PlayerDeop:
        return UiIcon::Users;
    default:
        return UiIcon::None;
    }
}

bool App::drawItem(const DRAWITEMSTRUCT &item) {
    if (faces.draw(item))
        return true;
    if (item.CtlType == ODT_COMBOBOX) {
        fill(item.hDC, item.rcItem,
             (item.itemState & ODS_SELECTED) ? (lightTheme ? RGB(235, 237, 225) : RGB(45, 46, 40)) : Panel);
        if (item.itemID != static_cast<UINT>(-1)) {
            auto length = SendMessageW(item.hwndItem, CB_GETLBTEXTLEN, item.itemID, 0);
            if (length >= 0 && length < 32768) {
                std::wstring label(static_cast<size_t>(length) + 1, 0);
                SendMessageW(item.hwndItem, CB_GETLBTEXT, item.itemID,
                             reinterpret_cast<LPARAM>(label.data()));
                RECT rect = item.rcItem;
                rect.left += 10;
                rect.right -= 6;
                SetBkMode(item.hDC, TRANSPARENT);
                SetTextColor(item.hDC, (item.itemState & ODS_DISABLED) ? Muted : Foreground);
                SelectObject(item.hDC, font);
                DrawTextW(item.hDC, label.c_str(), -1, &rect,
                          DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            }
        }
        return true;
    }
    HDC dc = item.hDC;
    SetBkMode(dc, TRANSPARENT);
    auto itemFont = reinterpret_cast<HFONT>(SendMessageW(item.hwndItem, WM_GETFONT, 0, 0));
    SelectObject(dc, itemFont ? itemFont : font);
    if (item.CtlID == ServerSelector) {
        if (item.itemID >= servers.size())
            return true;
        auto &server = *servers[item.itemID];
        bool active = (item.itemState & ODS_SELECTED) != 0;
        fill(dc, item.rcItem, Surface);
        RECT tile = item.rcItem;
        tile.right -= 8;
        tile.bottom -= 2;
        roundedSurface(dc, tile, active ? Panel : Surface, active ? Accent : Border, 8);
        int ix = item.rcItem.left + 8, iy = item.rcItem.top + (item.rcItem.bottom - item.rcItem.top - 28) / 2;
        if (server.thumbnail) {
            HDC source = CreateCompatibleDC(dc);
            auto old = SelectObject(source, server.thumbnail);
            BitBlt(dc, ix, iy, 28, 28, source, 0, 0, SRCCOPY);
            SelectObject(source, old);
            DeleteDC(source);
        } else
            drawUiIcon(dc, UiIcon::Home, ix + 6, iy + 6, active ? Accent : Muted);
        RECT label = item.rcItem;
        label.left += 44;
        label.right -= 96;
        label.bottom = label.top + 24;
        SetTextColor(dc, Foreground);
        DrawTextW(dc, server.name.c_str(), -1, &label,
                  DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        RECT group = item.rcItem;
        group.left += 44;
        group.right -= 96;
        group.top += 24;
        group.bottom -= 2;
        SelectObject(dc, small);
        SetTextColor(dc, Muted);
        auto groupLabel = server.group == L"기본 그룹" ? tr(server.group) : server.group;
        DrawTextW(dc, groupLabel.c_str(), -1, &group, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(dc, itemFont ? itemFont : font);
        RECT status = item.rcItem;
        status.right -= 22;
        status.left = status.right - 66;
        SetTextColor(dc, server.process.running() ? Accent : Muted);
        auto state = tr(server.process.stopping()  ? L"Stopping…"
                        : server.process.running() ? (server.ready ? L"Online" : L"Starting…")
                                                   : L"Offline");
        DrawTextW(dc, state.c_str(), -1, &status, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        if (hoveredServer == static_cast<int>(item.itemID)) {
            bool removable = !server.process.running() && !(backupPending && backupOwner == item.itemID) &&
                             servers.size() > 1 && !closing;
            drawUiIcon(dc, UiIcon::Close, item.rcItem.right - 19,
                       (item.rcItem.top + item.rcItem.bottom - 16) / 2, removable ? Foreground : Border);
        }
        return true;
    }
    if (item.CtlType != ODT_BUTTON)
        return false;
    int id = static_cast<int>(item.CtlID);
    bool tab =
        (id >= DashboardTab && id <= SettingsTab) || (id >= SettingsGeneral && id <= SettingsAppearance);
    bool active =
        (id >= SettingsGeneral && id <= SettingsAppearance && id == SettingsGeneral + settingsPage) ||
        (tab && id == DashboardTab + page) || (id == RailHome && page == 0) ||
        (id == RailConsole && page == 1) || (id == RailSettings && page == 6);
    bool navigation = id >= DashboardTab && id <= SettingsTab;
    bool utility = id >= MenuFile && id <= MenuHelp;
    bool primary = id == WelcomeAction || id == Start || id == SaveSettings || id == Send || id == TaskAdd ||
                   id == BackupNow || id == ManagerApply;
    bool enabled = !(item.itemState & ODS_DISABLED);
    bool hover = GetPropW(item.hwndItem, L"Hover") != nullptr;
    COLORREF base = navigation || id == AddServer || id == GroupManager ? Background : Surface;
    if (page == 0 && current().jar.empty() && (id == WelcomeAction || id == Installer))
        base = Panel;
    fill(dc, item.rcItem, base);
    COLORREF background = primary && enabled ? Accent : Panel;
    COLORREF outline = Border;
    if (navigation || utility || tab) {
        background = active ? Panel : base;
        outline = background;
    }
    if (hover && enabled)
        background = lightTheme ? RGB(235, 237, 225) : RGB(45, 46, 40);
    if (primary && enabled) {
        background = (item.itemState & ODS_SELECTED) ? RGB(159, 177, 89) : Accent;
        outline = background;
    }
    if (!enabled) {
        background = base;
        outline = lightTheme ? RGB(226, 231, 238) : RGB(34, 43, 57);
    }
    if (GetFocus() == item.hwndItem)
        outline = Accent;
    roundedSurface(dc, item.rcItem, background, outline, navigation ? 8 : 7);
    COLORREF color = enabled ? Foreground : Muted;
    if (primary && enabled)
        color = lightTheme ? RGB(255, 255, 255) : RGB(25, 29, 15);
    if (navigation)
        color = active ? Accent : Muted;
    if ((id == Kill || id == RemoveServer || id == TaskDelete || id == ManagerDelete) && enabled)
        color = lightTheme ? RGB(184, 52, 62) : RGB(245, 138, 148);
    if (utility)
        color = Muted;
    SetTextColor(dc, color);
    RECT label = item.rcItem;
    auto value = text(item.hwndItem);
    auto icon = iconFor(id);
    // Navigation gets a stable left alignment; ordinary buttons use centered labels.
    if (navigation) {
        drawUiIcon(dc, icon, label.left + 16, (label.top + label.bottom - 16) / 2, color);
        label.left += 48;
        label.right -= 12;
        DrawTextW(dc, value.c_str(), -1, &label,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    } else {
        SIZE extent{};
        GetTextExtentPoint32W(dc, value.c_str(), static_cast<int>(value.size()), &extent);
        if (icon != UiIcon::None && !utility) {
            int start = std::max(label.left + 8, (label.left + label.right - extent.cx - 24) / 2);
            drawUiIcon(dc, icon, start, (label.top + label.bottom - 16) / 2, color);
            label.left = start + 24;
            label.right -= 6;
        }
        DrawTextW(dc, value.c_str(), -1, &label,
                  (icon == UiIcon::None || utility ? DT_CENTER : DT_LEFT) | DT_VCENTER | DT_SINGLELINE |
                      DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    return true;
}

void App::paintWindow() {
    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(window, &paint);
    RECT r{};
    GetClientRect(window, &r);
    fill(dc, r, Surface);
    RECT sidebar{0, 0, 232, r.bottom};
    fill(dc, sidebar, Background);
    RECT edge{231, 0, 232, r.bottom};
    fill(dc, edge, Border);
    SetBkMode(dc, TRANSPARENT);
    roundedSurface(dc, {20, 24, 54, 58}, Accent, Accent, 9);
    drawUiIcon(dc, UiIcon::Home, 28, 33, lightTheme ? RGB(255, 255, 255) : RGB(25, 29, 15));
    static HFONT brandFont =
        CreateFontW(-22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                    CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    SelectObject(dc, brandFont);
    SetTextColor(dc, Foreground);
    RECT title{66, 24, 222, 57};
    DrawTextW(dc, L"Server Desk", -1, &title, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    SelectObject(dc, small);
    SetTextColor(dc, Muted);
    RECT subtitle{22, 73, 218, 95};
    DrawTextW(dc, L"MINECRAFT WORKSPACE", -1, &subtitle, DT_LEFT | DT_SINGLELINE);
    RECT label{24, 116, 216, 136};
    auto caption = tr(L"서버 관리");
    DrawTextW(dc, caption.c_str(), -1, &label, DT_LEFT | DT_SINGLELINE);
    RECT top{256, 16, 650, 36};
    auto workspace = tr(L"작업 공간 / 서버");
    DrawTextW(dc, workspace.c_str(), -1, &top, DT_LEFT | DT_SINGLELINE);
    RECT line{256, 198, r.right - 28, 199};
    fill(dc, line, Border);
    RECT bottomLine{20, r.bottom - 120, 212, r.bottom - 119};
    fill(dc, bottomLine, Border);
    if (page == 0 && current().jar.empty() && current().launch.mode == 0) {
        roundedSurface(dc, {256, 208, r.right - 28, r.bottom - 28}, Panel, Border, 12);
    } else if (page == 0) {
        int width = r.right - 28 - 256 - 244 - 16, card = (width - 24) / 3;
        for (int i = 0; i < 3; ++i)
            roundedSurface(dc, {256 + i * (card + 12), 208, 256 + i * (card + 12) + card, 292}, Panel, Border,
                           10);
    }
    EndPaint(window, &paint);
}
} // namespace wrapper

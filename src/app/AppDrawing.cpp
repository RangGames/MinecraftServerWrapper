#include "App.h"
#include "UiSurface.h"
#include "AppResources.h"

namespace wrapper {

void App::paintGraph(HDC dc, RECT rect) {
    fill(dc, rect, Panel);
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, small);
    const auto &history = current().samples;
    const COLORREF colors[]{Blue, Purple, Green};
    const wchar_t *labels[]{L"CPU", L"RAM", L"PLAYERS"};
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    auto argb = [](COLORREF c, BYTE alpha) {
        return Gdiplus::Color(alpha, GetRValue(c), GetGValue(c), GetBValue(c));
    };
    int bandHeight = (rect.bottom - rect.top) / 3;
    for (int chart = 0; chart < 3; ++chart) {
        RECT band{rect.left + 12, rect.top + chart * bandHeight + 6, rect.right - 12,
                  rect.top + (chart + 1) * bandHeight - 6};
        int yTop = band.top + 24, yBottom = band.bottom - 4;
        double maximum = chart == 0 ? 100 : chart == 1 ? static_cast<double>(current().memory) : 10;
        for (const auto &sample : history)
            maximum = std::max(maximum, (chart == 1   ? sample.ram
                                         : chart == 2 ? sample.players
                                                      : sample.cpu) *
                                            1.1);
        // Label on the left, live value on the right, both above the plot.
        SetTextColor(dc, Muted);
        auto chartLabel = tr(labels[chart]);
        RECT caption{band.left, band.top, band.right, band.top + 20};
        DrawTextW(dc, chartLabel.c_str(), -1, &caption, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        std::wstring value = chart == 0   ? decimal(current().process.cpuPercent()) + L" %"
                             : chart == 1 ? decimal(current().process.memoryMB(), 0) + L" MB"
                                          : std::to_wstring(current().players.size());
        SetTextColor(dc, colors[chart]);
        DrawTextW(dc, value.c_str(), -1, &caption, DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        Gdiplus::Pen grid(argb(CardStroke, 255), 1.0f);
        g.DrawLine(&grid, static_cast<float>(band.left), yBottom + 0.5f, static_cast<float>(band.right),
                   yBottom + 0.5f);
        if (history.empty() || yBottom - yTop < 4)
            continue;
        std::vector<Gdiplus::PointF> points;
        points.reserve(history.size() + 2);
        for (size_t i = 0; i < history.size(); ++i) {
            double sample = chart == 0 ? history[i].cpu : chart == 1 ? history[i].ram : history[i].players;
            float x = band.right - static_cast<float>(history.size() - 1 - i) * (band.right - band.left) / 119.0f;
            float y = yBottom - static_cast<float>(std::clamp(sample / maximum, 0.0, 1.0) * (yBottom - yTop));
            points.push_back({std::max(x, static_cast<float>(band.left)), y});
        }
        if (points.size() == 1)
            points.insert(points.begin(), {points.front().X - 2, points.front().Y});
        // Soft area fill under the line, then the line itself.
        std::vector<Gdiplus::PointF> area(points);
        area.push_back({points.back().X, static_cast<float>(yBottom)});
        area.push_back({points.front().X, static_cast<float>(yBottom)});
        Gdiplus::SolidBrush shade(argb(colors[chart], lightTheme ? 28 : 40));
        g.FillPolygon(&shade, area.data(), static_cast<INT>(area.size()));
        Gdiplus::Pen line(argb(colors[chart], 255), 2.0f);
        line.SetLineJoin(Gdiplus::LineJoinRound);
        g.DrawLines(&line, points.data(), static_cast<INT>(points.size()));
    }
}

LRESULT CALLBACK App::graphProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR data) {
    auto *app = reinterpret_cast<App *>(data);
    if (msg == WM_PAINT) {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);
        RECT rect{};
        GetClientRect(hwnd, &rect);
        // Draw off-screen so the once-per-second refresh does not flicker.
        HDC memory = CreateCompatibleDC(dc);
        HBITMAP bitmap = CreateCompatibleBitmap(dc, std::max(1L, rect.right), std::max(1L, rect.bottom));
        auto old = SelectObject(memory, bitmap);
        app->paintGraph(memory, rect);
        BitBlt(dc, 0, 0, rect.right, rect.bottom, memory, 0, 0, SRCCOPY);
        SelectObject(memory, old);
        DeleteObject(bitmap);
        DeleteDC(memory);
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
        fill(dc, all, Panel);
        RECT rule{all.left, all.bottom - 1, all.right, all.bottom};
        fill(dc, rule, CardStroke);
        SetTextColor(dc, Muted);
        SetBkMode(dc, TRANSPARENT);
        SelectObject(dc, app->small);
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
    if (msg == WM_ERASEBKGND)
        return 1;
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

namespace {
// Small filled circle used for server state dots.
void dot(HDC dc, int cx, int cy, int radius, COLORREF color) {
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::SolidBrush brush(Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)));
    g.FillEllipse(&brush, static_cast<float>(cx - radius), static_cast<float>(cy - radius),
                  static_cast<float>(radius * 2), static_cast<float>(radius * 2));
}
HFONT semibold(HFONT base) {
    LOGFONTW logical{};
    if (!GetObjectW(base, sizeof(logical), &logical))
        return nullptr;
    logical.lfWeight = FW_SEMIBOLD;
    return CreateFontIndirectW(&logical);
}
} // namespace

bool App::drawItem(const DRAWITEMSTRUCT &item) {
    if (faces.draw(item))
        return true;
    if (item.CtlType == ODT_COMBOBOX) {
        bool field = (item.itemState & ODS_COMBOBOXEDIT) != 0;
        bool chosen = !field && (item.itemState & ODS_SELECTED);
        fill(item.hDC, item.rcItem, Field);
        if (chosen) {
            RECT row = item.rcItem;
            InflateRect(&row, -3, -1);
            roundedSurface(item.hDC, row, Subtle, Subtle, 4);
            int middle = (row.top + row.bottom) / 2;
            fill(item.hDC, {row.left, middle - 7, row.left + 3, middle + 7}, Accent);
        }
        if (item.itemID != static_cast<UINT>(-1)) {
            auto length = SendMessageW(item.hwndItem, CB_GETLBTEXTLEN, item.itemID, 0);
            if (length >= 0 && length < 32768) {
                std::wstring label(static_cast<size_t>(length) + 1, 0);
                SendMessageW(item.hwndItem, CB_GETLBTEXT, item.itemID,
                             reinterpret_cast<LPARAM>(label.data()));
                RECT rect = item.rcItem;
                rect.left += field ? 8 : 12;
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
        bool hover = hoveredServer == static_cast<int>(item.itemID);
        fill(dc, item.rcItem, Surface);
        RECT tile = item.rcItem;
        tile.right -= 8;
        tile.bottom -= 2;
        roundedSurface(dc, tile, active ? Panel : hover ? ControlHover : Surface, active ? Accent : CardStroke, 8);
        int ix = item.rcItem.left + 10, iy = item.rcItem.top + (item.rcItem.bottom - item.rcItem.top - 28) / 2;
        if (server.thumbnail) {
            HDC source = CreateCompatibleDC(dc);
            auto old = SelectObject(source, server.thumbnail);
            BitBlt(dc, ix, iy, 28, 28, source, 0, 0, SRCCOPY);
            SelectObject(source, old);
            DeleteDC(source);
        } else {
            roundedSurface(dc, {ix, iy, ix + 28, iy + 28}, Subtle, Subtle, 6);
            drawUiIcon(dc, UiIcon::Home, ix + 5, iy + 6, active ? Accent : Muted);
        }
        bool online = server.process.running() && server.ready && !server.process.stopping();
        bool busy = server.recoverAt || (server.process.running() && !online);
        COLORREF tone = online ? Success : busy ? Caution : Muted;
        RECT label = item.rcItem;
        label.left += 48;
        label.right -= 30;
        label.top += 4;
        label.bottom = label.top + 22;
        SetTextColor(dc, Foreground);
        DrawTextW(dc, server.name.c_str(), -1, &label,
                  DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(dc, small);
        dot(dc, label.left + 4, label.bottom + 9, 4, tone);
        RECT detail = item.rcItem;
        detail.left = label.left + 14;
        detail.right -= 30;
        detail.top = label.bottom;
        detail.bottom -= 4;
        SetTextColor(dc, Muted);
        auto state = tr(server.recoverAt                 ? L"Recovery…"
                        : server.process.stopping()      ? L"Stopping…"
                        : server.process.running()       ? (server.ready ? L"Online" : L"Starting…")
                                                         : L"Offline");
        auto groupLabel = server.group == L"기본 그룹" ? tr(server.group) : server.group;
        auto line = state + L"  ·  " + groupLabel;
        DrawTextW(dc, line.c_str(), -1, &detail, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(dc, itemFont ? itemFont : font);
        if (hover) {
            bool removable = !server.process.running() && !(backupPending && backupOwner == item.itemID) &&
                             servers.size() > 1 && !closing;
            drawUiIcon(dc, UiIcon::Close, item.rcItem.right - 30,
                       (item.rcItem.top + item.rcItem.bottom - 16) / 2, removable ? Foreground : Border);
        }
        return true;
    }
    if (item.CtlType != ODT_BUTTON)
        return false;
    int id = static_cast<int>(item.CtlID);
    bool navigation = id >= DashboardTab && id <= SettingsTab;
    bool pivot = id >= SettingsGeneral && id <= SettingsAppearance;
    bool utility = (id >= MenuFile && id <= MenuHelp) || id == GroupManager || id == AddServer;
    bool active = (pivot && id == SettingsGeneral + settingsPage) || (navigation && id == DashboardTab + page) ||
                  (id == RailHome && page == 0) || (id == RailConsole && page == 1) ||
                  (id == RailSettings && page == 6);
    bool primary = id == WelcomeAction || id == Start || id == SaveSettings || id == Send || id == TaskAdd ||
                   id == BackupNow || id == ManagerApply;
    bool danger = id == Kill || id == RemoveServer || id == TaskDelete || id == ManagerDelete;
    bool enabled = !(item.itemState & ODS_DISABLED);
    bool pressed = (item.itemState & ODS_SELECTED) != 0;
    bool hover = GetPropW(item.hwndItem, L"Hover") != nullptr;
    bool focused = GetFocus() == item.hwndItem && (item.itemState & ODS_NOFOCUSRECT) == 0;
    COLORREF base = backdrop(item.hwndItem);
    RECT box = item.rcItem;
    // Paint into an off-screen bitmap: hover/press repaints would otherwise flash the backdrop.
    HDC target = dc;
    int width = box.right - box.left, height = box.bottom - box.top;
    HDC memory = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, std::max(1, width), std::max(1, height));
    auto oldBitmap = SelectObject(memory, bitmap);
    SetBkMode(memory, TRANSPARENT);
    SelectObject(memory, itemFont ? itemFont : font);
    dc = memory;
    OffsetRect(&box, -box.left, -box.top);
    fill(dc, box, base);
    COLORREF color = enabled ? Foreground : blend(Muted, base, 0.35);
    auto value = text(item.hwndItem);
    auto icon = iconFor(id);
    if (navigation) {
        // Windows 11 navigation view: subtle fill for hover/selection, accent pill on the selected item.
        if (active || (hover && enabled))
            roundedSurface(dc, box, active ? Subtle : blend(base, Subtle, pressed ? 0.5 : 0.7),
                           active ? Subtle : blend(base, Subtle, pressed ? 0.5 : 0.7), 6);
        if (active) {
            int middle = (box.top + box.bottom) / 2;
            roundedSurface(dc, {box.left, middle - 8, box.left + 3, middle + 8}, Accent, Accent, 1);
        }
        drawUiIcon(dc, icon, box.left + 14, (box.top + box.bottom - 16) / 2, active ? Accent : color);
        RECT label = box;
        label.left += 46;
        label.right -= 10;
        HFONT strong = active ? semibold(itemFont ? itemFont : font) : nullptr;
        if (strong)
            SelectObject(dc, strong);
        SetTextColor(dc, color);
        DrawTextW(dc, value.c_str(), -1, &label,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        if (strong) {
            SelectObject(dc, itemFont ? itemFont : font);
            DeleteObject(strong);
        }
    } else if (pivot) {
        // Pivot tabs: text only, accent underline on the selected tab.
        if (hover && enabled && !active)
            roundedSurface(dc, box, blend(base, Subtle, 0.6), blend(base, Subtle, 0.6), 6);
        SetTextColor(dc, active ? Foreground : Muted);
        RECT label = box;
        label.left += 8;
        label.right -= 8;
        SIZE extent{};
        GetTextExtentPoint32W(dc, value.c_str(), static_cast<int>(value.size()), &extent);
        drawUiIcon(dc, icon, (box.left + box.right - extent.cx - 24) / 2, (box.top + box.bottom - 16) / 2 - 2,
                   active ? Accent : Muted);
        label.left = (box.left + box.right - extent.cx - 24) / 2 + 24;
        label.bottom -= 4;
        DrawTextW(dc, value.c_str(), -1, &label, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        if (active) {
            int middle = (box.left + box.right) / 2;
            roundedSurface(dc, {middle - 12, box.bottom - 3, middle + 12, box.bottom}, Accent, Accent, 1);
        }
    } else {
        COLORREF background = Control, outline = Border;
        if (utility) {
            background = hover && enabled ? blend(base, Subtle, pressed ? 0.5 : 0.8) : base;
            outline = id == GroupManager || id == AddServer ? blend(base, Border, 0.6) : background;
        } else if (primary && enabled) {
            background = pressed ? blend(Accent, base, 0.25) : hover ? blend(Accent, base, 0.12) : Accent;
            outline = background;
            color = pressed ? blend(OnAccent, Accent, 0.3) : OnAccent;
        } else if (enabled) {
            background = pressed ? blend(Control, base, 0.5) : hover ? ControlHover : Control;
            if (pressed)
                color = Muted;
        } else {
            background = blend(Control, base, 0.5);
            outline = blend(Border, base, 0.5);
        }
        if (danger && enabled)
            color = Danger;
        if (focused)
            outline = Foreground;
        roundedSurface(dc, box, background, outline, 6);
        if (utility && id >= MenuFile && id <= MenuHelp)
            color = enabled ? Foreground : color;
        SetTextColor(dc, color);
        RECT label = box;
        SIZE extent{};
        GetTextExtentPoint32W(dc, value.c_str(), static_cast<int>(value.size()), &extent);
        bool withIcon = icon != UiIcon::None && !(id >= MenuFile && id <= MenuHelp);
        if (withIcon) {
            int start = std::max(label.left + 10, (label.left + label.right - extent.cx - 26) / 2);
            drawUiIcon(dc, icon, start, (label.top + label.bottom - 16) / 2, color);
            label.left = start + 26;
            label.right -= 6;
        }
        DrawTextW(dc, value.c_str(), -1, &label,
                  (withIcon ? DT_LEFT : DT_CENTER) | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    BitBlt(target, item.rcItem.left, item.rcItem.top, width, height, memory, 0, 0, SRCCOPY);
    SelectObject(memory, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memory);
    return true;
}

void App::paintWindow() {
    PAINTSTRUCT paint{};
    HDC target = BeginPaint(window, &paint);
    RECT r{};
    GetClientRect(window, &r);
    // Double-buffer the shell; child controls are excluded by WS_CLIPCHILDREN.
    HDC dc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, std::max(1L, r.right), std::max(1L, r.bottom));
    auto oldBitmap = SelectObject(dc, bitmap);
    fill(dc, r, Background);
    // Content layer with a rounded leading corner, as in Windows 11 apps.
    roundedSurface(dc, {SidebarWidth, 0, r.right + 16, r.bottom + 16}, Surface, CardStroke, 8);
    SetBkMode(dc, TRANSPARENT);
    static HICON appIcon = static_cast<HICON>(
        LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 24, 24, LR_DEFAULTCOLOR));
    if (appIcon)
        DrawIconEx(dc, 22, 22, appIcon, 24, 24, 0, nullptr, DI_NORMAL);
    static HFONT brandFont =
        CreateFontW(-18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                    CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    SelectObject(dc, brandFont);
    SetTextColor(dc, Foreground);
    RECT title{56, 18, SidebarWidth - 12, 50};
    DrawTextW(dc, L"Server Desk", -1, &title, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    SelectObject(dc, small);
    SetTextColor(dc, Muted);
    RECT caption{24, 66, SidebarWidth - 16, 86};
    auto captionText = tr(L"서버 관리");
    DrawTextW(dc, captionText.c_str(), -1, &caption, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    for (const auto &card : cards)
        roundedSurface(dc, card, Panel, CardStroke, 8);
    if (statusPill.right > statusPill.left)
        roundedSurface(dc, statusPill, statusFill, statusFill, (statusPill.bottom - statusPill.top) / 2);
    BitBlt(target, paint.rcPaint.left, paint.rcPaint.top, paint.rcPaint.right - paint.rcPaint.left,
           paint.rcPaint.bottom - paint.rcPaint.top, dc, paint.rcPaint.left, paint.rcPaint.top, SRCCOPY);
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
    EndPaint(window, &paint);
}
} // namespace wrapper

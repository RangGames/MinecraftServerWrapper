#include "App.h"

namespace wrapper {

void App::layout() {
    if (!font || servers.empty() || IsIconic(window))
        return;
    RECT rect{};
    GetClientRect(window, &rect);
    int w = rect.right, h = rect.bottom;
    // Content layer starts at the sidebar edge; pages begin below the server header card.
    int left = SidebarWidth + 24, right = w - 24, width = right - left, top = 196, bottom = h - 24;
    int side = 280, content = width - side - 16;
    std::vector<int> visible;
    cards.clear();
    auto place = [&](int id, int x, int y, int width_, int height_) {
        visible.push_back(id);
        MoveWindow(control(id), x, y, std::max(1, width_), std::max(1, height_), TRUE);
        if (!(GetWindowLongPtrW(control(id), GWL_STYLE) & WS_VISIBLE))
            ShowWindow(control(id), SW_SHOWNA);
    };
    auto card = [&](int x, int y, int width_, int height_) { cards.push_back({x, y, x + width_, y + height_}); };
    // A card hugging a control with a small inset so its rounded corners stay visible.
    auto framed = [&](int id, int x, int y, int width_, int height_, int inset = 6) {
        card(x, y, width_, height_);
        place(id, x + inset, y + inset, width_ - inset * 2, height_ - inset * 2);
    };
    // Form labels share the field's row and are vertically centered against it.
    auto label = [&](int id, int x, int y, int width_) {
        HWND hwnd = control(id);
        auto style = GetWindowLongPtrW(hwnd, GWL_STYLE);
        if (!(style & SS_CENTERIMAGE))
            SetWindowLongPtrW(hwnd, GWL_STYLE, style | SS_CENTERIMAGE);
        place(id, x, y, width_, 34);
    };
    auto playersCard = [&](int x, int y, int width_, int height_) {
        card(x, y, width_, height_);
        place(206, x + 16, y + 12, width_ - 32, 24);
        place(PlayerFaces::ControlId, x + 6, y + 44, width_ - 12, height_ - 50);
    };

    // Sidebar: navigation and server actions.
    for (int i = 0; i < PageCount; ++i)
        place(DashboardTab + i, 12, 92 + i * 44, SidebarWidth - 24, 40);
    place(GroupManager, 12, h - 96, SidebarWidth - 24, 36);
    place(AddServer, 12, h - 52, SidebarWidth - 24, 36);

    // Top row: server tiles and the application menus.
    for (int i = 0; i < 4; ++i)
        place(MenuFile + i, right - 4 * 76 + i * 76, 28, 72, 30);
    place(ServerSelector, left, 16, width - 4 * 76 - 16, 52);

    // Server header card: name, state pill, metadata and lifecycle actions.
    card(left, 80, width, 96);
    int actions = right - 16 - (96 + 88 + 100 + 104 + 136 + 4 * 8);
    place(ServerTitle, left + 20, 92, actions - left - 36, 38);
    statusPill = {left + 20, 138, left + 20 + 100, 138 + 26};
    place(Status, statusPill.left + 10, statusPill.top + 3, 80, 20);
    place(ServerMeta, statusPill.right + 12, 141, actions - statusPill.right - 28, 22);
    int x = actions;
    for (auto [id, size] : std::initializer_list<std::pair<int, int>>{
             {Start, 96}, {Stop, 88}, {Restart, 100}, {Kill, 104}, {Folder, 136}}) {
        place(id, x, 111, size, 34);
        x += size + 8;
    }

    if (page == 0 && current().jar.empty() && current().launch.mode == 0) {
        card(left, top, width, std::min(bottom - top, 380));
        place(WelcomeTitle, left + 40, top + 48, width - 80, 44);
        place(WelcomeBody, left + 40, top + 104, width - 80, 56);
        place(WelcomeSteps, left + 40, top + 176, width - 80, 30);
        place(WelcomeAction, left + 40, top + 236, 180, 40);
        place(Installer, left + 232, top + 236, 156, 40);
    } else if (page == 0) {
        int metric = (content - 24) / 3;
        int ids[3][2]{{CpuTitle, CpuValue}, {RamTitle, RamValue}, {UptimeTitle, UptimeValue}};
        for (int i = 0; i < 3; ++i) {
            int cx = left + i * (metric + 12);
            card(cx, top, metric, 96);
            place(ids[i][0], cx + 18, top + 14, metric - 36, 22);
            place(ids[i][1], cx + 18, top + 40, metric - 36, 42);
        }
        int consoleHeight = std::max(170, (bottom - top - 108) * 2 / 5);
        int graphTop = top + 108, graphHeight = bottom - graphTop - 12 - consoleHeight;
        card(left, graphTop, content, graphHeight);
        place(GraphTitle, left + 18, graphTop + 12, content - 36, 24);
        place(Graph, left + 6, graphTop + 40, content - 12, graphHeight - 46);
        int consoleTop = graphTop + graphHeight + 12;
        card(left, consoleTop, content, bottom - consoleTop);
        place(ServerHint, left + 18, consoleTop + 10, content - 36, 22);
        place(Console, left + 10, consoleTop + 36, content - 20, bottom - consoleTop - 44);
        playersCard(left + content + 16, top, side, bottom - top);
    } else if (page == 1) {
        place(Wrap, left, top + 2, 116, 30);
        place(AutoScroll, left + 124, top + 2, 124, 30);
        place(SearchText, left + 260, top, std::max(80, content - 260 - 340), 34);
        place(FindNext, left + content - 332, top, 96, 34);
        place(Clear, left + content - 228, top, 100, 34);
        place(ExportLog, left + content - 120, top, 120, 34);
        framed(Console, left, top + 46, content, bottom - top - 46 - 48, 8);
        place(Command, left, bottom - 36, content - 108, 36);
        place(Send, left + content - 100, bottom - 36, 100, 36);
        playersCard(left + content + 16, top, side, bottom - top);
    } else if (page == 2) {
        place(PluginHeading, left, top, width, 32);
        place(PluginNote, left, top + 36, width, 24);
        place(PluginRefresh, left, top + 74, 120, 34);
        place(PluginImport, left + 128, top + 74, 140, 34);
        place(PluginToggle, left + 276, top + 74, 156, 34);
        place(PluginManage, left + 440, top + 74, 148, 34);
        place(PluginFolder, right - 156, top + 74, 156, 34);
        framed(PluginList, left, top + 120, width, bottom - top - 120);
    } else if (page == 3) {
        playersCard(left, top, 320, bottom - top);
        card(left + 336, top, width - 336, 248);
        int px = left + 336 + 20;
        place(PlayerInfo, px, top + 18, width - 336 - 40, 52);
        place(PlayerKick, px, top + 84, 140, 34);
        place(PlayerBan, px + 148, top + 84, 140, 34);
        place(PlayerOp, px, top + 130, 140, 34);
        place(PlayerDeop, px + 148, top + 130, 140, 34);
        place(PlayerManage, px, top + 190, 288, 34);
    } else if (page == 4) {
        place(BackupHeading, left, top, width, 32);
        place(BackupNote, left, top + 36, width, 26);
        place(BackupPath, left, top + 74, width - 136, 34);
        place(BackupChoose, right - 128, top + 74, 128, 34);
        place(BackupNow, left, top + 118, 130, 34);
        place(BackupFolder, left + 138, top + 118, 150, 34);
        place(BackupOptions, left + 296, top + 118, 130, 34);
        place(BackupRestore, left + 434, top + 118, 156, 34);
        place(BackupInfo, left, top + 160, width, 24);
        framed(BackupList, left, top + 190, width, bottom - top - 190);
    } else if (page == 5) {
        place(TaskHeading, left, top, width, 32);
        place(TaskNote, left, top + 36, width, 24);
        place(TaskSummary, left, top + 62, width, 40);
        // Editor card: action, interval, command, presets and add/update.
        int ey = top + 108, inner = width - 40, ex = left + 20;
        card(left, ey, width, 150);
        place(TaskTypeLabel, ex, ey + 14, 180, 22);
        place(TaskIntervalLabel, ex + 202, ey + 14, 110, 22);
        place(TaskCommandLabel, ex + 326, ey + 14, inner - 326, 22);
        place(TaskType, ex, ey + 40, 190, 160);
        place(TaskInterval, ex + 202, ey + 40, 110, 32);
        place(TaskCommand, ex + 326, ey + 40, inner - 326, 32);
        place(TaskPreset, ex, ey + 92, 190, 180);
        place(PresetName, ex + 202, ey + 90, 174, 34);
        place(PresetSave, ex + 384, ey + 90, 116, 34);
        place(PresetDelete, ex + 508, ey + 90, 124, 34);
        place(TaskAdd, left + width - 20 - 244, ey + 90, 118, 34);
        place(TaskUpdate, left + width - 20 - 118, ey + 90, 118, 34);
        framed(TaskList, left, ey + 162, width, bottom - (ey + 162) - 48);
        place(TaskRun, left, bottom - 36, 130, 36);
        place(TaskToggle, left + 138, bottom - 36, 160, 36);
        place(TaskTiming, left + 306, bottom - 36, 130, 36);
        place(TaskDelete, right - 110, bottom - 36, 110, 36);
    } else {
        place(SettingsHeading, left, top, width, 32);
        for (int i = 0; i < 4; ++i)
            place(SettingsGeneral + i, left + i * 164, top + 40, 156, 40);
        // Form card for the selected section; Save sits outside the card in the bottom-right corner.
        int cardTop = top + 92;
        card(left, cardTop, width, bottom - cardTop - 52);
        int inner = left + 24, innerRight = right - 24, innerWidth = innerRight - inner;
        place(SectionTitle, inner, cardTop + 18, innerWidth, 30);
        place(SectionHint, inner, cardTop + 50, innerWidth, 40);
        int field = inner + 158, fieldWidth = innerWidth - 276, form = cardTop + 104;
        if (settingsPage == 0) {
            label(205, inner, form, 146);
            place(ServerName, field, form, std::min(520, innerWidth - 158), 34);
            label(GroupNameLabel, inner, form + 56, 146);
            place(GroupName, field, form + 56, 320, 34);
            place(GroupManageSettings, field + 332, form + 56, 130, 34);
            label(ImageNote, inner, form + 112, 146);
            place(ProfileImage, field, form + 112, innerWidth - 400, 34);
            place(ChooseImage, innerRight - 228, form + 112, 112, 34);
            place(ResetImage, innerRight - 108, form + 112, 108, 34);
            place(RemoveServer, inner, form + 176, 174, 34);
        } else if (settingsPage == 1) {
            label(201, inner, form, 146);
            place(Java, field, form, fieldWidth - 120, 34);
            place(JavaDetect, innerRight - 220, form, 108, 34);
            place(BrowseJava, innerRight - 104, form, 104, 34);
            label(202, inner, form + 52, 146);
            place(Jar, field, form + 52, fieldWidth - 116, 34);
            place(ChooseServerFolder, innerRight - 220, form + 52, 108, 34);
            place(BrowseJar, innerRight - 104, form + 52, 104, 34);
            label(203, inner, form + 104, 146);
            place(Memory, field, form + 104, 120, 34);
            label(204, field + 136, form + 104, 210);
            place(Advanced, innerRight - 264, form + 104, 156, 34);
            place(Installer, innerRight - 100, form + 104, 100, 34);
            place(PathNote, inner, form + 160, innerWidth, 44);
            place(PathValidity, inner, form + 212, innerWidth, 48);
        } else if (settingsPage == 2) {
            int col = innerWidth / 2;
            label(PortLabel, inner, form, 146);
            place(PortValue, field, form, 120, 34);
            label(MaxPlayersLabel, inner + col, form, 146);
            place(MaxPlayersValue, inner + col + 158, form, 120, 34);
            label(ModeLabel, inner, form + 52, 146);
            place(ModeValue, field, form + 52, 190, 180);
            label(DifficultyLabel, inner + col, form + 52, 146);
            place(DifficultyValue, inner + col + 158, form + 52, 190, 180);
            place(OnlineMode, inner, form + 110, col - 12, 30);
            place(Whitelist, inner + col, form + 110, col, 30);
            place(Pvp, inner, form + 152, col - 12, 30);
            place(Flight, inner + col, form + 152, col, 30);
            place(CommandBlocks, inner, form + 194, col - 12, 30);
            place(PropertiesReload, inner, form + 244, 154, 34);
        } else {
            label(LanguageLabel, inner, form, 146);
            place(LanguageChoice, field, form, 240, 120);
            label(ThemeLabel, inner, form + 56, 146);
            place(ThemeChoice, field, form + 56, 280, 150);
            label(BundledFontLabel, inner, form + 112, 146);
            place(BundledFontChoice, field, form + 112, 280, 150);
            place(FontChoose, field, form + 166, 174, 34);
            place(FontInfo, inner, form + 220, innerWidth, 44);
        }
        place(SaveSettings, right - 174, bottom - 38, 174, 38);
    }

    for (HWND child = GetWindow(window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
        int id = GetDlgCtrlID(child);
        if (std::find(visible.begin(), visible.end(), id) == visible.end() &&
            (GetWindowLongPtrW(child, GWL_STYLE) & WS_VISIBLE))
            ShowWindow(child, SW_HIDE);
    }
    if (consoleDirty && (page == 0 || page == 1))
        restoreConsole();
    if (autoScroll && (page == 0 || page == 1)) {
        scrollEnd();
        SetPropW(control(Console), L"WrapperTailPending", reinterpret_cast<HANDLE>(1));
    }
    InvalidateRect(window, nullptr, FALSE);
}

} // namespace wrapper

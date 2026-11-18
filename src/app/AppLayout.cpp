#include "App.h"

namespace wrapper {

void App::layout() {
    if (!font || servers.empty() || IsIconic(window))
        return;
    RECT rect{};
    GetClientRect(window, &rect);
    int w = rect.right, h = rect.bottom;
    int left = 256, right = w - 28, width = right - left, top = 208, bottom = h - 28;
    int side = 244, content = width - side - 16;
    std::vector<int> visible;
    auto place = [&](int id, int x, int y, int width_, int height_) {
        visible.push_back(id);
        MoveWindow(control(id), x, y, std::max(1, width_), std::max(1, height_), TRUE);
        if (!(GetWindowLongPtrW(control(id), GWL_STYLE) & WS_VISIBLE))
            ShowWindow(control(id), SW_SHOWNA);
    };
    for (int i = 0; i < 4; ++i)
        place(MenuFile + i, right - 324 + i * 81, 12, 76, 28);
    place(GroupManager, 20, h - 100, 196, 36);
    place(AddServer, 20, h - 56, 196, 36);
    place(ServerSelector, left, 52, width, 52);
    place(ServerTitle, left, 122, width - 520, 36);
    place(Status, right - 514, 126, 90, 28);
    place(Start, right - 410, 122, 90, 36);
    place(Stop, right - 310, 122, 84, 36);
    place(Restart, right - 216, 122, 88, 36);
    place(Kill, right - 118, 122, 118, 36);
    place(ServerMeta, left, 168, width - 138, 24);
    place(Folder, right - 128, 164, 128, 28);
    for (int i = 0; i < PageCount; ++i)
        place(DashboardTab + i, 16, 146 + i * 48, 204, 42);
    if (page == 0 && current().jar.empty() && current().launch.mode == 0) {
        place(WelcomeTitle, left + 40, top + 64, width - 80, 48);
        place(WelcomeBody, left + 40, top + 128, width - 80, 56);
        place(WelcomeSteps, left + 40, top + 212, width - 80, 30);
        place(WelcomeAction, left + 40, top + 278, 180, 42);
        place(Installer, left + 236, top + 278, 156, 42);
    } else if (page == 0) {
        int card = (content - 24) / 3;
        place(CpuTitle, left + 16, top + 10, card - 28, 22);
        place(CpuValue, left + 16, top + 34, card - 28, 34);
        place(RamTitle, left + card + 28, top + 10, card - 28, 22);
        place(RamValue, left + card + 28, top + 34, card - 28, 34);
        place(UptimeTitle, left + card * 2 + 40, top + 10, card - 28, 22);
        place(UptimeValue, left + card * 2 + 40, top + 34, card - 28, 34);
        place(GraphTitle, left + 12, top + 96, content - 24, 24);
        int graphHeight = std::max(160, bottom - top - 260);
        place(Graph, left, top + 126, content, graphHeight);
        place(ServerHint, left + 8, top + 138 + graphHeight, content - 16, 24);
        place(Console, left + 8, top + 166 + graphHeight, content - 16, bottom - (top + 166 + graphHeight));
        place(206, left + content + 16, top + 8, side, 24);
        place(PlayerFaces::ControlId, left + content + 16, top + 42, side, bottom - top - 42);
    } else if (page == 1) {
        place(Wrap, left, top, 108, 28);
        place(AutoScroll, left + 112, top, 112, 28);
        place(SearchText, left + 238, top + 2, std::max(80, content - 570), 26);
        place(FindNext, left + content - 326, top, 90, 28);
        place(Clear, left + content - 230, top, 100, 28);
        place(ExportLog, left + content - 124, top, 124, 28);
        place(Console, left, top + 38, content, bottom - top - 82);
        place(Command, left, bottom - 34, content - 94, 32);
        place(Send, left + content - 86, bottom - 34, 86, 32);
        place(206, left + content + 16, top + 2, side, 24);
        place(PlayerFaces::ControlId, left + content + 16, top + 38, side, bottom - top - 38);
    } else if (page == 2) {
        place(PluginHeading, left, top, width, 32);
        place(PluginNote, left, top + 40, width, 24);
        place(PluginRefresh, left, top + 78, 120, 32);
        place(PluginImport, left + 132, top + 78, 140, 32);
        place(PluginToggle, left + 284, top + 78, 156, 32);
        place(PluginManage, left + 452, top + 78, 148, 32);
        place(PluginFolder, right - 156, top + 78, 156, 32);
        place(PluginList, left, top + 122, width, bottom - top - 122);
    } else if (page == 3) {
        place(206, left, top, width, 28);
        place(PlayerFaces::ControlId, left, top + 42, 300, bottom - top - 42);
        place(PlayerInfo, left + 324, top + 44, width - 324, 58);
        place(PlayerKick, left + 324, top + 122, 130, 34);
        place(PlayerBan, left + 468, top + 122, 130, 34);
        place(PlayerOp, left + 324, top + 170, 130, 34);
        place(PlayerDeop, left + 468, top + 170, 130, 34);
        place(PlayerManage, left + 324, top + 220, 280, 34);
    } else if (page == 4) {
        place(BackupHeading, left, top, width, 32);
        place(BackupNote, left, top + 40, width, 28);
        place(BackupPath, left, top + 84, width - 136, 30);
        place(BackupChoose, right - 124, top + 84, 124, 30);
        place(BackupNow, left, top + 128, 130, 32);
        place(BackupFolder, left + 144, top + 128, 150, 32);
        place(BackupOptions, left + 310, top + 128, 130, 32);
        place(BackupRestore, left + 454, top + 128, 156, 32);
        place(BackupInfo, left, top + 166, width, 24);
        place(BackupList, left, top + 194, width, bottom - top - 194);
    } else if (page == 5) {
        place(TaskHeading, left, top, width, 32);
        place(TaskNote, left, top + 36, width, 30);
        place(TaskSummary, left, top + 70, width, 44);
        place(TaskTypeLabel, left, top + 120, 180, 22);
        place(TaskIntervalLabel, left + 202, top + 120, 110, 22);
        place(TaskCommandLabel, left + 326, top + 120, width - 326, 22);
        place(TaskType, left, top + 148, 190, 160);
        place(TaskInterval, left + 202, top + 148, 110, 30);
        place(TaskCommand, left + 326, top + 148, width - 326, 30);
        place(TaskPreset, left, top + 194, 190, 180);
        place(PresetName, left + 202, top + 194, 174, 32);
        place(PresetSave, left + 384, top + 194, 116, 32);
        place(PresetDelete, left + 508, top + 194, 124, 32);
        place(TaskAdd, right - 246, top + 194, 118, 32);
        place(TaskUpdate, right - 118, top + 194, 118, 32);
        place(TaskList, left, top + 244, width, bottom - top - 292);
        place(TaskRun, left, bottom - 34, 130, 32);
        place(TaskToggle, left + 144, bottom - 34, 160, 32);
        place(TaskTiming, left + 320, bottom - 34, 130, 32);
        place(TaskDelete, right - 100, bottom - 34, 100, 32);
    } else {
        place(SettingsHeading, left, top, width, 32);
        for (int i = 0; i < 4; ++i)
            place(SettingsGeneral + i, left + i * 164, top + 46, 156, 36);
        place(SectionTitle, left, top + 104, width, 30);
        place(SectionHint, left, top + 140, width, 42);
        int field = left + 158, fieldWidth = width - 276, form = top + 202;
        if (settingsPage == 0) {
            place(205, left, form + 4, 146, 26);
            place(ServerName, field, form, std::min(520, width - 158), 32);
            place(GroupNameLabel, left, form + 60, 146, 26);
            place(GroupName, field, form + 56, 320, 32);
            place(GroupManageSettings, field + 332, form + 56, 130, 32);
            place(ImageNote, left, form + 116, 146, 26);
            place(ProfileImage, field, form + 112, width - 400, 32);
            place(ChooseImage, right - 228, form + 112, 112, 32);
            place(ResetImage, right - 108, form + 112, 108, 32);
            place(RemoveServer, left, form + 174, 174, 34);
        } else if (settingsPage == 1) {
            place(201, left, form + 4, 146, 26);
            place(Java, field, form, fieldWidth - 120, 32);
            place(JavaDetect, right - 220, form, 108, 32);
            place(BrowseJava, right - 104, form, 104, 32);
            place(202, left, form + 56, 146, 26);
            place(Jar, field, form + 52, fieldWidth - 116, 32);
            place(ChooseServerFolder, right - 220, form + 52, 108, 32);
            place(BrowseJar, right - 104, form + 52, 104, 32);
            place(203, left, form + 108, 146, 26);
            place(Memory, field, form + 104, 120, 32);
            place(204, field + 136, form + 108, 210, 26);
            place(Advanced, right - 260, form + 104, 156, 32);
            place(Installer, right - 100, form + 104, 100, 32);
            place(PathNote, left, form + 160, width, 44);
            place(PathValidity, left, form + 212, width, 48);
        } else if (settingsPage == 2) {
            int col = width / 2;
            place(PortLabel, left, form + 4, 146, 26);
            place(PortValue, field, form, 120, 32);
            place(MaxPlayersLabel, left + col, form + 4, 146, 26);
            place(MaxPlayersValue, left + col + 158, form, 120, 32);
            place(ModeLabel, left, form + 56, 146, 26);
            place(ModeValue, field, form + 52, 190, 180);
            place(DifficultyLabel, left + col, form + 56, 146, 26);
            place(DifficultyValue, left + col + 158, form + 52, 190, 180);
            place(OnlineMode, left, form + 110, col, 30);
            place(Whitelist, left + col, form + 110, col, 30);
            place(Pvp, left, form + 152, col, 30);
            place(Flight, left + col, form + 152, col, 30);
            place(CommandBlocks, left, form + 194, col, 30);
            place(PropertiesReload, left, form + 244, 154, 32);
        } else {
            place(LanguageLabel, left, form + 4, 146, 26);
            place(LanguageChoice, field, form, 240, 120);
            place(ThemeLabel, left, form + 60, 146, 26);
            place(ThemeChoice, field, form + 56, 280, 150);
            place(BundledFontLabel, left, form + 116, 146, 26);
            place(BundledFontChoice, field, form + 112, 280, 150);
            place(FontChoose, field, form + 166, 174, 34);
            place(FontInfo, left, form + 220, width, 44);
        }
        place(SaveSettings, right - 174, bottom - 40, 174, 36);
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

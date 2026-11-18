#include "App.h"

namespace wrapper {

HWND App::control(int id) const {
    return GetDlgItem(window, id);
}

Server &App::current() {
    return *servers[selected];
}

void App::set(int id, const std::wstring &value) {
    bool editable = id == GroupName || id == PresetName || id == Console || id == Java || id == Jar ||
                    id == Memory || id == ServerName || id == BackupPath || id == TaskInterval ||
                    id == TaskCommand || id == PortValue || id == MaxPlayersValue || id == Command;
    if (!editable) {
        auto found = displayedText.find(id);
        if (found != displayedText.end() && found->second == value)
            return;
        displayedText[id] = value;
    }
    bool raw = editable || id == ProfileImage || id == Console || id == ServerTitle || id == ServerMeta;
    auto display = raw ? value : tr(value);
    if (text(control(id)) != display)
        SetWindowTextW(control(id), display.c_str());
}
void App::enable(int id, bool state) {
    HWND hwnd = control(id);
    if (hwnd && (IsWindowEnabled(hwnd) != FALSE) != state)
        EnableWindow(hwnd, state);
}

int App::message(const std::wstring &value, const std::wstring &title, UINT flags, HWND owner) {
    if (!owner)
        owner = groupWindow && GetActiveWindow() == groupWindow ? groupWindow : window;
    // The group manager is a separate top-level window; keep it from acting while the main window
    // waits on a confirmation (it would otherwise change state the caller already checked).
    bool lockGroup = groupWindow && owner != groupWindow && IsWindowEnabled(groupWindow);
    if (lockGroup)
        EnableWindow(groupWindow, FALSE);
    auto result = showThemedMessage(owner, tr(value), tr(title), flags, font, heading,
                                    {Surface, Panel, Border, Foreground, Muted, Accent, lightTheme}, english);
    if (lockGroup && groupWindow)
        EnableWindow(groupWindow, TRUE);
    return result;
}
void App::error(const std::wstring &value, HWND owner) {
    message(value, L"Minecraft Server Manager", MB_OK | MB_ICONERROR, owner);
}
bool App::confirm(const std::wstring &value, HWND owner) {
    return message(value, L"확인", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2, owner) == IDYES;
}

HWND App::add(const wchar_t *type, const wchar_t *label, DWORD style, int id) {
    if (_wcsicmp(type, L"BUTTON") == 0 || (_wcsicmp(type, L"STATIC") == 0 && *label))
        labels[id] = label;
    if (_wcsicmp(type, L"STATIC") == 0 && !(style & SS_TYPEMASK) &&
        !(style & (SS_EDITCONTROL | SS_ELLIPSISMASK)))
        style |= SS_ENDELLIPSIS;
    auto display = (_wcsicmp(type, L"EDIT") == 0) ? std::wstring(label) : tr(label);
    if (_wcsicmp(type, L"COMBOBOX") == 0)
        style |= CBS_OWNERDRAWFIXED | CBS_HASSTRINGS;
    HWND child = CreateWindowExW(0, type, display.c_str(), WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, window,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr),
                                 nullptr);
    SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    if (_wcsicmp(type, L"EDIT") == 0)
        SendMessageW(child, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(10, 10));
    SetWindowTheme(child, L"", L"");
    if (_wcsicmp(type, L"EDIT") == 0 || _wcsicmp(type, L"COMBOBOX") == 0 || _wcsicmp(type, L"BUTTON") == 0)
        SetWindowSubclass(child, inputProc, 2, reinterpret_cast<DWORD_PTR>(this));
    return child;
}

void App::button(const wchar_t *label, int id) {
    add(L"BUTTON", label, WS_TABSTOP | BS_OWNERDRAW, id);
}

void App::check(const wchar_t *label, int id, bool checked) {
    add(L"BUTTON", label, WS_TABSTOP | BS_AUTOCHECKBOX, id);
    SendMessageW(control(id), BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

void App::styleFont(int id, HFONT value) {
    SendMessageW(control(id), WM_SETFONT, reinterpret_cast<WPARAM>(value), TRUE);
}

void App::listColumns(int id, const std::vector<std::pair<std::wstring, int>> &columns) {
    HWND list = add(WC_LISTVIEWW, L"", WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, id);
    SetWindowSubclass(list, scrollProc, 3, reinterpret_cast<DWORD_PTR>(this));
    ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    ListView_SetBkColor(list, Panel);
    ListView_SetTextBkColor(list, Panel);
    ListView_SetTextColor(list, Foreground);
    for (size_t i = 0; i < columns.size(); ++i) {
        LVCOLUMNW column{};
        column.mask = LVCF_TEXT | LVCF_WIDTH;
        column.cx = columns[i].second;
        columnLabels[id].push_back(columns[i].first);
        auto label = tr(columns[i].first);
        column.pszText = label.data();
        SendMessageW(list, LVM_INSERTCOLUMNW, i, reinterpret_cast<LPARAM>(&column));
    }
}

void App::listRow(int id, int row, const std::vector<std::wstring> &values) {
    for (size_t i = 0; i < values.size(); ++i) {
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = row;
        item.iSubItem = static_cast<int>(i);
        bool raw = (id == PluginList && i != 3) || (id == BackupList && i == 0) || (id == TaskList && i == 3);
        auto value = raw ? values[i] : tr(values[i]);
        item.pszText = value.data();
        SendMessageW(control(id), i == 0 ? LVM_INSERTITEMW : LVM_SETITEMTEXTW, i == 0 ? 0 : row,
                     reinterpret_cast<LPARAM>(&item));
    }
}

int App::listSelection(int id) const {
    return ListView_GetNextItem(control(id), -1, LVNI_SELECTED);
}

void App::refreshPageData() {
    if (page == 2)
        refreshPlugins();
    if (page == 4)
        refreshBackups();
    if (page == 5)
        refreshTasks();
}

void App::showPage(int value) {
    page = std::clamp(value, 0, PageCount - 1);
    refreshPageData();
    layout();
    updateControls();
    InvalidateRect(window, nullptr, FALSE);
    for (int i = 0; i < PageCount; ++i)
        InvalidateRect(control(DashboardTab + i), nullptr, TRUE);
    for (int id : {RailHome, RailConsole, RailSettings})
        InvalidateRect(control(id), nullptr, TRUE);
}

void App::updateControls() {
    if (servers.empty())
        return;
    auto &server = current();
    bool running = server.process.running();
    bool copying = overlapsBackup(server);
    bool inUse = overlapsRunningDirectory(server);
    for (int id : {Java, BrowseJava, Jar, BrowseJar, ChooseServerFolder, Memory, Eula, JavaDetect, Advanced})
        enable(id, !running && !copying && !closing);
    enable(ServerName, !closing);
    enable(GroupName, !closing);
    enable(GroupManager, !closing);
    enable(Start, !running && !copying && !closing && (!server.jar.empty() || server.launch.mode != 0));
    enable(Stop, (running && !server.process.stopping()) || server.recoverAt != 0);
    enable(Kill, running);
    enable(Restart, running && !server.process.stopping() && !closing);
    enable(Command, running && !server.process.stopping());
    enable(Send, running && !server.process.stopping());
    enable(RemoveServer, servers.size() > 1 && !running && !copying && !closing);
    enable(AddServer, !closing);
    for (int id : {PluginImport, PluginToggle, PluginManage})
        enable(id, !inUse && !copying && !closing && !pluginLoading);
    enable(BackupNow, !backupPending && !closing);
    enable(BackupChoose, !copying);
    enable(BackupRestore, !inUse && !backupPending && !closing);
    for (int id : {PlayerKick, PlayerOp, PlayerDeop, PlayerBan, PlayerManage})
        enable(id, running && !server.process.stopping());
    std::wstring status = server.recoverAt            ? L"Recovery…"
                          : !running                  ? L"Offline"
                          : server.process.stopping() ? L"Stopping…"
                          : server.ready              ? L"Online"
                                                      : L"Starting…";
    set(Status, status);
    statusColors();
    set(ServerTitle, server.name);
    auto path = serverDirectory(server);
    auto filename = server.launch.mode >= 2 ? fs::path(server.launch.target).filename().wstring()
                                            : fs::path(server.jar).filename().wstring();
    set(ServerMeta, path.empty()
                        ? tr(L"설정에서 Java와 서버 JAR을 지정하세요.")
                        : tr(server.kind) + L"   ·   " + filename +
                              (running ? L"   ·   PID " + std::to_wstring(server.process.pid()) : L""));
    set(206, std::to_wstring(server.players.size()) + L" Players Online");
    set(CpuValue, decimal(server.process.cpuPercent()) + L" %");
    set(RamValue, decimal(server.process.memoryMB(), 0) + L" MB");
    set(UptimeValue, duration(server.process.uptimeSeconds()));
    set(PlayerInfo, L"목록에서 플레이어를 선택하세요. OP·추방·차단 명령은 확인 후 서버에 전송됩니다.");
    set(BackupInfo, backupPending ? L"백업 중: " + std::to_wstring(backupJob.files()) + L"개 파일 복사됨"
                                  : L"실행 중 백업은 정상 종료 후 진행하며, 완료 후 서버를 다시 시작합니다.");
    const wchar_t *titles[]{L"서버 프로필", L"Java 및 서버 실행", L"게임 및 접속 설정", L"화면과 언어"};
    const wchar_t *hints[]{
        L"서버 이름과 목록에 표시할 이미지를 설정하세요. 수정 후 설정 저장을 누르세요.",
        L"서버가 중지된 상태에서 실행 경로와 메모리를 설정하세요. 수정 후 설정 저장을 누르세요.",
        L"server.properties에 저장되며 다음 서버 시작 시 적용됩니다. 알 수 없는 항목과 주석은 보존합니다.",
        L"기본 제공 글꼴을 선택하세요. 기타 글꼴은 시스템에서 고를 수 있습니다. 변경 후 설정 저장을 "
        L"누르세요."};
    set(SectionTitle, titles[settingsPage]);
    bool proxy = server.kind == L"Velocity" || server.kind == L"Waterfall" || server.kind == L"BungeeCord";
    set(SectionHint,
        settingsPage == 2 && proxy
            ? L"프록시는 자체 설정 파일을 사용합니다. 서버 속성 편집 대신 서버 폴더의 설정 파일을 수정하세요."
            : hints[settingsPage]);
    for (int id : {PortValue, MaxPlayersValue, ModeValue, DifficultyValue, OnlineMode, Whitelist, Pvp, Flight,
                   CommandBlocks})
        enable(id, !copying && !closing && !proxy);
    bool listChanged = false;
    for (auto &item : servers) {
        int state = !item->process.running() ? 0 : item->process.stopping() ? 1 : item->ready ? 2 : 3;
        if (item->paintedState != state) {
            item->paintedState = state;
            listChanged = true;
        }
    }
    if (listChanged)
        InvalidateRect(control(ServerSelector), nullptr, FALSE);
}

void App::menu(int id) {
    HMENU menu = CreatePopupMenu();
    auto item = [&](UINT command, const wchar_t *label) {
        AppendMenuW(menu, MF_STRING, command, tr(label).c_str());
    };
    if (id == MenuFile) {
        item(AddServer, L"서버 프로필 추가");
        item(ExportLog, L"현재 콘솔 저장…");
        item(600, L"종료");
    } else if (id == MenuServer) {
        item(Start, L"서버 시작");
        item(Stop, L"정상 종료");
        item(Restart, L"재시작");
        item(Kill, L"강제 종료");
        item(Folder, L"서버 폴더 열기");
    } else if (id == MenuTools) {
        item(601, L"server.properties 열기");
        item(602, L"logs/latest.log 열기");
        item(603, L"플러그인 관리");
        item(604, L"백업 관리");
        item(Advanced, L"고급 실행 · 자동 복구");
        item(JavaDetect, L"설치된 Java 탐색");
        item(Installer, L"서버 설치 도우미");
        item(RemoteSettings, L"원격 웹 패널 · 계정");
        item(BackupOptions, L"백업 압축 · 보관 설정");
        item(BackupRestore, L"선택한 백업 복원");
        item(TaskTiming, L"선택한 작업의 실행 시각");
        item(PlayerManage, L"플레이어 · 화이트리스트 관리");
        item(PluginManage, L"플러그인 교체 · 삭제");
        item(GroupActions, L"그룹 재시작 · 명령");
        item(ExtraProperties, L"추가 서버 속성");
    } else {
        item(605, L"사용 방법");
    }
    RECT rect{};
    GetWindowRect(control(id), &rect);
    int command =
        TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN, rect.left, rect.bottom, 0, window, nullptr);
    DestroyMenu(menu);
    if (command)
        SendMessageW(window, WM_COMMAND, command, 0);
}

void App::create(HWND hwnd) {
    window = hwnd;
    auto makeFont = [](int height, int weight, const wchar_t *face) {
        return CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
    };
    font = makeFont(-15, FW_NORMAL, L"Segoe UI");
    small = makeFont(-12, FW_NORMAL, L"Segoe UI");
    mono = makeFont(-14, FW_NORMAL, L"Consolas");
    large = makeFont(-26, FW_NORMAL, L"Segoe UI");
    heading = makeFont(-22, FW_NORMAL, L"Segoe UI");
    bgBrush = CreateSolidBrush(Background);
    surfaceBrush = CreateSolidBrush(Surface);
    panelBrush = CreateSolidBrush(Panel);
    fieldBrush = CreateSolidBrush(Field);
    loadSettings();
    applyTheme();
    for (int i = 0; i < 4; ++i)
        button(i == 0 ? L"파일" : i == 1 ? L"서버" : i == 2 ? L"도구" : L"도움말", MenuFile + i);
    add(L"LISTBOX", L"",
        WS_TABSTOP | WS_HSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_MULTICOLUMN |
            LBS_NOINTEGRALHEIGHT,
        ServerSelector);
    SendMessageW(control(ServerSelector), LB_SETITEMHEIGHT, 0, 48);
    SendMessageW(control(ServerSelector), LB_SETCOLUMNWIDTH, 292, 0);
    button(L"그룹 관리자", GroupManager);
    button(L"서버 추가", AddServer);
    button(L"홈", RailHome);
    button(L"콘솔", RailConsole);
    button(L"설정", RailSettings);
    add(L"STATIC", L"", 0, ServerTitle);
    styleFont(ServerTitle, large);
    add(L"STATIC", L"Offline", SS_CENTER | SS_CENTERIMAGE, Status);
    add(L"STATIC", L"", SS_PATHELLIPSIS, ServerMeta);
    styleFont(ServerMeta, small);
    button(L"Start", Start);
    button(L"Stop", Stop);
    button(L"Kill", Kill);
    button(L"Restart", Restart);
    button(L"폴더 열기", Folder);
    for (int i = 0; i < PageCount; ++i)
        button(Pages[i], DashboardTab + i);
    add(L"STATIC", L"CPU", 0, CpuTitle);
    add(L"STATIC", L"RAM", 0, RamTitle);
    add(L"STATIC", L"UPTIME", 0, UptimeTitle);
    add(L"STATIC", L"0 %", 0, CpuValue);
    add(L"STATIC", L"0 MB", 0, RamValue);
    add(L"STATIC", L"0h 00m 00s", 0, UptimeValue);
    for (int id : {CpuValue, RamValue, UptimeValue})
        styleFont(id, large);
    add(L"STATIC", L"서버 통계 · 최근 120초", 0, GraphTitle);
    HWND graph = add(L"STATIC", L"", 0, Graph);
    SetWindowSubclass(graph, graphProc, 1, reinterpret_cast<DWORD_PTR>(this));
    add(L"STATIC", L"최근 콘솔", 0, ServerHint);
    styleFont(ServerHint, small);
    createConsole();
    check(L"Line wrap", Wrap, wrapping);
    check(L"Auto scroll", AutoScroll, autoScroll);
    add(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, SearchText);
    button(L"찾기", FindNext);
    button(L"지우기", Clear);
    button(L"로그 저장", ExportLog);
    add(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, Command);
    SendMessageW(control(Command), EM_SETLIMITTEXT, 512, 0);
    button(L"전송", Send);
    add(L"STATIC", L"0 Players Online", 0, 206);
    if (!faces.create(window, font))
        append(current(), L"[Wrapper] 플레이어 얼굴 목록 초기화 실패\r\n");
    add(L"STATIC", L"", 0, PlayerInfo);
    button(L"추방 (kick)", PlayerKick);
    button(L"차단 (ban)", PlayerBan);
    button(L"OP 부여", PlayerOp);
    button(L"OP 해제", PlayerDeop);
    add(L"STATIC", L"Plugins", 0, PluginHeading);
    styleFont(PluginHeading, heading);
    add(L"STATIC", L"plugins 폴더의 JAR 파일을 관리합니다. 변경은 다음 서버 시작 시 적용됩니다.", 0,
        PluginNote);
    button(L"새로 고침", PluginRefresh);
    button(L"JAR 가져오기", PluginImport);
    button(L"활성 / 비활성", PluginToggle);
    button(L"plugins 폴더", PluginFolder);
    listColumns(PluginList,
                {{L"파일 이름", 270}, {L"실제 이름", 220}, {L"버전", 130}, {L"상태", 180}, {L"크기", 100}});
    add(L"STATIC", L"Backups", 0, BackupHeading);
    styleFont(BackupHeading, heading);
    add(L"STATIC",
        L"실행 중이면 정상 종료 → 백업 → 재실행합니다. 저장 위치는 서버 폴더 밖이어야 "
        L"합니다.",
        0, BackupNote);
    add(L"EDIT", L"", ES_READONLY | ES_AUTOHSCROLL, BackupPath);
    button(L"저장 위치…", BackupChoose);
    button(L"지금 백업", BackupNow);
    button(L"백업 폴더 열기", BackupFolder);
    button(L"압축 · 보관", BackupOptions);
    button(L"선택 백업 복원", BackupRestore);
    button(L"실행 시각", TaskTiming);
    button(L"화이트리스트 · 차단 해제", PlayerManage);
    button(L"교체 · 제거", PluginManage);
    add(L"STATIC", L"", 0, BackupInfo);
    listColumns(BackupList, {{L"백업 폴더", 600}, {L"상태", 160}});
    add(L"STATIC", L"Scheduler", 0, TaskHeading);
    styleFont(TaskHeading, heading);
    add(L"STATIC", L"1. 동작 선택   2. 반복 간격 입력   3. 작업 추가 · 목록에서 선택하면 수정할 수 있습니다.",
        0, TaskNote);
    add(L"STATIC", L"동작", 0, TaskTypeLabel);
    add(L"STATIC", L"간격 (초)", 0, TaskIntervalLabel);
    add(L"STATIC", L"서버 명령", 0, TaskCommandLabel);
    add(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, TaskType);
    for (int i = 0; i < 3; ++i)
        SendMessageW(control(TaskType), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(taskName(i)));
    SendMessageW(control(TaskType), CB_SETCURSEL, 0, 0);
    add(L"EDIT", L"300", WS_TABSTOP | ES_NUMBER, TaskInterval);
    add(L"EDIT", L"save-all", WS_TABSTOP | ES_AUTOHSCROLL, TaskCommand);
    add(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, TaskPreset);
    add(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, PresetName);
    SendMessageW(control(PresetName), EM_SETLIMITTEXT, 48, 0);
    button(L"프리셋 저장", PresetSave);
    button(L"프리셋 삭제", PresetDelete);
    add(L"STATIC", L"", SS_EDITCONTROL, TaskSummary);
    button(L"작업 수정", TaskUpdate);
    button(L"작업 추가", TaskAdd);
    button(L"지금 실행", TaskRun);
    button(L"일시 중지 / 재개", TaskToggle);
    button(L"작업 삭제", TaskDelete);
    listColumns(TaskList, {{L"동작", 190},
                           {L"간격", 110},
                           {L"다음 실행", 150},
                           {L"명령", 260},
                           {L"최근 실행", 175},
                           {L"결과", 280}});
    add(L"STATIC", L"첫 서버를 연결하세요", 0, WelcomeTitle);
    add(L"STATIC", L"서버 파일을 선택하면 콘솔, 플레이어와 백업을 한곳에서 관리할 수 있습니다.",
        SS_EDITCONTROL, WelcomeBody);
    add(L"STATIC", L"01  서버 파일 선택     02  Java · 메모리 설정     03  서버 시작", 0, WelcomeSteps);
    button(L"서버 연결하기", WelcomeAction);
    createSettings();
    for (int id : {PluginList, BackupList, TaskList})
        SetWindowSubclass(ListView_GetHeader(control(id)), headerProc, 1, reinterpret_cast<DWORD_PTR>(this));
    SetWindowSubclass(control(ServerSelector), serverListProc, 1, reinterpret_cast<DWORD_PTR>(this));
    SetWindowSubclass(control(ServerSelector), scrollProc, 3, reinterpret_cast<DWORD_PTR>(this));
    SetWindowSubclass(control(PlayerFaces::ControlId), scrollProc, 3, reinterpret_cast<DWORD_PTR>(this));
    applyFont();
    styleFont(WelcomeTitle, large);
    applyTheme();
    applyLanguage();
    taskEditorState();
    loadFields();
    refreshServerList();
    updateControls();
    layout();
    DragAcceptFiles(window, TRUE);
    DragAcceptFiles(control(PluginList), TRUE);
    SetTimer(window, 1, timerInterval, nullptr);
    remote.setNotify(window, WM_APP + 2);
    startConfiguredServices();
}

bool App::keyboard(MSG &msg) {
    if (groupWindow && (msg.hwnd == groupWindow || IsChild(groupWindow, msg.hwnd))) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            DestroyWindow(groupWindow);
            return true;
        }
        return IsDialogMessageW(groupWindow, &msg) != FALSE;
    }
    if (msg.message == WM_KEYDOWN) {
        if (msg.wParam == VK_ESCAPE)
            return true;
        if ((GetKeyState(VK_CONTROL) & 0x8000) && msg.wParam == 'F') {
            showPage(1);
            SetFocus(control(SearchText));
            return true;
        }
        if (GetFocus() == control(Command)) {
            if (msg.wParam == VK_RETURN) {
                sendCommand();
                return true;
            }
            if ((msg.wParam == VK_UP || msg.wParam == VK_DOWN) && !commandHistory.empty()) {
                if (msg.wParam == VK_UP && historyPosition)
                    --historyPosition;
                if (msg.wParam == VK_DOWN && historyPosition < commandHistory.size())
                    ++historyPosition;
                set(Command, historyPosition < commandHistory.size() ? commandHistory[historyPosition] : L"");
                SendMessageW(control(Command), EM_SETSEL, text(control(Command)).size(),
                             text(control(Command)).size());
                return true;
            }
        }
        if (GetFocus() == control(SearchText) && msg.wParam == VK_RETURN) {
            findNext();
            return true;
        }
    }
    return IsDialogMessageW(window, &msg) != FALSE;
}

} // namespace wrapper

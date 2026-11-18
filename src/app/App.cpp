#include "App.h"
#include "AppResources.h"

namespace wrapper {

void App::timer() {
    if (timersBusy || servers.empty())
        return;
    timersBusy = true;
    auto now = GetTickCount64();
    bool sample = now - lastSample >= 1000;
    bool anyRunning = false, stateChanged = false;
    for (size_t i = 0; i < servers.size(); ++i) {
        auto &server = *servers[i];
        bool wasRunning = server.process.running();
        auto uptime = server.process.uptimeSeconds();
        receive(server, server.process.poll());
        if (wasRunning && !server.process.running()) {
            stateChanged = true;
            server.players.clear();
            server.line.clear();
            server.droppingLine = false;
            server.ready = false;
            if (i == selected)
                faces.clear();
            bool restart = server.restart;
            server.restart = false;
            if (server.backupAfterStop && !closing) {
                server.backupAfterStop = false;
                backupPending = false;
                runBackup(i, false);
            } else if (restart && !closing)
                startServer(server, false);
            else if (!closing && server.autoRecover && !server.intentionalStop && !overlapsBackup(server)) {
                if (uptime >= 300)
                    server.recoveryAttempts = 0;
                if (server.recoveryAttempts < server.recoveryLimit) {
                    ++server.recoveryAttempts;
                    server.recoverAt = now + server.recoveryDelay * 1000ULL;
                    append(server, L"[Recovery] 재시작 대기: " + std::to_wstring(server.recoveryAttempts) +
                                       L"회\r\n");
                } else
                    append(server,
                           L"[Recovery] 연속 실패 한도 도달. 설정을 확인한 뒤 수동으로 시작하세요.\r\n");
            }
        }
        if (!closing && server.recoverAt && now >= server.recoverAt && !overlapsBackup(server)) {
            server.recoverAt = 0;
            if (!startServer(server, false))
                append(server, L"[Recovery] 실행 실패. 수동 점검이 필요합니다.\r\n");
        }
        if (sample) {
            server.process.sample();
            if (server.process.running()) {
                server.samples.push_back({server.process.cpuPercent(), server.process.memoryMB(),
                                          static_cast<double>(server.players.size())});
                if (server.samples.size() > 120)
                    server.samples.erase(server.samples.begin());
            }
            if (!closing)
                for (auto &task : server.tasks)
                    if (task.enabled && (task.minuteOfDay < 0 ? now >= task.due : wallNow() >= task.nextWall))
                        runTask(i, task);
        }
        anyRunning |= server.process.running();
    }
    applyRemote();
    if (installerActive) {
        auto output = installerProcess.poll();
        if (!output.empty())
            append(current(), L"[Installer] " + output);
        if (!installerProcess.running()) {
            installerActive = false;
            if (!closing)
                message(L"설치 프로세스 종료 코드: " + std::to_wstring(installerProcess.exitCode()) +
                            L"\n설치 폴더에서 실행 JAR 또는 BAT를 선택하세요.\n" + installerTarget,
                        L"서버 설치", MB_OK | MB_ICONINFORMATION);
        }
    }
    faces.poll();
    pollPlugins();
    if (auto result = backupJob.takeResult()) {
        append(*servers[backupOwner], L"[Wrapper] " + *result + L"\r\n");
        backupPending = false;
        auto &owner = *servers[backupOwner];
        for (auto &task : owner.tasks)
            if (task.type == 2 && task.lastResult == L"백업 요청")
                task.lastResult = *result;
        saveSettings();
        if (owner.resumeAfterBackup && !closing) {
            owner.resumeAfterBackup = false;
            startServer(owner, false);
        }
        stateChanged = true;
        if (page == 4)
            refreshBackups();
    }
    if (sample) {
        lastSample = now;
        if (page == 5)
            refreshTasks();
        if (page == 0)
            InvalidateRect(control(Graph), nullptr, FALSE);
    }
    if (sample || stateChanged) {
        updateControls();
        if (groupWindow)
            refreshGroupManager();
    }
    unsigned desired =
        anyRunning || backupPending || pluginLoading || installerActive || remote.running() ? 100 : 1000;
    if (desired != timerInterval) {
        timerInterval = desired;
        SetTimer(window, 1, timerInterval, nullptr);
    }
    timersBusy = false;
    if (closing && !anyRunning && !backupPending && !installerActive) {
        saveSettings();
        DestroyWindow(window);
    }
}

void App::attach(HWND hwnd) {
    window = hwnd;
}

LRESULT App::dispatch(UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_DWMCOLORIZATIONCOLORCHANGED:
    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
        if (theme == 2)
            applyTheme();
        return 0;
    case WM_DROPFILES: {
        HDROP drop = reinterpret_cast<HDROP>(w);
        UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        std::vector<fs::path> paths;
        if (count <= 64)
            for (UINT i = 0; i < count; ++i) {
                std::vector<wchar_t> path(DragQueryFileW(drop, i, nullptr, 0) + 1);
                DragQueryFileW(drop, i, path.data(), static_cast<UINT>(path.size()));
                paths.emplace_back(path.data());
            }
        DragFinish(drop);
        if (page == 2)
            importPluginFiles(paths);
        else if (count != 1)
            error(L"파일 또는 폴더를 한 번에 하나씩 놓으세요.");
        else
            acceptPath(paths.front());
        return 0;
    }
    case WM_SIZE:
        layout();
        return 0;
    case WM_PAINT:
        paintWindow();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_TIMER:
        timer();
        return 0;
    case WM_MEASUREITEM: {
        auto *item = reinterpret_cast<MEASUREITEMSTRUCT *>(l);
        if (item->CtlType == ODT_COMBOBOX) {
            item->itemHeight = 28;
            return TRUE;
        }
        if (item->CtlID == ServerSelector) {
            item->itemHeight = 44;
            item->itemWidth = 260;
            return TRUE;
        }
        if (item->CtlID == PlayerFaces::ControlId) {
            item->itemHeight = 64;
            return TRUE;
        }
        break;
    }
    case WM_DRAWITEM:
        if (drawItem(*reinterpret_cast<DRAWITEMSTRUCT *>(l)))
            return TRUE;
        break;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(w);
        int id = GetDlgCtrlID(reinterpret_cast<HWND>(l));
        wchar_t controlClass[32]{};
        GetClassNameW(reinterpret_cast<HWND>(l), controlClass, 32);
        bool field = _wcsicmp(controlClass, L"EDIT") == 0 || msg == WM_CTLCOLOREDIT ||
                     msg == WM_CTLCOLORLISTBOX || id == Console || id == BackupPath;
        bool card = id == WelcomeTitle || id == WelcomeBody || id == WelcomeSteps || id == CpuTitle ||
                    id == CpuValue || id == RamTitle || id == RamValue || id == UptimeTitle ||
                    id == UptimeValue;
        COLORREF color = Foreground;
        if (id == ServerMeta || id == Footer || id == ServerHint || id == PluginNote || id == BackupNote ||
            id == TaskNote || id == SettingsNote)
            color = Muted;
        if (id == Status)
            color = current().process.running() ? Accent : Muted;
        if (id == CpuTitle)
            color = Purple;
        if (id == RamTitle)
            color = Blue;
        if (id == UptimeTitle)
            color = Green;
        SetTextColor(dc, color);
        SetBkColor(dc, field || card ? Panel : Surface);
        return reinterpret_cast<LRESULT>(field || card ? panelBrush : surfaceBrush);
    }
    case WM_NOTIFY: {
        auto *notification = reinterpret_cast<NMHDR *>(l);
        if (notification->code == NM_CUSTOMDRAW) {
            auto h = notification->hwndFrom;
            wchar_t type[24]{};
            GetClassNameW(h, type, 24);
            if (_wcsicmp(type, L"BUTTON") == 0 &&
                (GetWindowLongPtrW(h, GWL_STYLE) & BS_TYPEMASK) == BS_AUTOCHECKBOX) {
                auto *draw = reinterpret_cast<NMCUSTOMDRAW *>(l);
                if (draw->dwDrawStage == CDDS_PREPAINT) {
                    drawCheckbox(h, draw->hdc);
                    return CDRF_SKIPDEFAULT;
                }
                if (draw->dwDrawStage == CDDS_PREERASE)
                    return CDRF_SKIPDEFAULT;
            }
        }
        if (notification->code == LVN_GETEMPTYMARKUP)
            return FALSE;
        if (notification->idFrom == TaskList && notification->code == LVN_ITEMCHANGED && !loading) {
            auto *change = reinterpret_cast<NMLISTVIEW *>(l);
            if ((change->uChanged & LVIF_STATE) && ((change->uNewState ^ change->uOldState) & LVIS_SELECTED))
                loadTaskEditor();
        }
        break;
    }
    case WM_COMMAND: {
        int id = LOWORD(w), code = HIWORD(w);
        if (id == IDCANCEL)
            return 0;
        if (id == ServerSelector && code == LBN_SELCHANGE) {
            auto index = SendMessageW(control(ServerSelector), LB_GETCURSEL, 0, 0);
            if (index >= 0)
                selectServer(static_cast<size_t>(index));
            return 0;
        }
        if (id >= SettingsGeneral && id <= SettingsAppearance && code == BN_CLICKED) {
            settingsPage = id - SettingsGeneral;
            layout();
            updateControls();
            return 0;
        }
        if ((id == Java || id == Jar) && code == EN_KILLFOCUS && !loading) {
            validatePathFields();
            return 0;
        }
        if (id == LanguageChoice && code == CBN_SELCHANGE) {
            return 0;
        }
        if ((id == PortValue || id == MaxPlayersValue) && code == EN_CHANGE && !loading)
            propertiesDirty = true;
        if ((id == ModeValue || id == DifficultyValue) && code == CBN_SELCHANGE && !loading)
            propertiesDirty = true;
        if (id >= OnlineMode && id <= CommandBlocks && code == BN_CLICKED)
            propertiesDirty = true;
        if (id == TaskType && code == CBN_SELCHANGE) {
            taskEditorState();
            return 0;
        }
        if (id == TaskPreset && code == CBN_SELCHANGE) {
            auto index = SendMessageW(control(id), CB_GETCURSEL, 0, 0);
            auto seconds = SendMessageW(control(id), CB_GETITEMDATA, index, 0);
            if (index > 0 && seconds > 0 && seconds <= 864000)
                set(TaskInterval, std::to_wstring(seconds));
            if (index >= 7 && static_cast<size_t>(index - 7) < intervalPresets.size())
                set(PresetName, intervalPresets[index - 7].first);
            enable(PresetDelete, index >= 7);
            return 0;
        }
        if (code != BN_CLICKED)
            break;
        if (id >= DashboardTab && id <= SettingsTab) {
            showPage(id - DashboardTab);
            return 0;
        }
        if (id >= MenuFile && id <= MenuHelp) {
            menu(id);
            return 0;
        }
        switch (id) {
        case WelcomeAction:
            settingsPage = 1;
            showPage(6);
            SetFocus(control(Jar));
            break;
        case RailHome:
            showPage(0);
            break;
        case RailConsole:
            showPage(1);
            break;
        case RailSettings:
            showPage(6);
            break;
        case Advanced:
        case JavaDetect:
        case Installer:
        case RemoteSettings:
        case BackupOptions:
        case BackupRestore:
        case TaskTiming:
        case PlayerManage:
        case PluginManage:
        case GroupActions:
        case ExtraProperties:
            operation(id);
            break;
        case AddServer:
            if (servers.size() < 128)
                addServer();
            else
                error(L"서버 프로필은 최대 128개까지 등록할 수 있습니다.");
            break;
        case RemoveServer:
            removeServer(selected);
            break;
        case SaveSettings:
            if (readFields()) {
                if (propertiesDirty && !saveProperties())
                    break;
                bool nextEnglish = SendMessageW(control(LanguageChoice), CB_GETCURSEL, 0, 0) == 1;
                unsigned nextTheme =
                    static_cast<unsigned>(SendMessageW(control(ThemeChoice), CB_GETCURSEL, 0, 0));
                auto bundled = SendMessageW(control(BundledFontChoice), CB_GETCURSEL, 0, 0);
                auto nextFont = bundled == 0   ? std::wstring(L"NanumGothic")
                                : bundled == 1 ? std::wstring(L"Noto Sans KR")
                                               : draftFontFace;
                unsigned nextPoints = bundled == 2 ? draftFontPoints : fontPoints;
                bool languageChanged = nextEnglish != english, themeChanged = nextTheme != theme,
                     fontChanged = nextFont != fontFace || nextPoints != fontPoints;
                english = nextEnglish;
                theme = nextTheme;
                fontFace = nextFont;
                fontPoints = nextPoints;
                if (fontChanged)
                    applyFont();
                if (themeChanged)
                    applyTheme();
                if (languageChanged)
                    applyLanguage();
                bool saved = saveSettings();
                refreshServerList();
                updateControls();
                RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME);
                if (saved)
                    set(SectionHint, L"설정을 저장했습니다.");
                else
                    error(L"설정 저장에 실패했습니다. 실행 폴더 권한을 확인하세요.");
            }
            break;
        case ChooseImage:
            chooseProfileImage();
            break;
        case ResetImage:
            draftImage.clear();
            if (current().thumbnail)
                DeleteObject(current().thumbnail);
            current().thumbnail = imageBitmap(serverDirectory(current()) / L"server-icon.png");
            set(ProfileImage, L"자동: server-icon.png (없으면 기본 아이콘)");
            InvalidateRect(control(ServerSelector), nullptr, FALSE);
            break;
        case ChooseServerFolder:
            chooseServerFolder();
            break;
        case BrowseJava:
            browseFile(true);
            break;
        case BrowseJar:
            browseFile(false);
            break;
        case Start:
            startServer(current());
            break;
        case Stop:
            cancelAutomaticWork(current());
            stopServer(current());
            break;
        case Restart:
            stopServer(current(), true);
            break;
        case Kill:
            forceServer(current());
            break;
        case Send:
            sendCommand();
            break;
        case Clear:
            current().log.clear();
            current().spans.clear();
            set(Console, L"");
            break;
        case Wrap:
            wrapping = SendMessageW(control(Wrap), BM_GETCHECK, 0, 0) == BST_CHECKED;
            createConsole(true);
            saveSettings();
            break;
        case AutoScroll:
            autoScroll = SendMessageW(control(AutoScroll), BM_GETCHECK, 0, 0) == BST_CHECKED;
            if (autoScroll)
                scrollEnd();
            saveSettings();
            break;
        case FindNext:
            findNext();
            break;
        case ExportLog:
            exportLog();
            break;
        case EulaLink:
            ShellExecuteW(window, L"open", L"https://www.minecraft.net/eula", nullptr, nullptr,
                          SW_SHOWNORMAL);
            break;
        case Folder:
            openFolder(serverDirectory(current()));
            break;
        case PlayerKick:
            playerAction(L"kick");
            break;
        case PlayerBan:
            playerAction(L"ban");
            break;
        case PlayerOp:
            playerAction(L"op");
            break;
        case PlayerDeop:
            playerAction(L"deop");
            break;
        case PluginRefresh:
            refreshPlugins();
            break;
        case PluginImport:
            importPlugin();
            break;
        case PluginToggle:
            togglePlugin();
            break;
        case PluginFolder: {
            auto path = serverDirectory(current());
            if (path.empty())
                error(L"서버 JAR을 먼저 지정하세요.");
            else {
                std::error_code ec;
                fs::create_directories(path / L"plugins", ec);
                openFolder(path / L"plugins");
            }
            break;
        }
        case BackupChoose:
            chooseBackupFolder();
            break;
        case BackupNow:
            current().backup = normalizePathInput(text(control(BackupPath)), settings.parent_path());
            runBackup(selected);
            break;
        case BackupFolder:
            openFolder(current().backup);
            break;
        case PropertiesReload:
            loadProperties();
            break;
        case FontChoose:
            chooseFont();
            break;
        case TaskUpdate:
            addTask(true);
            break;
        case GroupManager:
        case GroupManageSettings:
            openGroupManager();
            break;
        case PresetSave:
            savePreset();
            break;
        case PresetDelete:
            deletePreset();
            break;
        case TaskAdd:
            addTask();
            break;
        case TaskDelete:
        case TaskToggle:
        case TaskRun:
            taskAction(id);
            break;
        case 600:
            SendMessageW(window, WM_CLOSE, 0, 0);
            break;
        case 601:
        case 602: {
            auto dir = serverDirectory(current());
            auto path = dir / (id == 601 ? L"server.properties" : L"logs/latest.log");
            std::error_code ec;
            if (dir.empty() || !fs::is_regular_file(path, ec))
                error(L"파일이 아직 없습니다. 서버를 한 번 실행한 뒤 확인하세요.");
            else {
                auto quoted = L"\"" + path.wstring() + L"\"";
                ShellExecuteW(window, L"open", L"notepad.exe", quoted.c_str(), nullptr, SW_SHOWNORMAL);
            }
            break;
        }
        case 603:
            showPage(2);
            break;
        case 604:
            showPage(4);
            break;
        case 605: {
            auto help =
                tr(L"설정에서 실행 경로를 지정하고 설정 저장을 누른 뒤 시작하세요. 서버 추가 시 EULA를 한 번 "
                   L"확인합니다. 콘솔에서 명령·검색·컬러 로그를 사용할 수 있습니다. 스케줄러는 앱이 실행 "
                   L"중일 때만 동작합니다. 실행 중인 서버를 닫을 때는 월드 저장과 정상 종료를 기다립니다.");
            message(help, L"사용 방법", MB_OK | MB_ICONINFORMATION);
            break;
        }
        }
        return 0;
    }
    case WM_CLOSE: {
        if (backupPending && !backupJob.busy() && servers[backupOwner]->backupAfterStop) {
            servers[backupOwner]->backupAfterStop = servers[backupOwner]->resumeAfterBackup = false;
            backupPending = false;
        }
        bool running = backupPending || installerActive;
        for (const auto &server : servers)
            running |= server->process.running();
        if (running) {
            if (!closing) {
                closing = true;
                closeTime = GetTickCount64();
                if (installerActive)
                    installerProcess.stop();
                for (auto &server : servers) {
                    server->recoverAt = 0;
                    server->backupAfterStop = server->resumeAfterBackup = false;
                    server->restart = false;
                    if (server->process.running())
                        stopServer(*server);
                }
            } else if (GetTickCount64() - closeTime >= 5000 &&
                       confirm(L"아직 실행 중인 서버를 모두 강제 종료할까요? 저장하지 않은 월드 데이터가 "
                               L"손실될 수 있습니다.")) {
                if (installerActive)
                    installerProcess.kill();
                for (auto &server : servers)
                    if (server->process.running())
                        server->process.kill();
            }
            updateControls();
            return 0;
        }
        saveSettings();
        DestroyWindow(window);
        return 0;
    }
    case WM_DESTROY:
        if (groupWindow)
            DestroyWindow(groupWindow);
        KillTimer(window, 1);
        remote.stop();
        faces.shutdown();
        DeleteObject(font);
        DeleteObject(mono);
        DeleteObject(large);
        DeleteObject(heading);
        DeleteObject(small);
        DeleteObject(bgBrush);
        DeleteObject(surfaceBrush);
        DeleteObject(panelBrush);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, msg, w, l);
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    auto app = reinterpret_cast<App *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        app = static_cast<App *>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
        app->attach(hwnd);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (msg == WM_CREATE) {
        app->create(hwnd);
        return 0;
    }
    if (msg == WM_GETMINMAXINFO) {
        reinterpret_cast<MINMAXINFO *>(l)->ptMinTrackSize = {1320, 860};
        return 0;
    }
    if (app && msg != WM_NCCREATE)
        return app->dispatch(msg, w, l);
    return DefWindowProcW(hwnd, msg, w, l);
}

int runApplication(HINSTANCE instance, int show) {
    SetProcessDPIAware();
    HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX common{sizeof(common), ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&common);
    WNDCLASSEXW cls{};
    cls.cbSize = sizeof(cls);
    cls.lpfnWndProc = windowProc;
    cls.hInstance = instance;
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    cls.hIcon =
        static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
                                      GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED));
    cls.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
                                                GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
                                                LR_SHARED));
    cls.hbrBackground = static_cast<HBRUSH>(GetStockObject(DKGRAY_BRUSH));
    cls.lpszClassName = L"MinecraftServerWrapperWindow";
    if (!RegisterClassExW(&cls)) {
        if (SUCCEEDED(com))
            CoUninitialize();
        return 1;
    }
    struct Imaging {
        ULONG_PTR token = 0;
        Imaging() {
            Gdiplus::GdiplusStartupInput input;
            Gdiplus::GdiplusStartup(&token, &input, nullptr);
        }
        ~Imaging() {
            if (token)
                Gdiplus::GdiplusShutdown(token);
        }
    } imaging;
    HMODULE richEdit = LoadLibraryW(L"Msftedit.dll");
    if (!richEdit) {
        if (SUCCEEDED(com))
            CoUninitialize();
        return 1;
    }
    struct PrivateFonts {
        std::vector<fs::path> files;
        PrivateFonts() {
            wchar_t module[32768]{};
            GetModuleFileNameW(nullptr, module, 32768);
            auto directory = fs::path(module).parent_path() / L"assets" / L"fonts";
            for (const auto *name : {L"NanumGothic-Regular.ttf", L"NotoSansKR-Variable.ttf"}) {
                auto path = directory / name;
                if (AddFontResourceExW(path.c_str(), FR_PRIVATE, nullptr))
                    files.push_back(path);
            }
        }
        ~PrivateFonts() {
            for (const auto &path : files)
                RemoveFontResourceExW(path.c_str(), FR_PRIVATE, nullptr);
        }
    } privateFonts;
    App app;
    HWND window = CreateWindowExW(0, cls.lpszClassName, L"Minecraft Server Manager",
                                  WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 1440,
                                  900, nullptr, nullptr, instance, &app);
    if (!window) {
        if (SUCCEEDED(com))
            CoUninitialize();
        return 1;
    }
    ShowWindow(window, show);
    MSG msg{};
    int result;
    while ((result = GetMessageW(&msg, nullptr, 0, 0)) > 0) {
        if (!app.keyboard(msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (SUCCEEDED(com))
        CoUninitialize();
    return result < 0 ? 1 : static_cast<int>(msg.wParam);
}

} // namespace wrapper

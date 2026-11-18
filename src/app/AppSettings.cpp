#include "App.h"
#include <set>

namespace wrapper {

std::wstring App::ini(const std::wstring &section, const std::wstring &key,
                      const std::wstring &fallback) const {
    std::vector<wchar_t> value(32768);
    GetPrivateProfileStringW(section.c_str(), key.c_str(), fallback.c_str(), value.data(),
                             static_cast<DWORD>(value.size()), settings.c_str());
    return value.data();
}

unsigned App::iniNumber(const std::wstring &section, const std::wstring &key, unsigned fallback,
                        unsigned max) const {
    auto value = ini(section, key, std::to_wstring(fallback));
    if (value.empty() || value.size() > 9 || value.find_first_not_of(L"0123456789") != std::wstring::npos)
        return fallback;
    auto parsed = std::stoul(value);
    return parsed <= max ? static_cast<unsigned>(parsed) : fallback;
}

void App::loadSettings() {
    std::vector<wchar_t> executable(32768);
    GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    settings = fs::path(executable.data()).parent_path() / L"wrapper.ini";
    unsigned groupCount = iniNumber(L"Workspace", L"GroupCount", 0, 256);
    for (unsigned i = 0; i < groupCount; ++i)
        groupRegistry.add(ini(L"Workspace", L"Group" + std::to_wstring(i)));
    unsigned count = iniNumber(L"Workspace", L"Count", 0, 128);
    if (!count) {
        auto server = std::make_unique<Server>();
        server->java = ini(L"Server", L"100", L"java.exe");
        server->jar = ini(L"Server", L"102");
        server->memory = std::max(512u, iniNumber(L"Server", L"104", 2048, 1048576));
        servers.push_back(std::move(server));
    } else {
        for (unsigned i = 0; i < count; ++i) {
            auto server = std::make_unique<Server>();
            auto section = L"Profile" + std::to_wstring(i);
            server->group = ini(section, L"Group", L"기본 그룹");
            server->name = ini(section, L"Name", L"Minecraft Server " + std::to_wstring(i + 1));
            server->java = ini(section, L"Java", L"java.exe");
            server->jar = ini(section, L"Jar");
            server->memory = std::max(512u, iniNumber(section, L"Memory", 2048, 1048576));
            server->eulaAccepted = iniNumber(section, L"EulaAccepted", 0, 1) != 0;
            server->backup = ini(section, L"Backup");
            server->id = ini(section, L"Id", newToken());
            server->launch.mode = iniNumber(section, L"LaunchMode", 0, 3);
            server->launch.minimumMemory = std::max(1u, iniNumber(section, L"MinimumMemory", 512, 1048576));
            server->launch.jvm = ini(section, L"JvmOptions");
            server->launch.arguments = ini(section, L"Arguments");
            server->launch.target = ini(section, L"LaunchTarget");
            server->launch.directory = ini(section, L"WorkingDirectory");
            server->autoStart = iniNumber(section, L"AutoStart", 0, 1);
            server->autoRecover = iniNumber(section, L"AutoRecover", 0, 1);
            server->recoveryDelay = std::max(1u, iniNumber(section, L"RecoveryDelay", 15, 3600));
            server->recoveryLimit = std::max(1u, iniNumber(section, L"RecoveryLimit", 5, 100));
            server->zipBackup = iniNumber(section, L"ZipBackup", 1, 1);
            server->backupKeep = iniNumber(section, L"BackupKeep", 10, 1000);
            server->image = ini(section, L"Image");
            unsigned tasks = iniNumber(section, L"Tasks", 0, 256);
            for (unsigned t = 0; t < tasks; ++t) {
                auto prefix = L"Task" + std::to_wstring(t);
                Task task;
                task.type = static_cast<int>(iniNumber(section, prefix + L"Type", 0, 2));
                task.seconds = std::max(1u, iniNumber(section, prefix + L"Seconds", 300, 864000));
                task.command = ini(section, prefix + L"Command");
                task.enabled = iniNumber(section, prefix + L"Enabled", 1, 1) != 0;
                auto timing = iniNumber(section, prefix + L"Minute", 1440, 1440);
                task.minuteOfDay = timing == 1440 ? -1 : static_cast<int>(timing);
                task.nextWall = nextDaily(task.minuteOfDay);
                task.lastResult = ini(section, prefix + L"Result", L"대기");
                task.lastRun = ini(section, prefix + L"LastRun");
                task.due = GetTickCount64() + task.seconds * 1000ULL;
                server->tasks.push_back(std::move(task));
            }
            servers.push_back(std::move(server));
        }
    }
    remoteEnabled = iniNumber(L"Remote", L"Enabled", 0, 1);
    remotePort = std::max(1024u, iniNumber(L"Remote", L"Port", 25560, 65535));
    remoteUsers = ini(L"Remote", L"Users");
    unsigned presetCount = iniNumber(L"Workspace", L"PresetCount", 0, 32);
    for (unsigned i = 0; i < presetCount; ++i) {
        auto prefix = L"Preset" + std::to_wstring(i);
        auto name = ini(L"Workspace", prefix + L"Name");
        auto seconds = iniNumber(L"Workspace", prefix + L"Seconds", 0, 864000);
        if (!name.empty() && name.size() <= 48 && seconds)
            intervalPresets.emplace_back(name, seconds);
    }
    selected = std::min<size_t>(iniNumber(L"Workspace", L"Selected", 0, 128), servers.size() - 1);
    english = ini(L"Workspace", L"Language", L"ko") == L"en";
    theme = iniNumber(L"Workspace", L"Theme", 0, 2);
    fontFace = ini(L"Workspace", L"Font", L"NanumGothic");
    draftFontFace = fontFace;
    fontPoints = std::clamp(iniNumber(L"Workspace", L"FontSize", 11, 16), 9u, 16u);
    draftFontPoints = fontPoints;
    wrapping = iniNumber(L"Workspace", L"Wrap", 1, 1) != 0;
    autoScroll = iniNumber(L"Workspace", L"AutoScroll", 1, 1) != 0;
    std::set<std::wstring> profileIds;
    for (auto &server : servers) {
        if (server->id.size() != 64 ||
            server->id.find_first_not_of(L"0123456789abcdef") != std::wstring::npos ||
            profileIds.count(server->id))
            server->id = newToken();
        profileIds.insert(server->id);
        server->launch.directory = normalizePathInput(server->launch.directory, settings.parent_path());
        server->launch.target = normalizePathInput(server->launch.target, settings.parent_path());
        server->java = normalizePathInput(server->java, settings.parent_path(), true);
        server->jar = normalizePathInput(server->jar, settings.parent_path());
        server->backup = normalizePathInput(server->backup, settings.parent_path());
        auto kind = JarArchive(server->jar).serverType();
        server->kind =
            server->launch.mode == 3 ? L"Bedrock / Native" : std::wstring(kind.begin(), kind.end());
        auto name = GroupRegistry::normalize(server->group);
        if (name.empty())
            name = L"기본 그룹";
        groupRegistry.add(name);
        server->group = groupRegistry.canonical(name);
        if (server->group.empty())
            server->group = L"기본 그룹";
        refreshServerDirectory(*server);
        loadThumbnail(*server);
    }
}

bool App::saveSettings() {
    SettingsSnapshot snapshot(settings);
    auto write = [&](const std::wstring &section, const std::wstring &key, const std::wstring &value) {
        snapshot.write(section, key, value);
    };
    write(L"Remote", L"Enabled", remoteEnabled ? L"1" : L"0");
    write(L"Remote", L"Port", std::to_wstring(remotePort));
    write(L"Remote", L"Users", remoteUsers);
    write(L"Workspace", L"GroupCount", std::to_wstring(groupRegistry.names().size()));
    for (size_t i = 0; i < groupRegistry.names().size(); ++i)
        write(L"Workspace", L"Group" + std::to_wstring(i), groupRegistry.names()[i]);
    write(L"Workspace", L"PresetCount", std::to_wstring(intervalPresets.size()));
    for (size_t i = 0; i < intervalPresets.size(); ++i) {
        auto prefix = L"Preset" + std::to_wstring(i);
        write(L"Workspace", prefix + L"Name", intervalPresets[i].first);
        write(L"Workspace", prefix + L"Seconds", std::to_wstring(intervalPresets[i].second));
    }
    write(L"Workspace", L"Theme", std::to_wstring(theme));
    write(L"Workspace", L"Language", english ? L"en" : L"ko");
    write(L"Workspace", L"Font", fontFace);
    write(L"Workspace", L"FontSize", std::to_wstring(fontPoints));
    write(L"Workspace", L"Count", std::to_wstring(servers.size()));
    write(L"Workspace", L"Selected", std::to_wstring(selected));
    write(L"Workspace", L"Wrap", wrapping ? L"1" : L"0");
    write(L"Workspace", L"AutoScroll", autoScroll ? L"1" : L"0");
    for (size_t i = 0; i < servers.size(); ++i) {
        auto &server = *servers[i];
        auto section = L"Profile" + std::to_wstring(i);
        write(section, L"Id", server.id);
        write(section, L"LaunchMode", std::to_wstring(server.launch.mode));
        write(section, L"MinimumMemory", std::to_wstring(server.launch.minimumMemory));
        write(section, L"JvmOptions", server.launch.jvm);
        write(section, L"Arguments", server.launch.arguments);
        write(section, L"LaunchTarget", server.launch.target);
        write(section, L"WorkingDirectory", server.launch.directory);
        write(section, L"AutoStart", server.autoStart ? L"1" : L"0");
        write(section, L"AutoRecover", server.autoRecover ? L"1" : L"0");
        write(section, L"RecoveryDelay", std::to_wstring(server.recoveryDelay));
        write(section, L"RecoveryLimit", std::to_wstring(server.recoveryLimit));
        write(section, L"ZipBackup", server.zipBackup ? L"1" : L"0");
        write(section, L"BackupKeep", std::to_wstring(server.backupKeep));
        write(section, L"Group", server.group);
        write(section, L"Name", server.name);
        write(section, L"Java", server.java);
        write(section, L"Jar", server.jar);
        write(section, L"Memory", std::to_wstring(server.memory));
        write(section, L"Backup", server.backup);
        write(section, L"EulaAccepted", server.eulaAccepted ? L"1" : L"0");
        write(section, L"Image", server.image);
        write(section, L"Tasks", std::to_wstring(server.tasks.size()));
        for (size_t t = 0; t < server.tasks.size(); ++t) {
            auto &task = server.tasks[t];
            auto prefix = L"Task" + std::to_wstring(t);
            write(section, prefix + L"Minute",
                  std::to_wstring(task.minuteOfDay < 0 ? 1440 : task.minuteOfDay));
            write(section, prefix + L"Result", task.lastResult);
            write(section, prefix + L"LastRun", task.lastRun);
            write(section, prefix + L"Type", std::to_wstring(task.type));
            write(section, prefix + L"Seconds", std::to_wstring(task.seconds));
            write(section, prefix + L"Command", task.command);
            write(section, prefix + L"Enabled", task.enabled ? L"1" : L"0");
        }
    }
    bool saved = snapshot.commit();
    // Retry on the next timer tick; report a failure streak only once.
    settingsDirty = !saved;
    if (!saved && !saveFailureReported)
        append(
            current(),
            L"[Wrapper] 설정 저장 실패: 이전 설정을 유지했습니다. 실행 폴더의 쓰기 권한을 확인하세요.\r\n");
    saveFailureReported = !saved;
    return saved;
}

bool App::readFields(bool validate) {
    if (loading || servers.empty())
        return true;
    auto ram = text(control(Memory));
    if (ram.empty() || ram.size() > 7 || ram.find_first_not_of(L"0123456789") != std::wstring::npos ||
        std::stoul(ram) < 512 || std::stoul(ram) > 1048576) {
        if (validate) {
            showPage(6);
            error(L"메모리는 512~1048576 사이의 정수(MB)로 입력하세요.");
        }
        return false;
    }
    auto java = normalizePathInput(text(control(Java)), settings.parent_path(), true);
    auto jar = normalizePathInput(text(control(Jar)), settings.parent_path());
    std::wstring resolved, validationError;
    if (!jar.empty() && ((current().launch.mode < 2 && !validateJavaPath(java, resolved, validationError)) ||
                         (current().launch.mode == 0 && !validateServerJar(jar, validationError)))) {
        if (validate) {
            settingsPage = 1;
            showPage(6);
            set(PathValidity, validationError);
        }
        return false;
    }
    if (current().launch.mode < 2 && jar.empty() && !java.empty() && java != L"java.exe" &&
        !validateJavaPath(java, resolved, validationError)) {
        if (validate) {
            settingsPage = 1;
            showPage(6);
            set(PathValidity, validationError);
        }
        return false;
    }
    if (!jar.empty())
        set(PathValidity, L"Java 실행 파일과 서버 JAR을 확인했습니다.");
    auto &server = current();
    auto name = text(control(ServerName));
    server.name = name.empty() ? L"Minecraft Server" : name;
    auto previousJar = server.jar;
    server.java = normalizePathInput(text(control(Java)), settings.parent_path(), true);
    server.jar = normalizePathInput(text(control(Jar)), settings.parent_path());
    server.memory = static_cast<unsigned>(std::stoul(ram));
    server.backup = normalizePathInput(text(control(BackupPath)), settings.parent_path());
    server.image = draftImage;
    set(Java, server.java);
    set(Jar, server.jar);
    set(BackupPath, server.backup);
    if (previousJar != server.jar) {
        refreshServerDirectory(server);
        auto kind = JarArchive(server.jar).serverType();
        server.kind = server.launch.mode == 3 ? L"Bedrock / Native" : std::wstring(kind.begin(), kind.end());
        loadThumbnail(server);
        if (!propertiesDirty)
            loadProperties();
    }
    loadThumbnail(server);
    if (page == 0)
        layout();
    return true;
}

void App::loadFields() {
    loading = true;
    auto &server = current();
    draftImage = server.image;
    set(GroupName, server.group == L"기본 그룹" ? tr(server.group) : server.group);
    set(ServerName, server.name);
    set(Java, server.java);
    set(Jar, server.jar);
    set(Memory, std::to_wstring(server.memory));
    set(BackupPath, server.backup);
    set(ProfileImage, server.image.empty() ? tr(L"자동: server-icon.png (없으면 기본 아이콘)") : server.image);
    SendMessageW(control(Eula), BM_SETCHECK, BST_UNCHECKED, 0);
    loading = false;
    loadProperties();
    set(PathValidity, L"");
}

void App::applyFont() {
    int dpi = UiDpi;
    auto make = [&](int points, int weight) {
        return CreateFontW(-MulDiv(points, dpi, 72), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
                           fontFace.c_str());
    };
    HFONT next = make(fontPoints, FW_NORMAL), nextSmall = make(std::max(9u, fontPoints - 2), FW_NORMAL),
          nextLarge = make(fontPoints + 9, FW_SEMIBOLD), nextHeading = make(fontPoints + 4, FW_SEMIBOLD);
    if (!next || !nextSmall || !nextLarge || !nextHeading) {
        DeleteObject(next);
        DeleteObject(nextSmall);
        DeleteObject(nextLarge);
        DeleteObject(nextHeading);
        return;
    }
    HFONT old = font, oldSmall = small, oldLarge = large, oldHeading = heading;
    font = next;
    small = nextSmall;
    large = nextLarge;
    heading = nextHeading;
    for (HWND child = GetWindow(window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        if (GetDlgCtrlID(child) == PluginList || GetDlgCtrlID(child) == BackupList ||
            GetDlgCtrlID(child) == TaskList)
            SendMessageW(ListView_GetHeader(child), WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
    if (groupWindow)
        for (HWND child = GetWindow(groupWindow, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
            SendMessageW(child, WM_SETFONT,
                         reinterpret_cast<WPARAM>(GetDlgCtrlID(child) == ManagerHeading ? heading : font),
                         TRUE);
    for (int id : {ServerTitle, CpuValue, RamValue, UptimeValue})
        styleFont(id, large);
    for (int id : {SettingsHeading, PluginHeading, BackupHeading, TaskHeading})
        styleFont(id, heading);
    for (int id : {ServerMeta, Footer, ServerHint, PathNote, FontInfo, RailHome, RailConsole, RailSettings})
        styleFont(id, small);
    // Keep log columns aligned while the rest of the application uses the chosen family.
    styleFont(Console, mono);
    DeleteObject(old);
    DeleteObject(oldSmall);
    DeleteObject(oldLarge);
    DeleteObject(oldHeading);
    set(FontInfo,
        fontFace + L" · " + std::to_wstring(fontPoints) + L" pt · " +
            tr(fontFace == L"NanumGothic" || fontFace == L"Noto Sans KR" ? L"기본 제공" : L"시스템 글꼴"));
    InvalidateRect(window, nullptr, TRUE);
}

void App::chooseFont() {
    LOGFONTW value{};
    GetObjectW(font, sizeof(value), &value);
    CHOOSEFONTW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpLogFont = &value;
    dialog.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_LIMITSIZE | CF_NOVERTFONTS;
    dialog.nSizeMin = 9;
    dialog.nSizeMax = 16;
    if (ChooseFontW(&dialog)) {
        draftFontFace = value.lfFaceName;
        draftFontPoints = static_cast<unsigned>(dialog.iPointSize / 10);
        SendMessageW(control(BundledFontChoice), CB_SETCURSEL, 2, 0);
        set(FontInfo, draftFontFace + L" · " + std::to_wstring(draftFontPoints) + L" pt");
    }
}

} // namespace wrapper

namespace wrapper {
void App::validatePathFields() {
    auto java = normalizePathInput(text(control(Java)), settings.parent_path(), true),
         jar = normalizePathInput(text(control(Jar)), settings.parent_path());
    std::wstring resolved, errorMessage;
    if (current().launch.mode != 0) {
        set(PathValidity, L"고급 실행 프로필을 사용합니다. 도구 → 고급 실행에서 설정하세요.");
        return;
    }
    if (jar.empty()) {
        set(PathValidity, L"서버 JAR 경로를 입력하세요.");
        return;
    }
    if (!validateJavaPath(java, resolved, errorMessage) || !validateServerJar(jar, errorMessage))
        set(PathValidity, errorMessage);
    else
        set(PathValidity, L"Java 실행 파일과 서버 JAR을 확인했습니다.");
}
} // namespace wrapper

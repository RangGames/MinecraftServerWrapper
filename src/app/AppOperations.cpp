#include "App.h"
#include <set>
namespace wrapper {
namespace {
bool number(const std::wstring &v, unsigned lo, unsigned hi) {
    return !v.empty() && v.size() < 8 && v.find_first_not_of(L"0123456789") == std::wstring::npos &&
           std::stoul(v) >= lo && std::stoul(v) <= hi;
}
std::wstring wide(const std::string &s) {
    int n =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (!n && !s.empty())
        throw std::invalid_argument("invalid UTF-8");
    std::wstring v(n, 0);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), v.data(), n);
    return v;
}
std::vector<std::string> split(const std::string &s, char separator) {
    std::vector<std::string> r;
    std::stringstream in(s);
    std::string v;
    while (std::getline(in, v, separator))
        r.push_back(v);
    if (!s.empty() && s.back() == separator)
        r.emplace_back();
    return r;
}
std::wstring chooseFile(HWND owner, const wchar_t *filter) {
    std::vector<wchar_t> path(32768);
    OPENFILENAMEW d{};
    d.lStructSize = sizeof(d);
    d.hwndOwner = owner;
    d.lpstrFile = path.data();
    d.nMaxFile = static_cast<DWORD>(path.size());
    d.lpstrFilter = filter;
    d.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameW(&d) ? std::wstring(path.data()) : L"";
}
std::vector<std::wstring> installedJava() {
    std::set<std::wstring> found;
    std::vector<fs::path> roots;
    auto env = [&](const wchar_t *key) {
        wchar_t value[32768]{};
        GetEnvironmentVariableW(key, value, 32768);
        return std::wstring(value);
    };
    auto home = env(L"JAVA_HOME");
    std::error_code homeError;
    if (!home.empty() && fs::is_regular_file(fs::path(home) / L"bin/java.exe", homeError))
        found.insert((fs::path(home) / L"bin/java.exe").wstring());
    std::wstringstream paths(env(L"PATH"));
    std::wstring entry;
    while (std::getline(paths, entry, L';'))
        if (!entry.empty()) {
            std::error_code ec;
            auto java = fs::path(entry) / L"java.exe";
            if (fs::is_regular_file(java, ec))
                found.insert(java.wstring());
        }
    for (auto base : {env(L"ProgramFiles"), env(L"ProgramFiles(x86)"), env(L"LOCALAPPDATA")})
        if (!base.empty())
            for (auto folder : {L"Java", L"Eclipse Adoptium", L"Eclipse Foundation", L"Microsoft", L"Zulu",
                                L"BellSoft", L"Programs/Eclipse Adoptium"})
                roots.push_back(fs::path(base) / folder);
    for (const auto &root : roots) {
        std::error_code ec;
        fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
        while (!ec && it != end) {
            if (it.depth() > 3)
                it.disable_recursion_pending();
            if (it->path().filename() == L"java.exe" && it->is_regular_file(ec))
                found.insert(it->path().wstring());
            it.increment(ec);
        }
    }
    return {found.begin(), found.end()};
}
} // namespace
void App::startConfiguredServices() {
    if (remoteEnabled) {
        std::wstring err;
        try {
            if (!remote.start(remotePort, parseRemoteUsers(remoteUsers), err))
                append(current(), L"[Remote] " + err + L"\r\n");
        } catch (...) {
            append(current(), L"[Remote] 계정 설정 오류\r\n");
        }
    }
    for (auto &s : servers)
        if (s->autoStart) {
            if (s->eulaAccepted)
                startServer(*s, false);
            else
                append(*s, L"[Autostart] EULA 동의가 필요합니다. 수동으로 시작하세요.\r\n");
        }
}
void App::operation(int id) {
    auto &s = current();
    auto form = [&](const std::wstring &title, const std::wstring &help, std::vector<FormField> &fields,
                    auto validate) {
        return editForm(window, font, title, help, fields, validate,
                        {Surface, Panel, Border, Foreground, Muted, Accent, lightTheme});
    };
    auto accept = [](const std::vector<FormField> &) -> std::wstring { return {}; };
    if (closing || operationsBusy)
        return;
    struct BusyGuard {
        bool &value;
        BusyGuard(bool &v) : value(v) { value = true; }
        ~BusyGuard() { value = false; }
    } guard(operationsBusy);
    if (id == Advanced) {
        if (s.process.running() || overlapsBackup(s)) {
            error(L"서버와 백업이 중지된 상태에서 변경하세요.");
            return;
        }
        const std::vector<std::wstring> modes{L"JAR", L"Java 인자 / @args", L"BAT", L"일반 EXE (Bedrock 등)"},
            yesno{L"아니요", L"예"};
        std::vector<FormField> f{{L"실행 모드", modes[s.launch.mode], modes},
                                 {L"작업 폴더 (빈 값: JAR 폴더)", s.launch.directory},
                                 {L"BAT / EXE 경로", s.launch.target},
                                 {L"최소 메모리 MB", std::to_wstring(s.launch.minimumMemory)},
                                 {L"추가 JVM 옵션", s.launch.jvm},
                                 {L"서버 인자 / @args", s.launch.arguments},
                                 {L"앱 시작 시 자동 실행", yesno[s.autoStart], yesno},
                                 {L"장애 시 자동 복구", yesno[s.autoRecover], yesno},
                                 {L"재시작 대기 초", std::to_wstring(s.recoveryDelay)},
                                 {L"연속 재시도 한도", std::to_wstring(s.recoveryLimit)}};
        if (form(L"고급 실행 · 자동 복구",
                 L"Java 인자 모드는 @user_jvm_args.txt @libraries/.../win_args.txt 등을 입력합니다.\nBAT에는 "
                 L"pause를 제거하세요. BAT 인자는 스크립트 내부에 설정합니다.\n복구 실패 횟수는 5분 이상 "
                 L"정상 실행 후 또는 수동 시작 시 초기화합니다.",
                 f, [&](const auto &v) -> std::wstring {
                     if (!number(v[3].value, 1, s.memory) || !number(v[8].value, 1, 3600) ||
                         !number(v[9].value, 1, 100))
                         return L"메모리·대기 시간·재시도 한도를 확인하세요.";
                     auto mode =
                         static_cast<int>(std::find(modes.begin(), modes.end(), v[0].value) - modes.begin());
                     auto dir = normalizePathInput(v[1].value, settings.parent_path());
                     auto target = normalizePathInput(v[2].value, settings.parent_path());
                     std::error_code ec;
                     if (!dir.empty() && !fs::is_directory(dir, ec))
                         return L"작업 폴더가 존재해야 합니다.";
                     if (mode >= 2 && (!fs::is_regular_file(target, ec) ||
                                       (mode == 2 && fs::path(target).extension() != L".bat" &&
                                        fs::path(target).extension() != L".cmd") ||
                                       (mode == 3 && fs::path(target).extension() != L".exe")))
                         return L"실행 모드에 맞는 BAT/CMD 또는 EXE 파일을 지정하세요.";
                     LaunchOptions o{mode,       static_cast<unsigned>(std::stoul(v[3].value)),
                                     v[4].value, v[5].value,
                                     target,     dir};
                     try {
                         buildLaunchCommand(s.java, s.jar, s.memory, true, o);
                     } catch (...) {
                         return L"실행 옵션을 확인하세요. BAT 경로의 제어 문자와 BAT 추가 인자는 지원하지 "
                                L"않습니다.";
                     }
                     if (mode != 0 && dir.empty())
                         return L"JAR 이외 모드는 작업 폴더를 지정하세요.";
                     return {};
                 })) {
            s.launch.mode =
                static_cast<int>(std::find(modes.begin(), modes.end(), f[0].value) - modes.begin());
            s.launch.directory = normalizePathInput(f[1].value, settings.parent_path());
            s.launch.target = normalizePathInput(f[2].value, settings.parent_path());
            s.launch.minimumMemory = std::stoul(f[3].value);
            s.launch.jvm = f[4].value;
            s.launch.arguments = f[5].value;
            s.autoStart = f[6].value == L"예";
            s.autoRecover = f[7].value == L"예";
            s.recoveryDelay = std::stoul(f[8].value);
            s.recoveryLimit = std::stoul(f[9].value);
            s.recoveryAttempts = 0;
            s.recoverAt = 0;
            if (s.launch.mode == 3)
                s.kind = L"Bedrock / Native";
            refreshServerDirectory(s);
            saveSettings();
            updateControls();
        }
    } else if (id == JavaDetect) {
        if (s.process.running() || overlapsBackup(s)) {
            error(L"서버를 중지한 뒤 변경하세요.");
            return;
        }
        auto paths = installedJava();
        if (paths.empty()) {
            error(L"Java를 찾지 못했습니다. Java를 설치하거나 직접 경로를 선택하세요.");
            return;
        }
        std::vector<FormField> f{{L"Java 실행 파일", paths.front(), paths}};
        if (form(L"설치된 Java",
                 L"JAVA_HOME, PATH와 주요 Java 설치 폴더를 검색했습니다.\n서버 버전에 맞는 Java를 "
                 L"선택하세요. 선택 후 설정 저장을 누르세요.",
                 f, accept))
            set(Java, f[0].value);
    } else if (id == Installer) {
        if (installerActive) {
            error(L"설치가 진행 중입니다.");
            return;
        }
        std::vector<FormField> choice{
            {L"서버 종류", L"Fabric", {L"Fabric", L"Forge", L"Paper", L"Purpur", L"Vanilla", L"Bedrock"}},
            {L"작업", L"공식 다운로드 페이지", {L"공식 다운로드 페이지", L"다운로드한 설치 JAR 실행"}}};
        if (!form(L"서버 설치 도우미",
                  L"공식 사이트에서 원하는 버전을 다운로드하세요.\nFabric / Forge 설치 JAR은 이 도우미로 빈 "
                  L"폴더에 설치할 수 있습니다.\n다른 배포판은 다운로드 후 JAR 또는 EXE 실행 프로필을 "
                  L"설정하세요.",
                  choice, accept))
            return;
        if (choice[1].value == L"공식 다운로드 페이지") {
            std::map<std::wstring, std::wstring> urls{
                {L"Fabric", L"https://fabricmc.net/use/server/"},
                {L"Forge", L"https://files.minecraftforge.net/"},
                {L"Paper", L"https://papermc.io/downloads/paper"},
                {L"Purpur", L"https://purpurmc.org/downloads"},
                {L"Vanilla", L"https://www.minecraft.net/en-us/download/server"},
                {L"Bedrock", L"https://www.minecraft.net/en-us/download/server/bedrock"}};
            ShellExecuteW(window, L"open", urls[choice[0].value].c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            return;
        }
        if (choice[0].value != L"Fabric" && choice[0].value != L"Forge") {
            error(L"통합 설치는 Fabric / Forge 설치 JAR을 지원합니다.");
            return;
        }
        auto jar = chooseFile(window, L"설치 JAR\0*.jar\0\0");
        if (jar.empty())
            return;
        std::vector<FormField> f{{L"Java 실행 파일", s.java},
                                 {L"새 설치 폴더 (빈 폴더)", L""},
                                 {L"Minecraft 버전 (Fabric 필수)", L""}};
        if (!form(L"서버 설치",
                  L"설치 JAR을 실행하고 진행 로그를 콘솔에 표시합니다.\n기존 서버를 덮어쓰지 않도록 비어 "
                  L"있는 폴더만 허용합니다.\n설치 후 생성된 서버 JAR / BAT를 실행 프로필에 지정하세요.",
                  f, [&](const auto &v) -> std::wstring {
                      std::wstring resolved, err;
                      if (!validateJavaPath(v[0].value, resolved, err))
                          return err;
                      if (v[1].value.empty())
                          return L"새 폴더를 입력하세요.";
                      auto dir = fs::path(normalizePathInput(v[1].value, settings.parent_path()));
                      std::error_code ec;
                      if (fs::exists(dir, ec) && (!fs::is_directory(dir, ec) || !fs::is_empty(dir, ec)))
                          return L"비어 있는 폴더를 선택하세요.";
                      if (ec)
                          return L"설치 폴더를 확인하세요.";
                      auto canonical = fs::weakly_canonical(dir, ec);
                      if (ec)
                          return L"설치 경로 오류";
                      for (const auto &server : servers)
                          if (resolvedPathsOverlap(canonical, server->canonicalDirectory))
                              return L"등록된 서버 폴더와 겹칩니다.";
                      if (backupPending && resolvedPathsOverlap(canonical, backupSource))
                          return L"백업 폴더와 겹칩니다.";
                      if (choice[0].value == L"Fabric" &&
                          (v[2].value.empty() || v[2].value.size() > 64 ||
                           v[2].value.find_first_not_of(
                               L"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-_") !=
                               std::wstring::npos))
                          return L"Fabric용 Minecraft 버전을 입력하세요.";
                      return {};
                  }))
            return;
        installerTarget = normalizePathInput(f[1].value, settings.parent_path());
        std::error_code ec;
        fs::create_directories(installerTarget, ec);
        if (ec) {
            error(L"설치 폴더 생성 실패");
            return;
        }
        LaunchOptions o;
        o.mode = 1;
        o.directory = installerTarget;
        o.arguments =
            L"-jar " + quoteArgument(jar) +
            (choice[0].value == L"Forge" ? L" --installServer"
                                         : L" server -dir " + quoteArgument(installerTarget) +
                                               L" -mcversion " + f[2].value + L" -downloadMinecraft");
        std::wstring err, resolved;
        validateJavaPath(f[0].value, resolved, err);
        installerActive = installerProcess.start(resolved, jar, 2048, err, false, o);
        if (!installerActive)
            error(err);
        else {
            showPage(1);
            append(s, L"[Installer] 설치 시작: " + installerTarget + L"\r\n");
        }
    } else if (id == RemoteSettings) {
        if (remoteUsers.empty())
            remoteUsers = L"admin:admin:" + newToken();
        std::vector<FormField> f{{L"웹 패널 사용", remoteEnabled ? L"예" : L"아니요", {L"아니요", L"예"}},
                                 {L"포트", std::to_wstring(remotePort)},
                                 {L"계정 (세미콜론으로 구분)", remoteUsers},
                                 {L"새 계정 이름 (선택)", L""},
                                 {L"새 계정 권한", L"viewer", {L"viewer", L"operator", L"admin"}}};
        if (!form(L"원격 웹 패널 · 계정",
                  L"형식: 이름:권한:64자리키;이름:권한:64자리키 (빈 값은 계정 삭제).\n새 계정 이름을 "
                  L"입력하면 임의 API 키를 생성합니다. 기존 이름이면 키를 교체합니다.\n127.0.0.1만 "
                  L"수신합니다. 외부 접속에는 SSH 터널 또는 HTTPS 프록시를 사용하세요.",
                  f, [&](const auto &v) -> std::wstring {
                      if (!number(v[1].value, 1024, 65535))
                          return L"포트 범위: 1024~65535";
                      try {
                          auto u = parseRemoteUsers(v[2].value);
                          if (v[0].value == L"예" && u.empty() && v[3].value.empty())
                              return L"계정이 필요합니다.";
                          if (!v[3].value.empty())
                              parseRemoteUsers(v[3].value + L":" + v[4].value + L":" + newToken());
                      } catch (...) {
                          return L"계정 형식·권한·중복·64자리 소문자 16진수 키를 확인하세요.";
                      }
                      return {};
                  }))
            return;
        auto oldEnabled = remoteEnabled;
        auto oldPort = remotePort;
        auto oldUsers = remoteUsers;
        remoteEnabled = f[0].value == L"예";
        remotePort = std::stoul(f[1].value);
        remoteUsers = f[2].value;
        std::wstring generated;
        if (!f[3].value.empty()) {
            auto users = parseRemoteUsers(remoteUsers);
            generated = newToken();
            std::wstring rebuilt;
            for (const auto &u : users)
                if (wide(u.name) != f[3].value) {
                    if (!rebuilt.empty())
                        rebuilt += L";";
                    rebuilt += wide(u.name + ":" + u.role + ":" + u.token);
                }
            if (!rebuilt.empty())
                rebuilt += L";";
            remoteUsers = rebuilt + f[3].value + L":" + f[4].value + L":" + generated;
        }
        if (!saveSettings()) {
            remoteEnabled = oldEnabled;
            remotePort = oldPort;
            remoteUsers = oldUsers;
            error(L"설정 저장 실패");
            return;
        }
        remote.stop();
        std::wstring err;
        if (remoteEnabled && !remote.start(remotePort, parseRemoteUsers(remoteUsers), err)) {
            error(err);
            return;
        }
        if (!generated.empty())
            message(L"새 API 키 (계정 설정에서도 확인 가능):\n" + generated, L"API 키",
                    MB_OK | MB_ICONINFORMATION);
        if (remoteEnabled)
            ShellExecuteW(window, L"open", (L"http://127.0.0.1:" + std::to_wstring(remotePort)).c_str(),
                          nullptr, nullptr, SW_SHOWNORMAL);
    } else if (id == BackupOptions) {
        std::vector<FormField> f{{L"ZIP 압축", s.zipBackup ? L"예" : L"아니요", {L"아니요", L"예"}},
                                 {L"보관 개수 (0: 자동 삭제 안 함)", std::to_wstring(s.backupKeep)}};
        if (form(L"백업 설정",
                 L"백업은 저장 위치 / 프로필 ID 아래에 보관됩니다.\n새 백업 성공 후 이 프로필의 완료된 "
                 L"백업만 정리합니다.\n실행 중 백업은 정상 종료 → 백업 → 재실행 순서로 처리합니다.",
                 f, [&](const auto &v) -> std::wstring {
                     return number(v[1].value, 0, 1000) ? L"" : L"보관 개수 범위: 0~1000";
                 })) {
            s.zipBackup = f[0].value == L"예";
            s.backupKeep = std::stoul(f[1].value);
            saveSettings();
        }
    } else if (id == BackupRestore) {
        auto i = listSelection(BackupList);
        if (i < 0 || static_cast<size_t>(i) >= backupFiles.size()) {
            showPage(4);
            error(L"복원할 완료 백업을 선택하세요.");
            return;
        }
        if (overlapsRunningDirectory(s) || backupPending || installerActive) {
            error(L"서버·설치·백업을 모두 중지하세요.");
            return;
        }
        if (backupFiles[i].wstring().find(L".partial") != std::wstring::npos) {
            error(L"미완료 백업은 복원할 수 없습니다.");
            return;
        }
        if (!confirm(L"선택한 백업으로 서버 폴더를 복원할까요?\n현재 폴더는 .before-restore 이름으로 "
                     L"보존합니다. 자동 실행하지 않습니다."))
            return;
        std::wstring err;
        if (!backupJob.restore(backupFiles[i], serverDirectory(s), err)) {
            error(err);
            return;
        }
        s.recoverAt = 0;
        s.resumeAfterBackup = false;
        backupOwner = selected;
        backupSource = s.canonicalDirectory;
        backupPending = true;
        updateControls();
    } else if (id == TaskTiming) {
        auto i = listSelection(TaskList);
        if (i < 0 || static_cast<size_t>(i) >= s.tasks.size()) {
            showPage(5);
            error(L"작업을 먼저 선택하세요.");
            return;
        }
        auto &task = s.tasks[i];
        std::vector<FormField> f{
            {L"예약 방식", task.minuteOfDay < 0 ? L"간격" : L"매일 시각", {L"간격", L"매일 시각"}},
            {L"시 (0~23)", std::to_wstring(std::max(0, task.minuteOfDay) / 60)},
            {L"분 (0~59)", std::to_wstring(std::max(0, task.minuteOfDay) % 60)}};
        if (form(L"작업 실행 시각",
                 L"매일 시각은 Windows의 로컬 시간을 사용합니다. 앱이 켜져 있을 때 실행합니다.\n앱 "
                 L"재실행·재개 시 다음 미래 시각을 예약하며 놓친 회차를 몰아서 실행하지 않습니다.",
                 f, [&](const auto &v) -> std::wstring {
                     return number(v[1].value, 0, 23) && number(v[2].value, 0, 59)
                                ? L""
                                : L"유효한 시와 분을 입력하세요.";
                 })) {
            task.minuteOfDay = f[0].value == L"간격"
                                   ? -1
                                   : static_cast<int>(std::stoul(f[1].value) * 60 + std::stoul(f[2].value));
            task.nextWall = nextDaily(task.minuteOfDay);
            task.due = GetTickCount64() + task.seconds * 1000ULL;
            saveSettings();
            refreshTasks();
        }
    } else if (id == PlayerManage) {
        std::vector<FormField> f{
            {L"플레이어 이름", faces.selectedName()},
            {L"동작",
             L"whitelist add",
             {L"whitelist add", L"whitelist remove", L"whitelist list", L"whitelist reload", L"pardon",
              L"ban", L"kick", L"op", L"deop", L"banlist players"}}};
        if (form(
                L"플레이어 · 화이트리스트",
                L"오프라인 플레이어 이름도 입력할 수 있습니다. 서버 실행 중 명령을 "
                L"전송합니다.\n차단·운영자·화이트리스트 파일은 서버가 관리합니다. 결과는 콘솔에서 "
                L"확인하세요.",
                f, [&](const auto &v) -> std::wstring {
                    bool noName = v[1].value == L"whitelist list" || v[1].value == L"whitelist reload" ||
                                  v[1].value == L"banlist players";
                    return noName ||
                                   (!v[0].value.empty() && v[0].value.size() <= 16 &&
                                    v[0].value.find_first_not_of(
                                        L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") ==
                                        std::wstring::npos)
                               ? L""
                               : L"유효한 Java 플레이어 이름을 입력하세요.";
                })) {
            auto cmd = f[1].value;
            if (cmd != L"whitelist list" && cmd != L"whitelist reload" && cmd != L"banlist players")
                cmd += L" " + f[0].value;
            if (confirm(cmd + L" 명령을 실행할까요?")) {
                if (!s.process.send(cmd))
                    error(L"서버가 실행 중인지 확인하세요.");
                else {
                    append(s, L"> " + cmd + L"\r\n");
                    showPage(1);
                }
            }
        }
    } else if (id == PluginManage) {
        auto i = listSelection(PluginList);
        if (i < 0 || static_cast<size_t>(i) >= pluginFiles.size()) {
            showPage(2);
            error(L"플러그인을 선택하세요.");
            return;
        }
        if (overlapsRunningDirectory(s) || overlapsBackup(s)) {
            error(L"서버와 백업을 중지하세요.");
            return;
        }
        std::vector<FormField> f{{L"동작", L"JAR 교체", {L"JAR 교체", L"플러그인 제거"}}};
        if (!form(L"플러그인 파일 관리",
                  L"교체·제거 시 원본 JAR은 plugins/.wrapper-previous 아래에 보존합니다.\n새 버전의 호환성과 "
                  L"의존성은 배포처에서 확인하세요.",
                  f, accept))
            return;
        auto old = pluginFiles[i];
        std::wstring replacement;
        if (f[0].value == L"JAR 교체") {
            replacement = chooseFile(window, L"Plugin JAR\0*.jar\0\0");
            if (replacement.empty())
                return;
            JarArchive archive(replacement);
            if (!archive.valid()) {
                error(L"유효한 JAR이 아닙니다.");
                return;
            }
        }
        if (!confirm(old.filename().wstring() + L" 파일을 " + f[0].value + L"할까요?"))
            return;
        try {
            auto dir = old.parent_path() / L".wrapper-previous";
            fs::create_directories(dir);
            auto saved = dir / (old.filename().wstring() + L"." + newToken().substr(0, 12));
            fs::rename(old, saved);
            try {
                if (!replacement.empty())
                    fs::copy_file(replacement, old);
            } catch (...) {
                fs::rename(saved, old);
                throw;
            }
            refreshPlugins();
        } catch (...) {
            error(L"파일 작업 실패. 권한·잠금 상태를 확인하세요.");
        }
    } else if (id == GroupActions) {
        std::vector<std::wstring> groups{L"전체 그룹"};
        for (const auto &g : groupRegistry.names())
            groups.push_back(g);
        std::vector<FormField> f{{L"대상 그룹", groups.front(), groups},
                                 {L"동작", L"재시작", {L"재시작", L"명령 전송", L"강제 종료"}},
                                 {L"명령", L"save-all"}};
        if (!form(L"그룹 일괄 작업",
                  L"대상 그룹의 실행 중인 서버에 적용합니다.\n강제 종료는 저장하지 않은 월드 데이터를 잃을 "
                  L"수 있습니다.",
                  f, [&](const auto &v) -> std::wstring {
                      return v[1].value != L"명령 전송" ||
                                     (!v[2].value.empty() &&
                                      v[2].value.find_first_of(L"\r\n") == std::wstring::npos &&
                                      utf8(v[2].value).size() <= 2047)
                                 ? L""
                                 : L"한 줄 명령을 입력하세요.";
                  }))
            return;
        if (!confirm(f[0].value + L"에 " + f[1].value + L" 작업을 실행할까요?"))
            return;
        for (auto &server : servers)
            if ((f[0].value == L"전체 그룹" || server->group == f[0].value) && server->process.running()) {
                if (f[1].value == L"재시작") {
                    if (overlapsBackup(*server)) {
                        append(*server, L"[Group] 백업 중 재시작 건너뜀\r\n");
                        continue;
                    }
                    stopServer(*server, true);
                } else if (f[1].value == L"강제 종료") {
                    cancelAutomaticWork(*server);
                    server->process.kill();
                } else {
                    auto cmd = f[2].value;
                    if (cmd == L"stop" || cmd == L"end" || cmd == L"shutdown") {
                        cancelAutomaticWork(*server);
                        stopServer(*server);
                    } else
                        append(*server, server->process.send(cmd) ? L"[Group] > " + cmd + L"\r\n"
                                                                  : L"[Group] 명령 전송 실패\r\n");
                }
            }
    } else if (id == ExtraProperties) {
        if (overlapsBackup(s) || s.kind == L"Velocity" || s.kind == L"BungeeCord" || s.kind == L"Waterfall" ||
            s.launch.mode == 3) {
            error(L"이 서버 또는 백업 상태에서는 Java 서버 속성을 편집할 수 없습니다.");
            return;
        }
        ServerProperties props;
        auto path = serverDirectory(s) / L"server.properties";
        if (serverDirectory(s).empty() || !props.load(path)) {
            error(L"속성 파일을 읽지 못했습니다.");
            return;
        }
        std::vector<FormField> f{{L"MOTD (ASCII / \\uXXXX)", wide(props.get("motd", "A Minecraft Server"))},
                                 {L"View distance", wide(props.get("view-distance", "10"))},
                                 {L"Simulation distance", wide(props.get("simulation-distance", "10"))},
                                 {L"Level name", wide(props.get("level-name", "world"))}};
        if (form(L"추가 서버 속성",
                 L"다음 재시작부터 적용합니다. 기존 주석과 다른 속성은 보존합니다.\nMOTD에 한글을 넣을 때는 "
                 L"Java properties 형식의 \\uXXXX를 사용하세요.",
                 f, [&](const auto &v) -> std::wstring {
                     return number(v[1].value, 2, 32) && number(v[2].value, 2, 32) && !v[0].value.empty() &&
                                    v[0].value.find_first_of(L"\r\n") == std::wstring::npos &&
                                    !v[3].value.empty() &&
                                    v[3].value.find_first_of(L"/\\:\r\n") == std::wstring::npos &&
                                    v[3].value != L"." && v[3].value != L".."
                                ? L""
                                : L"거리 범위 2~32, MOTD와 월드 이름을 확인하세요.";
                 })) {
            if (!props.save(path, {{"motd", utf8(f[0].value)},
                                   {"view-distance", utf8(f[1].value)},
                                   {"simulation-distance", utf8(f[2].value)},
                                   {"level-name", utf8(f[3].value)}}))
                error(L"속성 저장 실패");
        }
    }
}
void App::applyRemote() {
    if (operationsBusy)
        return;
    for (auto &request : remote.pending()) {
        if (request->cancelled)
            continue;
        std::string result;
        try {
            result = remoteRequest(*request);
        } catch (...) {
            result = "400\n{\"error\":\"invalid request\"}";
        }
        request->result.set_value(std::move(result));
    }
}
std::string App::remoteRequest(const RemoteRequest &r) {
    auto fail = [](int status, const char *message) {
        return std::to_string(status) + "\n{\"error\":" + jsonString(message) + "}";
    };
    if (closing)
        return fail(503, "manager shutting down");
    if (r.method != "GET" && r.role == "viewer")
        return fail(403, "read only account");
    if (r.path == "/api/servers" && r.method == "GET") {
        std::string result = "{\"servers\":[";
        bool first = true;
        for (const auto &server : servers) {
            if (!first)
                result += ',';
            first = false;
            result += "{\"id\":" + jsonString(utf8(server->id)) +
                      ",\"name\":" + jsonString(utf8(server->name)) +
                      ",\"pid\":" + std::to_string(server->process.pid()) +
                      ",\"running\":" + (server->process.running() ? "true" : "false") +
                      ",\"cpu\":" + std::to_string(server->process.cpuPercent()) +
                      ",\"memory\":" + std::to_string(server->process.memoryMB()) +
                      ",\"uptime\":" + std::to_string(server->process.uptimeSeconds()) +
                      ",\"players\":" + std::to_string(server->players.size()) + "}";
        }
        return result + "],\"role\":" + jsonString(r.role) + "}";
    }
    if (r.path.starts_with("/api/users")) {
        if (r.role != "admin")
            return fail(403, "admin required");
        auto users = parseRemoteUsers(remoteUsers);
        if (r.path == "/api/users" && r.method == "GET") {
            std::string out = "{\"users\":[";
            bool first = true;
            for (const auto &u : users) {
                if (!first)
                    out += ',';
                first = false;
                out += "{\"name\":" + jsonString(u.name) + ",\"role\":" + jsonString(u.role) + "}";
            }
            return out + "]}";
        }
        if (r.path == "/api/users" && r.method == "POST") {
            auto added = parseRemoteUsers(wide(r.body));
            if (added.size() != 1)
                return fail(400, "one user required: name:role:token");
            auto u = added.front();
            users.erase(
                std::remove_if(users.begin(), users.end(), [&](const auto &v) { return v.name == u.name; }),
                users.end());
            users.push_back(u);
        } else if (r.method == "DELETE" && r.path.starts_with("/api/users/")) {
            auto name = r.path.substr(11);
            users.erase(
                std::remove_if(users.begin(), users.end(), [&](const auto &u) { return u.name == name; }),
                users.end());
        } else
            return fail(404, "unknown endpoint");
        if (std::none_of(users.begin(), users.end(), [](const auto &u) { return u.role == "admin"; }))
            return fail(400, "retain at least one admin");
        std::wstring value;
        for (const auto &u : users) {
            if (!value.empty())
                value += L";";
            value += wide(u.name + ":" + u.role + ":" + u.token);
        }
        parseRemoteUsers(value);
        auto old = remoteUsers;
        remoteUsers = value;
        if (!saveSettings()) {
            remoteUsers = old;
            return fail(500, "settings write failed");
        }
        remote.setUsers(std::move(users));
        return "{\"ok\":true}";
    }
    auto parts = split(r.path, '/');
    if (parts.size() < 5 || parts[1] != "api" || parts[2] != "servers")
        return fail(404, "unknown endpoint");
    auto found =
        std::find_if(servers.begin(), servers.end(), [&](const auto &s) { return utf8(s->id) == parts[3]; });
    if (found == servers.end())
        return fail(404, "unknown server");
    if (parts.size() > 6 || (parts.size() > 5 && parts[4] != "tasks"))
        return fail(404, "unknown endpoint");
    auto &s = **found;
    auto index = static_cast<size_t>(found - servers.begin());
    auto action = parts[4];
    if (r.method == "GET") {
        if (action == "log") {
            auto tail = s.log.size() > 50000 ? s.log.substr(s.log.size() - 50000) : s.log;
            return "{\"log\":" + jsonString(utf8(tail)) + "}";
        }
        if (action == "tasks") {
            std::string out = "{\"tasks\":[";
            for (size_t i = 0; i < s.tasks.size(); ++i) {
                auto &t = s.tasks[i];
                if (i)
                    out += ',';
                out += "{\"index\":" + std::to_string(i) + ",\"type\":" + std::to_string(t.type) +
                       ",\"seconds\":" + std::to_string(t.seconds) +
                       ",\"minute\":" + std::to_string(t.minuteOfDay) +
                       ",\"enabled\":" + (t.enabled ? "true" : "false") +
                       ",\"command\":" + jsonString(utf8(t.command)) +
                       ",\"lastRun\":" + jsonString(utf8(t.lastRun)) +
                       ",\"result\":" + jsonString(utf8(t.lastResult)) + "}";
            }
            return out + "]}";
        }
        if (action == "backups") {
            std::string out = "{\"backups\":[";
            bool first = true;
            std::error_code ec;
            fs::directory_iterator it(fs::path(s.backup) / s.id, ec), end;
            while (!ec && it != end) {
                if (!first)
                    out += ',';
                first = false;
                out += jsonString(utf8(it->path().filename().wstring()));
                it.increment(ec);
            }
            return out + "]}";
        }
        return fail(404, "unknown endpoint");
    }
    if (action == "tasks") {
        auto previous = s.tasks;
        if (r.method == "POST") {
            auto values = split(r.body, '|');
            if (values.size() != 4)
                return fail(400, "body: type|seconds|minute(-1 interval)|command");
            if (!number(wide(values[0]), 0, 2) || !number(wide(values[1]), 1, 864000) ||
                (values[2] != "-1" && !number(wide(values[2]), 0, 1439)))
                return fail(400, "invalid task timing");
            Task t;
            t.type = std::stoi(values[0]);
            t.seconds = std::stoul(values[1]);
            t.minuteOfDay = std::stoi(values[2]);
            t.command = wide(values[3]);
            if (t.command.starts_with(L"/"))
                t.command.erase(0, 1);
            if (t.type != 0)
                t.command.clear();
            if (t.type == 0 && (t.command.empty() || utf8(t.command).size() > 2047 ||
                                t.command.find_first_of(L"\r\n") != std::wstring::npos))
                return fail(400, "invalid command");
            t.due = GetTickCount64() + t.seconds * 1000ULL;
            t.nextWall = nextDaily(t.minuteOfDay);
            if (parts.size() == 6) {
                if (!number(wide(parts[5]), 0, 255) || std::stoul(parts[5]) >= s.tasks.size())
                    return fail(404, "unknown task");
                s.tasks[std::stoul(parts[5])] = t;
            } else {
                if (s.tasks.size() >= 256)
                    return fail(409, "task limit");
                s.tasks.push_back(t);
            }
        } else if (r.method == "DELETE" && parts.size() == 6) {
            if (!number(wide(parts[5]), 0, 255) || std::stoul(parts[5]) >= s.tasks.size())
                return fail(404, "unknown task");
            s.tasks.erase(s.tasks.begin() + std::stoul(parts[5]));
        } else
            return fail(404, "unknown endpoint");
        if (!saveSettings()) {
            s.tasks = std::move(previous);
            return fail(500, "settings write failed");
        }
        if (selected == index)
            refreshTasks();
    } else if (r.method == "POST") {
        if (action == "start") {
            s.recoveryAttempts = 0;
            if (!s.eulaAccepted)
                return fail(409, "accept EULA locally first");
            if (!startServer(s, false))
                return fail(409, "cannot start; inspect console");
        } else if (action == "stop" || action == "restart") {
            if (action == "stop" && s.recoverAt) {
                cancelAutomaticWork(s);
                return "{\"ok\":true}";
            }
            if (!s.process.running() || s.process.stopping())
                return fail(409, "server is not ready for power action");
            cancelAutomaticWork(s);
            stopServer(s, action == "restart");
        } else if (action == "kill") {
            if (r.role != "admin")
                return fail(403, "admin required for force kill");
            cancelAutomaticWork(s);
            if (!s.process.kill())
                return fail(409, "not running");
        } else if (action == "command") {
            auto cmd = wide(r.body);
            if (cmd.starts_with(L"/"))
                cmd.erase(0, 1);
            if (cmd == L"stop" || cmd == L"end" || cmd == L"shutdown") {
                cancelAutomaticWork(s);
                if (!s.process.running())
                    return fail(409, "server stopped");
                stopServer(s);
            } else if (!s.process.send(cmd))
                return fail(409, "command rejected");
            append(s, L"[Remote " + wide(r.user) + L"] > " + cmd + L"\r\n");
        } else if (action == "backup") {
            if (backupPending || s.backupAfterStop)
                return fail(409, "backup busy");
            runBackup(index, false);
            if (!backupPending && !s.backupAfterStop)
                return fail(409, "backup rejected; inspect console");
        } else
            return fail(404, "unknown endpoint");
    } else
        return fail(404, "unknown endpoint");
    append(s, L"[Remote " + wide(r.user) + L"] " + wide(r.method + " " + action) + L"\r\n");
    return "{\"ok\":true}";
}
} // namespace wrapper

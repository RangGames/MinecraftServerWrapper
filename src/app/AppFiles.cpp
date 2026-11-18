#include "App.h"

namespace wrapper {

fs::path App::serverDirectory(const Server &server) const {
    return !server.launch.directory.empty() ? fs::path(server.launch.directory)
           : server.jar.empty()             ? fs::path{}
                                            : fs::path(server.jar).parent_path();
}
void App::refreshServerDirectory(Server &server) {
    auto path = serverDirectory(server);
    std::error_code ec;
    server.canonicalDirectory = path.empty() ? fs::path{} : fs::weakly_canonical(path, ec);
    if (ec)
        server.canonicalDirectory.clear();
}
bool App::overlapsRunningDirectory(const Server &server) const {
    if (installerActive && resolvedPathsOverlap(server.canonicalDirectory, fs::path(installerTarget)))
        return true;
    for (const auto &other : servers)
        if (other->process.running()) {
            if (!server.jar.empty() && server.canonicalDirectory.empty())
                return true;
            if (resolvedPathsOverlap(server.canonicalDirectory, other->activeDirectory))
                return true;
        }
    return false;
}
bool App::overlapsBackup(const Server &server) const {
    return backupPending && (server.canonicalDirectory.empty()
                                 ? !server.jar.empty()
                                 : resolvedPathsOverlap(server.canonicalDirectory, backupSource));
}

void App::browseFile(bool java, const fs::path &initial) {
    std::vector<wchar_t> path(32768);
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    auto initialFolder = initial.empty() ? serverDirectory(current()) : initial;
    dialog.lpstrInitialDir = initialFolder.empty() ? nullptr : initialFolder.c_str();
    dialog.hwndOwner = window;
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.lpstrFilter =
        java ? L"Java 실행 파일\0java.exe\0실행 파일\0*.exe\0\0" : L"Minecraft 서버 JAR\0*.jar\0\0";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&dialog)) {
        acceptPath(path.data());
    }
}

HBITMAP App::imageBitmap(const fs::path &path) {
    std::error_code ec;
    auto bytes = fs::file_size(path, ec);
    if (ec || bytes > 10 * 1024 * 1024)
        return nullptr;
    Gdiplus::Bitmap original(path.c_str());
    if (original.GetLastStatus() != Gdiplus::Ok || !original.GetWidth() || !original.GetHeight() ||
        original.GetWidth() > 8192 || original.GetHeight() > 8192)
        return nullptr;
    Gdiplus::Bitmap scaled(28, 28, PixelFormat32bppARGB);
    Gdiplus::Graphics graphics(&scaled);
    // Flatten onto the card color of the current theme (thumbnails are reloaded on theme changes).
    graphics.Clear(Gdiplus::Color(255, GetRValue(Panel), GetGValue(Panel), GetBValue(Panel)));
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    double ratio = std::min(28.0 / original.GetWidth(), 28.0 / original.GetHeight());
    int width = std::max(1, static_cast<int>(original.GetWidth() * ratio)),
        height = std::max(1, static_cast<int>(original.GetHeight() * ratio));
    graphics.DrawImage(&original, (28 - width) / 2, (28 - height) / 2, width, height);
    HBITMAP result = nullptr;
    scaled.GetHBITMAP(Gdiplus::Color(255, GetRValue(Panel), GetGValue(Panel), GetBValue(Panel)), &result);
    return result;
}

void App::loadThumbnail(Server &server) {
    if (server.thumbnail)
        DeleteObject(server.thumbnail);
    auto automatic = serverDirectory(server);
    server.thumbnail = server.image.empty()
                           ? (automatic.empty() ? nullptr : imageBitmap(automatic / L"server-icon.png"))
                           : imageBitmap(server.image);
    if (control(ServerSelector))
        InvalidateRect(control(ServerSelector), nullptr, FALSE);
}

void App::setProfileImage(const fs::path &path) {
    HBITMAP bitmap = imageBitmap(path);
    if (!bitmap) {
        error(L"PNG, JPG, BMP 이미지를 선택하세요 (최대 10 MB, 8192 × 8192).");
        return;
    }
    auto directory = settings.parent_path() / L"profile-images";
    std::error_code ec;
    fs::create_directories(directory, ec);
    auto destination = directory / (std::to_wstring(GetCurrentProcessId()) + L"-" +
                                    std::to_wstring(GetTickCount64()) + path.extension().wstring());
    if (!ec)
        fs::copy_file(path, destination, fs::copy_options::none, ec);
    if (ec) {
        DeleteObject(bitmap);
        error(L"프로필 이미지를 저장하지 못했습니다. 실행 폴더 권한을 확인하세요.");
        return;
    }
    auto &server = current();
    if (server.thumbnail)
        DeleteObject(server.thumbnail);
    server.thumbnail = bitmap;
    draftImage = destination.wstring();
    set(ProfileImage, draftImage);
    InvalidateRect(control(ServerSelector), nullptr, FALSE);
}

void App::chooseProfileImage() {
    std::vector<wchar_t> path(32768);
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.lpstrFilter = L"이미지 (PNG, JPG, BMP)\0*.png;*.jpg;*.jpeg;*.bmp\0\0";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&dialog))
        setProfileImage(path.data());
}

void App::acceptPath(const fs::path &input) {
    auto path = fs::path(normalizePathInput(input.wstring(), settings.parent_path()));
    auto extension = path.extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
    if (page == 2 && extension == L".jar") {
        importPluginFiles({path});
        return;
    }
    if (extension == L".png" || extension == L".jpg" || extension == L".jpeg" || extension == L".bmp") {
        setProfileImage(path);
        return;
    }
    if (closing || current().process.running() || overlapsBackup(current())) {
        error(L"서버와 백업이 중지된 상태에서 경로를 변경하세요.");
        return;
    }
    std::error_code ec;
    if (fs::is_directory(path, ec)) {
        std::vector<fs::path> jars;
        fs::directory_iterator it(path, ec), end;
        while (!ec && it != end) {
            auto ext = it->path().extension().wstring();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
            if (it->is_regular_file(ec) && ext == L".jar")
                jars.push_back(it->path());
            it.increment(ec);
        }
        if (ec) {
            error(L"서버 폴더를 읽을 수 없습니다.");
            return;
        }
        if (jars.size() != 1) {
            showPage(6);
            error(L"폴더에 JAR이 없거나 여러 개 있습니다. JAR 선택에서 실행 파일을 지정하세요.");
            browseFile(false, path);
            return;
        }
        path = jars.front();
        extension = L".jar";
    }
    if (!fs::is_regular_file(path, ec) || (extension != L".jar" && extension != L".exe")) {
        error(L"서버 JAR, Java 실행 파일, 서버 폴더 또는 이미지를 선택하세요.");
        return;
    }
    set(extension == L".exe" ? Java : Jar, path.wstring());
    if (extension == L".jar")
        SendMessageW(control(Eula), BM_SETCHECK, BST_UNCHECKED, 0);
    validatePathFields();
    showPage(6);
}

void App::chooseServerFolder() {
    BROWSEINFOW browse{};
    browse.hwndOwner = window;
    browse.lpszTitle = L"서버 JAR이 들어 있는 폴더를 선택하세요.";
    browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    auto item = SHBrowseForFolderW(&browse);
    if (!item)
        return;
    wchar_t path[MAX_PATH]{};
    bool found = SHGetPathFromIDListW(item, path) != FALSE;
    CoTaskMemFree(item);
    if (found)
        acceptPath(path);
}

void App::openFolder(const fs::path &path) {
    if (path.empty()) {
        error(L"설정에서 서버 JAR 또는 폴더를 먼저 지정하세요.");
        return;
    }
    if (reinterpret_cast<INT_PTR>(
            ShellExecuteW(window, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <= 32)
        error(L"폴더를 열 수 없습니다.");
}

void App::exportLog() {
    std::vector<wchar_t> path(32768);
    wcscpy_s(path.data(), path.size(), L"console.log");
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.lpstrDefExt = L"log";
    dialog.lpstrFilter = L"Log file\0*.log\0Text file\0*.txt\0\0";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (GetSaveFileNameW(&dialog)) {
        std::ofstream out(fs::path(path.data()), std::ios::binary);
        auto bytes = utf8(current().log);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        out.close();
        if (!out)
            error(L"로그 파일을 저장하지 못했습니다.");
    }
}

void App::refreshPlugins() {
    pendingPluginSelection = listSelection(PluginList);
    pluginLoading = true;
    enable(PluginToggle, false);
    timerInterval = 100;
    SetTimer(window, 1, timerInterval, nullptr);
    ListView_DeleteAllItems(control(PluginList));
    pluginFiles.clear();
    auto directory = serverDirectory(current());
    pluginTicket = pluginCatalog.request(directory.empty() ? fs::path{} : directory / L"plugins",
                                         settings.parent_path() / L"plugin-cache");
}
void App::pollPlugins() {
    auto result = pluginCatalog.poll();
    if (!result || result->ticket != pluginTicket)
        return;
    pluginLoading = false;
    ListView_DeleteAllItems(control(PluginList));
    pluginFiles.clear();
    for (size_t i = 0; i < result->entries.size(); ++i) {
        const auto &entry = result->entries[i];
        pluginFiles.push_back(entry.path);
        listRow(PluginList, static_cast<int>(i),
                {entry.path.filename().wstring(), entry.name.empty() ? L"—" : entry.name,
                 entry.version.empty() ? L"—" : entry.version,
                 entry.path.extension() == L".disabled" ? L"비활성 (다음 시작)" : L"활성 (다음 시작)",
                 decimal(entry.size / 1024.0, 0) + L" KB"});
    }
    if (pendingPluginSelection >= 0 && static_cast<size_t>(pendingPluginSelection) < pluginFiles.size())
        ListView_SetItemState(control(PluginList), pendingPluginSelection, LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
    updateControls();
}

void App::importPlugin() {
    refreshServerDirectory(current());
    if (overlapsRunningDirectory(current()) || overlapsBackup(current())) {
        error(L"이 폴더의 서버 실행 또는 백업이 끝난 뒤 변경하세요.");
        return;
    }
    auto directory = serverDirectory(current());
    if (directory.empty()) {
        error(L"서버 JAR을 먼저 지정하세요.");
        return;
    }
    std::vector<wchar_t> path(32768);
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.lpstrFilter = L"Plugin JAR\0*.jar\0\0";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&dialog))
        return;
    importPluginFiles({fs::path(path.data())});
}
bool App::importPluginFiles(const std::vector<fs::path> &paths) {
    refreshServerDirectory(current());
    if (closing || overlapsRunningDirectory(current()) || overlapsBackup(current())) {
        error(L"서버 실행 또는 백업이 끝난 뒤 플러그인을 추가하세요.");
        return false;
    }
    auto directory = serverDirectory(current());
    if (directory.empty()) {
        error(L"서버 JAR을 먼저 지정하세요.");
        return false;
    }
    if (paths.empty() || paths.size() > 64) {
        error(L"플러그인 JAR을 한 번에 1~64개 놓으세요.");
        return false;
    }
    for (const auto &path : paths) {
        auto extension = path.extension().wstring();
        std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
        if (extension != L".jar" || !JarArchive(path).valid()) {
            error(L"유효한 플러그인 JAR 파일을 선택하세요.");
            return false;
        }
    }
    std::error_code ec;
    fs::create_directories(directory / L"plugins", ec);
    unsigned copied = 0;
    if (!ec)
        for (const auto &path : paths) {
            fs::copy_file(path, directory / L"plugins" / path.filename(), fs::copy_options::none, ec);
            if (ec)
                break;
            ++copied;
        }
    refreshPlugins();
    if (ec) {
        error(std::to_wstring(copied) +
              L"개 추가 후 복사를 중단했습니다. 같은 이름의 파일과 쓰기 권한을 확인하세요.");
        return false;
    }
    append(current(), L"[Plugins] " + std::to_wstring(copied) +
                          L"개 플러그인을 추가했습니다. 다음 시작부터 적용됩니다.\r\n");
    return true;
}

void App::togglePlugin() {
    refreshServerDirectory(current());
    if (overlapsRunningDirectory(current()) || overlapsBackup(current())) {
        error(L"이 폴더의 서버 실행 또는 백업이 끝난 뒤 변경하세요.");
        return;
    }
    int index = listSelection(PluginList);
    if (index < 0 || static_cast<size_t>(index) >= pluginFiles.size())
        return;
    auto from = pluginFiles[index], to = from;
    if (from.extension() == L".disabled")
        to.replace_extension();
    else
        to += L".disabled";
    std::error_code ec;
    if (fs::exists(to, ec)) {
        error(L"변경할 이름의 파일이 이미 존재합니다.");
        return;
    }
    fs::rename(from, to, ec);
    if (ec)
        error(L"플러그인 파일 이름을 변경하지 못했습니다.");
    refreshPlugins();
}

void App::chooseBackupFolder() {
    BROWSEINFOW browse{};
    browse.hwndOwner = window;
    browse.lpszTitle = L"서버 폴더 밖의 백업 저장 위치를 선택하세요.";
    browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE item = SHBrowseForFolderW(&browse);
    if (!item)
        return;
    wchar_t path[MAX_PATH];
    if (SHGetPathFromIDListW(item, path)) {
        current().backup = path;
        set(BackupPath, path);
        saveSettings();
        refreshBackups();
    }
    CoTaskMemFree(item);
}

void App::refreshBackups() {
    ListView_DeleteAllItems(control(BackupList));
    backupFiles.clear();
    if (current().backup.empty())
        return;
    std::error_code ec;
    fs::path directory = extendedPath(fs::path(current().backup) / current().id);
    if (!fs::is_directory(directory, ec))
        return;
    fs::directory_iterator it(directory, ec), end;
    while (!ec && it != end) {
        if ((it->is_directory(ec) || it->path().extension() == L".zip") && !ec)
            backupFiles.push_back(it->path());
        if (ec)
            break;
        it.increment(ec);
    }
    std::sort(backupFiles.rbegin(), backupFiles.rend());
    for (size_t i = 0; i < backupFiles.size(); ++i)
        listRow(BackupList, static_cast<int>(i),
                {backupFiles[i].filename().wstring(),
                 backupFiles[i].extension() == L".partial" ? L"미완료" : L"완료"});
}

void App::runBackup(size_t owner, bool interactive) {
    auto &server = *servers[owner];
    refreshServerDirectory(server);
    auto fail = [&](const std::wstring &message) {
        append(server, L"[Wrapper] " + message + L"\r\n");
        if (interactive)
            error(message);
    };
    if (installerActive && resolvedPathsOverlap(server.canonicalDirectory, fs::path(installerTarget))) {
        fail(L"이 폴더에 서버를 설치하고 있습니다.");
        return;
    }
    if (backupJob.busy() || backupPending) {
        fail(L"이전 백업이 진행 중입니다.");
        return;
    }
    if (server.process.running()) {
        if (server.process.stopping()) {
            fail(L"서버 종료를 기다리고 있습니다.");
            return;
        }
        if (server.backup.empty()) {
            fail(L"백업 저장 위치를 먼저 선택하세요.");
            return;
        }
        if (interactive && !confirm(L"서버를 정상 종료하고 백업 후 다시 시작할까요?"))
            return;
        backupPending = true;
        backupOwner = owner;
        backupSource = server.canonicalDirectory;
        server.backupAfterStop = server.resumeAfterBackup = true;
        stopServer(server);
        append(server, L"[Backup] 정상 종료 후 백업을 예약했습니다.\r\n");
        return;
    }
    if (overlapsRunningDirectory(server)) {
        server.resumeAfterBackup = false;
        fail(L"이 폴더를 사용하는 서버를 모두 정상 종료하세요. 실행 중인 월드는 복사하지 않습니다.");
        return;
    }
    if (server.backup.empty()) {
        fail(L"백업 저장 위치를 먼저 선택하세요.");
        return;
    }
    auto directory = serverDirectory(server);
    if (directory.empty()) {
        fail(L"서버 JAR을 먼저 지정하세요.");
        return;
    }
    std::wstring message;
    if (!backupJob.start(directory, fs::path(server.backup) / server.id, message, server.zipBackup,
                         server.backupKeep)) {
        if (server.resumeAfterBackup) {
            server.resumeAfterBackup = false;
            startServer(server, false);
        }
        fail(message);
        return;
    }
    backupOwner = owner;
    backupSource = server.canonicalDirectory;
    backupPending = true;
    append(server, L"[Wrapper] 백업을 시작했습니다.\r\n");
    updateControls();
}

} // namespace wrapper

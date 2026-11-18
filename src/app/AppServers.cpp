#include "App.h"

namespace wrapper {

void App::append(Server &server, const std::wstring &value, COLORREF color) {
    size_t start = server.log.size();
    server.log += value;
    if (!server.spans.empty() && server.spans.back().color == color)
        server.spans.back().length += value.size();
    else
        server.spans.push_back({start, value.size(), color});
    bool render = &server == &current() && (page == 0 || page == 1) && !IsIconic(window);
    if (&server == &current() && !render)
        consoleDirty = true;
    if (server.log.size() > 500000) {
        auto boundary = server.log.find(L'\n', 100000);
        size_t removed = boundary == std::wstring::npos ? 100000 : boundary + 1;
        // Rich Edit stores each CRLF as one character; count them to translate log offsets.
        auto crlfIn = [](const std::wstring &text, size_t from, size_t to) {
            LONG count = 0;
            for (size_t k = from; k + 1 < to; ++k)
                if (text[k] == L'\r' && text[k + 1] == L'\n')
                    ++count;
            return count;
        };
        LONG cut = static_cast<LONG>(removed) - crlfIn(server.log, 0, removed);
        server.log.erase(0, removed);
        std::vector<Server::Span> kept;
        for (auto span : server.spans) {
            if (span.start + span.length <= removed)
                continue;
            size_t end = span.start + span.length - removed;
            span.start = span.start > removed ? span.start - removed : 0;
            span.length = end - span.start;
            kept.push_back(span);
        }
        server.spans = std::move(kept);
        if (!render)
            return;
        // Trim the control in place instead of replaying ~400k characters of colored text.
        HWND log = control(Console);
        bool trimmed = false;
        // The new chunk itself may be larger than what is kept (e.g. a burst read in one poll);
        // then part of it was trimmed too and only a full restore is correct.
        if (!consoleDirty && value.size() <= server.log.size() && removed < start) {
            size_t shown = server.log.size() - value.size();
            GETTEXTLENGTHEX query{GTL_NUMCHARS | GTL_PRECISE, 1200};
            LONG expected = static_cast<LONG>(shown) - crlfIn(server.log, 0, shown);
            SendMessageW(log, EM_SETREADONLY, FALSE, 0);
            CHARRANGE head{0, cut};
            SendMessageW(log, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&head));
            SendMessageW(log, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(L""));
            SendMessageW(log, EM_SETREADONLY, TRUE, 0);
            trimmed = SendMessageW(log, EM_GETTEXTLENGTHEX, reinterpret_cast<WPARAM>(&query), 0) == expected;
        }
        if (!trimmed) {
            restoreConsole();
            scrollEnd();
            return;
        }
    }
    if (!render)
        return;
    HWND log = control(Console);
    DWORD begin = 0, end = 0;
    LRESULT firstLine = 0;
    if (!batchingConsole) {
        SendMessageW(log, EM_GETSEL, reinterpret_cast<WPARAM>(&begin), reinterpret_cast<LPARAM>(&end));
        firstLine = SendMessageW(log, EM_GETFIRSTVISIBLELINE, 0, 0);
    }
    appendConsoleRun(value, color);
    if (!batchingConsole) {
        if (autoScroll)
            scrollEnd();
        else {
            SendMessageW(log, EM_SETSEL, begin, end);
            auto now = SendMessageW(log, EM_GETFIRSTVISIBLELINE, 0, 0);
            SendMessageW(log, EM_LINESCROLL, 0, firstLine - now);
        }
    }
}

void App::receive(Server &server, const std::wstring &value) {
    if (value.empty())
        return;
    std::wstring plain;
    bool visible = &server == &current() && (page == 0 || page == 1) && !IsIconic(window);
    HWND log = control(Console);
    const bool pauseRedraw = visible && (GetWindowLongPtrW(log, GWL_STYLE) & WS_VISIBLE) != 0;
    CHARRANGE selectedRange{};
    LRESULT firstLine = 0;
    auto runs = server.colors.decode(value);
    if (visible) {
        SendMessageW(log, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selectedRange));
        firstLine = SendMessageW(log, EM_GETFIRSTVISIBLELINE, 0, 0);
        batchingConsole = true;
        if (pauseRedraw)
            SendMessageW(log, WM_SETREDRAW, FALSE, 0);
        SendMessageW(log, EM_SETREADONLY, FALSE, 0);
    }
    for (const auto &run : runs) {
        append(server, run.text, run.color);
        plain += run.text;
    }
    if (visible) {
        SendMessageW(log, EM_SETREADONLY, TRUE, 0);
        batchingConsole = false;
        if (autoScroll)
            scrollEnd();
        else {
            SendMessageW(log, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&selectedRange));
            auto now = SendMessageW(log, EM_GETFIRSTVISIBLELINE, 0, 0);
            SendMessageW(log, EM_LINESCROLL, 0, firstLine - now);
        }
        if (pauseRedraw) {
            SendMessageW(log, WM_SETREDRAW, TRUE, 0);
            RedrawWindow(log, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME);
        }
    }
    for (wchar_t c : plain) {
        if (c == L'\n') {
            if (!server.droppingLine) {
                if (auto event = parsePlayerLog(server.line)) {
                    auto found = std::find(server.players.begin(), server.players.end(), event->name);
                    if (event->joined && found == server.players.end())
                        server.players.push_back(event->name);
                    else if (!event->joined && found != server.players.end())
                        server.players.erase(found);
                }
                if (server.line.find(L"Done (") != std::wstring::npos ||
                    (server.launch.mode == 3 && server.line.find(L"Server started.") != std::wstring::npos))
                    server.ready = true;
            }
            server.line.clear();
            server.droppingLine = false;
        } else if (!server.droppingLine) {
            if (server.line.size() >= 65536) {
                server.line.clear();
                server.droppingLine = true;
            } else
                server.line += c;
        }
    }
    if (&server == &current()) {
        faces.feed(plain);
        set(206,std::to_wstring(server.players.size())+L" Players Online");
    }
}

void App::refreshServerList() {
    refreshGroups();
    SendMessageW(control(ServerSelector), LB_RESETCONTENT, 0, 0);
    for (const auto &server : servers)
        SendMessageW(control(ServerSelector), LB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(server->name.c_str()));
    SendMessageW(control(ServerSelector), LB_SETCURSEL, selected, 0);
}

void App::selectServer(size_t index) {
    if (index >= servers.size())
        return;
    loadThumbnail(current());
    taskRows.clear();
    selected = index;
    loadFields();
    faces.clear();
    for (const auto &name : current().players)
        faces.feed(name + L" joined the game\n");
    restoreConsole();
    scrollEnd();
    refreshServerList();
    refreshPageData();
    updateControls();
    layout();
    saveSettings();
}

void App::addServer() {
    if (!confirm(L"Minecraft EULA에 동의하나요? 서버 등록을 진행하려면 동의가 "
                 L"필요합니다.\nhttps://www.minecraft.net/eula"))
        return;
    auto server = std::make_unique<Server>();
    server->id = newToken();
    server->eulaAccepted = true;
    server->name = L"Minecraft Server " + std::to_wstring(servers.size() + 1);
    server->java = current().java;
    servers.push_back(std::move(server));
    selectServer(servers.size() - 1);
    showPage(6);
    SetFocus(control(ServerName));
}

void App::removeServer(size_t index) {
    if (index >= servers.size())
        return;
    if (servers.size() == 1) {
        error(L"최소 한 개의 서버 프로필이 필요합니다.");
        return;
    }
    auto busy = [&] {
        return index >= servers.size() || servers.size() == 1 || servers[index]->process.running() ||
               servers[index]->recoverAt != 0 || (backupPending && backupOwner == index);
    };
    if (busy()) {
        error(L"실행 또는 백업 중인 프로필은 제거할 수 없습니다.");
        return;
    }
    if (!confirm(servers[index]->name + L" 프로필을 제거할까요? 서버 파일은 삭제되지 않습니다."))
        return;
    // Timers, the remote API and the group manager keep running while the confirmation is open.
    if (busy()) {
        error(L"실행 또는 백업 중인 프로필은 제거할 수 없습니다.");
        return;
    }
    servers.erase(servers.begin() + static_cast<std::ptrdiff_t>(index));
    if (backupOwner > index)
        --backupOwner;
    if (selected > index)
        --selected;
    hoveredServer = -1;
    selected = std::min(selected, servers.size() - 1);
    loadFields();
    faces.clear();
    for (const auto &name : current().players)
        faces.feed(name + L" joined the game\n");
    restoreConsole();
    refreshServerList();
    refreshPageData();
    updateControls();
    saveSettings();
}

bool App::ensureEula(Server &server, bool interactive) {
    auto path = serverDirectory(server) / L"eula.txt";
    std::ifstream file(path);
    std::string line;
    bool accepted = false;
    while (std::getline(file, line)) {
        if (!line.empty() && line.front() == '#')
            continue;
        line.erase(std::remove_if(line.begin(), line.end(),
                                  [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r'; }),
                   line.end());
        if (line == "eula=true")
            accepted = true;
        if (line == "eula=false")
            accepted = false;
    }
    file.close();
    if (accepted) {
        server.eulaAccepted = true;
        return true;
    }
    if (!server.eulaAccepted) {
        if (!interactive) {
            append(server, L"[Wrapper] EULA 동의가 필요해 자동 시작을 건너뛰었습니다.\r\n");
            return false;
        }
        if (&server != &current() || !confirm(L"Minecraft EULA에 동의하나요? 서버 등록을 진행하려면 동의가 "
                                              L"필요합니다.\nhttps://www.minecraft.net/eula"))
            return false;
        server.eulaAccepted = true;
        saveSettings();
    }
    std::ofstream out(path, std::ios::app);
    out << "\n# Accepted explicitly in Minecraft Server Manager\neula=true\n";
    out.close();
    if (!out) {
        if (interactive)
            error(L"eula.txt 저장에 실패했습니다. 서버 폴더 권한을 확인하세요.");
        else
            append(server, L"[Wrapper] eula.txt 저장에 실패했습니다. 서버 폴더 권한을 확인하세요.\r\n");
        return false;
    }
    return true;
}

bool App::startServer(Server &server, bool interactive) {
    refreshServerDirectory(server);
    if (server.process.running() || closing)
        return false;
    if (installerActive && resolvedPathsOverlap(server.canonicalDirectory, fs::path(installerTarget))) {
        if (interactive)
            error(L"이 폴더에 서버를 설치하고 있습니다.");
        return false;
    }
    if (interactive) {
        server.recoveryAttempts = 0;
        server.recoverAt = 0;
    }
    if (overlapsBackup(server)) {
        if (interactive)
            error(L"백업이 끝난 뒤 서버를 시작하세요.");
        return false;
    }
    std::error_code ec;
    auto jar =
        fs::absolute(server.jar.empty() && server.launch.mode != 0 ? serverDirectory(server) / L"launcher.jar"
                                                                   : fs::path(server.jar),
                     ec);
    std::wstring resolved, validationError;
    if (ec || (server.launch.mode < 2 && !validateJavaPath(server.java, resolved, validationError)) ||
        (server.launch.mode == 0 && !validateServerJar(jar, validationError))) {
        append(server, L"[Wrapper] " + validationError + L"\r\n");
        if (interactive) {
            settingsPage = 1;
            showPage(6);
            error(validationError);
        }
        return false;
    }
    // Never run the same server directory concurrently under two profiles.
    auto directory = server.canonicalDirectory;
    if (directory.empty()) {
        append(server, L"[Wrapper] 서버 폴더를 확인할 수 없습니다.\r\n");
        return false;
    }
    for (const auto &other : servers) {
        if (other.get() == &server || !other->process.running())
            continue;
        auto otherDir = other->activeDirectory;
        if (_wcsicmp(directory.c_str(), otherDir.c_str()) == 0) {
            if (interactive)
                error(L"같은 서버 폴더가 다른 프로필에서 실행 중입니다.");
            return false;
        }
    }
    if (!ensureEula(server, interactive))
        return false;
    std::wstring errorMessage;
    if (!server.process.start(resolved, jar, server.memory, errorMessage,
                              server.kind != L"Velocity" && server.kind != L"Waterfall" &&
                                  server.kind != L"BungeeCord",
                              server.launch)) {
        append(server, L"[Wrapper] 시작 실패: " + errorMessage + L"\r\n");
        if (interactive)
            error(errorMessage);
        return false;
    }
    server.activeDirectory = directory;
    server.intentionalStop = false;
    server.recoverAt = 0;
    server.colors.reset();
    server.ready = false;
    server.restart = false;
    server.players.clear();
    server.line.clear();
    server.droppingLine = false;
    server.samples.clear();
    if (&server == &current())
        faces.clear();
    append(server, L"\r\n[Wrapper] 서버 시작: " + jar.wstring() + L"\r\n");
    append(server, L"[Wrapper] 월드·설정·로그 폴더: " + jar.parent_path().wstring() + L"\r\n");
    timerInterval = 100;
    SetTimer(window, 1, timerInterval, nullptr);
    if (!groupOperation) {
        saveSettings();
        refreshServerList();
        updateControls();
    }
    return true;
}

void App::cancelAutomaticWork(Server &server) {
    server.intentionalStop = true;
    server.recoverAt = 0;
    server.restart = false;
    if (server.backupAfterStop && backupOwner < servers.size() && servers[backupOwner].get() == &server)
        backupPending = false;
    server.backupAfterStop = server.resumeAfterBackup = false;
}
void App::stopServer(Server &server, bool restart) {
    if (!server.process.running())
        return;
    auto command = server.kind == L"Velocity"                                      ? L"shutdown"
                   : (server.kind == L"Waterfall" || server.kind == L"BungeeCord") ? L"end"
                                                                                   : L"stop";
    server.intentionalStop = true;
    server.recoverAt = 0;
    if (server.process.stop(command)) {
        server.restart = restart;
        append(server, restart ? L"[Wrapper] 저장 후 서버를 재시작합니다.\r\n"
                               : L"[Wrapper] 정상 종료 명령을 전송했습니다. 종료를 기다립니다.\r\n");
    } else
        append(server, L"[Wrapper] 종료 명령을 전송하지 못했습니다.\r\n");
    if (!groupOperation)
        updateControls();
}

void App::forceServer(Server &server) {
    if (server.process.running() &&
        confirm(L"강제 종료할까요? 저장하지 않은 월드 데이터가 손실될 수 있습니다.")) {
        cancelAutomaticWork(server);
        if (!server.process.kill())
            error(L"프로세스를 종료하지 못했습니다.");
    }
}

void App::sendCommand() {
    auto command = text(control(Command));
    if (command.empty())
        return;
    if (command.front() == L'/')
        command.erase(0, 1);
    if (command == L"stop" || command == L"shutdown" || command == L"end") {
        cancelAutomaticWork(current());
        stopServer(current());
    } else if (!current().process.send(command)) {
        error(L"서버가 실행 중인지 확인하세요. 명령은 한 줄, UTF-8 2047바이트 이내로 입력하세요.");
        return;
    } else
        append(current(), L"> " + command + L"\r\n");
    if (!command.empty()) {
        if (commandHistory.empty() || commandHistory.back() != command)
            commandHistory.push_back(command);
        if (commandHistory.size() > 100)
            commandHistory.erase(commandHistory.begin());
    }
    autoScroll = true;
    SendMessageW(control(AutoScroll), BM_SETCHECK, BST_CHECKED, 0);
    scrollEnd();
    historyPosition = commandHistory.size();
    set(Command, L"");
}

void App::playerAction(const std::wstring &action) {
    auto name = faces.selectedName();
    if (name.empty()) {
        error(L"목록에서 플레이어를 선택하세요.");
        return;
    }
    if (!confirm(english ? L"Send \"" + action + L"\" to player " + name + L"?"
                         : name + L" 플레이어에게 " + action + L" 명령을 실행할까요?"))
        return;
    auto command = action + L" " + name;
    if (current().process.send(command))
        append(current(), L"> " + command + L"\r\n");
    else
        error(L"서버가 실행 중인지 확인하세요.");
}

LRESULT CALLBACK App::serverListProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR data) {
    auto *app = reinterpret_cast<App *>(data);
    if (msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL) {
        // One tile per column: the wheel moves the row of server tiles left or right.
        int delta = GET_WHEEL_DELTA_WPARAM(w);
        int step = (msg == WM_MOUSEWHEEL ? -delta : delta) > 0 ? 1 : -1;
        auto count = static_cast<int>(SendMessageW(hwnd, LB_GETCOUNT, 0, 0));
        auto top = static_cast<int>(SendMessageW(hwnd, LB_GETTOPINDEX, 0, 0));
        SendMessageW(hwnd, LB_SETTOPINDEX, std::clamp(top + step, 0, std::max(0, count - 1)), 0);
        return 0;
    }
    if (msg == WM_MOUSEMOVE || msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) {
        POINT point{static_cast<short>(LOWORD(l)), static_cast<short>(HIWORD(l))};
        auto hit = SendMessageW(hwnd, LB_ITEMFROMPOINT, 0, l);
        int index = HIWORD(hit) ? -1 : static_cast<int>(LOWORD(hit));
        if (msg == WM_MOUSEMOVE) {
            if (index != app->hoveredServer) {
                for (int item : {app->hoveredServer, index})
                    if (item >= 0) {
                        RECT tile{};
                        if (SendMessageW(hwnd, LB_GETITEMRECT, item, reinterpret_cast<LPARAM>(&tile)) != LB_ERR)
                            InvalidateRect(hwnd, &tile, FALSE);
                    }
                app->hoveredServer = index;
            }
            TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&track);
        } else if (index >= 0 && static_cast<size_t>(index) < app->servers.size()) {
            RECT rect{};
            SendMessageW(hwnd, LB_GETITEMRECT, index, reinterpret_cast<LPARAM>(&rect));
            if (point.x >= rect.right - S(36)) {
                if (msg == WM_LBUTTONUP && !app->closing)
                    app->removeServer(static_cast<size_t>(index));
                return 0;
            }
        }
    } else if (msg == WM_MOUSELEAVE) {
        app->hoveredServer = -1;
        InvalidateRect(hwnd, nullptr, FALSE);
    }
    return DefSubclassProc(hwnd, msg, w, l);
}

} // namespace wrapper

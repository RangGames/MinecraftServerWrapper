#include <winsock2.h>
#include <ws2tcpip.h>
#include "ServerProcess.h"
#include "BackupJob.h"
#include "RemotePanel.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace fs = std::filesystem;
void require(bool ok, const char *name) {
    if (!ok)
        throw std::runtime_error(name);
    std::cout << "PASS: " << name << std::endl;
}
template <class F> void waitFor(F test, const char *name, int timeout = 10000) {
    auto end = GetTickCount64() + timeout;
    while (GetTickCount64() < end) {
        if (test()) {
            require(true, name);
            return;
        }
        Sleep(20);
    }
    throw std::runtime_error(name);
}
std::string read(const fs::path &p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}
std::wstring resultOf(BackupJob &job) {
    std::wstring result;
    waitFor(
        [&] {
            if (auto r = job.takeResult()) {
                result = *r;
                return true;
            }
            return false;
        },
        "background job completes", 60000);
    return result;
}
std::string http(unsigned port, const std::string &request) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    DWORD timeout = 5000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeout), sizeof(timeout));
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(static_cast<u_short>(port));
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(s, reinterpret_cast<sockaddr *>(&a), sizeof(a)))
        throw std::runtime_error("HTTP connect");
    size_t sent = 0;
    while (sent < request.size()) {
        int n = send(s, request.data() + sent, static_cast<int>(request.size() - sent), 0);
        if (n <= 0)
            throw std::runtime_error("HTTP send");
        sent += n;
    }
    std::string result;
    char bytes[4096];
    int n;
    while ((n = recv(s, bytes, sizeof(bytes), 0)) > 0)
        result.append(bytes, n);
    closesocket(s);
    return result;
}
int main(int argc, char **argv) {
    auto root = fs::temp_directory_path() / (L"wrapper-operations-한글 ' " + newToken().substr(0, 12));
    try {
        require(argc == 2, "mock server argument supplied");
        fs::create_directories(root);
        fs::path mock = fs::absolute(argv[1]);
        require(extendedPath(extendedPath(root)) == extendedPath(root), "extended paths are idempotent");
        auto token = newToken();
        require(token.size() == 64 && token != newToken(), "cryptographic API keys unique and 256 bits");
        auto users = parseRemoteUsers(L"alice:admin:" + token + L";bob:viewer:" + newToken());
        require(users.size() == 2 && users[1].role == "viewer", "remote user roles parsed");
        bool rejected = false;
        try {
            parseRemoteUsers(L"bad:owner:" + token);
        } catch (...) {
            rejected = true;
        }
        require(rejected, "unknown roles rejected");
        rejected = false;
        try {
            parseRemoteUsers(L"alice:admin:" + token + L";bob:viewer:" + token);
        } catch (...) {
            rejected = true;
        }
        require(rejected, "duplicate API keys rejected");
        require(jsonString("a\n\"\\") == "\"a\\n\\\"\\\\\"" ||
                    jsonString("a\n\"\\") == "\"a\\u000a\\\"\\\\\"",
                "JSON control characters escaped");
        auto now = wallNow();
        auto tomorrow = nextDaily(0, now);
        require(tomorrow > now && tomorrow <= now + 90000 && nextDaily(-1) == 0,
                "daily schedule selects next future local time");
        LaunchOptions options;
        options.minimumMemory = 128;
        options.jvm = L"-XX:+UseG1GC";
        options.arguments = L"--port 25566";
        auto command = buildLaunchCommand(L"C:\\Program Files\\Java\\java.exe", root / L"server test.jar",
                                          2048, true, options);
        require(command.find(L"-Xms128M") != std::wstring::npos &&
                    command.find(L"-XX:+UseG1GC") != std::wstring::npos &&
                    command.find(L"--port 25566") != std::wstring::npos,
                "JVM and server arguments configurable");
        options.mode = 1;
        options.arguments = L"@user_jvm_args.txt @libraries/server_args.txt";
        command = buildLaunchCommand(L"java.exe", L"", 2048, true, options);
        require(command.find(L" -jar ") == std::wstring::npos &&
                    command.find(L"@libraries/server_args.txt") != std::wstring::npos,
                "argument-file launch does not force JAR mode");
        options.mode = 2;
        options.target = L"C:\\servers\\test & bad.bat";
        options.arguments.clear();
        rejected = false;
        try {
            buildLaunchCommand(L"", L"", 2048, false, options);
        } catch (...) {
            rejected = true;
        }
        require(rejected, "BAT shell metacharacters rejected");
        ServerProcess process;
        options = {};
        options.mode = 3;
        options.target = mock.wstring();
        options.directory = (root / L"live").wstring();
        options.arguments = L"\"argument with spaces\"";
        fs::create_directory(options.directory);
        std::wstring error, output;
        require(process.start(L"", root / L"unused.jar", 2048, error, false, options),
                "native executable launch");
        waitFor(
            [&] {
                output += process.poll();
                return output.find(L"Done (") != std::wstring::npos;
            },
            "native server output captured");
        require(read(root / L"live/args.txt").find("argument with spaces") != std::string::npos,
                "quoted native arguments preserved");
        require(process.stop(), "normal shutdown requested");
        waitFor(
            [&] {
                process.poll();
                return !process.running();
            },
            "normal shutdown completes");
        require(read(root / L"live/stopped.txt") == "saved", "normal shutdown saves before exiting");
        // A BAT path containing spaces must execute with correct cmd.exe quoting.
        auto script = root / L"launch server.bat";
        {
            std::ofstream batch(script);
            batch << "@echo off\r\n\"" << mock.string() << "\" \"batch argument\"\r\n";
        }
        options.mode = 2;
        options.target = script.wstring();
        options.arguments.clear();
        output.clear();
        require(process.start(L"", root / L"unused.jar", 2048, error, false, options), "BAT launch");
        waitFor(
            [&] {
                output += process.poll();
                return output.find(L"Done (") != std::wstring::npos;
            },
            "BAT server output captured");
        require(read(root / L"live/args.txt").find("batch argument") != std::string::npos,
                "BAT quoting preserves arguments");
        process.stop();
        waitFor(
            [&] {
                process.poll();
                return !process.running();
            },
            "BAT shutdown completes");
        auto source = root / L"source";
        fs::create_directories(source / L"world/region");
        std::ofstream(source / L"world/region/test.mca") << "world-original";
        auto deep =
            source / std::wstring(100, L'a') / std::wstring(100, L'b') / (std::wstring(64, L'c') + L".txt");
        fs::create_directories(extendedPath(deep.parent_path()));
        std::ofstream(extendedPath(deep)) << "long-path-data";
        BackupJob backup;
        auto backups = root / L"backups";
        require(backup.start(source, backups, error, true, 2), "ZIP backup starts");
        auto backupResult = resultOf(backup);
        if (!backupResult.starts_with(L"백업 완료")) {
            int n = WideCharToMultiByte(CP_UTF8, 0, backupResult.data(),
                                        static_cast<int>(backupResult.size()), nullptr, 0, nullptr, nullptr);
            std::string bytes(n, 0);
            WideCharToMultiByte(CP_UTF8, 0, backupResult.data(), static_cast<int>(backupResult.size()),
                                bytes.data(), n, nullptr, nullptr);
            std::cerr << bytes << std::endl;
        }
        require(backupResult.starts_with(L"백업 완료"), "ZIP backup succeeds");
        std::vector<fs::path> archives;
        for (const auto &e : fs::directory_iterator(backups))
            if (e.path().extension() == L".zip")
                archives.push_back(e.path());
        require(archives.size() == 1, "completed ZIP published atomically");
        auto live = root / L"restore target";
        fs::create_directory(live);
        std::ofstream(live / L"old.txt") << "preserve-me";
        require(backup.restore(extendedPath(archives.front()), live, error), "ZIP restore starts");
        auto restored = resultOf(backup);
        if (!restored.starts_with(L"복원 완료"))
            std::wcerr << restored << '\n';
        require(restored.starts_with(L"복원 완료"), "ZIP restore succeeds");
        require(read(live / L"world/region/test.mca") == "world-original", "restored world data matches");
        require(read(extendedPath(live / deep.lexically_relative(source))) == "long-path-data",
                "long nested paths survive ZIP backup and restore");
        bool preserved = false;
        for (const auto &e : fs::directory_iterator(root))
            if (e.path().filename().wstring().starts_with(L"restore target.before-restore-"))
                preserved = read(e.path() / L"old.txt") == "preserve-me";
        require(preserved, "pre-restore server preserved");
        std::ofstream(backups / L"unrelated.txt") << "keep";
        for (int i = 0; i < 3; ++i) {
            Sleep(25);
            require(backup.start(source, backups, error, true, 2), "retained ZIP backup starts");
            require(resultOf(backup).starts_with(L"백업 완료"), "retained backup succeeds");
        }
        size_t count = 0;
        for (const auto &e : fs::directory_iterator(backups))
            if (e.path().extension() == L".zip")
                ++count;
        require(count == 2 && fs::exists(backups / L"unrelated.txt"),
                "retention prunes only owned completed snapshots");
        require(backup.start(source, root / L"folder-backups", error, false, 0), "folder backup starts");
        require(resultOf(backup).starts_with(L"백업 완료"), "folder backup succeeds");
        require(!backup.start(source, source / L"nested", error),
                "backup destination inside server rejected");
        auto evil = root / L"unsafe.zip";
        auto ps = L"$ErrorActionPreference='Stop'; Add-Type -AssemblyName System.IO.Compression, "
                  L"System.IO.Compression.FileSystem; "
                  L"$z=[IO.Compression.ZipFile]::Open(" +
                  psLiteral(evil.wstring()) +
                  L",[IO.Compression.ZipArchiveMode]::Create); $e=$z.CreateEntry('../escape.txt'); "
                  L"$w=New-Object IO.StreamWriter($e.Open());$w.Write('bad');$w.Dispose();$z.Dispose()";
        require(runPowerShell(ps), "unsafe archive fixture created");
        require(backup.restore(evil, live, error), "unsafe archive validation starts");
        require(resultOf(backup).starts_with(L"복원 실패") && !fs::exists(root / L"escape.txt") &&
                    read(live / L"world/region/test.mca") == "world-original",
                "ZIP traversal rejected and live server preserved");
        RemotePanel panel;
        require(panel.start(0, users, error), "loopback panel starts");
        unsigned port = panel.port();
        auto base = " HTTP/1.1\r\nHost: 127.0.0.1:" + std::to_string(port) + "\r\n";
        require(http(port, "GET /" + base + "\r\n").find("200 Response") != std::string::npos,
                "web panel served");
        require(http(port, "GET /api/servers" + base + "\r\n").find("401 Response") != std::string::npos,
                "unauthenticated API denied");
        require(http(port, "POST /api/servers/x/start" + base + "Authorization: Bearer " + users[1].token +
                               "\r\nContent-Length: 0\r\n\r\n")
                        .find("403 Response") != std::string::npos,
                "viewer cannot mutate server");
        require(http(port, "GET / HTTP/1.1\r\nHost: attacker.example\r\n\r\n").find("400 Response") !=
                    std::string::npos,
                "foreign Host rejected");
        auto pending = std::async(std::launch::async, [&] {
            return http(port,
                        "GET /api/servers" + base + "Authorization: Bearer " + users[0].token + "\r\n\r\n");
        });
        waitFor(
            [&] {
                auto requests = panel.pending();
                if (requests.empty())
                    return false;
                require(requests.front()->role == "admin" && requests.front()->user == "alice",
                        "authenticated identity reaches UI queue");
                requests.front()->result.set_value("{\"servers\":[]}");
                return true;
            },
            "API dispatched to UI queue");
        require(pending.get().find("{\"servers\":[]}") != std::string::npos, "API response returned");
        panel.stop();
        require(!panel.running(), "remote panel stops cleanly");
        require(removePathTree(root), "long-path test artifacts removed");
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << "\nArtifacts: " << root << std::endl;
        return 1;
    }
}

#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "ConsoleColors.h"
#include "SettingsSnapshot.h"
#include "PathUtils.h"
#include "ServerProperties.h"
#include "PluginCatalog.h"
#include "JarArchive.h"
#include "Localization.h"
#include "PathValidation.h"
#include "GroupRegistry.h"
#include "PlayerLog.h"
namespace fs = std::filesystem;
void require(bool value, const char *name) {
    if (!value)
        throw std::runtime_error(name);
    std::cout << "PASS: " << name << '\n';
}
uint32_t crc(const std::string &data) {
    uint32_t n = ~0u;
    for (unsigned char c : data) {
        n ^= c;
        for (int i = 0; i < 8; ++i)
            n = (n >> 1) ^ ((0u - (n & 1)) & 0xedb88320u);
    }
    return ~n;
}
void jar(const fs::path &path, const std::string &filename, const std::string &content, bool broken = false) {
    std::ofstream out(path, std::ios::binary);
    auto w = [&](uint32_t n, int bytes) {
        for (int i = 0; i < bytes; ++i)
            out.put(static_cast<char>(n >> (i * 8)));
    };
    auto checksum = crc(content) + (broken ? 1 : 0);
    uint32_t compressed = static_cast<uint32_t>(content.size() + 5);
    w(0x04034b50, 4);
    w(20, 2);
    w(0, 2);
    w(8, 2);
    w(0, 4);
    w(checksum, 4);
    w(compressed, 4);
    w(static_cast<uint32_t>(content.size()), 4);
    w(static_cast<uint32_t>(filename.size()), 2);
    w(0, 2);
    out << filename;
    w(1, 1);
    w(static_cast<uint32_t>(content.size()), 2);
    w(~static_cast<uint32_t>(content.size()), 2);
    out << content;
    uint32_t offset = static_cast<uint32_t>(out.tellp());
    w(0x02014b50, 4);
    w(20, 2);
    w(20, 2);
    w(0, 2);
    w(8, 2);
    w(0, 4);
    w(checksum, 4);
    w(compressed, 4);
    w(static_cast<uint32_t>(content.size()), 4);
    w(static_cast<uint32_t>(filename.size()), 2);
    w(0, 2);
    w(0, 2);
    w(0, 2);
    w(0, 2);
    w(0, 4);
    w(0, 4);
    out << filename;
    uint32_t length = static_cast<uint32_t>(out.tellp()) - offset;
    w(0x06054b50, 4);
    w(0, 2);
    w(0, 2);
    w(1, 2);
    w(1, 2);
    w(length, 4);
    w(offset, 4);
    w(0, 2);
}
PluginCatalog::Result scan(PluginCatalog &catalog, const fs::path &directory, const fs::path &cache) {
    auto ticket = catalog.request(directory, cache);
    auto until = GetTickCount64() + 10000;
    while (GetTickCount64() < until) {
        if (auto result = catalog.poll(); result && result->ticket == ticket)
            return std::move(*result);
        Sleep(10);
    }
    throw std::runtime_error("plugin scan timeout");
}
int main() {
    fs::path root = fs::temp_directory_path() / (L"wrapper-core-" + std::to_wstring(GetCurrentProcessId()));
    fs::create_directories(root);
    try {
        SetEnvironmentVariableW(L"WRAPPER_CORE_ROOT", root.c_str());
        require(normalizePathInput(L" \"%WRAPPER_CORE_ROOT%\\server.jar\" ", root) ==
                    (root / L"server.jar").wstring(),
                "quoted environment paths");
        require(normalizePathInput(L"java.exe", root, true) == L"java.exe", "Java PATH command preserved");
        require(pathsOverlap(root / L"a", root / L"a" / L"world") &&
                    !pathsOverlap(root / L"a", root / L"abc"),
                "path boundaries and nested backup locks");
        auto ini = root / L"settings.ini";
        {
            SettingsSnapshot s(ini);
            s.write(L"Workspace", L"Font", L"나눔고딕");
            require(s.commit(), "Unicode settings commit");
        }
        HANDLE lock = CreateFileW(ini.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        {
            SettingsSnapshot s(ini);
            s.write(L"Workspace", L"Font", L"changed");
            require(!s.commit(), "failed atomic replacement reported");
        }
        CloseHandle(lock);
        wchar_t font[80]{};
        GetPrivateProfileStringW(L"Workspace", L"Font", L"", font, 80, ini.c_str());
        require(std::wstring(font) == L"나눔고딕", "old settings survive failed replacement");
        auto properties = root / L"server.properties";
        std::ofstream(properties) << "# keep comment\nserver-port=25565\ncustom-plugin-value=unchanged\n";
        ServerProperties props;
        require(props.load(properties) &&
                    props.save(properties, {{"server-port", "25566"}, {"pvp", "false"}}),
                "properties save");
        props.load(properties);
        require(props.get("server-port", "") == "25566" &&
                    props.get("custom-plugin-value", "") == "unchanged",
                "unknown properties preserved");
        ConsoleColors colors;
        auto a = colors.decode(L"plain\x1b[38;2;12;");
        auto b = colors.decode(L"34;56mRGB\x1b[0mnormal");
        require(a.size() == 1 && a[0].text == L"plain" && b.size() == 2 && b[0].text == L"RGB" &&
                    b[0].color == RGB(12, 34, 56) && b[1].color == ConsoleDefaultColor,
                "fragmented ANSI truecolor and reset");
        auto c = colors.decode(L"\u00a7x\u00a71\u00a72\u00a73\u00a74\u00a75\u00a76hex");
        require(c.size() == 1 && c[0].color == RGB(0x12, 0x34, 0x56), "Minecraft hex colors");
        auto d = colors.decode(L"\x1b[38;5;196mred");
        require(d.size() == 1 && d[0].color == RGB(255, 0, 0), "ANSI 256 colors");
        require(localize(L"Dashboard", false) == L"대시보드" &&
                    localize(L"설정 저장", true) == L"Save settings",
                "Korean and English catalog");
        fs::create_directory(root / L"plugins");
        auto plugin = root / L"plugins" / L"test.jar";
        jar(plugin, "plugin.yml", "name: ActualName\nversion: '1.0'\n");
        require(JarArchive(plugin).read("plugin.yml").find("ActualName") != std::string::npos,
                "deflated JAR metadata and CRC");
        PluginCatalog catalog;
        auto first = scan(catalog, root / L"plugins", root / L"cache");
        require(first.entries.size() == 1 && first.entries[0].name == L"ActualName" &&
                    first.entries[0].version == L"1.0" && !first.entries[0].cached,
                "actual plugin name and version");
        auto second = scan(catalog, root / L"plugins", root / L"cache");
        require(second.entries[0].cached, "SHA-256 text cache reuse");
        auto originalTime = fs::last_write_time(plugin);
        auto originalSize = fs::file_size(plugin);
        jar(plugin, "plugin.yml", "name: ActualName\nversion: '2.0'\n");
        fs::last_write_time(plugin, originalTime);
        auto third = scan(catalog, root / L"plugins", root / L"cache");
        require(fs::file_size(plugin) == originalSize && third.entries[0].version == L"2.0" &&
                    !third.entries[0].cached,
                "content change invalidates cache despite identical size and timestamp");
        jar(root / L"bad.jar", "plugin.yml", "name: Broken", true);
        require(JarArchive(root / L"bad.jar").read("plugin.yml").empty(), "corrupt metadata rejected");
        jar(root / L"velocity.jar", "META-INF/MANIFEST.MF",
            "Main-Class: com.velocitypowered.proxy.Velocity\n");
        require(JarArchive(root / L"velocity.jar").serverType() == "Velocity", "proxy server detection");
        jar(root / L"paper.jar", "META-INF/MANIFEST.MF", "Main-Class: io.papermc.paperclip.Main\n");
        require(JarArchive(root / L"paper.jar").serverType() == "Paper family (distribution unknown)",
                "ambiguous Paperclip distribution is explicit");
        std::wstring resolved, validationError;
        wchar_t module[32768]{};
        GetModuleFileNameW(nullptr, module, 32768);
        require(validateJavaPath(module, resolved, validationError), "PE executable path validation");
        std::ofstream(root / L"fake.exe") << "not an executable";
        require(!validateJavaPath((root / L"fake.exe").wstring(), resolved, validationError),
                "fake executable rejected");
        require(validateServerJar(root / L"velocity.jar", validationError), "runnable JAR manifest accepted");
        require(!validateServerJar(root / L"bad.jar", validationError), "invalid server JAR rejected");
        require(!validateServerJar(root / L"missing.jar", validationError), "missing server JAR rejected");
        GroupRegistry groups;
        require(groups.add(L"  Lobby  ") && groups.canonical(L"lobby") == L"Lobby",
                "group normalization and lookup");
        require(!groups.add(L"LOBBY") && !groups.add(L" "), "duplicate and empty groups rejected");
        require(groups.rename(L"Lobby", L"Archive") && groups.remove(L"Archive"),
                "group rename and deletion");
        require(!groups.rename(L"기본 그룹", L"Other") && !groups.remove(L"기본 그룹"),
                "default group protected");
        require(!parsePlayerLog(L"ordinary log") &&
                    parsePlayerLog(L"[12:00 INFO]: Notch joined the game").has_value(),
                "player parser fast path preserves join events");
        require(parsePlayerLog(L"Notch join\x1b[32med the game").has_value(),
                "ANSI codes inside join marker remain supported");
        require(!groups.add(std::wstring(49, L'x')) && !groups.add(L"bad\nname"), "group name bounds");
        fs::remove_all(root);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << "\nArtifacts: " << root << '\n';
        return 1;
    }
}

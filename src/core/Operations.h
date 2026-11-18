#pragma once
#include <windows.h>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>
struct LaunchOptions {
    int mode = 0; // 0 JAR, 1 Java arguments/@args, 2 BAT, 3 native executable
    unsigned minimumMemory = 512;
    std::wstring jvm, arguments, target, directory;
};
bool removePathTree(const std::filesystem::path &path);
std::filesystem::path extendedPath(const std::filesystem::path &path);
std::wstring quoteArgument(const std::wstring &value);
std::wstring buildLaunchCommand(const std::wstring &java, const std::filesystem::path &jar, unsigned memory,
                                bool nogui, const LaunchOptions &options);
std::wstring newToken();
long long wallNow();
long long nextDaily(int minute, long long now = 0);
std::wstring localTimestamp();
struct FormField {
    std::wstring label, value;
    std::vector<std::wstring> choices{};
};
struct FormPalette {
    COLORREF background = RGB(26, 26, 26), input = RGB(33, 33, 33), border = RGB(58, 58, 56),
             foreground = RGB(235, 235, 231), muted = RGB(165, 165, 158), accent = RGB(190, 207, 119);
    bool light = false;
};
bool editForm(HWND owner, HFONT font, const std::wstring &title, const std::wstring &help,
              std::vector<FormField> &fields,
              std::function<std::wstring(const std::vector<FormField> &)> validate = {},
              FormPalette palette = {});
bool runPowerShell(const std::wstring &script);
std::wstring psLiteral(const std::wstring &value);

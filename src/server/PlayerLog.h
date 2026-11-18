#pragma once
#include <optional>
#include <regex>
#include <string>
struct PlayerLogEvent {
    std::wstring name;
    bool joined;
};
inline std::optional<PlayerLogEvent> parsePlayerLog(const std::wstring &line) {
    auto candidate = [](const std::wstring &value) {
        return value.find(L"logged in with entity id") != std::wstring::npos ||
               value.find(L"lost connection:") != std::wstring::npos ||
               value.find(L"joined the game") != std::wstring::npos ||
               value.find(L"left the game") != std::wstring::npos;
    };
    bool colored = line.find(L'\x1b') != std::wstring::npos;
    if (!colored && !candidate(line))
        return std::nullopt;
    static const std::wregex ansi(L"\x1b\\[[0-?]*[ -/]*[@-~]");
    auto message = colored ? std::regex_replace(line, ansi, L"") : line;
    if (colored && !candidate(message))
        return std::nullopt;
    if (!message.empty() && message.back() == L'\r')
        message.pop_back();
    if (!message.empty() && message.front() == L'[') {
        auto marker = message.find(L"]: ");
        if (marker != std::wstring::npos)
            message.erase(0, marker + 3);
    }
    static const std::wregex login(
        LR"(^([A-Za-z0-9_]{1,16})\[/[^\r\n]*\] logged in with entity id -?[0-9]+ at .+$)");
    static const std::wregex quit(LR"(^([A-Za-z0-9_]{1,16}) lost connection:.*$)");
    static const std::wregex joinLeave(LR"(^([A-Za-z0-9_]{1,16}) (joined|left) the game$)");
    std::wsmatch match;
    if (std::regex_match(message, match, login))
        return PlayerLogEvent{match[1].str(), true};
    if (std::regex_match(message, match, quit))
        return PlayerLogEvent{match[1].str(), false};
    if (std::regex_match(message, match, joinLeave))
        return PlayerLogEvent{match[1].str(), match[2].str() == L"joined"};
    return std::nullopt;
}

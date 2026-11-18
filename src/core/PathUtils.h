#pragma once
#include <windows.h>
#include <filesystem>
#include <string>
#include <vector>
inline std::wstring normalizePathInput(std::wstring value, const std::filesystem::path &base,
                                       bool allowCommand = false) {
    auto first = value.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos)
        return {};
    value = value.substr(first, value.find_last_not_of(L" \t\r\n") - first + 1);
    if (value.size() >= 2 && value.front() == L'"' && value.back() == L'"')
        value = value.substr(1, value.size() - 2);
    DWORD size = ExpandEnvironmentStringsW(value.c_str(), nullptr, 0);
    if (size && size <= 32768) {
        std::wstring expanded(size, 0);
        if (ExpandEnvironmentStringsW(value.c_str(), expanded.data(), size) == size) {
            expanded.resize(size - 1);
            value = std::move(expanded);
        }
    }
    std::filesystem::path path(value);
    if (allowCommand && !path.has_parent_path())
        return value;
    if (path.is_relative())
        path = base / path;
    return path.lexically_normal().wstring();
}
inline bool resolvedPathsOverlap(const std::filesystem::path &left, const std::filesystem::path &right) {
    if (left.empty() || right.empty())
        return false;
    // A trailing separator yields an empty last element ("C:\\S\\a\\" vs "C:\\S\\a\\b"); skip it.
    auto parts = [](const std::filesystem::path &value) {
        std::vector<std::wstring> result;
        for (const auto &part : value)
            if (!part.empty())
                result.push_back(part.wstring());
        return result;
    };
    auto a = parts(left), b = parts(right);
    size_t i = 0;
    while (i < a.size() && i < b.size() && _wcsicmp(a[i].c_str(), b[i].c_str()) == 0)
        ++i;
    return i == a.size() || i == b.size();
}
inline bool pathsOverlap(const std::filesystem::path &a, const std::filesystem::path &b) {
    if (a.empty() || b.empty())
        return false;
    std::error_code ec;
    auto left = std::filesystem::weakly_canonical(a, ec);
    if (ec)
        return true;
    auto right = std::filesystem::weakly_canonical(b, ec);
    if (ec)
        return true;
    return resolvedPathsOverlap(left, right);
}

#include "ServerProperties.h"
#include <windows.h>
#include <fstream>
#include <set>
namespace {
std::string keyOf(const std::string &line) {
    auto start = line.find_first_not_of(" \t\r");
    if (start == std::string::npos || line[start] == '#' || line[start] == '!')
        return {};
    return line.substr(start, line.find_first_of("=: \t", start) - start);
}
} // namespace
bool ServerProperties::load(const std::filesystem::path &path) {
    lines_.clear();
    std::error_code ec;
    if (!std::filesystem::exists(path, ec))
        return !ec;
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        lines_.push_back(line);
    }
    return !in.bad();
}
std::string ServerProperties::get(const std::string &key, const std::string &fallback) const {
    std::string value = fallback;
    for (const auto &line : lines_)
        if (keyOf(line) == key) {
            auto pos = line.find_first_of("=:");
            if (pos == std::string::npos)
                pos = line.find_first_of(" \t");
            if (pos != std::string::npos) {
                pos = line.find_first_not_of(" \t", pos + 1);
                value = pos == std::string::npos ? "" : line.substr(pos);
            }
        }
    return value;
}
bool ServerProperties::save(const std::filesystem::path &path,
                            const std::map<std::string, std::string> &changes) const {
    wchar_t temporary[MAX_PATH]{};
    if (!GetTempFileNameW(path.parent_path().c_str(), L"prp", 0, temporary))
        return false;
    std::ofstream out(temporary, std::ios::binary);
    std::set<std::string> written;
    bool continuing = false;
    for (const auto &line : lines_) {
        auto found = changes.find(keyOf(line));
        // Preserve unknown properties, comments and their continuation lines verbatim.
        if (continuing || found == changes.end())
            out << line << "\r\n";
        else if (written.insert(found->first).second) {
            if (!line.empty() && line.back() == '\\') {
                out.close();
                DeleteFileW(temporary);
                return false;
            }
            out << found->first << '=' << found->second << "\r\n";
        }
        size_t slashes = 0;
        for (auto i = line.rbegin(); i != line.rend() && *i == '\\'; ++i)
            ++slashes;
        continuing = (slashes % 2) != 0;
    }
    for (const auto &[key, value] : changes)
        if (!written.count(key))
            out << key << '=' << value << "\r\n";
    out.close();
    bool ok = static_cast<bool>(out);
    HANDLE file =
        CreateFileW(temporary, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        ok = false;
    else {
        ok = FlushFileBuffers(file) && ok;
        CloseHandle(file);
    }
    if (ok)
        ok =
            MoveFileExW(temporary, path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    if (!ok)
        DeleteFileW(temporary);
    return ok;
}

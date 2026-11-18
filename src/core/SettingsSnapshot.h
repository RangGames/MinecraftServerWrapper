#pragma once
#include <windows.h>
#include <filesystem>
#include <string>
#include <cstddef>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>
// Build a complete Unicode INI in memory, write it next to the old file in one call, flush it,
// then replace the old file atomically. Values are read back with GetPrivateProfileStringW.
class SettingsSnapshot {
    struct Section {
        std::wstring name;
        std::vector<std::pair<std::wstring, std::wstring>> entries;
        std::unordered_map<std::wstring, size_t> index;
    };
    std::filesystem::path target_;
    std::vector<Section> sections_;
    std::unordered_map<std::wstring, size_t> sectionIndex_;

    static std::wstring folded(const std::wstring &value) {
        std::wstring result(value);
        CharUpperBuffW(result.data(), static_cast<DWORD>(result.size()));
        return result;
    }
    // GetPrivateProfileString strips one pair of enclosing quotes and trims surrounding blanks.
    // Quote such values so they round-trip unchanged; ordinary values stay unquoted.
    static std::wstring encoded(const std::wstring &value) {
        if (value.empty())
            return value;
        auto special = [](wchar_t c) { return c == L'"' || c == L'\'' || c == L' ' || c == L'\t'; };
        return special(value.front()) || special(value.back()) ? L"\"" + value + L"\"" : value;
    }

  public:
    explicit SettingsSnapshot(const std::filesystem::path &target) : target_(target) {}
    void write(const std::wstring &section, const std::wstring &key, const std::wstring &value) {
        auto sectionKey = folded(section);
        auto found = sectionIndex_.find(sectionKey);
        if (found == sectionIndex_.end()) {
            found = sectionIndex_.emplace(sectionKey, sections_.size()).first;
            sections_.push_back({section, {}, {}});
        }
        auto &entries = sections_[found->second];
        auto keyName = folded(key);
        auto existing = entries.index.find(keyName);
        // Line breaks would split the entry; the profile API cannot represent them either.
        std::wstring clean(value);
        for (auto &c : clean)
            if (c == L'\r' || c == L'\n')
                c = L' ';
        if (existing != entries.index.end())
            entries.entries[existing->second].second = encoded(clean);
        else {
            entries.index.emplace(keyName, entries.entries.size());
            entries.entries.emplace_back(key, encoded(clean));
        }
    }
    bool commit() {
        std::wstring body = L"\xFEFF";
        for (const auto &section : sections_) {
            body += L"[" + section.name + L"]\r\n";
            for (const auto &[key, value] : section.entries)
                body += key + L"=" + value + L"\r\n";
        }
        wchar_t name[MAX_PATH]{};
        if (!GetTempFileNameW(target_.parent_path().c_str(), L"ini", 0, name))
            return false;
        HANDLE file = CreateFileW(name, GENERIC_WRITE | DELETE, 0, nullptr, TRUNCATE_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            DeleteFileW(name);
            return false;
        }
        DWORD bytes = static_cast<DWORD>(body.size() * sizeof(wchar_t)), written = 0;
        bool ok = WriteFile(file, body.data(), bytes, &written, nullptr) && written == bytes &&
                  FlushFileBuffers(file);
        // A plain replace fails while anyone has the INI open (antivirus, editors, other readers), even with
        // FILE_SHARE_DELETE. POSIX-semantics rename (Windows 10 1809+) replaces it anyway.
        bool replaced = ok && posixReplace(file, target_);
        CloseHandle(file);
        // Fallback for older systems or readers that deny delete sharing: retry the replace briefly.
        for (int attempt = 0; ok && !replaced && attempt < 20; ++attempt) {
            if (MoveFileExW(name, target_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                replaced = true;
                break;
            }
            DWORD error = GetLastError();
            if (error != ERROR_ACCESS_DENIED && error != ERROR_SHARING_VIOLATION)
                break;
            Sleep(25);
        }
        if (!replaced)
            DeleteFileW(name);
        return replaced;
    }

  private:
    static bool posixReplace(HANDLE file, const std::filesystem::path &target) {
        // FILE_RENAME_INFO with the Flags member (FileRenameInfoEx); laid out by hand so it does not depend
        // on the SDK's _WIN32_WINNT setting.
        struct Header {
            DWORD flags;
            HANDLE root;
            DWORD length;
        };
        const std::wstring &path = target.native();
        std::vector<unsigned char> buffer(sizeof(Header) + (path.size() + 1) * sizeof(wchar_t));
        auto *info = reinterpret_cast<Header *>(buffer.data());
        info->flags = 0x1 | 0x2; // FILE_RENAME_FLAG_REPLACE_IF_EXISTS | FILE_RENAME_FLAG_POSIX_SEMANTICS
        info->root = nullptr;
        info->length = static_cast<DWORD>(path.size() * sizeof(wchar_t));
        memcpy(buffer.data() + offsetof(FILE_RENAME_INFO, FileName), path.c_str(),
               (path.size() + 1) * sizeof(wchar_t));
        return SetFileInformationByHandle(file, static_cast<FILE_INFO_BY_HANDLE_CLASS>(22), // FileRenameInfoEx
                                          buffer.data(), static_cast<DWORD>(buffer.size())) != FALSE;
    }
};

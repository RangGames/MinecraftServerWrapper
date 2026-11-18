#pragma once
#include <windows.h>
#include <filesystem>
#include <string>
// Build a complete Unicode INI next to the old file, then replace it atomically.
class SettingsSnapshot {
    std::filesystem::path target_, temporary_;
    bool ok_ = false;

  public:
    explicit SettingsSnapshot(const std::filesystem::path &target) : target_(target) {
        wchar_t name[MAX_PATH]{};
        if (!GetTempFileNameW(target.parent_path().c_str(), L"ini", 0, name))
            return;
        temporary_ = name;
        HANDLE file =
            CreateFileW(name, GENERIC_WRITE, 0, nullptr, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return;
        DWORD written = 0;
        ok_ = WriteFile(file, "\xff\xfe", 2, &written, nullptr) && written == 2;
        CloseHandle(file);
    }
    ~SettingsSnapshot() {
        if (!temporary_.empty())
            DeleteFileW(temporary_.c_str());
    }
    void write(const std::wstring &section, const std::wstring &key, const std::wstring &value) {
        if (ok_)
            ok_ = WritePrivateProfileStringW(section.c_str(), key.c_str(), value.c_str(),
                                             temporary_.c_str()) != FALSE;
    }
    bool commit() {
        if (!ok_)
            return false;
        WritePrivateProfileStringW(nullptr, nullptr, nullptr, temporary_.c_str());
        HANDLE file = CreateFileW(temporary_.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;
        bool flushed = FlushFileBuffers(file) != FALSE;
        CloseHandle(file);
        if (!flushed || !MoveFileExW(temporary_.c_str(), target_.c_str(),
                                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            return false;
        temporary_.clear();
        return true;
    }
};

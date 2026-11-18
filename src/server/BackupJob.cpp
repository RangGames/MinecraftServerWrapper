#include "BackupJob.h"
#include "Operations.h"
#include <windows.h>
#include <algorithm>
#include <cwctype>
#include <fstream>
#include <vector>
namespace fs = std::filesystem;
namespace {
fs::path nativeCanonical(const fs::path &path, std::error_code &error) {
    auto full = extendedPath(path);
    HANDLE file = CreateFileW(full.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = std::error_code(GetLastError(), std::system_category());
        return {};
    }
    std::wstring value(32768, 0);
    DWORD size =
        GetFinalPathNameByHandleW(file, value.data(), static_cast<DWORD>(value.size()), FILE_NAME_NORMALIZED);
    DWORD code = GetLastError();
    CloseHandle(file);
    if (!size || size >= value.size()) {
        error = std::error_code(code, std::system_category());
        return {};
    }
    error.clear();
    value.resize(size);
    return value;
}
struct NativeEntry {
    fs::path path;
    DWORD attributes;
};
std::vector<NativeEntry> nativeEntries(const fs::path &directory) {
    WIN32_FIND_DATAW data{};
    auto pattern = directory.wstring() + L"\\*";
    HANDLE find = FindFirstFileW(pattern.c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
            return {};
        throw fs::filesystem_error("directory enumeration", directory,
                                   std::error_code(GetLastError(), std::system_category()));
    }
    std::vector<NativeEntry> result;
    try {
        do {
            if (wcscmp(data.cFileName, L".") && wcscmp(data.cFileName, L".."))
                result.push_back({directory / data.cFileName, data.dwFileAttributes});
        } while (FindNextFileW(find, &data));
        auto error = GetLastError();
        if (error != ERROR_NO_MORE_FILES)
            throw fs::filesystem_error("directory enumeration", directory,
                                       std::error_code(error, std::system_category()));
    } catch (...) {
        FindClose(find);
        throw;
    }
    FindClose(find);
    return result;
}
void copyTree(const fs::path &source, const fs::path &destination, std::stop_token stop,
              std::atomic<unsigned long long> &files, const fs::path &excluded, bool skipLinks) {
    std::vector<std::pair<fs::path, fs::path>> pending{{source, destination}};
    auto excludedPath = excluded.empty() ? std::wstring{} : extendedPath(excluded).wstring();
    while (!pending.empty()) {
        auto [input, output] = std::move(pending.back());
        pending.pop_back();
        for (const auto &entry : nativeEntries(input)) {
            if (stop.stop_requested())
                throw std::runtime_error("copy canceled");
            if (entry.attributes & FILE_ATTRIBUTE_REPARSE_POINT) {
                if (skipLinks)
                    continue;
                throw std::runtime_error("link in backup");
            }
            auto target = output / entry.path.filename();
            if (entry.attributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (!CreateDirectoryW(target.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
                    throw fs::filesystem_error("create backup directory", target,
                                               std::error_code(GetLastError(), std::system_category()));
                pending.emplace_back(entry.path, target);
            } else {
                if (!excludedPath.empty() && _wcsicmp(entry.path.c_str(), excludedPath.c_str()) == 0)
                    continue;
                if (!CopyFileW(entry.path.c_str(), target.c_str(), TRUE))
                    throw fs::filesystem_error("copy backup file", entry.path, target,
                                               std::error_code(GetLastError(), std::system_category()));
                ++files;
            }
        }
    }
}
} // namespace
BackupJob::~BackupJob() {
    if (worker_.joinable()) {
        worker_.request_stop();
        worker_.join();
    }
}
std::optional<std::wstring> BackupJob::takeResult() {
    std::lock_guard lock(mutex_);
    auto result = std::move(result_);
    result_.reset();
    return result;
}
bool BackupJob::start(const fs::path &source, const fs::path &destinationRoot, std::wstring &error, bool zip,
                      unsigned keep) {
    if (busy_) {
        error = L"이미 백업 중입니다.";
        return false;
    }
    if (worker_.joinable())
        worker_.join();
    std::error_code ec;
    auto src = fs::canonical(source, ec);
    if (ec || !fs::is_directory(src, ec)) {
        error = L"서버 폴더를 찾을 수 없습니다.";
        return false;
    }
    auto dest = fs::weakly_canonical(destinationRoot, ec);
    if (ec || dest.empty()) {
        error = L"백업 경로를 확인하세요.";
        return false;
    }
    auto sourcePart = src.begin(), destinationPart = dest.begin();
    while (sourcePart != src.end() && destinationPart != dest.end() &&
           _wcsicmp(sourcePart->c_str(), destinationPart->c_str()) == 0) {
        ++sourcePart;
        ++destinationPart;
    }
    if (sourcePart == src.end()) {
        error = L"백업 위치는 서버 폴더 밖으로 지정하세요.";
        return false;
    }
    src = extendedPath(src);
    dest = extendedPath(dest);
    SYSTEMTIME time{};
    GetLocalTime(&time);
    wchar_t name[80];
    swprintf_s(name, L"%04u-%02u-%02u_%02u-%02u-%02u-%03u", time.wYear, time.wMonth, time.wDay, time.wHour,
               time.wMinute, time.wSecond, time.wMilliseconds);
    fs::path target = dest / (L"snapshot-" + std::wstring(name) + (zip ? L".zip" : L""));
    fs::path partial = dest / (L"snapshot-" + std::wstring(name) + L".partial");
    std::vector<wchar_t> module(32768);
    GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
    auto wrapper = fs::weakly_canonical(fs::path(module.data()), ec);
    files_ = 0;
    busy_ = true;
    {
        std::lock_guard lock(mutex_);
        result_.reset();
    }
    try {
        worker_ = std::jthread([this, src, target, partial, wrapper, zip, keep](std::stop_token stop) {
            std::wstring result;
            try {
                fs::create_directories(partial.parent_path());
                if (!fs::create_directory(partial))
                    throw std::runtime_error("Backup name already exists");
                copyTree(src, partial, stop, files_, wrapper, true);
                {
                    std::ofstream marker(partial / L".wrapper-backup", std::ios::binary);
                    marker << "MinecraftServerWrapper backup v1";
                    marker.close();
                    if (!marker)
                        throw std::runtime_error("marker failed");
                }
                if (zip) {
                    auto archive = target.wstring() + L".partial";
                    auto script =
                        L"$ErrorActionPreference='Stop'; try { Add-Type -AssemblyName "
                        L"System.IO.Compression.FileSystem; [IO.Compression.ZipFile]::CreateFromDirectory(" +
                        psLiteral(partial.wstring()) + L"," + psLiteral(archive) +
                        L"); exit 0 } catch { exit 1 }";
                    if (!runPowerShell(script))
                        throw std::runtime_error("compression failed");
                    fs::rename(archive, target);
                    removePathTree(partial);
                } else
                    fs::rename(partial, target);
                // Only prune completed snapshots created in this profile's dedicated backup folder.
                if (keep) {
                    std::vector<fs::path> snapshots;
                    for (const auto &entry : nativeEntries(target.parent_path())) {
                        auto filename = entry.path.filename().wstring();
                        if (!filename.starts_with(L"snapshot-") ||
                            filename.find(L".partial") != std::wstring::npos ||
                            (entry.attributes & FILE_ATTRIBUTE_REPARSE_POINT))
                            continue;
                        if ((!(entry.attributes & FILE_ATTRIBUTE_DIRECTORY) &&
                             entry.path.extension() == L".zip") ||
                            ((entry.attributes & FILE_ATTRIBUTE_DIRECTORY) &&
                             fs::is_regular_file(entry.path / L".wrapper-backup")))
                            snapshots.push_back(entry.path);
                    }
                    std::sort(snapshots.rbegin(), snapshots.rend());
                    for (size_t i = keep; i < snapshots.size(); ++i) {
                        bool removed = removePathTree(snapshots[i]);
                        if (!removed)
                            result += L" 보관 정리 실패: " + snapshots[i].filename().wstring();
                    }
                }
                result = L"백업 완료: " + target.wstring() + result;
            } catch (const std::exception &failure) {
                auto detail = std::string(failure.what());
                std::wstring reason(detail.begin(), detail.end());
                result = L"백업을 완료하지 못했습니다. 공간·권한을 확인하세요. 불완전한 파일: " +
                         partial.wstring() + L" (" + reason + L")";
            }
            {
                std::lock_guard lock(mutex_);
                result_ = std::move(result);
            }
            busy_ = false;
        });
    } catch (...) {
        busy_ = false;
        error = L"백업 작업을 시작하지 못했습니다.";
        return false;
    }
    return true;
}
bool BackupJob::restore(const fs::path &backup, const fs::path &destination, std::wstring &error) {
    if (busy_) {
        error = L"백업 작업이 진행 중입니다.";
        return false;
    }
    if (worker_.joinable())
        worker_.join();
    std::error_code ec;
    auto src = nativeCanonical(backup, ec);
    if (ec) {
        error = L"백업을 찾을 수 없습니다.";
        return false;
    }
    auto dst = nativeCanonical(destination, ec);
    if (ec || !fs::is_directory(dst)) {
        error = L"복원 대상 폴더를 확인하세요.";
        return false;
    }
    auto overlap = [](const fs::path &a, const fs::path &b) {
        auto x = a.begin(), y = b.begin();
        while (x != a.end() && y != b.end() && _wcsicmp(x->c_str(), y->c_str()) == 0) {
            ++x;
            ++y;
        }
        return x == a.end() || y == b.end();
    };
    wchar_t module[32768]{};
    GetModuleFileNameW(nullptr, module, 32768);
    if (overlap(src, dst) || overlap(dst, extendedPath(fs::path(module))) || fs::is_symlink(src)) {
        error = L"백업·서버·실행 중인 관리자 경로가 겹칩니다. 관리자를 서버 폴더 밖으로 옮기세요.";
        return false;
    }
    src = extendedPath(src);
    dst = extendedPath(dst);
    auto name = dst.filename().wstring();
    auto suffix = newToken().substr(0, 12);
    auto stage = dst.parent_path() / (name + L".restore-" + suffix + L".partial");
    auto previous = dst.parent_path() / (name + L".before-restore-" + suffix);
    busy_ = true;
    files_ = 0;
    {
        std::lock_guard lock(mutex_);
        result_.reset();
    }
    try {
        worker_ = std::jthread([this, src, dst, stage, previous](std::stop_token stop) {
            std::wstring result;
            try {
                fs::create_directory(stage);
                if (fs::is_regular_file(src) && src.extension() == L".zip") {
                    // Validate every archive entry before extracting anything. Stage outside the live server.
                    auto script =
                        L"$ErrorActionPreference='Stop'; try { Add-Type -AssemblyName "
                        L"System.IO.Compression.FileSystem; $z=[IO.Compression.ZipFile]::OpenRead(" +
                        psLiteral(src.wstring()) + L"); try { $root=[IO.Path]::GetFullPath(" +
                        psLiteral(stage.wstring()) +
                        L")+[IO.Path]::DirectorySeparatorChar; $size=0L; $count=0; $names=New-Object "
                        L"'Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase); "
                        L"foreach($e in $z.Entries){ $count++; $size+=$e.Length; if($count -gt 1000000 -or "
                        L"$size -gt 274877906944){throw 'Archive too large'}; "
                        L"$n=$e.FullName.Replace('/',[IO.Path]::DirectorySeparatorChar); "
                        L"if([IO.Path]::IsPathRooted($n) -or $n.Contains(':') -or "
                        L"$n.Split([IO.Path]::DirectorySeparatorChar) -contains '..' -or "
                        L"$n.Split([IO.Path]::DirectorySeparatorChar) -contains '.'){throw 'Unsafe entry'}; "
                        L"$p=[IO.Path]::GetFullPath([IO.Path]::Combine($root,$n)); "
                        L"if(!$p.StartsWith($root,[StringComparison]::OrdinalIgnoreCase) -or "
                        L"!$names.Add($p)){throw 'Unsafe path'}; if(($e.ExternalAttributes -band 0x400) -ne "
                        L"0 -or (($e.ExternalAttributes -shr 16) -band 0xF000) -eq 0xA000){throw 'Link "
                        L"entry'} } } finally {$z.Dispose()}; [IO.Compression.ZipFile]::ExtractToDirectory(" +
                        psLiteral(src.wstring()) + L"," + psLiteral(stage.wstring()) +
                        L"); exit 0 } catch {exit 1}";
                    if (!runPowerShell(script))
                        throw std::runtime_error("invalid zip");
                } else if (fs::is_directory(src)) {
                    copyTree(src, stage, stop, files_, {}, false);
                } else
                    throw std::runtime_error("unsupported backup");
                if (!fs::is_regular_file(stage / L".wrapper-backup"))
                    throw std::runtime_error("unmarked backup");
                if (stop.stop_requested())
                    throw std::runtime_error("canceled");
                fs::rename(dst, previous);
                try {
                    fs::rename(stage, dst);
                } catch (...) {
                    fs::rename(previous, dst);
                    throw;
                }
                result = L"복원 완료. 이전 서버 보존 위치: " + previous.wstring();
            } catch (...) {
                result = L"복원 실패. 원본 서버를 보존했습니다. 임시 위치: " + stage.wstring();
            }
            {
                std::lock_guard lock(mutex_);
                result_ = std::move(result);
            }
            busy_ = false;
        });
    } catch (...) {
        busy_ = false;
        error = L"복원 스레드를 시작하지 못했습니다.";
        return false;
    }
    return true;
}

#pragma once
#include <atomic>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
class BackupJob {
  public:
    ~BackupJob();
    bool start(const std::filesystem::path &source, const std::filesystem::path &destinationRoot,
               std::wstring &error, bool zip = true, unsigned keep = 10);
    bool restore(const std::filesystem::path &backup, const std::filesystem::path &destination,
                 std::wstring &error);
    bool busy() const { return busy_; }
    unsigned long long files() const { return files_; }
    std::optional<std::wstring> takeResult();

  private:
    std::jthread worker_;
    std::atomic<bool> busy_{false};
    std::atomic<unsigned long long> files_{0};
    std::mutex mutex_;
    std::optional<std::wstring> result_;
};

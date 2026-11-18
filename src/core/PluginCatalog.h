#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <optional>
struct PluginEntry {
    std::filesystem::path path;
    std::wstring name, version;
    std::uintmax_t size = 0;
    bool cached = false;
};
class PluginCatalog {
  public:
    struct Result {
        unsigned long long ticket;
        std::vector<PluginEntry> entries;
    };
    PluginCatalog();
    ~PluginCatalog();
    unsigned long long request(const std::filesystem::path &directory, const std::filesystem::path &cache);
    std::optional<Result> poll();

  private:
    std::mutex mutex_;
    std::condition_variable wake_;
    unsigned long long ticket_ = 0;
    std::filesystem::path directory_, cache_;
    bool pending_ = false;
    std::optional<Result> result_;
    std::jthread worker_;
    void work(std::stop_token stop);
};

#pragma once
#include <windows.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include "Operations.h"

class ServerProcess final {
  public:
    ~ServerProcess();
    bool start(const std::wstring &java, const std::filesystem::path &jar, unsigned memoryMB,
               std::wstring &error, bool nogui = true, const LaunchOptions &options = {});
    bool send(const std::wstring &command);
    bool stop(const std::wstring &command = L"stop");
    bool kill();
    std::wstring poll();
    bool running() const { return process_ != nullptr; }
    // Blocks up to `milliseconds` for the server process to exit; true if it has exited.
    bool waitExit(unsigned long milliseconds) const {
        return !process_ || WaitForSingleObject(process_, milliseconds) == WAIT_OBJECT_0;
    }
    bool stopping() const { return stopping_; }
    unsigned long pid() const { return pid_; }
    unsigned long exitCode() const { return exitCode_; }
    unsigned long long uptimeSeconds() const;
    double cpuPercent() const { return cpu_; }
    double memoryMB() const { return memoryMB_; }
    void sample();

  private:
    HANDLE process_ = nullptr, job_ = nullptr, input_ = nullptr, output_ = nullptr;
    std::thread reader_, writer_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<std::string> commands_;
    std::string incoming_, pendingUtf8_;
    std::atomic<bool> quit_{false};
    bool stopping_ = false;
    DWORD pid_ = 0, exitCode_ = 0;
    ULONGLONG started_ = 0, sampledAt_ = 0, lastCpuTime_ = 0;
    double cpu_ = 0, memoryMB_ = 0;
    void readLoop();
    void writeLoop();
    void cleanup();
};

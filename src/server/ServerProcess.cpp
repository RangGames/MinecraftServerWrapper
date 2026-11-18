#include "ServerProcess.h"
#include <psapi.h>
#include <algorithm>
#include <vector>

namespace {
void closeHandle(HANDLE &handle) {
    if (handle)
        CloseHandle(handle);
    handle = nullptr;
}
std::wstring winError(DWORD code = GetLastError()) {
    wchar_t *buffer = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, 0, reinterpret_cast<wchar_t *>(&buffer), 0, nullptr);
    std::wstring result = buffer ? buffer : L"Windows error";
    if (buffer)
        LocalFree(buffer);
    return result + L" (" + std::to_wstring(code) + L")";
}
std::string utf8(const std::wstring &text) {
    int n = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr,
                                nullptr);
    std::string result(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), n, nullptr,
                        nullptr);
    return result;
}
ULONGLONG ticks(FILETIME value) {
    return (static_cast<ULONGLONG>(value.dwHighDateTime) << 32) | value.dwLowDateTime;
}
} // namespace
ServerProcess::~ServerProcess() {
    cleanup();
}
void ServerProcess::cleanup() {
    if (job_)
        TerminateJobObject(job_, 1);
    if (process_)
        WaitForSingleObject(process_, INFINITE);
    {
        std::lock_guard lock(mutex_);
        quit_ = true;
    }
    wake_.notify_all();
    if (reader_.joinable())
        reader_.join();
    if (writer_.joinable())
        writer_.join();
    closeHandle(input_);
    closeHandle(output_);
    closeHandle(process_);
    closeHandle(job_);
    stopping_ = false;
    pid_ = 0;
    cpu_ = memoryMB_ = 0;
}
bool ServerProcess::start(const std::wstring &java, const std::filesystem::path &jar, unsigned memory,
                          std::wstring &error, bool nogui, const LaunchOptions &options) {
    if (running()) {
        error = L"이미 실행 중입니다.";
        return false;
    }
    HANDLE childInput = nullptr, childOutput = nullptr;
    auto fail = [&](DWORD code) {
        closeHandle(childInput);
        closeHandle(childOutput);
        cleanup();
        error = winError(code);
        return false;
    };
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    if (!CreatePipe(&output_, &childOutput, &security, 65536))
        return fail(GetLastError());
    if (!SetHandleInformation(output_, HANDLE_FLAG_INHERIT, 0))
        return fail(GetLastError());
    if (!CreatePipe(&childInput, &input_, &security, 65536))
        return fail(GetLastError());
    if (!SetHandleInformation(input_, HANDLE_FLAG_INHERIT, 0))
        return fail(GetLastError());
    job_ = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job_ || !SetInformationJobObject(job_, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
        return fail(GetLastError());
    std::wstring command;
    try {
        command = buildLaunchCommand(java, jar, memory, nogui, options);
    } catch (const std::exception &) {
        return fail(ERROR_INVALID_PARAMETER);
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = childInput;
    startup.hStdOutput = startup.hStdError = childOutput;
    PROCESS_INFORMATION info{};
    auto directory = options.directory.empty() ? jar.parent_path() : std::filesystem::path(options.directory);
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED,
                        nullptr, directory.c_str(), &startup, &info))
        return fail(GetLastError());
    process_ = info.hProcess;
    pid_ = info.dwProcessId;
    if (!AssignProcessToJobObject(job_, process_) || ResumeThread(info.hThread) == static_cast<DWORD>(-1)) {
        auto code = GetLastError();
        CloseHandle(info.hThread);
        TerminateProcess(process_, 1);
        return fail(code);
    }
    CloseHandle(info.hThread);
    closeHandle(childInput);
    closeHandle(childOutput);
    {
        std::lock_guard lock(mutex_);
        incoming_.clear();
        commands_.clear();
        quit_ = false;
    }
    pendingUtf8_.clear();
    stopping_ = false;
    started_ = GetTickCount64();
    sampledAt_ = lastCpuTime_ = 0;
    exitCode_ = 0;
    cpu_ = memoryMB_ = 0;
    try {
        reader_ = std::thread([this] { readLoop(); });
        writer_ = std::thread([this] { writeLoop(); });
    } catch (...) {
        cleanup();
        error = L"서버 입출력 스레드를 만들지 못했습니다.";
        return false;
    }
    sample();
    return true;
}
void ServerProcess::readLoop() {
    char buffer[8192];
    while (!quit_) {
        DWORD received = 0;
        // Block until output arrives or the child closes the pipe. No idle polling.
        if (!ReadFile(output_, buffer, sizeof(buffer), &received, nullptr) || !received)
            break;
        std::lock_guard lock(mutex_);
        if (incoming_.size() + received > 1024 * 1024)
            incoming_ = "\n[Wrapper] Display buffer full; see logs/latest.log.\n";
        incoming_.append(buffer, received);
    }
}
void ServerProcess::writeLoop() {
    for (;;) {
        std::unique_lock lock(mutex_);
        wake_.wait(lock, [this] { return quit_ || !commands_.empty(); });
        if (quit_)
            return;
        auto command = std::move(commands_.front());
        commands_.pop_front();
        lock.unlock();
        DWORD written = 0;
        if (!WriteFile(input_, command.data(), static_cast<DWORD>(command.size()), &written, nullptr) ||
            written != command.size()) {
            std::lock_guard report(mutex_);
            incoming_ += "\n[Wrapper] Could not send command.\n";
        }
    }
}
bool ServerProcess::send(const std::wstring &command) {
    if (!running() || stopping_ || command.empty() || command.find_first_of(L"\r\n") != std::wstring::npos)
        return false;
    auto bytes = utf8(command + L"\n");
    if (bytes.size() > 2048)
        return false;
    {
        std::lock_guard lock(mutex_);
        if (commands_.size() >= 32)
            return false;
        commands_.push_back(std::move(bytes));
    }
    wake_.notify_one();
    return true;
}
bool ServerProcess::stop(const std::wstring &command) {
    if (command.empty() || command.find_first_of(L"\r\n") != std::wstring::npos)
        return false;
    if (stopping_)
        return true;
    if (!running())
        return false;
    // Graceful shutdown must not fail because ordinary commands filled the queue.
    // Discard queued work so nothing runs after the shutdown request.
    {
        std::lock_guard lock(mutex_);
        if (quit_)
            return false;
        commands_.clear();
        commands_.push_front(utf8(command + L"\n"));
        stopping_ = true;
    }
    wake_.notify_one();
    return true;
}
bool ServerProcess::kill() {
    return job_ && TerminateJobObject(job_, 1);
}
std::wstring ServerProcess::poll() {
    bool ended = running() && WaitForSingleObject(process_, 0) == WAIT_OBJECT_0;
    if (ended) {
        GetExitCodeProcess(process_, &exitCode_);
        TerminateJobObject(job_, exitCode_);
        {
            std::lock_guard lock(mutex_);
            quit_ = true;
        }
        wake_.notify_all();
        if (reader_.joinable())
            reader_.join();
        if (writer_.joinable())
            writer_.join();
        char buffer[8192];
        DWORD available = 0, received = 0;
        while (PeekNamedPipe(output_, nullptr, 0, nullptr, &available, nullptr) && available) {
            if (!ReadFile(output_, buffer, std::min<DWORD>(available, sizeof(buffer)), &received, nullptr) ||
                !received)
                break;
            incoming_.append(buffer, received);
        }
    }
    {
        std::lock_guard lock(mutex_);
        pendingUtf8_ += incoming_;
        incoming_.clear();
    }
    size_t complete = pendingUtf8_.size();
    if (!ended && complete) {
        size_t start = complete - 1;
        while (start && (static_cast<unsigned char>(pendingUtf8_[start]) & 0xC0) == 0x80)
            --start;
        auto lead = static_cast<unsigned char>(pendingUtf8_[start]);
        size_t needed = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
        if (complete - start < needed)
            complete = start;
    }
    int size = MultiByteToWideChar(CP_UTF8, 0, pendingUtf8_.data(), static_cast<int>(complete), nullptr, 0);
    std::wstring decoded(size, 0), normalized;
    MultiByteToWideChar(CP_UTF8, 0, pendingUtf8_.data(), static_cast<int>(complete), decoded.data(), size);
    pendingUtf8_.erase(0, complete);
    for (wchar_t c : decoded) {
        if (c == L'\r' || c == 0)
            continue;
        if (c == L'\n')
            normalized += L'\r';
        normalized += c;
    }
    if (ended) {
        cleanup();
        normalized += L"\r\n[Wrapper] 서버 프로세스 종료. 종료 코드: " + std::to_wstring(exitCode_) + L"\r\n";
    }
    return normalized;
}
unsigned long long ServerProcess::uptimeSeconds() const {
    return running() ? (GetTickCount64() - started_) / 1000 : 0;
}
void ServerProcess::sample() {
    if (!running()) {
        cpu_ = memoryMB_ = 0;
        return;
    }
    PROCESS_MEMORY_COUNTERS memory{};
    memory.cb = sizeof(memory);
    if (GetProcessMemoryInfo(process_, &memory, sizeof(memory)))
        memoryMB_ = memory.WorkingSetSize / 1048576.0;
    FILETIME created{}, exited{}, kernel{}, user{};
    ULONGLONG now = GetTickCount64();
    if (GetProcessTimes(process_, &created, &exited, &kernel, &user)) {
        auto current = ticks(kernel) + ticks(user);
        DWORD processors = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
        if (sampledAt_ && now > sampledAt_ && current >= lastCpuTime_)
            cpu_ = std::clamp((current - lastCpuTime_) /
                                  ((now - sampledAt_) * 100.0 * std::max<DWORD>(1, processors)),
                              0.0, 100.0);
        lastCpuTime_ = current;
        sampledAt_ = now;
    }
}

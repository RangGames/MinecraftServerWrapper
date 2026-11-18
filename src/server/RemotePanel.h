#pragma once
#include <atomic>
#include <deque>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
struct RemoteUser {
    std::string name, role, token;
};
struct RemoteRequest {
    std::string method, path, body, user, role;
    std::promise<std::string> result;
    std::atomic<bool> cancelled{false};
};
std::string jsonString(const std::string &value);
std::vector<RemoteUser> parseRemoteUsers(const std::wstring &value);
class RemotePanel {
  public:
    ~RemotePanel();
    bool start(unsigned port, std::vector<RemoteUser> users, std::wstring &error);
    void stop();
    void setUsers(std::vector<RemoteUser> users);
    std::vector<std::shared_ptr<RemoteRequest>> pending();
    // Posts `message` to `window` (an HWND) whenever a request is queued.
    void setNotify(void *window, unsigned message) {
        notifyWindow_ = window;
        notifyMessage_ = message;
    }
    unsigned port() const { return port_; }
    bool running() const { return active_; }

  private:
    unsigned port_ = 0;
    std::atomic<bool> active_{false};
    std::atomic<void *> notifyWindow_{nullptr};
    std::atomic<unsigned> notifyMessage_{0};
    std::thread worker_;
    std::mutex mutex_;
    std::vector<RemoteUser> users_;
    std::deque<std::shared_ptr<RemoteRequest>> queue_;
};

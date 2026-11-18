#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <unordered_map>

// All public methods are called on the window's UI thread.
// The worker downloads bytes only; it never accesses windows or GDI objects.
class PlayerFaces final {
public:
    static constexpr int ControlId = 7001;
    PlayerFaces() = default;
    ~PlayerFaces();
    PlayerFaces(const PlayerFaces&) = delete;
    PlayerFaces& operator=(const PlayerFaces&) = delete;

    bool create(HWND parent, HFONT font = nullptr);
    void move(int x, int y, int width, int height);
    size_t count() const { return players_.size(); }
    std::wstring selectedName() const;
    void feed(const std::wstring& text);
    void poll();
    bool draw(const DRAWITEMSTRUCT& item) const;
    void clear();
    void setColors(COLORREF background, COLORREF selected, COLORREF text, COLORREF muted, COLORREF accent) {
        background_ = background;
        selected_ = selected;
        text_ = text;
        muted_ = muted;
        accent_ = accent;
    }
    void shutdown();

private:
    static constexpr int FaceSize = 48;
    static constexpr int RowHeight = 64;
    static constexpr size_t MaxLineLength = 65536;
    static constexpr size_t MaxImageBytes = 128 * 1024;
    struct Player {
        std::wstring name;
        std::uint64_t ticket;
        HBITMAP face = nullptr;
    };
    struct Request { std::wstring name; std::uint64_t ticket; };
    struct Result { std::uint64_t ticket; std::vector<BYTE> png; };

    COLORREF background_ = RGB(45, 45, 45), selected_ = RGB(58, 58, 58), text_ = RGB(240, 240, 240),
             muted_ = RGB(168, 168, 168), accent_ = RGB(96, 205, 255);
    HWND list_ = nullptr;
    ULONG_PTR gdiplusToken_ = 0;
    std::vector<Player> players_;
    std::wstring lineBuffer_;
    bool droppingLongLine_ = false;
    std::uint64_t nextTicket_ = 1;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Request> requests_;
    std::deque<Result> results_;
    std::jthread worker_;
    std::unordered_map<std::wstring, std::vector<BYTE>> cache_;

    void parseLine(const std::wstring& line);
    void joined(const std::wstring& name);
    void left(const std::wstring& name);
    void workerLoop(std::stop_token stop);
    static std::vector<BYTE> download(const std::wstring& name, std::stop_token stop);
    static HBITMAP decodePng(const std::vector<BYTE>& bytes);
};

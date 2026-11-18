#include "PlayerFaces.h"
#include "PlayerLog.h"
#include "UiScale.h"
#include <objidl.h>
#include <gdiplus.h>
#include <shlwapi.h>
#include <winhttp.h>
#include <algorithm>
#include <regex>
#include <utility>

namespace {
struct InternetHandle {
    HINTERNET value;
    explicit InternetHandle(HINTERNET h) : value(h) {}
    ~InternetHandle() {
        if (value)
            WinHttpCloseHandle(value);
    }
    InternetHandle(const InternetHandle &) = delete;
    InternetHandle &operator=(const InternetHandle &) = delete;
};
} // namespace

std::wstring PlayerFaces::selectedName() const {
    auto index = SendMessageW(list_, LB_GETCURSEL, 0, 0);
    return index >= 0 && static_cast<size_t>(index) < players_.size() ? players_[index].name : L"";
}

PlayerFaces::~PlayerFaces() {
    shutdown();
}

bool PlayerFaces::create(HWND parent, HFONT font) {
    if (list_)
        return false;
    list_ = CreateWindowExW(0, L"LISTBOX", L"",
                            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | LBS_OWNERDRAWFIXED |
                                LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
                            0, 0, 220, 300, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ControlId)),
                            GetModuleHandleW(nullptr), nullptr);
    if (!list_)
        return false;
    if (!font)
        font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    SendMessageW(list_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SendMessageW(list_, LB_SETITEMHEIGHT, 0, S(RowHeight));
    Gdiplus::GdiplusStartupInput startup;
    if (Gdiplus::GdiplusStartup(&gdiplusToken_, &startup, nullptr) != Gdiplus::Ok) {
        DestroyWindow(list_);
        list_ = nullptr;
        return false;
    }
    try {
        worker_ = std::jthread([this](std::stop_token stop) { workerLoop(stop); });
    } catch (...) {
        Gdiplus::GdiplusShutdown(gdiplusToken_);
        gdiplusToken_ = 0;
        DestroyWindow(list_);
        list_ = nullptr;
        return false;
    }
    return true;
}

void PlayerFaces::move(int x, int y, int width, int height) {
    if (list_)
        MoveWindow(list_, x, y, width, height, TRUE);
}

void PlayerFaces::feed(const std::wstring &text) {
    if (!list_)
        return;
    for (wchar_t c : text) {
        if (c == L'\n') {
            if (!droppingLongLine_) {
                if (!lineBuffer_.empty() && lineBuffer_.back() == L'\r')
                    lineBuffer_.pop_back();
                parseLine(lineBuffer_);
            }
            lineBuffer_.clear();
            droppingLongLine_ = false;
        } else if (!droppingLongLine_) {
            if (lineBuffer_.size() >= MaxLineLength) {
                lineBuffer_.clear();
                droppingLongLine_ = true;
            } else {
                lineBuffer_ += c;
            }
        }
    }
}

void PlayerFaces::parseLine(const std::wstring &original) {
    if (auto event = parsePlayerLog(original)) {
        if (event->joined)
            joined(event->name);
        else
            left(event->name);
    }
}

void PlayerFaces::joined(const std::wstring &name) {
    if (std::any_of(players_.begin(), players_.end(), [&](const Player &p) { return p.name == name; }))
        return;
    const auto ticket = nextTicket_++;
    players_.push_back({name, ticket, nullptr});
    auto inserted = SendMessageW(list_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
    if (inserted == LB_ERR || inserted == LB_ERRSPACE) {
        players_.pop_back();
        return;
    }
    {
        std::lock_guard lock(mutex_);
        if (requests_.size() < 512)
            requests_.push_back({name, ticket});
    }
    wake_.notify_one();
}

void PlayerFaces::left(const std::wstring &name) {
    for (size_t i = 0; i < players_.size(); ++i) {
        if (players_[i].name != name)
            continue;
        auto ticket = players_[i].ticket;
        SendMessageW(list_, LB_DELETESTRING, static_cast<WPARAM>(i), 0);
        if (players_[i].face)
            DeleteObject(players_[i].face);
        players_.erase(players_.begin() + i);
        {
            std::lock_guard lock(mutex_);
            std::erase_if(requests_, [&](const Request &r) { return r.ticket == ticket; });
        }
        InvalidateRect(list_, nullptr, TRUE);
        return;
    }
}

void PlayerFaces::poll() {
    std::deque<Result> completed;
    {
        std::lock_guard lock(mutex_);
        completed.swap(results_);
    }
    bool changed = false;
    for (auto &result : completed) {
        auto found = std::find_if(players_.begin(), players_.end(),
                                  [&](const Player &p) { return p.ticket == result.ticket; });
        // Ignore results for an old connection, even if the same name rejoined.
        if (found == players_.end() || result.png.empty())
            continue;
        HBITMAP bitmap = decodePng(result.png);
        if (!bitmap)
            continue;
        if (found->face)
            DeleteObject(found->face);
        found->face = bitmap;
        changed = true;
    }
    if (changed && list_)
        InvalidateRect(list_, nullptr, FALSE);
}

bool PlayerFaces::draw(const DRAWITEMSTRUCT &item) const {
    if (item.CtlID != ControlId)
        return false;
    if (item.itemID == static_cast<UINT>(-1) || item.itemID >= players_.size())
        return true;
    const auto &player = players_[item.itemID];
    bool selected = (item.itemState & ODS_SELECTED) != 0;
    int saved = SaveDC(item.hDC);
    SetDCBrushColor(item.hDC, background_);
    FillRect(item.hDC, &item.rcItem, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    if (selected) {
        // Fluent list selection: a subtle fill with a short accent pill on the leading edge.
        RECT fillRect{item.rcItem.left + S(2), item.rcItem.top + S(2), item.rcItem.right - S(2),
                      item.rcItem.bottom - S(2)};
        SetDCBrushColor(item.hDC, selected_);
        FillRect(item.hDC, &fillRect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        int middle = (item.rcItem.top + item.rcItem.bottom) / 2;
        RECT pill{item.rcItem.left + S(2), middle - S(9), item.rcItem.left + S(5), middle + S(9)};
        SetDCBrushColor(item.hDC, accent_);
        FillRect(item.hDC, &pill, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    }
    auto font = reinterpret_cast<HFONT>(SendMessageW(list_, WM_GETFONT, 0, 0));
    if (font)
        SelectObject(item.hDC, font);
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, text_);
    // Faces are downloaded at FaceSize and drawn at the UI scale (nearest neighbour keeps pixel art crisp).
    int face = S(FaceSize);
    int x = item.rcItem.left + S(12);
    int y = item.rcItem.top + (item.rcItem.bottom - item.rcItem.top - face) / 2;
    if (player.face) {
        HDC memory = CreateCompatibleDC(item.hDC);
        if (memory) {
            auto previous = SelectObject(memory, player.face);
            if (face == FaceSize)
                BitBlt(item.hDC, x, y, FaceSize, FaceSize, memory, 0, 0, SRCCOPY);
            else {
                SetStretchBltMode(item.hDC, COLORONCOLOR);
                StretchBlt(item.hDC, x, y, face, face, memory, 0, 0, FaceSize, FaceSize, SRCCOPY);
            }
            SelectObject(memory, previous);
            DeleteDC(memory);
        }
    } else {
        RECT placeholder{x, y, x + face, y + face};
        SetDCBrushColor(item.hDC, selected_);
        FillRect(item.hDC, &placeholder, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        auto previous = SetTextColor(item.hDC, muted_);
        DrawTextW(item.hDC, L"?", 1, &placeholder, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SetTextColor(item.hDC, previous);
    }
    RECT name = item.rcItem;
    name.left += face + S(24);
    name.right -= S(8);
    DrawTextW(item.hDC, player.name.c_str(), -1, &name,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    if (item.itemState & ODS_FOCUS)
        DrawFocusRect(item.hDC, &item.rcItem);
    if (saved)
        RestoreDC(item.hDC, saved);
    return true;
}

void PlayerFaces::clear() {
    if (list_ && IsWindow(list_))
        SendMessageW(list_, LB_RESETCONTENT, 0, 0);
    for (auto &player : players_)
        if (player.face)
            DeleteObject(player.face);
    players_.clear();
    lineBuffer_.clear();
    droppingLongLine_ = false;
    std::lock_guard lock(mutex_);
    requests_.clear();
    results_.clear();
}

void PlayerFaces::shutdown() {
    if (worker_.joinable()) {
        {
            std::lock_guard lock(mutex_);
            worker_.request_stop();
        }
        wake_.notify_all();
        worker_.join();
    }
    clear();
    if (gdiplusToken_) {
        Gdiplus::GdiplusShutdown(gdiplusToken_);
        gdiplusToken_ = 0;
    }
    list_ = nullptr;
}

std::vector<BYTE> PlayerFaces::download(const std::wstring &name, std::stop_token stop) {
    if (stop.stop_requested())
        return {};
    InternetHandle session(WinHttpOpen(L"MinecraftServerWrapper/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                       WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.value || !WinHttpSetTimeouts(session.value, 1500, 1500, 1500, 1500))
        return {};
    InternetHandle connection(WinHttpConnect(session.value, L"mc-heads.net", INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!connection.value || stop.stop_requested())
        return {};
    auto path = L"/avatar/" + name + L"/48";
    InternetHandle request(WinHttpOpenRequest(connection.value, L"GET", path.c_str(), nullptr,
                                              WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                              WINHTTP_FLAG_SECURE));
    if (!request.value || !WinHttpSendRequest(request.value, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                              WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
        return {};
    if (stop.stop_requested() || !WinHttpReceiveResponse(request.value, nullptr))
        return {};
    DWORD status = 0, statusSize = sizeof(status);
    if (!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX) ||
        status != 200)
        return {};
    std::vector<BYTE> bytes;
    BYTE buffer[4096];
    auto started = GetTickCount64();
    for (;;) {
        if (stop.stop_requested() || GetTickCount64() - started > 4000)
            return {};
        DWORD received = 0;
        if (!WinHttpReadData(request.value, buffer, sizeof(buffer), &received))
            return {};
        if (!received)
            break;
        if (bytes.size() + received > MaxImageBytes)
            return {};
        bytes.insert(bytes.end(), buffer, buffer + received);
    }
    return bytes;
}

HBITMAP PlayerFaces::decodePng(const std::vector<BYTE> &bytes) {
    static constexpr BYTE signature[]{0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    if (bytes.size() < sizeof(signature) ||
        !std::equal(std::begin(signature), std::end(signature), bytes.begin()))
        return nullptr;
    IStream *stream = SHCreateMemStream(bytes.data(), static_cast<UINT>(bytes.size()));
    if (!stream)
        return nullptr;
    HBITMAP bitmap = nullptr;
    {
        Gdiplus::Bitmap image(stream);
        if (image.GetLastStatus() == Gdiplus::Ok && image.GetWidth() == FaceSize &&
            image.GetHeight() == FaceSize) {
            if (image.GetHBITMAP(Gdiplus::Color(255, 245, 245, 245), &bitmap) != Gdiplus::Ok) {
                if (bitmap)
                    DeleteObject(bitmap);
                bitmap = nullptr;
            }
        }
    }
    stream->Release();
    return bitmap;
}

void PlayerFaces::workerLoop(std::stop_token stop) {
    for (;;) {
        Request request;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, [&] { return stop.stop_requested() || !requests_.empty(); });
            if (stop.stop_requested())
                return;
            request = std::move(requests_.front());
            requests_.pop_front();
        }
        std::vector<BYTE> png;
        if (auto cached = cache_.find(request.name); cached != cache_.end())
            png = cached->second;
        else {
            png = download(request.name, stop);
            if (!png.empty()) {
                if (cache_.size() >= 128)
                    cache_.erase(cache_.begin());
                cache_[request.name] = png;
            }
        }
        if (stop.stop_requested())
            return;
        std::lock_guard lock(mutex_);
        if (results_.size() < 512)
            results_.push_back({request.ticket, std::move(png)});
    }
}

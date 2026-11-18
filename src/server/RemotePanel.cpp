#include <winsock2.h>
#include <ws2tcpip.h>
#include "RemotePanel.h"
#include <algorithm>
#include <chrono>
#include <list>
#include <sstream>
#include <stdexcept>
namespace {
const char *panel =
    R"HTML(<!doctype html><html lang="ko"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>Minecraft Server Manager</title><style>body{background:#181b20;color:#eef;font:16px system-ui;max-width:1100px;margin:32px auto;padding:16px}input,button,select{font:inherit;padding:8px;margin:4px;background:#303640;color:#fff;border:1px solid #666;border-radius:5px}article{padding:18px;margin:16px 0;background:#262b32;border-radius:10px}pre{white-space:pre-wrap;max-height:380px;overflow:auto}small{color:#bbc}</style><h1>Minecraft Server Manager</h1><p>계정 API 키로 로그인하세요.</p><input id="key" type="password" placeholder="API key" autocomplete="off"><button id="login">연결</button><button id="logout">로그아웃</button><p id="status" role="status"></p><main id="servers"></main><script>
let token='',busy=false;const cards=new Map();
const el=id=>document.getElementById(id);
async function api(path,method='GET',body=''){let r=await fetch(path,{method,headers:{Authorization:'Bearer '+token,'Content-Type':'text/plain'},body:method==='GET'?undefined:body,cache:'no-store'});let data=await r.json();if(!r.ok||data.error)throw Error(data.error||r.status);return data}
function button(parent,label,fn){let b=document.createElement('button');b.textContent=label;b.onclick=async()=>{b.disabled=true;try{await fn();el('status').textContent='완료';}catch(e){el('status').textContent=e.message}finally{b.disabled=false}};parent.append(b)}
async function refresh(){if(!token||busy)return;busy=true;try{let data=await api('/api/servers');for(const [id,card] of cards)if(!data.servers.some(s=>s.id===id)){card.remove();cards.delete(id)}for(let s of data.servers){if(cards.has(s.id)){cards.get(s.id).querySelector('h2').textContent=s.name;cards.get(s.id).querySelector('p').textContent=`${s.running?'실행 중':'중지'} · CPU ${s.cpu}% · RAM ${s.memory} MB · ${s.uptime}초 · ${s.players}명`;continue}let a=document.createElement('article'),h=document.createElement('h2'),p=document.createElement('p');h.textContent=s.name;p.textContent=`${s.running?'실행 중':'중지'} · CPU ${s.cpu}% · RAM ${s.memory} MB · ${s.uptime}초 · ${s.players}명`;a.append(h,p);let base='/api/servers/'+s.id;for(let action of (data.role==='viewer'?[]:data.role==='admin'?['start','stop','restart','kill','backup']:['start','stop','restart','backup']))button(a,action,async()=>{if(action==='kill'&&!confirm('저장하지 않은 데이터가 손실될 수 있습니다. 강제 종료할까요?'))return;await api(base+'/'+action,'POST');await refresh()});if(data.role!=='viewer'){let cmd=document.createElement('input');cmd.placeholder='서버 명령';a.append(cmd);button(a,'명령 전송',()=>api(base+'/command','POST',cmd.value))}let pre=document.createElement('pre');button(a,'콘솔',async()=>{pre.textContent=(await api(base+'/log')).log});button(a,'예약 작업',async()=>{pre.textContent=JSON.stringify(await api(base+'/tasks'),null,2)});button(a,'백업 목록',async()=>{pre.textContent=JSON.stringify(await api(base+'/backups'),null,2)});a.append(pre);cards.set(s.id,a);el('servers').append(a)}el('status').textContent='연결됨'}catch(e){el('status').textContent=e.message}finally{busy=false}}
el('login').onclick=()=>{token=el('key').value;el('key').value='';cards.clear();el('servers').replaceChildren();refresh()};el('logout').onclick=()=>{token='';cards.clear();el('servers').replaceChildren();el('status').textContent='로그아웃됨'};setInterval(refresh,5000);
</script></html>)HTML";
constexpr size_t maxConnections = 8;
constexpr ULONGLONG idleTimeout = 1500;
// Waits until `socket` is readable (or writable) but never past `deadline`. Polls in short slices so stop()
// is noticed promptly; once stopping, a socket that does not become ready within one slice is abandoned.
bool waitReady(SOCKET socket, bool write, ULONGLONG deadline, const std::atomic<bool> &active) {
    for (;;) {
        auto now = GetTickCount64();
        if (now >= deadline)
            return false;
        fd_set set;
        FD_ZERO(&set);
        FD_SET(socket, &set);
        timeval timeout{0, static_cast<long>(std::min<ULONGLONG>(deadline - now, 100) * 1000)};
        int n = select(0, write ? nullptr : &set, write ? &set : nullptr, nullptr, &timeout);
        if (n != 0)
            return n > 0;
        if (!active)
            return false;
    }
}
// recv on a non-blocking socket: gives up after `idleTimeout` of silence, at `deadline`, or on stop().
int receive(SOCKET socket, char *buffer, size_t size, ULONGLONG deadline, const std::atomic<bool> &active) {
    deadline = std::min(deadline, GetTickCount64() + idleTimeout);
    for (;;) {
        int n = recv(socket, buffer, static_cast<int>(size), 0);
        if (n >= 0 || WSAGetLastError() != WSAEWOULDBLOCK)
            return n;
        if (!waitReady(socket, false, deadline, active))
            return -1;
    }
}
void reply(SOCKET socket, const std::atomic<bool> &active, int status, const std::string &body,
           const char *type = "application/json; charset=utf-8") {
    std::string header = "HTTP/1.1 " + std::to_string(status) + " Response\r\nContent-Type: " + type +
                         "\r\nContent-Length: " + std::to_string(body.size()) +
                         "\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: "
                         "nosniff\r\nX-Frame-Options: DENY\r\nContent-Security-Policy: default-src 'none'; "
                         "script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; "
                         "frame-ancestors 'none'; base-uri 'none'; form-action 'none'\r\n\r\n";
    auto bytes = header + body;
    size_t done = 0;
    auto deadline = GetTickCount64() + idleTimeout;
    while (done < bytes.size()) {
        int sent = send(socket, bytes.data() + done,
                        static_cast<int>(std::min<size_t>(bytes.size() - done, 65536)), 0);
        if (sent > 0) {
            done += sent;
            deadline = GetTickCount64() + idleTimeout;
        } else if (sent == 0 || WSAGetLastError() != WSAEWOULDBLOCK ||
                   !waitReady(socket, true, deadline, active))
            break;
    }
}
bool equalSecret(const std::string &a, const std::string &b) {
    unsigned diff = static_cast<unsigned>(a.size() ^ b.size());
    for (size_t i = 0; i < std::max(a.size(), b.size()); ++i)
        diff |= (i < a.size() ? static_cast<unsigned char>(a[i]) : 0) ^
                (i < b.size() ? static_cast<unsigned char>(b[i]) : 0);
    return diff == 0;
}
} // namespace
std::string jsonString(const std::string &v) {
    std::string r = "\"";
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned char c : v) {
        if (c == '"' || c == '\\') {
            r += '\\';
            r += c;
        } else if (c < 32) {
            r += "\\u00";
            r += hex[c >> 4];
            r += hex[c & 15];
        } else
            r += c;
    }
    return r + '"';
}
std::vector<RemoteUser> parseRemoteUsers(const std::wstring &value) {
    std::vector<RemoteUser> result;
    std::wstringstream stream(value);
    std::wstring entry;
    while (std::getline(stream, entry, L';')) {
        if (entry.empty())
            continue;
        if (result.size() >= 32)
            throw std::invalid_argument("too many users");
        auto a = entry.find(L':'), b = a == std::wstring::npos ? a : entry.find(L':', a + 1);
        if (a == std::wstring::npos || b == std::wstring::npos)
            throw std::invalid_argument("user format");
        auto name = entry.substr(0, a), role = entry.substr(a + 1, b - a - 1), token = entry.substr(b + 1);
        if (name.empty() || name.size() > 48 ||
            name.find_first_not_of(L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") !=
                std::wstring::npos ||
            (role != L"viewer" && role != L"operator" && role != L"admin") || token.size() != 64 ||
            token.find_first_not_of(L"0123456789abcdef") != std::wstring::npos)
            throw std::invalid_argument("invalid user");
        RemoteUser user{std::string(name.begin(), name.end()), std::string(role.begin(), role.end()),
                        std::string(token.begin(), token.end())};
        for (const auto &u : result)
            if (u.name == user.name || u.token == user.token)
                throw std::invalid_argument("duplicate user");
        result.push_back(std::move(user));
    }
    return result;
}
RemotePanel::~RemotePanel() {
    stop();
}
void RemotePanel::setUsers(std::vector<RemoteUser> users) {
    std::lock_guard lock(mutex_);
    users_ = std::move(users);
}
void RemotePanel::stop() {
    // The accept thread notices within one poll slice, then joins every connection thread; those poll in
    // short slices too, so shutdown never waits for an idle client or for the UI thread.
    active_ = false;
    if (worker_.joinable())
        worker_.join();
    std::lock_guard lock(mutex_);
    for (auto &request : queue_)
        request->cancelled = true;
    queue_.clear();
}
std::vector<std::shared_ptr<RemoteRequest>> RemotePanel::pending() {
    std::lock_guard lock(mutex_);
    std::vector<std::shared_ptr<RemoteRequest>> result(queue_.begin(), queue_.end());
    queue_.clear();
    return result;
}
void RemotePanel::serve(std::uintptr_t handle, unsigned port) {
    auto client = static_cast<SOCKET>(handle);
    auto respond = [&](int status, const std::string &body,
                       const char *type = "application/json; charset=utf-8") {
        reply(client, active_, status, body, type);
    };
    try {
        std::string bytes;
        char buffer[4096];
        size_t end = std::string::npos;
        const auto deadline = GetTickCount64() + 3000;
        while (active_ && GetTickCount64() < deadline && bytes.size() < 16384 &&
               (end = bytes.find("\r\n\r\n")) == std::string::npos) {
            int n = receive(client, buffer, sizeof(buffer), deadline, active_);
            if (n <= 0)
                break;
            bytes.append(buffer, n);
        }
        if (end == std::string::npos)
            return respond(400, "{\"error\":\"invalid headers\"}");
        std::istringstream input(bytes.substr(0, end));
        std::string method, path, version;
        input >> method >> path >> version;
        std::string line, authorization, host;
        size_t length = 0;
        bool invalid = false, hasLength = false;
        std::getline(input, line);
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            auto c = line.find(':');
            if (c == std::string::npos) {
                invalid = true;
                break;
            }
            auto key = line.substr(0, c), value = line.substr(c + 1);
            while (!value.empty() && value.front() == ' ')
                value.erase(0, 1);
            std::transform(key.begin(), key.end(), key.begin(),
                           [](unsigned char x) { return static_cast<char>(std::tolower(x)); });
            if (key == "content-length") {
                if (hasLength || value.empty() ||
                    value.find_first_not_of("0123456789") != std::string::npos) {
                    invalid = true;
                    break;
                }
                hasLength = true;
                length = std::stoul(value);
            }
            if (key == "authorization") {
                if (!authorization.empty())
                    invalid = true;
                authorization = value;
            }
            if (key == "host")
                host = value;
            if (key == "transfer-encoding")
                invalid = true;
        }
        if (invalid || length > 8192 || version != "HTTP/1.1" ||
            (method != "GET" && method != "POST" && method != "DELETE") ||
            !(host == "127.0.0.1:" + std::to_string(port) || host == "localhost:" + std::to_string(port)))
            return respond(400, "{\"error\":\"invalid request\"}");
        if (method == "GET" && path == "/")
            return respond(200, panel, "text/html; charset=utf-8");
        RemoteUser authenticated;
        {
            std::lock_guard lock(mutex_);
            for (const auto &u : users_)
                if (equalSecret(authorization, "Bearer " + u.token))
                    authenticated = u;
        }
        if (authenticated.name.empty())
            return respond(401, "{\"error\":\"authentication required\"}");
        if (method != "GET" && authenticated.role == "viewer")
            return respond(403, "{\"error\":\"read only account\"}");
        auto body = bytes.substr(end + 4);
        while (active_ && GetTickCount64() < deadline && body.size() < length) {
            int n =
                receive(client, buffer, std::min(sizeof(buffer), length - body.size()), deadline, active_);
            if (n <= 0)
                break;
            body.append(buffer, n);
        }
        if (body.size() != length)
            return respond(400, "{\"error\":\"invalid body length\"}");
        auto request = std::make_shared<RemoteRequest>();
        request->method = method;
        request->path = path;
        request->body = body;
        request->user = authenticated.name;
        request->role = authenticated.role;
        auto future = request->result.get_future();
        {
            std::lock_guard lock(mutex_);
            queue_.push_back(request);
        }
        if (auto target = static_cast<HWND>(notifyWindow_.load()))
            PostMessageW(target, notifyMessage_, 0, 0);
        bool ready = false;
        for (int i = 0; i < 100 && active_; ++i)
            if (future.wait_for(std::chrono::milliseconds(50)) == std::future_status::ready) {
                ready = true;
                break;
            }
        if (!ready) {
            request->cancelled = true;
            return respond(503, "{\"error\":\"manager busy; check action state before retry\"}");
        }
        auto result = future.get();
        int status = 200;
        auto split = result.find('\n');
        if (split != std::string::npos) {
            status = std::stoi(result.substr(0, split));
            result.erase(0, split + 1);
        }
        respond(status, result);
    } catch (...) {
        respond(400, "{\"error\":\"invalid request\"}");
    }
}
bool RemotePanel::start(unsigned port, std::vector<RemoteUser> users, std::wstring &error) {
    stop();
    if (users.empty()) {
        error = L"최소 한 개의 계정이 필요합니다.";
        return false;
    }
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data)) {
        error = L"네트워크 초기화 실패";
        return false;
    }
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    BOOL exclusive = TRUE;
    setsockopt(listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char *>(&exclusive),
               sizeof(exclusive));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<u_short>(port));
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (listener == INVALID_SOCKET ||
        bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) || listen(listener, 8)) {
        if (listener != INVALID_SOCKET)
            closesocket(listener);
        WSACleanup();
        error = L"웹 패널 포트 사용 중 또는 바인딩 실패";
        return false;
    }
    int addressSize = sizeof(address);
    if (getsockname(listener, reinterpret_cast<sockaddr *>(&address), &addressSize)) {
        closesocket(listener);
        WSACleanup();
        error = L"포트 조회 실패";
        return false;
    }
    port = ntohs(address.sin_port);
    port_ = port;
    setUsers(std::move(users));
    active_ = true;
    try {
        worker_ = std::thread([this, listener, port] {
            struct Connection {
                std::thread thread;
                std::atomic<bool> done{false};
            };
            std::list<Connection> connections;
            while (active_) {
                for (auto it = connections.begin(); it != connections.end();)
                    if (it->done) {
                        it->thread.join();
                        it = connections.erase(it);
                    } else
                        ++it;
                fd_set set;
                FD_ZERO(&set);
                FD_SET(listener, &set);
                timeval timeout{0, 100000};
                if (select(0, &set, nullptr, nullptr, &timeout) <= 0)
                    continue;
                SOCKET client = accept(listener, nullptr, nullptr);
                if (client == INVALID_SOCKET)
                    continue;
                // Every wait on a client polls in short slices, so stop() never blocks on a socket.
                u_long nonBlocking = 1;
                if (ioctlsocket(client, FIONBIO, &nonBlocking)) {
                    closesocket(client);
                    continue;
                }
                bool started = false;
                if (connections.size() < maxConnections) {
                    try {
                        auto &connection = connections.emplace_back();
                        try {
                            connection.thread = std::thread([this, client, port, &connection] {
                                serve(client, port);
                                closesocket(client);
                                connection.done = true;
                            });
                            started = true;
                        } catch (...) {
                            connections.pop_back();
                        }
                    } catch (...) {
                    }
                }
                if (!started) {
                    // Over capacity: discard what already arrived (so closing does not reset the
                    // connection before the reply is read) and refuse without blocking the accept loop.
                    char buffer[4096];
                    for (int i = 0; i < 4 && recv(client, buffer, sizeof(buffer), 0) > 0; ++i) {
                    }
                    reply(client, active_, 503, "{\"error\":\"too many connections\"}");
                    closesocket(client);
                }
            }
            closesocket(listener);
            for (auto &connection : connections)
                connection.thread.join();
            WSACleanup();
        });
    } catch (...) {
        active_ = false;
        closesocket(listener);
        WSACleanup();
        error = L"네트워크 스레드를 시작하지 못했습니다.";
        return false;
    }
    return true;
}

#include "Operations.h"
#include "UiSurface.h"
#include "UiInput.h"
#include <uxtheme.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <algorithm>
#include <ctime>
#include <stdexcept>
#include <sstream>
std::filesystem::path extendedPath(const std::filesystem::path &path) {
    auto value = path.wstring();
    if (value.starts_with(L"\\\\?\\"))
        return path;
    value = std::filesystem::absolute(path).wstring();
    std::replace(value.begin(), value.end(), L'/', L'\\');
    if (value.starts_with(L"\\\\?\\"))
        return value;
    if (value.starts_with(L"\\\\"))
        return L"\\\\?\\UNC\\" + value.substr(2);
    return L"\\\\?\\" + value;
}
bool removePathTree(const std::filesystem::path &path) {
    auto full = extendedPath(path);
    DWORD attributes = GetFileAttributesW(full.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
        return GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND;
    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY))
        return DeleteFileW(full.c_str()) != FALSE;
    if (!(attributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
        WIN32_FIND_DATAW data{};
        auto pattern = full.wstring() + L"\\*";
        HANDLE find = FindFirstFileW(pattern.c_str(), &data);
        if (find == INVALID_HANDLE_VALUE)
            return false;
        bool ok = true;
        do {
            if (wcscmp(data.cFileName, L".") && wcscmp(data.cFileName, L".."))
                ok = removePathTree(full / data.cFileName) && ok;
        } while (FindNextFileW(find, &data));
        auto error = GetLastError();
        FindClose(find);
        if (!ok || error != ERROR_NO_MORE_FILES)
            return false;
    }
    return RemoveDirectoryW(full.c_str()) != FALSE;
}
std::wstring quoteArgument(const std::wstring &arg) {
    std::wstring result = L"\"";
    size_t slashes = 0;
    for (wchar_t c : arg) {
        if (c == L'\\') {
            ++slashes;
            continue;
        }
        result.append(slashes * (c == L'"' ? 2 : 1), L'\\');
        if (c == L'"')
            result += L'\\';
        result += c;
        slashes = 0;
    }
    result.append(slashes * 2, L'\\');
    return result + L'"';
}
std::wstring buildLaunchCommand(const std::wstring &java, const std::filesystem::path &jar, unsigned memory,
                                bool nogui, const LaunchOptions &o) {
    for (const auto &s : {java, jar.wstring(), o.jvm, o.arguments, o.target, o.directory})
        if (s.find_first_of(L"\r\n") != std::wstring::npos || s.find(L'\0') != std::wstring::npos)
            throw std::invalid_argument("multiline command");
    if (o.mode < 0 || o.mode > 3 || !o.minimumMemory || o.minimumMemory > memory)
        throw std::invalid_argument("invalid launch options");
    std::wstring command;
    if (o.mode == 2) {
        // cmd.exe has its own quoting rules. Reject expansion/control characters in the target.
        if (o.target.empty() || o.target.find_first_of(L"\"%!&|<>^\r\n") != std::wstring::npos)
            throw std::invalid_argument("unsafe batch path");
        if (!o.arguments.empty())
            throw std::invalid_argument("put BAT arguments in the script");
        wchar_t system[MAX_PATH]{};
        GetSystemDirectoryW(system, MAX_PATH);
        command = quoteArgument(std::wstring(system) + L"\\cmd.exe") + L" /d /s /c \"\"" + o.target + L"\"\"";
    } else if (o.mode == 3) {
        if (o.target.empty())
            throw std::invalid_argument("missing executable");
        command = quoteArgument(o.target) + L" " + o.arguments;
    } else {
        command = quoteArgument(java) + L" -Xms" + std::to_wstring(o.minimumMemory) + L"M -Xmx" +
                  std::to_wstring(memory) +
                  L"M -Dfile.encoding=UTF-8 -Dstdout.encoding=UTF-8 -Dstderr.encoding=UTF-8 " + o.jvm;
        if (o.mode == 0)
            command += L" -Dterminal.jline=false -Dterminal.ansi=true -jar " + quoteArgument(jar.wstring()) +
                       (nogui ? L" nogui" : L"");
        command += L" " + o.arguments;
    }
    if (command.size() > 30000)
        throw std::invalid_argument("command too long");
    return command;
}
std::wstring newToken() {
    unsigned char bytes[32];
    if (BCryptGenRandom(nullptr, bytes, sizeof(bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
        throw std::runtime_error("random generator failed");
    constexpr wchar_t hex[] = L"0123456789abcdef";
    std::wstring value;
    for (auto b : bytes) {
        value += hex[b >> 4];
        value += hex[b & 15];
    }
    return value;
}
long long wallNow() {
    return static_cast<long long>(std::time(nullptr));
}
long long nextDaily(int minute, long long now) {
    if (minute < 0 || minute >= 1440)
        return 0;
    if (!now)
        now = wallNow();
    std::time_t t = static_cast<std::time_t>(now);
    std::tm local{};
    localtime_s(&local, &t);
    local.tm_hour = minute / 60;
    local.tm_min = minute % 60;
    local.tm_sec = 0;
    local.tm_isdst = -1;
    auto next = std::mktime(&local);
    if (next <= t) {
        ++local.tm_mday;
        local.tm_isdst = -1;
        next = std::mktime(&local);
    }
    return static_cast<long long>(next);
}
std::wstring localTimestamp() {
    SYSTEMTIME t{};
    GetLocalTime(&t);
    wchar_t value[40];
    swprintf_s(value, L"%04u-%02u-%02u %02u:%02u:%02u", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
               t.wSecond);
    return value;
}
namespace {
struct Form {
    HFONT font;
    const std::wstring &help;
    std::vector<FormField> &fields;
    std::function<std::wstring(const std::vector<FormField> &)> validate;
    FormPalette palette;
    HBRUSH background = nullptr, input = nullptr;
    bool accepted = false;
};
std::wstring read(HWND h) {
    std::wstring v(GetWindowTextLengthW(h) + 1, 0);
    v.resize(GetWindowTextW(h, v.data(), static_cast<int>(v.size())));
    return v;
}
LRESULT CALLBACK formInput(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR data) {
    auto f = reinterpret_cast<Form *>(data);
    if (m == WM_NCCALCSIZE) {
        auto result = DefSubclassProc(h, m, w, l);
        centerEditClient(h, w, l);
        return result;
    }
    if (m == WM_NCPAINT) {
        paintEditFrame(h, f->palette.input, GetFocus() == h ? f->palette.accent : f->palette.border);
        return 0;
    }
    auto result = DefSubclassProc(h, m, w, l);
    if (m == WM_SETFOCUS || m == WM_KILLFOCUS)
        InvalidateRect(h, nullptr, FALSE);
    if (m == WM_SIZE)
        SendMessageW(h, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(10, 10));
    if (m == WM_SETFONT)
        refreshEditFrame(h);
    if (m == WM_SETFOCUS || m == WM_KILLFOCUS)
        RedrawWindow(h, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME);
    return result;
}
LRESULT CALLBACK formProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    auto f = reinterpret_cast<Form *>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (m == WM_NCCREATE) {
        f = static_cast<Form *>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(f));
    }
    if (!f)
        return DefWindowProcW(h, m, w, l);
    if (m == WM_CREATE) {
        auto add = [&](const wchar_t *cls, const std::wstring &value, DWORD style, int id, int x, int y,
                       int width, int height) {
            HWND c = CreateWindowExW(0, cls, value.c_str(), WS_CHILD | WS_VISIBLE | style, x, y, width,
                                     height, h, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                     GetModuleHandleW(nullptr), nullptr);
            SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(f->font), TRUE);
            if (cls == std::wstring(L"EDIT")) {
                SendMessageW(c, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(10, 10));
                SetWindowSubclass(c, formInput, 1, reinterpret_cast<DWORD_PTR>(f));
                refreshEditFrame(c);
            }
            if (cls == std::wstring(L"COMBOBOX"))
                SetWindowTheme(c, f->palette.light ? L"Explorer" : L"DarkMode_CFD", nullptr);
            return c;
        };
        add(L"STATIC", f->help, 0, 90, 16, 12, 740, 64);
        int y = 84;
        for (size_t i = 0; i < f->fields.size(); ++i, y += 42) {
            auto &field = f->fields[i];
            add(L"STATIC", field.label, SS_LEFT, 200 + static_cast<int>(i), 16, y + 5, 235, 30);
            if (field.choices.empty()) {
                auto c = add(L"EDIT", field.value, WS_TABSTOP | ES_AUTOHSCROLL, 100 + static_cast<int>(i),
                             254, y, 500, 28);
                SendMessageW(c, EM_SETLIMITTEXT, 8192, 0);
            } else {
                auto c = add(L"COMBOBOX", L"",
                             WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS,
                             100 + static_cast<int>(i), 254, y, 500, 200);
                int selection = 0;
                for (size_t j = 0; j < field.choices.size(); ++j) {
                    SendMessageW(c, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(field.choices[j].c_str()));
                    if (field.value == field.choices[j])
                        selection = static_cast<int>(j);
                }
                SendMessageW(c, CB_SETCURSEL, selection, 0);
            }
        }
        add(L"STATIC", L"", SS_LEFT, 92, 16, y, 738, 42);
        add(L"BUTTON", L"저장 / 실행", WS_TABSTOP | BS_OWNERDRAW, IDOK, 494, y + 48, 126, 32);
        add(L"BUTTON", L"취소", WS_TABSTOP | BS_OWNERDRAW, IDCANCEL, 630, y + 48, 124, 32);
        return 0;
    }
    if (m == WM_ERASEBKGND)
        return 1;
    if (m == WM_PAINT) {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(h, &ps);
        RECT r{};
        GetClientRect(h, &r);
        FillRect(dc, &r, f->background);
        EndPaint(h, &ps);
        return 0;
    }
    if (m == WM_CTLCOLOREDIT || m == WM_CTLCOLORSTATIC || m == WM_CTLCOLORLISTBOX) {
        HDC dc = reinterpret_cast<HDC>(w);
        bool input = m != WM_CTLCOLORSTATIC;
        SetBkColor(dc, input ? f->palette.input : f->palette.background);
        SetTextColor(dc, GetDlgCtrlID(reinterpret_cast<HWND>(l)) == 92 ? RGB(231, 106, 120)
                         : input                                       ? f->palette.foreground
                                                                       : f->palette.muted);
        return reinterpret_cast<LRESULT>(input ? f->input : f->background);
    }
    if (m == WM_MEASUREITEM) {
        reinterpret_cast<MEASUREITEMSTRUCT *>(l)->itemHeight = 24;
        return TRUE;
    }
    if (m == WM_DRAWITEM) {
        const auto &item = *reinterpret_cast<DRAWITEMSTRUCT *>(l);
        HDC dc = item.hDC;
        RECT r = item.rcItem;
        SetBkMode(dc, TRANSPARENT);
        SelectObject(dc, f->font);
        if (item.CtlType == ODT_BUTTON) {
            bool primary = item.CtlID == IDOK;
            FillRect(dc, &r, f->background);
            roundedSurface(dc, r, primary ? f->palette.accent : f->palette.input,
                           (item.itemState & ODS_FOCUS) ? f->palette.accent : f->palette.border, 7);
            SetTextColor(dc, primary ? (f->palette.light ? RGB(255, 255, 255) : RGB(25, 29, 15))
                                     : f->palette.foreground);
            auto value = read(item.hwndItem);
            DrawTextW(dc, value.c_str(), -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        if (item.CtlType == ODT_COMBOBOX) {
            FillRect(dc, &r, f->input);
            SetTextColor(dc, f->palette.foreground);
            r.left += 10;
            if (item.itemID != static_cast<UINT>(-1)) {
                int n = static_cast<int>(SendMessageW(item.hwndItem, CB_GETLBTEXTLEN, item.itemID, 0));
                std::wstring value(n + 1, 0);
                SendMessageW(item.hwndItem, CB_GETLBTEXT, item.itemID,
                             reinterpret_cast<LPARAM>(value.data()));
                DrawTextW(dc, value.c_str(), n, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
            return TRUE;
        }
    }
    if (m == WM_COMMAND) {
        if (LOWORD(w) == IDCANCEL) {
            DestroyWindow(h);
            return 0;
        }
        if (LOWORD(w) == IDOK) {
            auto draft = f->fields;
            for (size_t i = 0; i < draft.size(); ++i)
                draft[i].value = read(GetDlgItem(h, 100 + static_cast<int>(i)));
            auto error = f->validate ? f->validate(draft) : L"";
            if (!error.empty()) {
                SetWindowTextW(GetDlgItem(h, 92), error.c_str());
                return 0;
            }
            f->fields = std::move(draft);
            f->accepted = true;
            DestroyWindow(h);
            return 0;
        }
    }
    if (m == WM_CLOSE) {
        DestroyWindow(h);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}
} // namespace
bool editForm(HWND owner, HFONT font, const std::wstring &title, const std::wstring &help,
              std::vector<FormField> &fields,
              std::function<std::wstring(const std::vector<FormField> &)> validate, FormPalette palette) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = formProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"WrapperOperationsForm";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    RegisterClassW(&wc);
    Form f{font, help, fields, std::move(validate), palette};
    f.background = CreateSolidBrush(palette.background);
    f.input = CreateSolidBrush(palette.input);
    RECT r{};
    GetWindowRect(owner, &r);
    MONITORINFO monitor{};
    monitor.cbSize = sizeof(monitor);
    GetMonitorInfoW(MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST), &monitor);
    int height = static_cast<int>(fields.size()) * 42 + 240;
    int x = std::clamp(r.left + 30, monitor.rcWork.left,
                       std::max(monitor.rcWork.left, monitor.rcWork.right - 790));
    int y = std::clamp(r.top + 20, monitor.rcWork.top,
                       std::max(monitor.rcWork.top, monitor.rcWork.bottom - height));
    auto h = CreateWindowExW(WS_EX_DLGMODALFRAME, wc.lpszClassName, title.c_str(), WS_CAPTION | WS_SYSMENU, x,
                             y, 790, height, owner, nullptr, wc.hInstance, &f);
    if (!h) {
        DeleteObject(f.background);
        DeleteObject(f.input);
        return false;
    }
    BOOL dark = !palette.light;
    DwmSetWindowAttribute(h, 20, &dark, sizeof(dark));
    DwmSetWindowAttribute(h, 35, &palette.background, sizeof(COLORREF));
    DwmSetWindowAttribute(h, 36, &palette.foreground, sizeof(COLORREF));
    EnableWindow(owner, FALSE);
    ShowWindow(h, SW_SHOW);
    SetFocus(GetDlgItem(h, 100));
    MSG msg{};
    while (IsWindow(h)) {
        int result = GetMessageW(&msg, nullptr, 0, 0);
        if (result <= 0) {
            if (!result)
                PostQuitMessage(static_cast<int>(msg.wParam));
            break;
        }
        if (!IsDialogMessageW(h, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (IsWindow(h))
        DestroyWindow(h);
    EnableWindow(owner, TRUE);
    SetActiveWindow(owner);
    DeleteObject(f.background);
    DeleteObject(f.input);
    return f.accepted;
}
std::wstring psLiteral(const std::wstring &v) {
    std::wstring s = L"'";
    for (auto c : v) {
        s += c;
        if (c == L'\'')
            s += c;
    }
    return s + L"'";
}
bool runPowerShell(const std::wstring &script) {
    DWORD size = 0;
    if (!CryptBinaryToStringW(reinterpret_cast<const BYTE *>(script.data()),
                              static_cast<DWORD>(script.size() * sizeof(wchar_t)),
                              CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &size))
        return false;
    std::wstring encoded(size, 0);
    CryptBinaryToStringW(reinterpret_cast<const BYTE *>(script.data()),
                         static_cast<DWORD>(script.size() * sizeof(wchar_t)),
                         CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, encoded.data(), &size);
    encoded.resize(size);
    wchar_t system[MAX_PATH]{};
    GetSystemDirectoryW(system, MAX_PATH);
    auto cmd = quoteArgument(std::wstring(system) + L"\\WindowsPowerShell\\v1.0\\powershell.exe") +
               L" -NoProfile -NonInteractive -EncodedCommand " + encoded;
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si,
                        &pi))
        return false;
    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    return code == 0;
}

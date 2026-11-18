#include "App.h"
namespace wrapper {
void App::openGroupManager() {
    if (groupWindow) {
        ShowWindow(groupWindow, SW_RESTORE);
        SetForegroundWindow(groupWindow);
        return;
    }
    WNDCLASSEXW cls{};
    cls.cbSize = sizeof(cls);
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpfnWndProc = groupManagerProc;
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    cls.lpszClassName = L"WrapperGroupManager";
    if (!GetClassInfoExW(cls.hInstance, cls.lpszClassName, &cls)) {
        cls.cbSize = sizeof(cls);
        cls.hInstance = GetModuleHandleW(nullptr);
        cls.lpfnWndProc = groupManagerProc;
        cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        cls.lpszClassName = L"WrapperGroupManager";
        if (!RegisterClassExW(&cls)) {
            error(L"그룹 관리자 창을 열지 못했습니다.");
            return;
        }
    }
    CreateWindowExW(WS_EX_CONTROLPARENT, cls.lpszClassName, tr(L"그룹 관리자").c_str(),
                    WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, S(980), S(680), window,
                    nullptr, cls.hInstance, this);
    if (groupWindow) {
        ShowWindow(groupWindow, SW_SHOW);
        SetForegroundWindow(groupWindow);
    }
}
void App::layoutGroupManager() {
    RECT rect{};
    GetClientRect(groupWindow, &rect);
    // Logical (96-DPI) coordinates, scaled on placement.
    int w = MulDiv(rect.right, 96, UiDpi), h = MulDiv(rect.bottom, 96, UiDpi);
    auto place = [&](int id, int x, int y, int width, int height) {
        MoveWindow(GetDlgItem(groupWindow, id), S(x), S(y), S(x + width) - S(x), S(y + height) - S(y), TRUE);
    };
    place(ManagerHeading, 24, 18, w - 268, 32);
    place(ManagerHint, 24, 54, w - 48, 42);
    place(ManagerGroups, 24, 112, 224, h - 228);
    place(ManagerMemberLabel, 270, 110, w - 294, 26);
    place(ManagerMembers, 270, 142, w - 294, h - 310);
    place(ManagerApply, 270, h - 154, 156, 34);
    place(ManagerStart, 438, h - 154, 136, 34);
    place(ManagerStop, 586, h - 154, 136, 34);
    place(ManagerStatus, w - 220, 24, 196, 26);
    place(ManagerNameLabel, 24, h - 96, 80, 28);
    place(ManagerName, 108, h - 98, std::min(440, w - 132), 32);
    place(ManagerCreate, 24, h - 50, 148, 34);
    place(ManagerRename, 184, h - 50, 132, 34);
    place(ManagerDelete, 328, h - 50, 148, 34);
    place(ManagerClose, w - 120, h - 50, 96, 34);
    InvalidateRect(groupWindow, nullptr, FALSE);
}
void App::refreshGroupManager(bool rebuild) {
    if (!groupWindow)
        return;
    auto ctl = [&](int id) { return GetDlgItem(groupWindow, id); };
    refreshGroups();
    if (rebuild || SendMessageW(ctl(ManagerGroups), LB_GETCOUNT, 0, 0) !=
                       static_cast<LRESULT>(serverGroups.size() + 1)) {
        SendMessageW(ctl(ManagerGroups), LB_RESETCONTENT, 0, 0);
        SendMessageW(ctl(ManagerGroups), LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(tr(L"전체 그룹").c_str()));
        size_t selection = 0;
        for (size_t i = 0; i < serverGroups.size(); ++i) {
            auto name = serverGroups[i] == L"기본 그룹" ? tr(serverGroups[i]) : serverGroups[i];
            SendMessageW(ctl(ManagerGroups), LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
            if (serverGroups[i] == selectedGroup)
                selection = i + 1;
        }
        SendMessageW(ctl(ManagerGroups), LB_SETCURSEL, selection, 0);
        SetWindowTextW(ctl(ManagerName), selectedGroup.c_str());
        SendMessageW(ctl(ManagerMembers), LB_RESETCONTENT, 0, 0);
    }
    auto count = SendMessageW(ctl(ManagerMembers), LB_GETCOUNT, 0, 0);
    bool reset = count != static_cast<LRESULT>(servers.size());
    auto top = SendMessageW(ctl(ManagerMembers), LB_GETTOPINDEX, 0, 0);
    std::vector<bool> selection(servers.size());
    for (size_t i = 0; i < servers.size(); ++i)
        selection[i] = reset ? (!selectedGroup.empty() && servers[i]->group == selectedGroup)
                             : SendMessageW(ctl(ManagerMembers), LB_GETSEL, i, 0) > 0;
    if (reset)
        SendMessageW(ctl(ManagerMembers), LB_RESETCONTENT, 0, 0);
    unsigned members = 0, running = 0;
    bool canStart = false, canStop = false;
    bool rowsChanged = reset;
    for (size_t i = 0; i < servers.size(); ++i) {
        const auto &server = *servers[i];
        if (selectedGroup.empty() || server.group == selectedGroup) {
            ++members;
            running += server.process.running();
            canStart |= !server.process.running();
            canStop |= server.process.running();
        }
        auto group = server.group == L"기본 그룹" ? tr(server.group) : server.group;
        auto state = tr(!server.process.running()   ? L"Offline"
                        : server.process.stopping() ? L"Stopping…"
                        : server.ready              ? L"Online"
                                                    : L"Starting…");
        auto value = server.name + L"   ·   " + group + L"   ·   " + state;
        bool changed = reset;
        if (!reset) {
            auto length = SendMessageW(ctl(ManagerMembers), LB_GETTEXTLEN, i, 0);
            std::wstring old(static_cast<size_t>(std::max<LRESULT>(0, length)) + 1, 0);
            SendMessageW(ctl(ManagerMembers), LB_GETTEXT, i, reinterpret_cast<LPARAM>(old.data()));
            old.resize(wcslen(old.c_str()));
            changed = old != value;
        }
        if (changed) {
            rowsChanged = true;
            if (!reset)
                SendMessageW(ctl(ManagerMembers), LB_DELETESTRING, i, 0);
            SendMessageW(ctl(ManagerMembers), LB_INSERTSTRING, i, reinterpret_cast<LPARAM>(value.c_str()));
        }
    }
    if (rowsChanged) {
        for (size_t i = 0; i < selection.size(); ++i)
            SendMessageW(ctl(ManagerMembers), LB_SETSEL, selection[i], i);
        SendMessageW(ctl(ManagerMembers), LB_SETTOPINDEX, std::max<LRESULT>(0, top), 0);
    }
    auto status = std::to_wstring(running) + L" / " + std::to_wstring(members) + L" " + tr(L"실행 중");
    if (text(ctl(ManagerStatus)) != status)
        SetWindowTextW(ctl(ManagerStatus), status.c_str());
    auto enable = [&](int id, bool state) {
        if ((IsWindowEnabled(ctl(id)) != FALSE) != state)
            EnableWindow(ctl(id), state);
    };
    enable(ManagerRename, !selectedGroup.empty() && selectedGroup != L"기본 그룹");
    enable(ManagerDelete, !selectedGroup.empty() && selectedGroup != L"기본 그룹");
    enable(ManagerApply, !selectedGroup.empty() && !closing);
    enable(ManagerStart, canStart && !closing);
    enable(ManagerStop, canStop && !closing);
}
void App::groupManagerAction(int id) {
    if (closing)
        return;
    auto ctl = [&](int value) { return GetDlgItem(groupWindow, value); };
    if (id == ManagerClose) {
        DestroyWindow(groupWindow);
        return;
    }
    if (id == ManagerStart || id == ManagerStop) {
        runGroup(id == ManagerStart);
        return;
    }
    auto before = groupRegistry;
    auto previousGroup = selectedGroup;
    std::vector<std::wstring> membership;
    for (const auto &server : servers)
        membership.push_back(server->group);
    auto name = GroupRegistry::normalize(text(ctl(ManagerName)));
    bool changed = false;
    if (id == ManagerCreate) {
        changed = groupRegistry.add(name);
        if (changed)
            selectedGroup = groupRegistry.canonical(name);
    } else if (id == ManagerRename) {
        auto old = selectedGroup;
        changed = groupRegistry.rename(old, name);
        if (changed) {
            selectedGroup = groupRegistry.canonical(name);
            for (auto &server : servers)
                if (server->group == old)
                    server->group = selectedGroup;
        }
    } else if (id == ManagerDelete) {
        if (selectedGroup.empty() || selectedGroup == L"기본 그룹")
            return;
        if (!confirm(L"그룹을 삭제할까요? 소속 서버는 기본 그룹으로 이동하며 서버 파일은 삭제하지 않습니다.",
                     groupWindow))
            return;
        auto old = selectedGroup;
        changed = groupRegistry.remove(old);
        if (changed) {
            for (auto &server : servers)
                if (server->group == old)
                    server->group = L"기본 그룹";
            selectedGroup = L"기본 그룹";
        }
    } else if (id == ManagerApply && !selectedGroup.empty()) {
        for (size_t i = 0; i < servers.size(); ++i) {
            if (SendMessageW(ctl(ManagerMembers), LB_GETSEL, i, 0) > 0)
                servers[i]->group = selectedGroup;
            else if (servers[i]->group == selectedGroup)
                servers[i]->group = L"기본 그룹";
        }
        changed = true;
    } else
        return;
    if (!changed) {
        error(
            L"그룹 이름을 확인하세요. 이름은 1~48자이며 중복될 수 없습니다. 기본 그룹은 변경할 수 없습니다.",
            groupWindow);
        return;
    }
    if (!saveSettings()) {
        groupRegistry = std::move(before);
        selectedGroup = previousGroup;
        for (size_t i = 0; i < servers.size(); ++i)
            servers[i]->group = membership[i];
        error(L"설정 저장에 실패했습니다. 실행 폴더 권한을 확인하세요.", groupWindow);
    }
    set(GroupName, current().group == L"기본 그룹" ? tr(current().group) : current().group);
    refreshServerList();
    refreshGroupManager(true);
    updateControls();
}
LRESULT CALLBACK App::groupManagerProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    auto *app = reinterpret_cast<App *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        app = static_cast<App *>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
        app->groupWindow = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (!app)
        return DefWindowProcW(hwnd, msg, w, l);
    switch (msg) {
    case WM_CREATE: {
        auto add = [&](const wchar_t *type, const wchar_t *label, DWORD style, int id) {
            auto display = app->tr(label);
            HWND child = CreateWindowExW(0, type, display.c_str(), WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0,
                                         hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                         GetModuleHandleW(nullptr), nullptr);
            SendMessageW(child, WM_SETFONT,
                         reinterpret_cast<WPARAM>(id == ManagerHeading ? app->heading : app->font), TRUE);
            SetWindowTheme(child, L"", L"");
            SetWindowSubclass(child, inputProc, 2, reinterpret_cast<DWORD_PTR>(app));
            return child;
        };
        add(L"STATIC", L"그룹 관리자", 0, ManagerHeading);
        add(L"STATIC", L"그룹을 선택하고 서버를 클릭해 소속을 지정하세요. 소속 적용을 누르면 저장됩니다.",
            SS_EDITCONTROL, ManagerHint);
        add(L"LISTBOX", L"",
            WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
            ManagerGroups);
        add(L"STATIC", L"서버 소속 · 클릭하여 선택 / 해제", 0, ManagerMemberLabel);
        add(L"LISTBOX", L"",
            WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_MULTIPLESEL | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS |
                LBS_NOINTEGRALHEIGHT,
            ManagerMembers);
        add(L"STATIC", L"그룹 이름", 0, ManagerNameLabel);
        auto edit = add(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, ManagerName);
        SendMessageW(edit, EM_SETLIMITTEXT, 48, 0);
        for (auto [id, label] : {std::pair{ManagerCreate, L"그룹 생성"},
                                 {ManagerRename, L"이름 변경"},
                                 {ManagerDelete, L"그룹 삭제"},
                                 {ManagerApply, L"소속 적용"},
                                 {ManagerStart, L"그룹 시작"},
                                 {ManagerStop, L"그룹 중지"},
                                 {ManagerClose, L"닫기"}})
            add(L"BUTTON", label, WS_TABSTOP | BS_OWNERDRAW, id);
        add(L"STATIC", L"", 0, ManagerStatus);
        app->refreshGroupManager(true);
        app->layoutGroupManager();
        BOOL dark = !app->lightTheme;
        DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
        return 0;
    }
    case WM_SIZE:
        app->layoutGroupManager();
        return 0;
    case WM_GETMINMAXINFO:
        reinterpret_cast<MINMAXINFO *>(l)->ptMinTrackSize = {S(870), S(580)};
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(w), code = HIWORD(w);
        if (id == ManagerGroups && code == LBN_SELCHANGE) {
            auto index = SendMessageW(GetDlgItem(hwnd, id), LB_GETCURSEL, 0, 0);
            app->selectedGroup = index > 0 && static_cast<size_t>(index - 1) < app->serverGroups.size()
                                     ? app->serverGroups[index - 1]
                                     : L"";
            app->refreshGroupManager(true);
            return 0;
        }
        if (code == BN_CLICKED)
            app->groupManagerAction(id);
        return 0;
    }
    case WM_MEASUREITEM:
        reinterpret_cast<MEASUREITEMSTRUCT *>(l)->itemHeight = S(32);
        return TRUE;
    case WM_DRAWITEM: {
        auto &item = *reinterpret_cast<DRAWITEMSTRUCT *>(l);
        if (item.CtlType == ODT_BUTTON)
            return app->drawItem(item);
        if (item.itemID == static_cast<UINT>(-1))
            return TRUE;
        auto length = SendMessageW(item.hwndItem, LB_GETTEXTLEN, item.itemID, 0);
        std::wstring label(static_cast<size_t>(std::max<LRESULT>(0, length)) + 1, 0);
        SendMessageW(item.hwndItem, LB_GETTEXT, item.itemID, reinterpret_cast<LPARAM>(label.data()));
        int saved = SaveDC(item.hDC);
        bool selected = (item.itemState & ODS_SELECTED) != 0;
        // Same selection treatment as the navigation view: subtle fill and a short accent pill.
        fill(item.hDC, item.rcItem, Panel);
        if (selected) {
            RECT row = item.rcItem;
            InflateRect(&row, -S(2), -S(1));
            fill(item.hDC, row, Subtle);
            int middle = (item.rcItem.top + item.rcItem.bottom) / 2;
            fill(item.hDC, {row.left, middle - S(8), row.left + S(3), middle + S(8)}, Accent);
        }
        RECT rect = item.rcItem;
        rect.left += S(14);
        rect.right -= S(8);
        SelectObject(item.hDC, app->font);
        SetBkMode(item.hDC, TRANSPARENT);
        SetTextColor(item.hDC, Foreground);
        DrawTextW(item.hDC, label.c_str(), -1, &rect,
                  DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        RestoreDC(item.hDC, saved);
        return TRUE;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN:
        return app->dispatch(msg, w, l);
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);
        RECT rect{};
        GetClientRect(hwnd, &rect);
        fill(dc, rect, Surface);
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        app->groupWindow = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, w, l);
}
} // namespace wrapper

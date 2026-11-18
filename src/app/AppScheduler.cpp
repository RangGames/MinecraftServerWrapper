#include "App.h"

namespace wrapper {

const wchar_t *App::taskName(int type) const {
    return type == 0 ? L"명령 전송" : type == 1 ? L"서버 재시작" : L"백업 (종료 후 재실행)";
}

void App::refreshTasks() {
    auto previous = listSelection(TaskList);
    auto now = GetTickCount64();
    auto &tasks = current().tasks;
    bool rebuild = taskRows.size() != tasks.size() ||
                   ListView_GetItemCount(control(TaskList)) != static_cast<int>(tasks.size());
    loading = true;
    if (rebuild) {
        ListView_DeleteAllItems(control(TaskList));
        taskRows.assign(tasks.size(), {});
    }
    for (size_t i = 0; i < tasks.size(); ++i) {
        const auto &task = tasks[i];
        auto remaining = task.minuteOfDay >= 0
                             ? static_cast<ULONGLONG>(std::max(0LL, task.nextWall - wallNow()))
                         : task.due > now ? (task.due - now + 999) / 1000
                                          : 0;
        std::vector<std::wstring> values{
            tr(taskName(task.type)),
            task.minuteOfDay < 0
                ? std::to_wstring(task.seconds) + tr(L"초")
                : L"매일 " + std::to_wstring(task.minuteOfDay / 60) + L":" +
                      (task.minuteOfDay % 60 < 10 ? L"0" : L"") + std::to_wstring(task.minuteOfDay % 60),
            task.enabled ? std::to_wstring(remaining) + tr(L"초 후") : tr(L"일시 중지"),
            task.command,
            task.lastRun,
            task.lastResult};
        for (size_t col = 0; col < values.size(); ++col) {
            if (!rebuild && col < taskRows[i].size() && taskRows[i][col] == values[col])
                continue;
            LVITEMW item{};
            item.mask = LVIF_TEXT;
            item.iItem = static_cast<int>(i);
            item.iSubItem = static_cast<int>(col);
            item.pszText = values[col].data();
            SendMessageW(control(TaskList), rebuild && col == 0 ? LVM_INSERTITEMW : LVM_SETITEMTEXTW,
                         rebuild && col == 0 ? 0 : i, reinterpret_cast<LPARAM>(&item));
        }
        taskRows[i] = std::move(values);
    }
    if (rebuild && previous >= 0 && static_cast<size_t>(previous) < tasks.size())
        ListView_SetItemState(control(TaskList), previous, LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
    loading = false;
    taskEditorState();
}

void App::addTask(bool update) {
    int index = listSelection(TaskList);
    if (update && (index < 0 || static_cast<size_t>(index) >= current().tasks.size()))
        return;
    auto seconds = text(control(TaskInterval));
    if (seconds.empty() || seconds.size() > 6 ||
        seconds.find_first_not_of(L"0123456789") != std::wstring::npos || std::stoul(seconds) < 1 ||
        std::stoul(seconds) > 864000) {
        error(L"작업 간격은 1~864000초로 입력하세요.");
        return;
    }
    if (!update && current().tasks.size() >= 256) {
        error(L"한 서버에 작업은 최대 256개까지 등록할 수 있습니다.");
        return;
    }
    Task task;
    task.type = static_cast<int>(SendMessageW(control(TaskType), CB_GETCURSEL, 0, 0));
    task.seconds = static_cast<unsigned>(std::stoul(seconds));
    task.command = text(control(TaskCommand));
    if (task.type < 0 || (task.type == 0 && (task.command.empty() ||
                                             task.command.find_first_of(L"\r\n") != std::wstring::npos ||
                                             utf8(task.command).size() > 2047))) {
        error(L"실행할 한 줄 명령을 입력하세요 (UTF-8 2047바이트 이내).");
        return;
    }
    if (task.command.starts_with(L"/"))
        task.command.erase(0, 1);
    if (task.type == 0 && task.command.empty()) {
        error(L"실행할 명령을 입력하세요.");
        return;
    }
    if (task.type != 0)
        task.command.clear();
    task.due = GetTickCount64() + task.seconds * 1000ULL;
    if (update) {
        task.enabled = current().tasks[index].enabled;
        task.minuteOfDay = current().tasks[index].minuteOfDay;
        task.nextWall = nextDaily(task.minuteOfDay);
        current().tasks[index] = std::move(task);
    } else
        current().tasks.push_back(std::move(task));
    saveSettings();
    refreshTasks();
}

void App::runTask(size_t owner, Task &task) {
    auto &server = *servers[owner];
    task.lastRun = localTimestamp();
    if (overlapsBackup(server)) {
        task.lastResult = L"건너뜀: 백업 진행 중";
        task.due = GetTickCount64() + task.seconds * 1000ULL;
        task.nextWall = nextDaily(task.minuteOfDay);
        saveSettings();
        return;
    }
    if (task.type == 0) {
        if (task.command == L"stop" || task.command == L"end" || task.command == L"shutdown")
            server.intentionalStop = true;
        bool sent = (task.command == L"stop" || task.command == L"end" || task.command == L"shutdown")
                        ? server.process.stop(server.kind == L"Velocity" ? L"shutdown"
                                              : (server.kind == L"Waterfall" || server.kind == L"BungeeCord")
                                                  ? L"end"
                                                  : L"stop")
                        : server.process.send(task.command);
        task.lastResult = sent ? L"명령 전달" : L"건너뜀: 서버 중지 또는 대기열 가득 참";
        append(server, sent ? L"[Scheduler] > " + task.command + L"\r\n"
                            : L"[Scheduler] 명령 건너뜀: 서버 중지 또는 전송 대기열을 확인하세요.\r\n");
    } else if (task.type == 1) {
        if (server.process.running()) {
            stopServer(server, true);
            task.lastResult = L"재시작 요청";
        } else {
            task.lastResult = L"건너뜀: 서버 중지";
            append(server, L"[Scheduler] 재시작 건너뜀: 서버가 중지되어 있습니다.\r\n");
        }
    } else {
        runBackup(owner, false);
        task.lastResult = (server.backupAfterStop || (backupPending && backupOwner == owner))
                              ? L"백업 요청"
                              : L"백업 실패 또는 건너뜀: 콘솔 확인";
    }
    task.due = GetTickCount64() + task.seconds * 1000ULL;
    task.nextWall = nextDaily(task.minuteOfDay);
    saveSettings();
}

void App::taskAction(int id) {
    int index = listSelection(TaskList);
    auto &tasks = current().tasks;
    if (index < 0 || static_cast<size_t>(index) >= tasks.size())
        return;
    if (id == TaskDelete)
        tasks.erase(tasks.begin() + index);
    else if (id == TaskToggle) {
        tasks[index].enabled = !tasks[index].enabled;
        tasks[index].nextWall = nextDaily(tasks[index].minuteOfDay);
        tasks[index].due = GetTickCount64() + tasks[index].seconds * 1000ULL;
    } else
        runTask(selected, tasks[index]);
    saveSettings();
    refreshTasks();
}

void App::taskEditorState() {
    int type = static_cast<int>(SendMessageW(control(TaskType), CB_GETCURSEL, 0, 0));
    enable(TaskCommand, type == 0);
    set(TaskSummary,
        type == 0 ? L"입력한 초 간격으로 명령을 전송합니다. 예: 300초는 5분마다 실행합니다.\n서버가 꺼져 "
                    L"있으면 실행하지 않습니다."
        : type == 1 ? L"입력한 초 간격으로 월드를 저장한 뒤 서버를 껐다 켭니다.\n이미 꺼져 있는 서버는 "
                      L"켜지지 않습니다."
                    : L"입력한 초 간격으로 서버 폴더를 백업합니다. 실행 중이면 정상 종료 후 백업하고 다시 "
                      L"시작합니다.\n먼저 백업 "
                      L"화면에서 저장 위치를 지정하세요.");
    enable(TaskUpdate, listSelection(TaskList) >= 0);
}

void App::loadTaskEditor() {
    int index = listSelection(TaskList);
    if (index < 0 || static_cast<size_t>(index) >= current().tasks.size()) {
        taskEditorState();
        return;
    }
    auto &task = current().tasks[index];
    SendMessageW(control(TaskType), CB_SETCURSEL, task.type, 0);
    set(TaskInterval, std::to_wstring(task.seconds));
    set(TaskCommand, task.command);
    taskEditorState();
}

} // namespace wrapper

namespace wrapper {
void App::refreshPresets() {
    auto previous = SendMessageW(control(TaskPreset), CB_GETCURSEL, 0, 0);
    SendMessageW(control(TaskPreset), CB_RESETCONTENT, 0, 0);
    const wchar_t *names[]{L"간격 프리셋…", L"1분마다",   L"5분마다", L"15분마다",
                           L"1시간마다",    L"6시간마다", L"매일"};
    constexpr unsigned values[]{0, 60, 300, 900, 3600, 21600, 86400};
    for (int i = 0; i < 7; ++i) {
        SendMessageW(control(TaskPreset), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(tr(names[i]).c_str()));
        SendMessageW(control(TaskPreset), CB_SETITEMDATA, i, values[i]);
    }
    for (size_t i = 0; i < intervalPresets.size(); ++i) {
        const auto &[name, seconds] = intervalPresets[i];
        auto label = name + L" · " + std::to_wstring(seconds) + (english ? L" sec" : L"초");
        auto index =
            SendMessageW(control(TaskPreset), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        SendMessageW(control(TaskPreset), CB_SETITEMDATA, index, seconds);
    }
    auto count = SendMessageW(control(TaskPreset), CB_GETCOUNT, 0, 0);
    SendMessageW(control(TaskPreset), CB_SETCURSEL, previous >= 0 && previous < count ? previous : 0, 0);
    enable(PresetDelete, previous >= 7 && previous < count);
}
void App::savePreset() {
    auto name = text(control(PresetName)), seconds = text(control(TaskInterval));
    if (name.empty() || name.size() > 48 || name.find_first_of(L"\r\n") != std::wstring::npos) {
        error(L"프리셋 이름을 1~48자로 입력하세요.");
        return;
    }
    if (seconds.empty() || seconds.size() > 6 ||
        seconds.find_first_not_of(L"0123456789") != std::wstring::npos || std::stoul(seconds) < 1 ||
        std::stoul(seconds) > 864000) {
        error(L"작업 간격은 1~864000초로 입력하세요.");
        return;
    }
    auto found = std::find_if(intervalPresets.begin(), intervalPresets.end(),
                              [&](const auto &preset) { return preset.first == name; });
    if (found == intervalPresets.end()) {
        if (intervalPresets.size() >= 32) {
            error(L"사용자 프리셋은 최대 32개까지 저장할 수 있습니다.");
            return;
        }
        intervalPresets.emplace_back(name, static_cast<unsigned>(std::stoul(seconds)));
        found = intervalPresets.end() - 1;
    } else
        found->second = static_cast<unsigned>(std::stoul(seconds));
    auto index = std::distance(intervalPresets.begin(), found) + 7;
    saveSettings();
    refreshPresets();
    SendMessageW(control(TaskPreset), CB_SETCURSEL, index, 0);
    enable(PresetDelete, true);
}
void App::deletePreset() {
    auto index = SendMessageW(control(TaskPreset), CB_GETCURSEL, 0, 0);
    if (index < 7 || static_cast<size_t>(index - 7) >= intervalPresets.size())
        return;
    intervalPresets.erase(intervalPresets.begin() + index - 7);
    saveSettings();
    SendMessageW(control(TaskPreset), CB_SETCURSEL, 0, 0);
    refreshPresets();
    set(PresetName, L"");
}
} // namespace wrapper

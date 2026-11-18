#include "App.h"
namespace wrapper {
void App::refreshGroups() {
    serverGroups = groupRegistry.names();
    if (!selectedGroup.empty() && groupRegistry.canonical(selectedGroup).empty())
        selectedGroup.clear();
}

void App::runGroup(bool start) {
    if (closing)
        return;
    unsigned failures = 0;
    groupOperation = true;
    for (auto &server : servers) {
        if (!selectedGroup.empty() && server->group != selectedGroup)
            continue;
        if (start) {
            if (!server->process.running()) {
                server->recoveryAttempts = 0;
                if (!startServer(*server, false))
                    ++failures;
            }
        } else {
            cancelAutomaticWork(*server);
            if (server->process.running())
                stopServer(*server);
        }
    }
    groupOperation = false;
    saveSettings();
    refreshServerList();
    updateControls();
    refreshGroupManager();
    if (failures)
        error(L"일부 서버를 시작하지 못했습니다. 각 서버의 콘솔과 경로 설정을 확인하세요.", groupWindow);
}
} // namespace wrapper

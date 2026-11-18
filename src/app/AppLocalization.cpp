#include "App.h"
namespace wrapper {
std::wstring App::tr(const std::wstring &value) const {
    return localize(value, english);
}
void App::applyLanguage() {
    displayedText.clear();
    taskRows.clear();
    for (const auto &[id, label] : labels)
        SetWindowTextW(control(id), tr(label).c_str());
    for (const auto &[id, columns] : columnLabels)
        for (size_t i = 0; i < columns.size(); ++i) {
            auto value = tr(columns[i]);
            LVCOLUMNW column{};
            column.mask = LVCF_TEXT;
            column.pszText = value.data();
            SendMessageW(control(id), LVM_SETCOLUMNW, i, reinterpret_cast<LPARAM>(&column));
        }
    auto combo = [&](int id, const std::vector<std::wstring> &values) {
        auto previous = SendMessageW(control(id), CB_GETCURSEL, 0, 0);
        SendMessageW(control(id), CB_RESETCONTENT, 0, 0);
        for (const auto &label : values)
            SendMessageW(control(id), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(tr(label).c_str()));
        SendMessageW(control(id), CB_SETCURSEL, std::max<LRESULT>(0, previous), 0);
    };
    combo(TaskType, {L"명령 전송", L"서버 재시작", L"백업 (종료 후 재실행)"});
    refreshPresets();
    serverGroups.clear();
    refreshGroups();
    set(GroupName, current().group == L"기본 그룹" ? tr(current().group) : current().group);
    SendMessageW(control(PresetName), EM_SETCUEBANNER, TRUE,
                 reinterpret_cast<LPARAM>(tr(L"프리셋 이름").c_str()));
    combo(ModeValue, {L"생존", L"크리에이티브", L"모험", L"관전"});
    combo(DifficultyValue, {L"평화로움", L"쉬움", L"보통", L"어려움"});
    combo(ThemeChoice, {L"다크", L"라이트", L"시스템 설정 따르기"});
    combo(BundledFontChoice, {L"나눔고딕 (기본)", L"Noto Sans KR", L"기타 시스템 글꼴"});
    SetWindowTextW(window, tr(L"Minecraft Server Manager").c_str());
    if (groupWindow) {
        SetWindowTextW(groupWindow, tr(L"그룹 관리자").c_str());
        for (auto [id, label] :
             {std::pair{ManagerHeading, L"그룹 관리자"},
              {ManagerHint,
               L"그룹을 선택하고 서버를 클릭해 소속을 지정하세요. 소속 적용을 누르면 저장됩니다."},
              {ManagerMemberLabel, L"서버 소속 · 클릭하여 선택 / 해제"},
              {ManagerNameLabel, L"그룹 이름"},
              {ManagerCreate, L"그룹 생성"},
              {ManagerRename, L"이름 변경"},
              {ManagerDelete, L"그룹 삭제"},
              {ManagerApply, L"소속 적용"},
              {ManagerStart, L"그룹 시작"},
              {ManagerStop, L"그룹 중지"},
              {ManagerClose, L"닫기"}})
            SetWindowTextW(GetDlgItem(groupWindow, id), tr(label).c_str());
        auto list = GetDlgItem(groupWindow, ManagerGroups);
        auto selectedRow = SendMessageW(list, LB_GETCURSEL, 0, 0);
        SendMessageW(list, LB_RESETCONTENT, 0, 0);
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(tr(L"전체 그룹").c_str()));
        for (const auto &name : serverGroups) {
            auto label = name == L"기본 그룹" ? tr(name) : name;
            SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        }
        SendMessageW(list, LB_SETCURSEL, selectedRow, 0);
        refreshGroupManager();
    }
    taskEditorState();
    refreshPageData();
    // These are composed at runtime, so rebuild them in the new language.
    validatePathFields();
    set(FontInfo, fontFace + L" · " + std::to_wstring(fontPoints) + L" pt · " +
                      tr(fontFace == L"NanumGothic" || fontFace == L"Noto Sans KR" ? L"기본 제공" : L"시스템 글꼴"));
    set(ProfileImage, draftImage.empty() ? tr(L"자동: server-icon.png (없으면 기본 아이콘)") : draftImage);
    InvalidateRect(window, nullptr, TRUE);
}
} // namespace wrapper

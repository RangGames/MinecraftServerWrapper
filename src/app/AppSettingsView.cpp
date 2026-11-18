#include "App.h"

namespace wrapper {
void App::createSettings() {
    add(L"STATIC", L"서버 설정", 0, SettingsHeading);
    styleFont(SettingsHeading, heading);
    add(L"STATIC",
        L"기본 JAR 실행과 고급 Java 인자·BAT·EXE 실행을 지원합니다. 기본 서버 파일은 JAR이 있는 폴더에 "
        L"저장됩니다.",
        0, SettingsNote);
    add(L"STATIC", L"서버 그룹", 0, GroupNameLabel);
    add(L"EDIT", L"", ES_READONLY | ES_AUTOHSCROLL, GroupName);
    SendMessageW(control(GroupName), EM_SETLIMITTEXT, 48, 0);
    add(L"STATIC", L"서버 이름", 0, 205);
    add(L"STATIC", L"Java 실행 파일", 0, 201);
    add(L"STATIC", L"서버 JAR", 0, 202);
    add(L"STATIC", L"최대 메모리", 0, 203);
    add(L"STATIC", L"MB  ·  최소 할당 512 MB", 0, 204);
    add(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, ServerName);
    add(L"EDIT", L"java.exe", WS_TABSTOP | ES_AUTOHSCROLL, Java);
    add(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, Jar);
    add(L"EDIT", L"2048", WS_TABSTOP | ES_NUMBER, Memory);
    button(L"찾기…", BrowseJava);
    button(L"Java 탐색", JavaDetect);
    button(L"고급 실행 · 복구", Advanced);
    button(L"설치 도우미", Installer);
    button(L"JAR 선택", BrowseJar);
    button(L"서버 폴더", ChooseServerFolder);
    add(L"EDIT", L"", ES_READONLY | ES_AUTOHSCROLL, ProfileImage);
    add(L"STATIC", L"프로필 이미지", 0, ImageNote);
    button(L"이미지 선택", ChooseImage);
    button(L"자동 이미지", ResetImage);
    add(L"STATIC",
        L"JAR·Java·이미지 또는 서버 폴더를 창에 끌어 놓으세요. 환경 변수와 따옴표 경로도 지원합니다.",
        SS_EDITCONTROL, PathNote);

    add(L"STATIC", L"", SS_EDITCONTROL, PathValidity);
    button(L"글꼴 선택…", FontChoose);
    add(L"STATIC", L"", 0, FontInfo);
    button(L"설정 저장", SaveSettings);
    button(L"프로필 제거", RemoveServer);
    button(L"그룹 관리…", GroupManageSettings);

    for (int i = 0; i < 4; ++i) {
        const wchar_t *names[]{L"일반", L"실행", L"서버 속성", L"화면·언어"};
        button(names[i], SettingsGeneral + i);
    }
    add(L"STATIC", L"", 0, SectionTitle);
    add(L"STATIC", L"", SS_EDITCONTROL, SectionHint);
    styleFont(SectionTitle, heading);
    add(L"STATIC", L"표시 언어", 0, LanguageLabel);
    add(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST, LanguageChoice);
    SendMessageW(control(LanguageChoice), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"한국어"));
    SendMessageW(control(LanguageChoice), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"English"));
    SendMessageW(control(LanguageChoice), CB_SETCURSEL, english ? 1 : 0, 0);
    add(L"STATIC", L"서버 포트", 0, PortLabel);
    add(L"EDIT", L"25565", WS_TABSTOP | ES_NUMBER, PortValue);
    add(L"STATIC", L"최대 플레이어", 0, MaxPlayersLabel);
    add(L"EDIT", L"20", WS_TABSTOP | ES_NUMBER, MaxPlayersValue);
    add(L"STATIC", L"게임 모드", 0, ModeLabel);
    add(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST, ModeValue);
    add(L"STATIC", L"난이도", 0, DifficultyLabel);
    add(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST, DifficultyValue);
    for (const auto *label : {L"생존", L"크리에이티브", L"모험", L"관전"})
        SendMessageW(control(ModeValue), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(tr(label).c_str()));
    for (const auto *label : {L"평화로움", L"쉬움", L"보통", L"어려움"})
        SendMessageW(control(DifficultyValue), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(tr(label).c_str()));
    check(L"정품 인증 (온라인 모드)", OnlineMode, true);
    check(L"화이트리스트 사용", Whitelist, false);
    check(L"플레이어 간 전투 (PvP)", Pvp, true);
    check(L"비행 허용", Flight, false);
    check(L"커맨드 블록 허용", CommandBlocks, false);
    button(L"파일에서 다시 읽기", PropertiesReload);
    add(L"STATIC", L"테마", 0, ThemeLabel);
    add(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST, ThemeChoice);
    for (const auto *label : {L"다크", L"라이트", L"시스템 설정 따르기"})
        SendMessageW(control(ThemeChoice), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(tr(label).c_str()));
    SendMessageW(control(ThemeChoice), CB_SETCURSEL, theme, 0);
    add(L"STATIC", L"기본 글꼴", 0, BundledFontLabel);
    add(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST, BundledFontChoice);
    for (const auto *label : {L"나눔고딕 (기본)", L"Noto Sans KR", L"기타 시스템 글꼴"})
        SendMessageW(control(BundledFontChoice), CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(tr(label).c_str()));
    SendMessageW(control(BundledFontChoice), CB_SETCURSEL,
                 fontFace == L"NanumGothic"    ? 0
                 : fontFace == L"Noto Sans KR" ? 1
                                               : 2,
                 0);
}

void App::loadProperties() {
    loading = true;
    propertiesDirty = false;
    auto directory = serverDirectory(current());
    ServerProperties properties;
    if (!directory.empty() && !properties.load(directory / L"server.properties")) {
        set(SectionHint, L"서버 속성을 읽지 못했습니다. 파일 권한을 확인하세요.");
        loading = false;
        return;
    }
    auto widen = [](const std::string &value) { return std::wstring(value.begin(), value.end()); };
    set(PortValue, widen(properties.get("server-port", "25565")));
    set(MaxPlayersValue, widen(properties.get("max-players", "20")));
    const std::string modes[]{"survival", "creative", "adventure", "spectator"},
        difficulties[]{"peaceful", "easy", "normal", "hard"};
    auto mode = properties.get("gamemode", "survival"), difficulty = properties.get("difficulty", "easy");
    int modeIndex = 0, difficultyIndex = 1;
    for (int i = 0; i < 4; ++i) {
        if (mode == modes[i] || mode == std::to_string(i))
            modeIndex = i;
        if (difficulty == difficulties[i] || difficulty == std::to_string(i))
            difficultyIndex = i;
    }
    SendMessageW(control(ModeValue), CB_SETCURSEL, modeIndex, 0);
    SendMessageW(control(DifficultyValue), CB_SETCURSEL, difficultyIndex, 0);
    for (auto [id, key, fallback] : {std::tuple{OnlineMode, "online-mode", "true"},
                                     {Whitelist, "white-list", "false"},
                                     {Pvp, "pvp", "true"},
                                     {Flight, "allow-flight", "false"},
                                     {CommandBlocks, "enable-command-block", "false"}})
        SendMessageW(control(id), BM_SETCHECK,
                     properties.get(key, fallback) == "true" ? BST_CHECKED : BST_UNCHECKED, 0);
    loading = false;
}
bool App::saveProperties() {
    refreshServerDirectory(current());
    if (current().kind == L"Velocity" || current().kind == L"Waterfall" || current().kind == L"BungeeCord") {
        error(
            L"프록시는 자체 설정 파일을 사용합니다. 서버 속성 편집 대신 서버 폴더의 설정 파일을 수정하세요.");
        return false;
    }
    if (overlapsBackup(current())) {
        error(L"백업이 끝난 뒤 서버 속성을 저장하세요. 실행 중인 서버에는 다음 재시작부터 적용됩니다.");
        return false;
    }
    auto directory = serverDirectory(current());
    if (directory.empty()) {
        error(L"서버 JAR을 먼저 지정하세요.");
        return false;
    }
    auto port = text(control(PortValue)), players = text(control(MaxPlayersValue));
    auto number = [](const std::wstring &value, unsigned maximum) {
        return !value.empty() && value.size() < 7 &&
               value.find_first_not_of(L"0123456789") == std::wstring::npos && std::stoul(value) > 0 &&
               std::stoul(value) <= maximum;
    };
    if (!number(port, 65535) || !number(players, 100000)) {
        error(L"포트는 1~65535, 최대 플레이어는 1~100000 사이의 정수로 입력하세요.");
        return false;
    }
    auto mode = SendMessageW(control(ModeValue), CB_GETCURSEL, 0, 0),
         difficulty = SendMessageW(control(DifficultyValue), CB_GETCURSEL, 0, 0);
    if (mode < 0 || mode > 3 || difficulty < 0 || difficulty > 3)
        return false;
    const std::string modes[]{"survival", "creative", "adventure", "spectator"},
        difficulties[]{"peaceful", "easy", "normal", "hard"};
    std::map<std::string, std::string> changes{{"server-port", utf8(port)},
                                               {"max-players", utf8(players)},
                                               {"gamemode", modes[mode]},
                                               {"difficulty", difficulties[difficulty]}};
    for (auto [id, key] : {std::pair{OnlineMode, "online-mode"},
                           {Whitelist, "white-list"},
                           {Pvp, "pvp"},
                           {Flight, "allow-flight"},
                           {CommandBlocks, "enable-command-block"}})
        changes[key] = SendMessageW(control(id), BM_GETCHECK, 0, 0) == BST_CHECKED ? "true" : "false";
    ServerProperties properties;
    if (!properties.load(directory / L"server.properties") ||
        !properties.save(directory / L"server.properties", changes)) {
        error(L"서버 속성을 저장하지 못했습니다. 권한과 파일 내용을 확인하세요.");
        return false;
    }
    propertiesDirty = false;
    return true;
}
} // namespace wrapper

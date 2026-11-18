#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <richedit.h>
#include "ConsoleColors.h"
#include "ServerProperties.h"
#include "PluginCatalog.h"
#include "JarArchive.h"
#include "Localization.h"
#include <map>
#include <unordered_map>
#include "PathUtils.h"
#include "PathValidation.h"
#include "GroupRegistry.h"
#include "UiMessageBox.h"
#include "SettingsSnapshot.h"
#include "UiIcons.h"
#include "UiScale.h"
#include <shellapi.h>
#include <shlobj.h>
#include <uxtheme.h>
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "PlayerFaces.h"
#include "PlayerLog.h"
#include "ServerProcess.h"
#include "BackupJob.h"
#include "Operations.h"
#include "RemotePanel.h"

namespace fs = std::filesystem;
namespace wrapper {
// Fluent-style palette; App::applyTheme() rewrites these for dark/light.
// Background: sidebar and title bar. Surface: content layer. Panel: cards. Field: inputs.
inline COLORREF Background = RGB(32, 32, 32), Surface = RGB(39, 39, 39), Panel = RGB(45, 45, 45);
inline COLORREF Border = RGB(62, 62, 62), CardStroke = RGB(53, 53, 53), Foreground = RGB(240, 240, 240),
                Muted = RGB(168, 168, 168);
inline COLORREF Field = RGB(52, 52, 52), Control = RGB(55, 55, 55), ControlHover = RGB(63, 63, 63),
                Subtle = RGB(58, 58, 58);
inline COLORREF Accent = RGB(96, 205, 255), OnAccent = RGB(0, 0, 0), Danger = RGB(255, 153, 164),
                Success = RGB(108, 203, 95), Caution = RGB(252, 185, 0);
inline COLORREF Blue = RGB(96, 205, 255), Purple = RGB(180, 160, 255), Green = RGB(108, 203, 95);
constexpr int SidebarWidth = 240;
inline COLORREF blend(COLORREF a, COLORREF b, double t) {
    auto mix = [t](int x, int y) { return static_cast<int>(x + (y - x) * t + 0.5); };
    return RGB(mix(GetRValue(a), GetRValue(b)), mix(GetGValue(a), GetGValue(b)), mix(GetBValue(a), GetBValue(b)));
}
enum Id {
    WelcomeTitle = 900,
    WelcomeBody,
    WelcomeSteps,
    WelcomeAction,
    Advanced = 800,
    JavaDetect,
    Installer,
    RemoteSettings,
    BackupOptions,
    BackupRestore,
    TaskTiming,
    PlayerManage,
    PluginManage,
    GroupActions,
    ExtraProperties,
    Java = 100,
    BrowseJava,
    Jar,
    BrowseJar,
    Memory,
    Eula,
    EulaLink,
    Start,
    Stop,
    Kill,
    Folder,
    Clear,
    Command,
    Send,
    Console,
    Status,
    Restart = 250,
    ServerSelector = 260,
    AddServer,
    RemoveServer,
    SaveSettings,
    ServerName,
    DashboardTab = 300,
    ConsoleTab,
    PluginsTab,
    PlayersTab,
    BackupsTab,
    SchedulerTab,
    SettingsTab,
    Wrap = 330,
    AutoScroll,
    SearchText,
    FindNext,
    ExportLog,
    PluginList = 350,
    PluginRefresh,
    PluginImport,
    PluginToggle,
    PluginFolder,
    BackupList = 370,
    BackupNow,
    BackupFolder,
    BackupChoose,
    BackupPath,
    BackupInfo,
    TaskList = 390,
    TaskType,
    TaskInterval,
    TaskCommand,
    TaskAdd,
    TaskDelete,
    TaskToggle,
    TaskRun,
    CpuValue = 420,
    RamValue,
    UptimeValue,
    Graph,
    ServerTitle,
    ServerMeta,
    PlayerKick = 440,
    PlayerOp,
    PlayerDeop,
    PlayerBan,
    PlayerInfo,
    MenuFile = 460,
    MenuServer,
    MenuTools,
    MenuHelp,
    RailHome,
    RailConsole,
    RailSettings,
    CpuTitle = 480,
    RamTitle,
    UptimeTitle,
    Footer,
    GraphTitle,
    ServerHint,
    SettingsHeading = 500,
    SettingsNote,
    PluginHeading,
    PluginNote,
    BackupHeading,
    BackupNote,
    TaskHeading,
    TaskNote,
    TaskTypeLabel,
    TaskIntervalLabel,
    TaskCommandLabel,
    ProfileImage = 520,
    ChooseImage,
    ResetImage,
    ImageNote,
    ChooseServerFolder,
    PathNote,
    FontChoose,
    FontInfo,
    TaskUpdate,
    TaskPreset,
    TaskSummary,
    SettingsGeneral = 550,
    SettingsLaunch,
    SettingsProperties,
    SettingsAppearance,
    SectionTitle,
    SectionHint,
    LanguageLabel,
    LanguageChoice,
    PortLabel,
    PortValue,
    MaxPlayersLabel,
    MaxPlayersValue,
    ModeLabel,
    ModeValue,
    DifficultyLabel,
    DifficultyValue,
    OnlineMode,
    Whitelist,
    Pvp,
    Flight,
    CommandBlocks,
    PropertiesReload,
    PropertiesSave,
    ThemeLabel,
    ThemeChoice,
    BundledFontLabel,
    BundledFontChoice,
    PresetName = 580,
    PresetSave,
    PresetDelete,
    PathValidity,
    GroupNameLabel = 590,
    GroupName,
    GroupManager = 595,
    GroupManageSettings,
    ManagerGroups = 720,
    ManagerName,
    ManagerMembers,
    ManagerCreate,
    ManagerRename,
    ManagerDelete,
    ManagerApply,
    ManagerStart,
    ManagerStop,
    ManagerClose,
    ManagerHeading,
    ManagerHint,
    ManagerMemberLabel,
    ManagerNameLabel,
    ManagerStatus
};
constexpr int PageCount = 7;
inline const wchar_t *Pages[PageCount]{L"Dashboard", L"Console",   L"Plugins", L"Players",
                                       L"Backups",   L"Scheduler", L"Settings"};

inline std::wstring text(HWND hwnd) {
    std::wstring value(GetWindowTextLengthW(hwnd) + 1, 0);
    value.resize(GetWindowTextW(hwnd, value.data(), static_cast<int>(value.size())));
    return value;
}
inline std::string utf8(const std::wstring &value) {
    int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0,
                                   nullptr, nullptr);
    std::string bytes(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), bytes.data(), size, nullptr,
                        nullptr);
    return bytes;
}
inline std::wstring decimal(double value, int digits = 1) {
    std::wostringstream out;
    out << std::fixed << std::setprecision(digits) << value;
    return out.str();
}
inline std::wstring duration(unsigned long long seconds) {
    std::wostringstream out;
    out << seconds / 3600 << L"h " << std::setw(2) << std::setfill(L'0') << seconds / 60 % 60 << L"m "
        << std::setw(2) << seconds % 60 << L"s";
    return out.str();
}
inline void fill(HDC dc, RECT rect, COLORREF color) {
    SetDCBrushColor(dc, color);
    FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}
struct Task {
    int type = 0;
    unsigned seconds = 300;
    std::wstring command;
    bool enabled = true;
    ULONGLONG due = 0;
    int minuteOfDay = -1;
    long long nextWall = 0;
    std::wstring lastResult = L"대기", lastRun;
};
struct Sample {
    double cpu, ram, players;
};
struct Server {
    std::wstring name = L"My Minecraft Server", java = L"java.exe", jar, backup;
    std::wstring group = L"기본 그룹";
    unsigned memory = 2048;
    LaunchOptions launch;
    bool autoStart = false, autoRecover = false, intentionalStop = false;
    unsigned recoveryDelay = 15, recoveryLimit = 5, recoveryAttempts = 0;
    ULONGLONG recoverAt = 0;
    bool backupAfterStop = false, resumeAfterBackup = false;
    bool zipBackup = true;
    unsigned backupKeep = 10;
    std::wstring id;
    std::wstring image, kind = L"Unknown";
    HBITMAP thumbnail = nullptr;
    ~Server() {
        if (thumbnail)
            DeleteObject(thumbnail);
    }
    fs::path canonicalDirectory, activeDirectory;
    ServerProcess process;
    struct Span {
        size_t start, length;
        COLORREF color;
    };
    std::vector<Span> spans;
    ConsoleColors colors;
    std::wstring log, line;
    bool droppingLine = false, ready = false, restart = false, eulaAccepted = false;
    int paintedState = -1;
    std::vector<std::wstring> players;
    std::vector<Sample> samples;
    std::vector<Task> tasks;
};

class App {
    HWND window = nullptr, groupWindow = nullptr;
    GroupRegistry groupRegistry;
    bool groupOperation = false;
    std::vector<std::vector<std::wstring>> taskRows;
    bool consoleDirty = false;
    HFONT font = nullptr, mono = nullptr, large = nullptr, heading = nullptr, small = nullptr;
    HBRUSH bgBrush = nullptr, surfaceBrush = nullptr, panelBrush = nullptr, fieldBrush = nullptr,
           statusBrush = nullptr;
    // Card rectangles (client coordinates) for the current page, rebuilt by layout() and painted by
    // paintWindow(); controls inside a card use the Panel color as their backdrop.
    std::vector<RECT> cards;
    RECT statusPill{};
    COLORREF statusFill = 0, statusText = 0;
    COLORREF backdrop(HWND child) const;
    void statusColors();
    PlayerFaces faces;
    std::vector<std::unique_ptr<Server>> servers;
    size_t selected = 0, backupOwner = 0;
    int page = 0, settingsPage = 0, hoveredServer = -1;
    bool english = false, propertiesDirty = false;
    unsigned theme = 0;
    bool lightTheme = false;
    std::map<int, std::wstring> labels, displayedText;
    unsigned timerInterval = 1000;
    void enable(int id, bool state);
    std::map<int, std::vector<std::wstring>> columnLabels;
    std::wstring fontFace = L"NanumGothic", draftImage, draftFontFace;
    unsigned draftFontPoints = 11;
    unsigned fontPoints = 11;
    bool wrapping = true, autoScroll = true, closing = false, loading = false, timersBusy = false;
    ULONGLONG closeTime = 0, lastSample = 0;
    fs::path settings, backupSource;
    std::vector<std::pair<std::wstring, unsigned>> intervalPresets;
    std::vector<std::wstring> serverGroups;
    std::wstring selectedGroup;
    BackupJob backupJob;
    PluginCatalog pluginCatalog;
    unsigned long long pluginTicket = 0;
    bool pluginLoading = false;
    int pendingPluginSelection = -1;
    std::vector<fs::path> pluginFiles, backupFiles;
    std::vector<std::wstring> commandHistory;
    size_t historyPosition = 0;
    HWND control(int id) const;
    Server &current();
    void set(int id, const std::wstring &value);
    int message(const std::wstring &value, const std::wstring &title, UINT flags, HWND owner = nullptr);
    void error(const std::wstring &value, HWND owner = nullptr);
    bool confirm(const std::wstring &value, HWND owner = nullptr);
    HWND add(const wchar_t *type, const wchar_t *label, DWORD style, int id);
    void button(const wchar_t *label, int id);
    void check(const wchar_t *label, int id, bool checked);
    void styleFont(int id, HFONT value);
    std::wstring ini(const std::wstring &section, const std::wstring &key,
                     const std::wstring &fallback = L"") const;
    unsigned iniNumber(const std::wstring &section, const std::wstring &key, unsigned fallback,
                       unsigned max) const;
    void loadSettings();
    std::wstring tr(const std::wstring &value) const;
    void applyLanguage();
    void applyTheme();
    void createSettings();
    void loadProperties();
    bool saveProperties();
    bool saveSettings();
    bool readFields(bool validate = true);
    void loadFields();
    void append(Server &server, const std::wstring &value, COLORREF color = ConsoleDefaultColor);
    void restoreConsole();
    void appendConsoleRun(const std::wstring &value, COLORREF color);
    void scrollEnd();
    void createConsole(bool preserve = false);
    void receive(Server &server, const std::wstring &value);
    void refreshServerList();
    void selectServer(size_t index);
    void addServer();
    void removeServer(size_t index);
    fs::path serverDirectory(const Server &server) const;
    bool overlapsRunningDirectory(const Server &server) const;
    bool overlapsBackup(const Server &server) const;

    bool ensureEula(Server &server, bool interactive = true);
    bool startServer(Server &server, bool interactive = true);
    void stopServer(Server &server, bool restart = false);
    void cancelAutomaticWork(Server &server);
    void forceServer(Server &server);
    void browseFile(bool java, const fs::path &initial = {});
    static HBITMAP imageBitmap(const fs::path &path);
    void loadThumbnail(Server &server);
    void setProfileImage(const fs::path &path);
    void chooseProfileImage();
    void acceptPath(const fs::path &input);
    void chooseServerFolder();
    void openFolder(const fs::path &path);
    void sendCommand();
    void findNext();
    void exportLog();
    void playerAction(const std::wstring &action);
    void listColumns(int id, const std::vector<std::pair<std::wstring, int>> &columns);
    void listRow(int id, int row, const std::vector<std::wstring> &values);
    int listSelection(int id) const;
    void refreshPlugins();
    void pollPlugins();
    void importPlugin();
    bool importPluginFiles(const std::vector<fs::path> &paths);
    void drawCheckbox(HWND hwnd, HDC dc);
    void drawCombo(HWND hwnd, HDC target);
    void togglePlugin();
    void chooseBackupFolder();
    void refreshBackups();
    void runBackup(size_t owner, bool interactive = true);
    const wchar_t *taskName(int type) const;
    void refreshTasks();
    void refreshPresets();
    void savePreset();
    void deletePreset();
    void refreshGroups();
    void openGroupManager();
    void refreshGroupManager(bool rebuild = false);
    void layoutGroupManager();
    void groupManagerAction(int id);
    static LRESULT CALLBACK groupManagerProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l);
    void refreshServerDirectory(Server &server);
    void validatePathFields();
    void runGroup(bool start);
    void addTask(bool update = false);
    void runTask(size_t owner, Task &task);
    void taskAction(int id);
    void refreshPageData();
    void showPage(int value);
    void updateControls();
    void timer();
    void operation(int id);
    void applyRemote();
    std::string remoteRequest(const RemoteRequest &request);
    RemotePanel remote;
    bool operationsBusy = false;
    bool remoteEnabled = false;
    unsigned remotePort = 25560;
    std::wstring remoteUsers;
    ServerProcess installerProcess;
    std::wstring installerTarget;
    bool installerActive = false;
    void startConfiguredServices();
    void layout();
    void paintGraph(HDC dc, RECT rect);
    static LRESULT CALLBACK graphProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR data);
    static LRESULT CALLBACK headerProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR data);
    static LRESULT CALLBACK scrollProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR data);
    static LRESULT CALLBACK inputProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR data);
    static LRESULT CALLBACK serverListProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR data);
    void applyFont();
    void chooseFont();
    void taskEditorState();
    void loadTaskEditor();
    UiIcon iconFor(int id) const;
    bool drawItem(const DRAWITEMSTRUCT &item);
    void paintWindow();
    void menu(int id);

  public:
    void attach(HWND hwnd);
    void create(HWND hwnd);
    LRESULT dispatch(UINT msg, WPARAM w, LPARAM l);
    bool keyboard(MSG &msg);

  private:
    bool backupPending = false;
    bool settingsDirty = false;
    int layoutDepth = 0;
    SIZE layoutSize{};
    HBITMAP shellBuffer = nullptr; // cached back buffer for paintWindow()
    SIZE shellSize{}; // client size of the last layout pass
    // Last rectangle layout() gave each control, keyed by window handle so a recreated control
    // (e.g. the console after toggling line wrap) is always placed again.
    std::unordered_map<HWND, RECT> placedRects;
    int themeApplied = -1; // light/dark the profile thumbnails were rendered for // scheduler results waiting to be written at the end of timer()
    bool batchingConsole = false;
};

} // namespace wrapper

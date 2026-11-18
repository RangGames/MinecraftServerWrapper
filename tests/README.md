# Windows GUI smoke test

Build the CMake target, then run in Windows PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\smoke.ps1 -BuildDirectory .\cmake-build-mcss -CaptureScreenshots
```

The script reads the compiler path from CMakeCache.txt, builds a small mock server,
and launches an isolated copy of the GUI. It uses temporary server folders and
never stops or modifies existing servers. `-KeepArtifacts` retains its fixture
folder for inspection. Screenshots are saved under the build's `screenshots/`.

Coverage:

- stdout/stderr and split UTF-8 characters;
- appending at the document end despite an earlier cursor selection;
- toggling Line wrap in both directions without changing the log text;
- login, join, disconnect, leave, IPv6, duplicate and fragmented log messages;
- chat lookalikes, oversized lines, reconnects and clearing the console;
- plugin JAR enable/disable while stopped;
- a completed stopped-server backup with matching world file contents;
- scheduled command delivery, pausing and persisting the task;
- graceful restart with a new process ID;
- two simultaneous server profiles with isolated command routing, logs and rosters;
- collecting a login while a different profile is selected;
- unexpected exit reporting and graceful shutdown of all servers on window close.

The mock server is not Minecraft. A real Minecraft connection is still a separate
integration test. External avatar downloads are not required for test success;
the captured GUI can display downloaded avatars when the service is reachable.

## Added core tests

`ctest --test-dir cmake-build-mcss --output-on-failure` runs the native `wrapper_core_tests` target.
It checks atomic Unicode INI replacement and failure preservation, path boundaries,
server.properties preservation, split ANSI truecolor/256-color/Minecraft colors,
DEFLATE JAR reading and CRC rejection, actual plugin names and versions,
SHA-256 cache reuse and invalidation when size and timestamp are unchanged,
and Velocity / ambiguous Paperclip detection.

The GUI test also exercises actual Rich Edit color formatting (using Unicode messages),
color preservation when recreating the console, quoted path normalization, profile image
copying, Korean/English switching, bundled fonts, dark/light settings and server.properties saving.
`performance.json` records short CPU samples for the manager process only, excluding Java;
a reported zero is below the sample's process-time resolution, not a guarantee of zero CPU.


The latest GUI checks also verify white default text on dark backgrounds and dark default text
on light backgrounds, live server renaming without changing its PID, server.properties edits
while running (applied on restart), command-triggered auto scroll, the actual last line being
visible in both console layouts, tab-separated output, and Escape leaving the app and server alive.
The custom scrollbar keeps native wheel, dragging and keyboard behavior.


New coverage: custom preset creation/application/persistence/deletion, targeted group start/stop
with a non-member left offline, proxy-specific graceful shutdown, invalid typed JAR rejection,
PE/JAR path validation, and repeated checkbox toggles/save with state preservation.

Hidden-console regression: receive output while Plugins, Players, Backups, Scheduler and Settings
are selected; assert the console's WS_VISIBLE flag stays clear. A >500,000-character log burst
also exercises trimming/restoration while hidden. Theme changes must preserve hidden state,
and switching back to Console must display the collected output.


Group manager coverage: separate modeless window, group creation and membership application,
empty groups across relaunch, renaming populated groups while running, and targeted bulk start/stop.
Themed dialogs are checked for error, information, EULA and deletion; modal ownership and
Escape cancellation must restore both the group manager and main window.
Core tests also cover group name normalization, duplicate rejection, rename/deletion and default-group protection.


## Operational feature tests

CTest now also runs `wrapper_operations_tests` with the built `wrapper_mock_server`.
It checks configurable JVM/native/BAT launch and argument quoting, daily local-time calculation,
random API keys and role validation, ZIP/folder backup, retention, restore with original-folder preservation,
ZIP traversal rejection, and HTTP authentication/Host/role checks. Tests use isolated temporary folders,
including Korean, spaces and apostrophes in paths.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\operations-smoke.ps1 -BuildDirectory .\cmake-build-mcss
```

This separate end-to-end script launches an isolated native app and mock server with persisted profiles.
It verifies autostart, recovery attempts/limit, intentional stop, API command and scheduler delivery,
running-server stop/ZIP-backup/restart, account creation/revocation, and persistence after relaunch.
It does not launch or change the user's configured Minecraft servers.
ZIP features require the Windows PowerShell/.NET components included with Windows.
Real Minecraft client connections and actual Forge/Fabric installation downloads remain separate integration checks.


## UI regression fixes

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\ui-regressions.ps1 -BuildDirectory .\cmake-build-mcss
```

This isolated GUI fixture checks scheduler/launch/advanced EDIT geometry for vertical centering,
samples the actual checkbox pixels while the mouse is held down, verifies mouse/Space toggling,
drops multiple JARs onto the plugin list and a JAR onto its parent page, confirms the server JAR and
selected page are preserved, and verifies imports are blocked while a server is running.
It brings its temporary test window to the front for the on-screen pixel check and closes it afterward.
Captures are written under the build's `screenshots/neutral-*` files.

cmake_minimum_required(VERSION 3.31)
project(MinecraftServerWrapper VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT WIN32)
    message(FATAL_ERROR "Minecraft Server Wrapper requires Windows")
endif()
enable_language(RC)
add_library(wrapper_core STATIC
    src/core/Operations.cpp
    src/core/ConsoleColors.cpp
    src/core/ServerProperties.cpp
    src/core/Localization.cpp
    src/core/PluginCatalog.cpp
    src/core/JarArchive.cpp
    src/core/PathValidation.cpp
    src/core/GroupRegistry.cpp
    third_party/miniz/miniz_tinfl.c)
target_include_directories(wrapper_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/server"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
target_compile_definitions(wrapper_core PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(wrapper_core PUBLIC bcrypt crypt32 gdiplus uxtheme dwmapi comctl32)
if(MSVC)
    target_compile_options(wrapper_core PRIVATE /W4 /utf-8)
else()
    target_compile_options(wrapper_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(MinecraftServerWrapper WIN32
    src/main.cpp
    resources/AppResources.rc
    src/app/App.cpp
    src/app/AppConsole.cpp
    src/app/AppControls.cpp
    src/app/AppDrawing.cpp
    src/app/AppFiles.cpp
    src/app/AppGroupManager.cpp
    src/app/AppGroups.cpp
    src/app/AppLayout.cpp
    src/app/AppLocalization.cpp
    src/app/AppOperations.cpp
    src/app/AppScheduler.cpp
    src/app/AppScrollbars.cpp
    src/app/AppServers.cpp
    src/app/AppSettings.cpp
    src/app/AppSettingsView.cpp
    src/app/AppTheme.cpp
    src/app/AppView.cpp
    src/ui/UiMessageBox.cpp
    src/server/ServerProcess.cpp
    src/server/BackupJob.cpp
    src/server/RemotePanel.cpp
    src/server/PlayerFaces.cpp)
target_precompile_headers(MinecraftServerWrapper PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src/app/App.h>")
target_compile_definitions(MinecraftServerWrapper PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
# The project root stays on the include path so the .rc file can reference assets/ and resources/.
target_include_directories(MinecraftServerWrapper PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources")
target_link_libraries(MinecraftServerWrapper PRIVATE wrapper_core ws2_32 comctl32 comdlg32 shell32 gdi32 winhttp gdiplus shlwapi psapi dwmapi uxtheme ole32)
if(MINGW)
    target_link_options(MinecraftServerWrapper PRIVATE -static)
endif()
if(MSVC)
    target_compile_options(MinecraftServerWrapper PRIVATE /W4 /utf-8)
else()
    target_compile_options(MinecraftServerWrapper PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_custom_command(TARGET MinecraftServerWrapper POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts" "$<TARGET_FILE_DIR:MinecraftServerWrapper>/assets/fonts")

include(CTest)
if(BUILD_TESTING)
    add_executable(wrapper_core_tests tests/core_tests.cpp)
    target_compile_definitions(wrapper_core_tests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(wrapper_core_tests PRIVATE wrapper_core)
    if(MINGW)
        target_link_options(wrapper_core_tests PRIVATE -static)
    endif()
    if(MSVC)
        target_compile_options(wrapper_core_tests PRIVATE /utf-8)
    endif()
    add_test(NAME wrapper_core COMMAND wrapper_core_tests)
endif()

if(BUILD_TESTING)
    add_executable(wrapper_mock_server tests/mock_server.cpp)
    if(MINGW)
        target_link_options(wrapper_mock_server PRIVATE -municode -static)
    endif()
    add_executable(wrapper_operations_tests tests/operations_tests.cpp
        src/server/ServerProcess.cpp src/server/BackupJob.cpp src/server/RemotePanel.cpp)
    target_link_libraries(wrapper_operations_tests PRIVATE wrapper_core ws2_32 psapi)
    if(MINGW)
        target_link_options(wrapper_operations_tests PRIVATE -static)
    endif()
    if(MSVC)
        target_compile_options(wrapper_mock_server PRIVATE /utf-8)
        target_compile_options(wrapper_operations_tests PRIVATE /utf-8)
    endif()
    add_test(NAME wrapper_operations COMMAND wrapper_operations_tests $<TARGET_FILE:wrapper_mock_server>)
    set_tests_properties(wrapper_operations PROPERTIES TIMEOUT 180)
endif()

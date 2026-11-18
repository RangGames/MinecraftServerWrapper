@echo off
rem Build a Release into cmake-build-release with CLions bundled MinGW/Ninja and create
rem dist\ServerDesk-<version>-win64.zip. Set CLION beforehand to use another CLion install.
rem Extra arguments are passed to scripts\package.ps1 (e.g. -SkipBuild).
setlocal
if not defined CLION set CLION=F:\Jetbrains\CLion\bin
set CMAKE=%CLION%\cmake\win\x64\bin\cmake.exe
set PATH=%CLION%\mingw\bin;%CLION%\ninja\win\x64;%PATH%
set PS=powershell
where pwsh >nul 2>&1 && set PS=pwsh
%PS% -NoProfile -ExecutionPolicy Bypass -File "%~dp0package.ps1" -BuildDirectory "%~dp0..\cmake-build-release" -CMake "%CMAKE%" %*

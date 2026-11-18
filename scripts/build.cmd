@echo off
rem Build with CLions bundled MinGW/Ninja into cmake-build-mcss (configures on first run).
setlocal
set CLION=F:\Jetbrains\CLion\bin
set CMAKE=%CLION%\cmake\win\x64\bin\cmake.exe
set PATH=%CLION%\mingw\bin;%CLION%\ninja\win\x64;%PATH%
set ROOT=%~dp0..
set BUILD=%ROOT%\cmake-build-mcss
if not exist "%BUILD%\CMakeCache.txt" (
    "%CMAKE%" -S "%ROOT%" -B "%BUILD%" -G Ninja -DCMAKE_BUILD_TYPE=Debug || exit /b 1
)
"%CMAKE%" --build "%BUILD%" -j 4 %*

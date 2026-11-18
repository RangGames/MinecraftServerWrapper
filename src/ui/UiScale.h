#pragma once
#include <windows.h>
// Layout is written in 96-DPI logical pixels; S() converts to device pixels for the process DPI.
// The process is system-DPI aware, so UiDpi is read once at startup (see runApplication).
inline int UiDpi = 96;
inline int S(int value) {
    return MulDiv(value, UiDpi, 96);
}

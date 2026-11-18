#pragma once
#include <windows.h>
#include <string>
#include <vector>
inline constexpr COLORREF ConsoleDefaultColor = CLR_INVALID;
struct ColoredText {
    std::wstring text;
    COLORREF color;
};
class ConsoleColors {
    std::wstring pending_;
    COLORREF color_ = ConsoleDefaultColor;

  public:
    std::vector<ColoredText> decode(const std::wstring &input);
    void reset() {
        pending_.clear();
        color_ = ConsoleDefaultColor;
    }
};

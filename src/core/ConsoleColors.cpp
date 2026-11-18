#include "ConsoleColors.h"
#include <algorithm>
#include <cwctype>
namespace {
constexpr COLORREF base = ConsoleDefaultColor;
constexpr COLORREF ansi[]{RGB(35, 35, 35),    RGB(205, 65, 65),   RGB(78, 185, 100),  RGB(210, 180, 70),
                          RGB(80, 135, 220),  RGB(185, 90, 195),  RGB(65, 185, 195),  RGB(210, 210, 210),
                          RGB(130, 130, 130), RGB(255, 100, 100), RGB(110, 235, 135), RGB(250, 225, 110),
                          RGB(130, 180, 255), RGB(235, 140, 245), RGB(120, 235, 245), RGB(255, 255, 255)};
COLORREF indexed(int n) {
    if (n < 16)
        return ansi[std::clamp(n, 0, 15)];
    if (n >= 232) {
        int c = 8 + 10 * (n - 232);
        return RGB(c, c, c);
    }
    n -= 16;
    auto c = [](int v) { return v ? 55 + 40 * v : 0; };
    return RGB(c(n / 36), c(n / 6 % 6), c(n % 6));
}
int hex(wchar_t c) {
    c = static_cast<wchar_t>(towlower(c));
    return c >= L'0' && c <= L'9' ? c - L'0' : c >= L'a' && c <= L'f' ? c - L'a' + 10 : -1;
}
} // namespace
std::vector<ColoredText> ConsoleColors::decode(const std::wstring &input) {
    pending_ += input;
    std::vector<ColoredText> result;
    auto put = [&](wchar_t c) {
        if (result.empty() || result.back().color != color_)
            result.push_back({{}, color_});
        result.back().text += c;
    };
    size_t i = 0;
    while (i < pending_.size()) {
        if (pending_[i] == L'\x1b') {
            if (i + 1 == pending_.size())
                break;
            if (pending_[i + 1] != L'[') {
                ++i;
                continue;
            }
            size_t end = i + 2;
            while (end < pending_.size() && !(pending_[end] >= L'@' && pending_[end] <= L'~') &&
                   end - i <= 128)
                ++end;
            if (end == pending_.size() && end - i <= 128)
                break;
            if (end - i > 128) {
                i = end;
                continue;
            }
            if (pending_[end] == L'm') {
                std::vector<int> codes;
                int n = 0;
                bool valid = true;
                for (size_t j = i + 2; j < end; ++j) {
                    wchar_t c = pending_[j];
                    if (c == L';' || c == L':') {
                        codes.push_back(n);
                        n = 0;
                    } else if (c >= L'0' && c <= L'9' && n < 10000)
                        n = n * 10 + c - L'0';
                    else {
                        valid = false;
                        break;
                    }
                }
                codes.push_back(n);
                if (valid)
                    for (size_t j = 0; j < codes.size(); ++j) {
                        int code = codes[j];
                        if (code == 0 || code == 39)
                            color_ = base;
                        else if (code >= 30 && code <= 37)
                            color_ = ansi[code - 30];
                        else if (code >= 90 && code <= 97)
                            color_ = ansi[code - 90 + 8];
                        else if ((code == 38 || code == 48) && j + 1 < codes.size()) {
                            if (codes[j + 1] == 2 && j + 4 < codes.size()) {
                                if (code == 38)
                                    color_ = RGB(std::clamp(codes[j + 2], 0, 255),
                                                 std::clamp(codes[j + 3], 0, 255),
                                                 std::clamp(codes[j + 4], 0, 255));
                                j += 4;
                            } else if (codes[j + 1] == 5 && j + 2 < codes.size()) {
                                if (code == 38)
                                    color_ = indexed(std::clamp(codes[j + 2], 0, 255));
                                j += 2;
                            }
                        }
                    }
            }
            i = end + 1;
            continue;
        }
        if (pending_[i] == L'\u00a7') {
            if (i + 1 == pending_.size())
                break;
            wchar_t c = static_cast<wchar_t>(towlower(pending_[i + 1]));
            if (c == L'x' || c == L'#') {
                size_t length = c == L'x' ? 14 : 8;
                if (pending_.size() - i < length)
                    break;
                int rgb = 0;
                bool valid = true;
                for (size_t j = 0; j < 6; ++j) {
                    size_t pos = i + 2 + j * (c == L'x' ? 2 : 1);
                    if (c == L'x') {
                        if (pending_[pos] != L'\u00a7')
                            valid = false;
                        ++pos;
                    }
                    int digit = hex(pending_[pos]);
                    if (digit < 0)
                        valid = false;
                    else
                        rgb = (rgb << 4) | digit;
                }
                if (valid) {
                    color_ = RGB((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
                    i += length;
                    continue;
                }
            }
            int digit = hex(c);
            constexpr COLORREF minecraft[]{
                RGB(0, 0, 0),     RGB(0, 0, 170),    RGB(0, 170, 0),    RGB(0, 170, 170),
                RGB(170, 0, 0),   RGB(170, 0, 170),  RGB(255, 170, 0),  RGB(170, 170, 170),
                RGB(85, 85, 85),  RGB(85, 85, 255),  RGB(85, 255, 85),  RGB(85, 255, 255),
                RGB(255, 85, 85), RGB(255, 85, 255), RGB(255, 255, 85), RGB(255, 255, 255)};
            if (digit >= 0) {
                color_ = minecraft[digit];
                i += 2;
                continue;
            }
            if (c == L'r') {
                color_ = base;
                i += 2;
                continue;
            }
            if (c >= L'k' && c <= L'o') {
                i += 2;
                continue;
            }
        }
        put(pending_[i++]);
    }
    pending_.erase(0, i);
    return result;
}

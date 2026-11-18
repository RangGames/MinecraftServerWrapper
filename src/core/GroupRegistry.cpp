#include "GroupRegistry.h"
#include <algorithm>
#include <cwctype>
namespace {
bool equal(const std::wstring &a, const std::wstring &b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](wchar_t x, wchar_t y) {
               return std::towlower(x) == std::towlower(y);
           });
}
} // namespace
std::wstring GroupRegistry::normalize(std::wstring name) {
    auto begin = name.find_first_not_of(L" \t\r\n");
    if (begin == std::wstring::npos)
        return {};
    name = name.substr(begin, name.find_last_not_of(L" \t\r\n") - begin + 1);
    if (name.size() > 48 || name.find_first_of(L"\r\n") != std::wstring::npos)
        return {};
    return name;
}
std::wstring GroupRegistry::canonical(const std::wstring &input) const {
    auto name = normalize(input);
    auto found =
        std::find_if(names_.begin(), names_.end(), [&](const auto &value) { return equal(value, name); });
    return found == names_.end() ? L"" : *found;
}
bool GroupRegistry::add(const std::wstring &input) {
    auto name = normalize(input);
    if (name.empty() || names_.size() >= MaxGroups || !canonical(name).empty())
        return false;
    names_.push_back(name);
    return true;
}
bool GroupRegistry::rename(const std::wstring &oldName, const std::wstring &input) {
    auto old = canonical(oldName), name = normalize(input);
    if (old.empty() || old == names_.front() || name.empty())
        return false;
    auto duplicate = canonical(name);
    if (!duplicate.empty() && duplicate != old)
        return false;
    *std::find(names_.begin(), names_.end(), old) = name;
    return true;
}
bool GroupRegistry::remove(const std::wstring &input) {
    auto name = canonical(input);
    if (name.empty() || name == names_.front())
        return false;
    names_.erase(std::find(names_.begin(), names_.end(), name));
    return true;
}

#pragma once
#include <string>
#include <vector>
class GroupRegistry {
    std::vector<std::wstring> names_{L"기본 그룹"};

  public:
    static constexpr size_t MaxGroups = 256;
    static std::wstring normalize(std::wstring name);
    const std::vector<std::wstring> &names() const { return names_; }
    std::wstring canonical(const std::wstring &name) const;
    bool add(const std::wstring &name);
    bool rename(const std::wstring &oldName, const std::wstring &newName);
    bool remove(const std::wstring &name);
};

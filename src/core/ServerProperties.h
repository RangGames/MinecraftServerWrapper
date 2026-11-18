#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>
class ServerProperties {
    std::vector<std::string> lines_;

  public:
    bool load(const std::filesystem::path &path);
    std::string get(const std::string &key, const std::string &fallback) const;
    bool save(const std::filesystem::path &path, const std::map<std::string, std::string> &changes) const;
};

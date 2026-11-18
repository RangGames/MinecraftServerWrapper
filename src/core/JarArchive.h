#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>
class JarArchive {
    struct Entry {
        std::string name;
        uint16_t method, flags;
        uint32_t compressed, size, offset, crc;
    };
    bool valid_ = false;
    std::filesystem::path path_;
    std::vector<Entry> entries_;

  public:
    explicit JarArchive(const std::filesystem::path &path);
    std::string read(const std::string &name) const;
    bool valid() const { return valid_; }
    bool contains(const std::string &name) const;
    std::string serverType() const;
};

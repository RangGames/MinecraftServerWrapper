#include "JarArchive.h"
#include "miniz/miniz_tinfl.h"
#include <fstream>
#include <algorithm>
#include <cctype>
namespace {
uint16_t u16(const unsigned char *p) {
    return p[0] | (p[1] << 8);
}
uint32_t u32(const unsigned char *p) {
    return u16(p) | (uint32_t(u16(p + 2)) << 16);
}
std::vector<unsigned char> readAt(const std::filesystem::path &path, uint64_t pos, size_t size) {
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return {};
    file.seekg(static_cast<std::streamoff>(pos));
    std::vector<unsigned char> data(size);
    if (!file.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(size)))
        return {};
    return data;
}
uint32_t crc32(const std::string &data) {
    uint32_t crc = ~0u;
    for (unsigned char c : data) {
        crc ^= c;
        for (int b = 0; b < 8; ++b)
            crc = (crc >> 1) ^ ((0u - (crc & 1)) & 0xedb88320u);
    }
    return ~crc;
}
} // namespace
JarArchive::JarArchive(const std::filesystem::path &path) : path_(path) {
    std::error_code ec;
    auto size = std::filesystem::file_size(path, ec);
    if (ec || size < 22)
        return;
    auto tail = readAt(path, size - std::min<uint64_t>(size, 65557),
                       static_cast<size_t>(std::min<uint64_t>(size, 65557)));
    if (tail.size() < 22)
        return;
    for (size_t i = tail.size() - 22;; --i) {
        auto *p = tail.data() + i;
        if (u32(p) == 0x06054b50 && i + 22 + u16(p + 20) == tail.size()) {
            uint32_t length = u32(p + 12), offset = u32(p + 16);
            uint16_t count = u16(p + 10);
            if (u16(p + 4) || u16(p + 6) || u16(p + 8) != count || count == 65535 ||
                length > 8 * 1024 * 1024 || uint64_t(offset) + length > size)
                return;
            auto directory = readAt(path, offset, length);
            size_t pos = 0;
            for (unsigned n = 0; n < count; ++n) {
                if (pos + 46 > directory.size() || u32(directory.data() + pos) != 0x02014b50) {
                    entries_.clear();
                    return;
                }
                p = directory.data() + pos;
                size_t name = u16(p + 28), extra = u16(p + 30), comment = u16(p + 32);
                if (pos + 46 + name + extra + comment > directory.size()) {
                    entries_.clear();
                    return;
                }
                entries_.push_back({std::string(reinterpret_cast<char *>(p + 46), name), u16(p + 10),
                                    u16(p + 8), u32(p + 20), u32(p + 24), u32(p + 42), u32(p + 16)});
                pos += 46 + name + extra + comment;
            }
            valid_ = true;
            return;
        }
        if (!i)
            break;
    }
}
bool JarArchive::contains(const std::string &name) const {
    return std::any_of(entries_.begin(), entries_.end(), [&](const Entry &e) { return e.name == name; });
}
std::string JarArchive::read(const std::string &name) const {
    for (const auto &e : entries_)
        if (e.name == name) {
            if ((e.flags & 1) || e.size > 256 * 1024 || e.compressed > 2 * 1024 * 1024 ||
                (e.method != 0 && e.method != 8))
                return {};
            auto local = readAt(path_, e.offset, 30);
            if (local.size() != 30 || u32(local.data()) != 0x04034b50)
                return {};
            uint64_t start = uint64_t(e.offset) + 30 + u16(local.data() + 26) + u16(local.data() + 28);
            auto data = readAt(path_, start, e.compressed);
            if (data.size() != e.compressed)
                return {};
            std::string output(e.size, 0);
            if (e.method == 0) {
                if (e.compressed != e.size)
                    return {};
                output.assign(data.begin(), data.end());
            } else if (tinfl_decompress_mem_to_mem(output.data(), output.size(), data.data(), data.size(),
                                                   0) != output.size())
                return {};
            return crc32(output) == e.crc ? output : std::string{};
        }
    return {};
}
std::string JarArchive::serverType() const {
    auto manifest = read("META-INF/MANIFEST.MF");
    std::transform(manifest.begin(), manifest.end(), manifest.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (manifest.find("velocity") != std::string::npos ||
        contains("com/velocitypowered/proxy/VelocityServer.class"))
        return "Velocity";
    if (manifest.find("waterfall") != std::string::npos)
        return "Waterfall";
    if (manifest.find("bungee") != std::string::npos || contains("net/md_5/bungee/Bootstrap.class"))
        return "BungeeCord";
    if (manifest.find("fabricmc") != std::string::npos ||
        contains("net/fabricmc/loader/impl/launch/server/FabricServerLauncher.class"))
        return "Fabric";
    if (manifest.find("minecraftforge") != std::string::npos ||
        contains("net/minecraftforge/server/ServerMain.class"))
        return "Forge";
    if (manifest.find("purpur") != std::string::npos)
        return "Purpur";
    if (manifest.find("paperclip") != std::string::npos || contains("io/papermc/paperclip/Main.class"))
        return "Paper family (distribution unknown)";
    if (manifest.find("paper") != std::string::npos)
        return "Paper";
    if (manifest.find("craftbukkit") != std::string::npos)
        return "CraftBukkit";
    if (manifest.find("spigot") != std::string::npos)
        return "Spigot";
    for (const auto &e : entries_) {
        if (e.name.starts_with("org/purpurmc/"))
            return "Purpur";
        if (e.name.starts_with("io/papermc/paper/"))
            return "Paper";
        if (e.name.starts_with("org/bukkit/craftbukkit/"))
            return "CraftBukkit";
    }
    if (manifest.find("net.minecraft") != std::string::npos || contains("net/minecraft/bundler/Main.class"))
        return "Vanilla";
    return "Unknown";
}

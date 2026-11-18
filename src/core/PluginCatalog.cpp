#include "PluginCatalog.h"
#include "JarArchive.h"
#include <windows.h>
#include <bcrypt.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <regex>
#include <algorithm>
namespace {
std::string sha256(const std::filesystem::path &path, std::stop_token stop) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        return {};
    if (BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return {};
    }
    std::ifstream file(path, std::ios::binary);
    char buffer[65536];
    bool ok = static_cast<bool>(file);
    while (file && !stop.stop_requested()) {
        file.read(buffer, sizeof(buffer));
        auto n = file.gcount();
        if (n && BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer), static_cast<ULONG>(n), 0) < 0) {
            ok = false;
            break;
        }
    }
    unsigned char digest[32]{};
    ok =
        ok && !file.bad() && !stop.stop_requested() && BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0;
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!ok)
        return {};
    std::ostringstream out;
    for (unsigned c : digest)
        out << std::hex << std::setw(2) << std::setfill('0') << c;
    return out.str();
}
std::string yaml(const std::string &source, const std::string &key) {
    std::istringstream in(source);
    std::string line;
    while (std::getline(in, line))
        if (line.starts_with(key + ":")) {
            auto value = line.substr(key.size() + 1);
            auto first = value.find_first_not_of(" \t\r");
            if (first == std::string::npos)
                return {};
            value = value.substr(first);
            value.erase(value.find_last_not_of(" \t\r") + 1);
            if (value.size() > 1 && (value.front() == '\'' || value.front() == '"')) {
                auto end = value.find(value.front(), 1);
                if (end != std::string::npos)
                    return value.substr(1, end - 1);
            }
            auto comment = value.find(" #");
            if (comment != std::string::npos)
                value.resize(comment);
            return value;
        }
    return {};
}
std::wstring wide(const std::string &value) {
    int n = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), n);
    return result;
}
std::pair<std::string, std::string> metadata(const std::filesystem::path &path) {
    JarArchive jar(path);
    for (const auto *entry : {"plugin.yml", "paper-plugin.yml", "bungee.yml"}) {
        auto source = jar.read(entry);
        if (!source.empty())
            return {yaml(source, "name"), yaml(source, "version")};
    }
    auto json = jar.read("velocity-plugin.json");
    auto get = [&](const std::string &key) {
        std::smatch m;
        std::regex expression("\"" + key + "\"\\s*:\\s*\"([^\"\\\\]*)\"");
        return std::regex_search(json, m, expression) ? m[1].str() : std::string{};
    };
    auto name = get("name");
    if (name.empty())
        name = get("id");
    return {name, get("version")};
}
} // namespace
PluginCatalog::PluginCatalog() : worker_([this](std::stop_token stop) { work(stop); }) {
}
PluginCatalog::~PluginCatalog() {
    worker_.request_stop();
    wake_.notify_all();
    worker_.join();
}
unsigned long long PluginCatalog::request(const std::filesystem::path &directory,
                                          const std::filesystem::path &cache) {
    std::lock_guard lock(mutex_);
    directory_ = directory;
    cache_ = cache;
    pending_ = true;
    ++ticket_;
    wake_.notify_one();
    return ticket_;
}
std::optional<PluginCatalog::Result> PluginCatalog::poll() {
    std::lock_guard lock(mutex_);
    auto result = std::move(result_);
    result_.reset();
    return result;
}
void PluginCatalog::work(std::stop_token stop) {
    while (!stop.stop_requested()) {
        std::unique_lock lock(mutex_);
        wake_.wait(lock, [&] { return pending_ || stop.stop_requested(); });
        if (stop.stop_requested())
            return;
        auto directory = directory_, cache = cache_;
        auto ticket = ticket_;
        pending_ = false;
        lock.unlock();
        Result result{ticket, {}};
        try {
            std::error_code ec;
            std::filesystem::create_directories(cache, ec);
            std::filesystem::directory_iterator it(directory, ec), end;
            while (!ec && it != end && !stop.stop_requested()) {
                auto path = it->path();
                auto filename = path.filename().wstring();
                std::transform(filename.begin(), filename.end(), filename.begin(), ::towlower);
                if (it->is_regular_file(ec) &&
                    (filename.ends_with(L".jar") || filename.ends_with(L".jar.disabled"))) {
                    auto time = std::filesystem::last_write_time(path, ec);
                    auto size = it->file_size(ec);
                    if (!ec) {
                        auto digest = sha256(path, stop);
                        std::string name, version;
                        bool cached = false;
                        if (!digest.empty()) {
                            auto file = cache / (digest + ".txt");
                            std::ifstream input(file);
                            std::string signature;
                            if (std::getline(input, signature) &&
                                signature == "plugin-metadata-v1 " + digest &&
                                (input >> std::quoted(name) >> std::quoted(version)) && name.size() < 4096 &&
                                version.size() < 4096)
                                cached = true;
                            if (!cached) {
                                auto data = metadata(path);
                                name = data.first;
                                version = data.second;
                                if (name.size() > 4096)
                                    name.clear();
                                if (version.size() > 4096)
                                    version.clear();
                                auto temporary = file;
                                temporary += L".tmp";
                                std::ofstream out(temporary, std::ios::binary);
                                out << "plugin-metadata-v1 " << digest << '\n'
                                    << std::quoted(name) << '\n'
                                    << std::quoted(version) << '\n';
                                out.close();
                                if (out)
                                    MoveFileExW(temporary.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING);
                            }
                        }
                        if (!stop.stop_requested() && std::filesystem::last_write_time(path, ec) == time &&
                            std::filesystem::file_size(path, ec) == size && !ec)
                            result.entries.push_back({path, wide(name), wide(version), size, cached});
                    }
                }
                ec.clear();
                it.increment(ec);
            }
            std::sort(result.entries.begin(), result.entries.end(),
                      [](const auto &a, const auto &b) { return a.path < b.path; });
        } catch (...) {
            result.entries.clear();
        }
        lock.lock();
        if (ticket == ticket_)
            result_ = std::move(result);
    }
}

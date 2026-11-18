#include "PathValidation.h"
#include "JarArchive.h"
#include <windows.h>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cwctype>
bool validateJavaPath(const std::wstring &input, std::wstring &resolved, std::wstring &error) {
    if (input.empty()) {
        error = L"Java 실행 파일 경로를 입력하세요.";
        return false;
    }
    std::filesystem::path path(input);
    if (!path.has_parent_path()) {
        wchar_t found[32768]{};
        if (!SearchPathW(nullptr, input.c_str(), L".exe", 32768, found, nullptr)) {
            error = L"Java 실행 파일을 찾을 수 없습니다. 경로 또는 PATH를 확인하세요.";
            return false;
        }
        path = found;
    }
    auto extension = path.extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
    std::error_code ec;
    if (extension != L".exe" || !std::filesystem::is_regular_file(path, ec) || ec) {
        error = L"Java 경로는 존재하는 .exe 파일이어야 합니다.";
        return false;
    }
    std::ifstream file(path, std::ios::binary);
    unsigned char dos[64]{};
    unsigned char signature[4]{};
    if (!file.read(reinterpret_cast<char *>(dos), 64) || dos[0] != 'M' || dos[1] != 'Z') {
        error = L"유효한 Windows 실행 파일이 아닙니다.";
        return false;
    }
    unsigned offset =
        unsigned(dos[60]) | (unsigned(dos[61]) << 8) | (unsigned(dos[62]) << 16) | (unsigned(dos[63]) << 24);
    file.seekg(offset);
    if (!file.read(reinterpret_cast<char *>(signature), 4) || signature[0] != 'P' || signature[1] != 'E' ||
        signature[2] || signature[3]) {
        error = L"유효한 Windows 실행 파일이 아닙니다.";
        return false;
    }
    resolved = path.wstring();
    return true;
}
bool validateServerJar(const std::filesystem::path &path, std::wstring &error) {
    auto extension = path.extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
    std::error_code ec;
    if (path.empty() || extension != L".jar" || !std::filesystem::is_regular_file(path, ec) || ec) {
        error = L"서버 경로는 존재하는 .jar 파일이어야 합니다.";
        return false;
    }
    JarArchive archive(path);
    auto manifest = archive.read("META-INF/MANIFEST.MF");
    std::transform(manifest.begin(), manifest.end(), manifest.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (!archive.valid() ||
        (!manifest.starts_with("main-class:") && manifest.find("\nmain-class:") == std::string::npos)) {
        error = L"실행 가능한 서버 JAR이 아닙니다. 손상된 파일 또는 Main-Class가 없는 JAR입니다.";
        return false;
    }
    return true;
}

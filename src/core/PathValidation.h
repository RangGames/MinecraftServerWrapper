#pragma once
#include <filesystem>
#include <string>
bool validateJavaPath(const std::wstring &input, std::wstring &resolved, std::wstring &error);
bool validateServerJar(const std::filesystem::path &path, std::wstring &error);

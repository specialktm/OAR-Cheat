#pragma once
#include "pch/pch.h"
#include <string>
#include <vector>

class fileManager {
public:
    fileManager() = delete;

    static std::string baseDir();
    static std::wstring baseDirW();
    static std::string configPath(const std::string& name);
    static std::string dumpPath(const std::string& leaf);
    static bool writeText(const std::string& path, const std::string& text);
    static bool readText(const std::string& path, std::string& out);
    static std::vector<std::string> listConfigs();
};

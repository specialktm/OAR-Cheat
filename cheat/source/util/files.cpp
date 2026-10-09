#include "pch/pch.h"
#include "util/files.hpp"

#include <shlobj.h>
#include <filesystem>
#include <fstream>

std::string fileManager::baseDir() {
    static std::string dir;
    static bool tried = false;
    if (tried) return dir;
    tried = true;
    char docs[MAX_PATH] = { 0 };
    if (FAILED(SHGetFolderPathA(nullptr, CSIDL_MYDOCUMENTS, nullptr, 0, docs)))
        return dir;
    dir = std::string(docs) + "\\KOAR";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) dir.clear();
    return dir;
}

std::wstring fileManager::baseDirW() {
    std::string n = baseDir();
    if (n.empty()) return L"";
    int w = MultiByteToWideChar(CP_UTF8, 0, n.c_str(), -1, nullptr, 0);
    if (w <= 0) return L"";
    std::wstring out((size_t)w, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, n.c_str(), -1, out.data(), w);
    while (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}

static std::string safe_name(const std::string& name) {
    std::string s = name.empty() ? "default" : name;
    for (auto& ch : s) {
        if (ch == '\\' || ch == '/' || ch == ':' || ch == '*' || ch == '?' ||
            ch == '"' || ch == '<' || ch == '>' || ch == '|')
            ch = '_';
    }
    return s;
}

std::string fileManager::configPath(const std::string& name) {
    return baseDir() + "\\" + safe_name(name) + ".ini";
}

std::string fileManager::dumpPath(const std::string& leaf) {
    return baseDir() + "\\" + leaf;
}

bool fileManager::writeText(const std::string& path, const std::string& text) {
    if (path.empty()) return false;
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    std::ofstream f(path, std::ios::trunc | std::ios::binary);
    if (!f) return false;
    f.write(text.data(), (std::streamsize)text.size());
    return !!f;
}

bool fileManager::readText(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

std::vector<std::string> fileManager::listConfigs() {
    std::vector<std::string> out;
    std::string dir = baseDir();
    if (dir.empty()) return out;
    std::error_code ec;
    for (auto& e : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!e.is_regular_file(ec)) continue;
        std::string fn = e.path().filename().string();
        if (fn.size() <= 4 || fn.compare(fn.size() - 4, 4, ".ini") != 0) continue;
        out.push_back(fn.substr(0, fn.size() - 4));
    }
    return out;
}

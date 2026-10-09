#pragma once
#include "pch/pch.h"
#include <vector>

namespace cfgio {
std::string path_for(const std::string& name);
bool save(const std::string& name, std::string& err);
bool load(const std::string& name, std::string& err);
std::vector<std::string> list();
}

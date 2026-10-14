#pragma once
#include "pch/pch.h"

namespace combat {
struct LEntry {
    bool kill = false;
    std::string name;
    std::string part;
    int dmg = 0;
    std::chrono::steady_clock::time_point at{};
};
void tick();
void clear();
std::vector<LEntry> entries();
}

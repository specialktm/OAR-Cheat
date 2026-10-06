#pragma once
#include "pch/pch.h"
// Loot crates (Promotion / Getaway): opened through the in-level
// ABP_CrateManager_C actor (SpawnCrate + OpenCrate), the same sequence the
// crate UI drives. No SteamIDs needed; grants land via gameplay.
namespace crates {
struct CrateInfo {
    std::string name;
    void* cls = nullptr; // UClass*
};
std::vector<CrateInfo> list_crates();
void open_all();
const char* last_line(); // short outcome for the Shop tab
} // namespace crates

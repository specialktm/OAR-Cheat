#pragma once
#include "pch/pch.h"
#include <functional>
#include <string>
#include <vector>

namespace crates {
struct CrateInfo {
    std::string name;
    void* cls = nullptr;
};
struct SteamItem {
    unsigned long long instance = 0;
    int def = 0;
    int qty = 0;
};
std::vector<CrateInfo> list_crates();
void open_all();
void grant_all();
void open_for_skin(int skinRowId);
void grant_async(const std::vector<int>& defs, const std::string& tag, std::function<void(int)> onDone);
void exchange_async(int skinDef, unsigned long long inst, const std::string& tag, std::function<void(int)> onDone);
void drop_async(const std::vector<int>& defs, const std::string& tag, std::function<void(int)> onDone);
void drop_all();
void sniff_drop();
void promo_all();
void promo_coins();
void scan_lists(bool force = false);
void tick_steam();
bool snapshot(std::vector<SteamItem>& out);
int steam_def_ids(std::vector<int>& out);
int count_unlocked_skins(int category);
int count_unlocked_def(int category, int skinDef);
const char* last_line();
}

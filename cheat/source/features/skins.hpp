#pragma once
#include "pch/pch.h"
#include <string>
#include <vector>

namespace skins {

struct SkinEntry {
    int id = -1;
    std::string name;
    bool hasCharm = false;
    std::string charmName;
    int steamId = 0;
};

struct OwnedSkin {
    int rowId = -1;
    int def = 0;
    std::string name;
    int cat = 0;
};

struct CharmEntry {
    int def = 0;
    std::string name;
    int coin = 0;
};

void scan();
void unlock_all();
void unlock_everything();
int equip_row(int rowId);
bool equip_single(int rowId, void* weapon);
bool row_info(int rowId, std::string& name, int& cat, std::string& charm, int& def);
std::vector<OwnedSkin> unlocked_skins();
std::vector<CharmEntry> charm_shop_items();
const std::vector<SkinEntry>& list();
int selected();
void setSelected(int i);
void apply_selected();
const char* status();

}

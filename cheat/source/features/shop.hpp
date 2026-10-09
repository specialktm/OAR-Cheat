#pragma once
#include "pch/pch.h"

namespace shop {
struct Status {
    bool hasController = false;
    int mapClasses = 0;
    int mapsGranted = 0;
    int itemClasses = 0;
    int itemsPatched = 0;
    int itemsGranted = 0;
    int liveActors = 0;
    int steamDefs = 0;
    int steamGranted = 0;
    bool steamOk = false;
    int mapsVerified = 0;
    int itemsVerified = 0;
    int coinsBefore = -1;
    int coinsAfter = -1;
    std::string note;
};
void free_all_shop();
void free_all_maps();
void free_all_skills();
Status last_status();

struct ItemInfo {
    std::string name;
    int cash = 0, coin = 0, level = 0, defid = 0, cat = 0;
    void* cls = nullptr;
};
std::vector<ItemInfo> all_items();
std::vector<ItemInfo> catalog(bool force = false);
int selected();
void set_selected(int i);
std::vector<std::string> inventory_names();
bool grant_class(void* uclass);
int grant_count();
void coins_legit();
}

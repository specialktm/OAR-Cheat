#pragma once
#include "pch/pch.h"
// Shop / map unlocks.
//
// Patched-method replacement (v2): the old approach scanned GObjects for live
// AShopItem actor instances and mutated them. That now yields 0 results -
// shop data lives only as Class Default Objects plus the authoritative
// per-player arrays on ARobberController_C. So we go controller-direct:
//
//   maps:  enumerate every ShopItem_Map_* UClass -> AddMap + verify in
//          UnlockedMaps[] -> SaveMaps
//   items: patch each ShopItem_* CDO (UnlockLevel=0, Cash/Coin=1) ->
//          AddInventoryItem -> SaveInventoryItems, plus Cash/Coins grant
//   skills: same class walk over Shop_Skill* (they share ShopItem_Base)
//
// All callable from menu + hotkey, null-safe, non-blocking.
namespace shop {
struct Status {
    bool hasController = false;
    int mapClasses = 0;     // ShopItem_Map_* classes found
    int mapsGranted = 0;    // AddMap calls that took effect
    int itemClasses = 0;    // ShopItem_* classes found
    int itemsPatched = 0;   // CDOs set to L0/$1
    int itemsGranted = 0;   // AddInventoryItem calls
    int liveActors = 0;     // legacy live-actor hits (expected 0 post-patch)
    int steamDefs = 0;      // SteamItemDefIDs collected from CDOs
    int steamGranted = 0;   // FSteamItemDetails returned by Steam
    bool steamOk = false;   // GenerateItems + controller feed succeeded
    int mapsVerified = 0;   // classes confirmed present in UnlockedMaps[]
    int itemsVerified = 0;  // classes confirmed present in ItemInventory[]
    int coinsBefore = -1;   // Steam coin balance around GrantCoinPacks
    int coinsAfter = -1;
    std::string note;
};
void free_all_shop();
void free_all_maps();
void free_all_skills();
Status last_status();

// Player-tab helpers.
struct ItemInfo {
    std::string name;
    int cash = 0, coin = 0, level = 0, defid = 0, cat = 0;
    void* cls = nullptr; // UClass* for drag-to-grant
};
std::vector<ItemInfo> all_items();        // every ShopItem_* class + CDO data
const std::vector<ItemInfo>& catalog(bool force = false); // cached (UI thread)
int selected();                           // shared item selection index
void set_selected(int i);
std::vector<std::string> inventory_names(); // current ItemInventory class names
bool grant_class(void* uclass);           // AddInventoryItem + save, one shot
int grant_count();                        // increments per successful grant
void coins_legit();                       // Steam coin packs (real paid currency)
} // namespace shop

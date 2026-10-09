# OAR-Cheat

Testing cheat for One-Armed Robber (UE 4.27, DX11). Overlay menu, ESP, shop/world/lobby tools, Steam inventory flows, loadout editor.

## Build

Needs VS2022. Build `cheat/cheat.vcxproj`, Developer/x64:

```
MSBuild cheat\cheat.vcxproj /p:Configuration=Developer /p:Platform=x64
```

Output lands in `cheat/bin/Developer/`. Close the game first (it locks the DLL), copy `cheat.dll` + `cheat.pdb` to `bin/Developer/`, then inject. Release is for playing, Developer keeps asserts.

Menu key is INSERT, unload key is END. First inject shows a loading screen, then the menu fades in.

## Menu

Purple-black theme, 880x580 with blur backdrop. Tabs: Home, ESP, Shop, World, Players, Lobby, Settings, Loadout. Hotkeys per action with toggle/hold/always modes, configs save to INI.

Every button reports a status line. If a status says `refused` or `FAILED`, that is the game or Steam saying no, not the UI. The logger window (`OS Cheats | On Top!`) keeps the full history.

## Steam: maps

Maps go through the controller, not Steam inventory. Every engine touch sits behind SEH and every pointer is validated first:

```cpp
static SDK::UObject* seh_cdo(SDK::UClass* cls) {
    __try { return cls->DefaultObject; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static bool seh_addmap(SDK::ARobberController_C* pc, SDK::UClass* cls) {
    __try { pc->AddMap(cls); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_additem(SDK::ARobberController_C* pc, SDK::UClass* cls) {
    __try { int idx = 0; pc->AddInventoryItem(cls, &idx); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_save_maps(SDK::ARobberController_C* pc) {
    __try { pc->SaveMaps(); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_save_inv(SDK::ARobberController_C* pc) {
    __try { pc->SaveInventoryItems(); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_patch_cdo(SDK::AShopItem_Base_C* it) {
    __try { it->UnlockLevel = 0; it->CashCost = 1; it->CoinCost = 1; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
```

Unlock all maps (`shop::free_all_maps()`). Enumerate `ShopItem_Map_*` subclasses, skip what's already in `UnlockedMaps[]`, grant the rest, save, then re-read the array to verify:

```cpp
auto classes = collect_subclasses(base_of("map"), "ShopItem_Map_");
for (auto* cls : classes) {
    bool has = false;
    bool readOk = seh_arr_contains_uclass(pc->UnlockedMaps, cls, has);
    if (readOk && has) continue;
    if (seh_addmap(pc, cls)) ++st.mapsGranted;
}
seh_save_maps(pc);
```

Same shape for shop items (`shop::free_all_shop()`), over `ShopItem_*` subclasses of `AShopItem_Base_C`, plus a CDO patch per class so everything costs $1, cash top-up, and verify against `ItemInventory[]`:

```cpp
auto items = collect_subclasses(base_of("item"), "ShopItem_");
for (auto* cls : items) {
    SDK::UObject* cdo = seh_cdo(cls);
    auto* base = static_cast<SDK::AShopItem_Base_C*>(cdo);
    if (seh_patch_cdo(base)) ++st.itemsPatched;
    bool has = false;
    bool readOk = seh_arr_contains_uclass(pc->ItemInventory, cls, has);
    if (readOk && has) continue;
    if (seh_additem(pc, cls)) ++st.itemsGranted;
}
seh_save_inv(pc);
```

One item is `shop::grant_class(cls)`, same `seh_additem` path. Status reports granted vs verified counts, nothing Steam-side to poll here, the save persists it.

Coins only come from the game's own grant flow. `shop::coins_legit()` runs `GrantCoinPacks()` -> `CheckCoinPacks()` -> `AddCurrentPromoItem()` and reports the balance before/after. Coin bundles (defs 1501/3151/8501/20001, type `bundle`) are claimed through promo eligibility in `crates::promo_coins()`:

```cpp
SDK::FSteamID sid = SDK::UUser::GetSteamID();
SDK::TArray<SDK::FSteamItemDef> arr{};
SDK::UInventory::GetEligiblePromoItemDefinitionIDs(sid, &arr);
// keep defs that match what you actually want, then:
SDK::FSteamInventoryResult res{};
SDK::UInventory::AddPromoItems(&res, wanted);
// poll GetResultStatus() until OK, then read with GetResultItems()
```

Promo is one claim per account. If Steam offers nothing, the status says so.

## Steam: skins and crates

Skins live in `DT_WeaponSkins`, crates in `DT_Crates` (structs `FWeaponSkinStruct` / `FCrateStruct`). `skins::scan()` walks rows 0..4095 plus every Steam def id and the shop catalog, and writes the full list to `Documents/KOAR/sku_dump.txt`. Always scan first.

Grants go through the async pipeline in `crates.cpp`: issue the call, the result handle is polled every tick in `crates::tick_steam()`, items land through `UpdateSteamInventory` / `UpdateItemsInInventory` / `OnSteamInventoryUpdated`. Granting is fire-and-forget from your side:

```cpp
std::vector<int> defs = { 2003, 2004, 2005 };   // crate generators, NOT the crates
crates::grant_async(defs, "gen", [](int got) {
    // got = items landed
});
```

Use generator defs (`type=generator` in the schema: 2003 Getaway, 2004 Vault, 2005 Free loot). Granting the crate items directly (1001-1005, `type=item`) does nothing, that call is dev-only and Steam refuses it. There is no drop-list def in this game's schema, so `TriggerItemDrop` only works if you captured a real one from a win screen (`sniff_drop()` reads it off the live WinUI widget, edge-triggered so stale widgets don't pollute it).

Opening a crate for a skin is a real exchange, consume the crate instance, mint the skin def:

```cpp
crates::exchange_async(skinDef, crateInstanceId, "open", [](int got) {
    if (got >= 0) skins::equip_row(skinRowId);
});
```

Equipping is by table row id through `ARobberController_C::EquipSkin(row, weapon, cat)`. Verify with `GetUnlockedWeaponSkins` before/after, `crates::count_unlocked_def(cat, def)` does exactly that.

Bulk flows already wired: `grant_all` (crates + coin defs), `drop_all` (sequential, generators via grant, rest via drop), `open_all`, `open_for_skin`, `unlock_all`, `unlock_everything`. All Steam results are async, statuses show `pending...` first.

## Reading result codes

Per-def lines log the raw Steam result. `-1` is our own timeout (no answer in ~12s), everything else negated is a real `ESteamResult`: `-2` Fail, `-8` InvalidParam, `-15` AccessDenied, `-25` LimitExceeded, `-42` NoMatch, `-84` RateLimitExceeded, `-104` InvalidItemType.

Don't trust the codes blindly. Steam sometimes reports Fail and still delivers, the schema has no drop lists so drop calls mostly die, and throttled calls sit pending until they time out. Ground truth is the `new arrival def X` lines and the `owned` count in the final summary. If `owned` doesn't move, nothing landed.

## Loadout replace

`Loadout` tab -> pick a slot, pick a gun in Armory -> EQUIP TO SLOT. It calls the slot widget's own `SetNewItem`/`UpdateItem` first (same functions the stash UI uses), then controller `OverrideIndexFromCategory` + `UpdateNewWeapon`, and only falls back to the `SetWeaponsServer` RPC if no widget exists. Everything mutating runs on the game thread through a `ProcessEvent` hook (`feat::queue_game`), never on the worker or render thread. Verify needs two consecutive matching reads before it reports `(saved)`, otherwise it rolls back and says so.

## Layout

- `cheat/source/ui/` menu shell, tabs, widgets, theme
- `cheat/source/features/` esp, shop, skins, crates, exploits, manager (action queue), capture, modelview
- `cheat/source/render/` DX11 hook, blur backdrop
- `cheat/source/steam/` identity/avatar
- `cheat/vendor/imgui` UI-1's 1.89.2 fork, `cheat/vendor/SDK` dumped game SDK
- `dump/` idmap/usmap/GObjects dumps and the 2_ + imgui_ reference zips

#include "pch/pch.h"
#include "features/shop.hpp"
#include "core/actor_iter.hpp"
#include "SDK/Basic.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/ShopItem_Base_classes.hpp"
#include "SDK/ShopItem_Map_base_classes.hpp"
#include "SDK/RobberController_classes.hpp"
#include "SDK/Shop_SkillBase_classes.hpp"
#include "SDK/RarityNum_structs.hpp"
#include "SDK/SteamCore_classes.hpp"
#include "SDK/SteamCore_structs.hpp"
#include "core/veh.hpp"
#include "logger/logger.hpp"

namespace shop {

static Status g_last{};
static std::mutex g_lastMtx;

static void set_last(const Status& st) {
    std::lock_guard<std::mutex> lk(g_lastMtx);
    g_last = st;
}
Status last_status() {
    std::lock_guard<std::mutex> lk(g_lastMtx);
    return g_last;
}

// ---- SEH leaves (no vector/string locals -> /EHsc compatible) ----
static int seh_gnum() {
    __try { return SDK::UObject::GObjects->Num(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
static SDK::UObject* seh_gat(int i) {
    __try { return SDK::UObject::GObjects->GetByIndex(i); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static SDK::UClass* seh_uclass_static() {
    __try { return SDK::UClass::StaticClass(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static bool seh_is_uclass(SDK::UObject* o, SDK::UClass* uclass, bool& out) {
    __try { out = o->IsA(uclass); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_isdef(SDK::UObject* o, bool& out) {
    __try { out = o->IsDefaultObject(); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_issub(SDK::UClass* cls, SDK::UClass* base, bool& out) {
    __try {
        for (SDK::UStruct* s = cls; s; s = s->Super) {
            if (s == base) { out = true; return true; }
        }
        out = false; return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static SDK::UObject* seh_cdo(SDK::UClass* cls) {
    __try { return cls->DefaultObject; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static SDK::ARobberController_C* seh_local_rc() {
    __try {
        auto* pc = reinterpret_cast<SDK::ARobberController_C*>(actors::local_player_controller());
        if (!IS_VALID_PTR(pc)) return nullptr;
        SDK::UClass* rc = SDK::ARobberController_C::StaticClass();
        if (!IS_VALID_PTR(rc)) return pc; // GObjects shaky: trust the cast
        return pc->IsA(rc) ? pc : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
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
static bool seh_save_cash(SDK::ARobberController_C* pc) {
    __try { pc->SaveCash(); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_grant_cash(SDK::ARobberController_C* pc) {
    // Cash is soft currency (savegame data): direct write + SaveCash works.
    // Coins are paid Steam currency: memory writes do NOT replicate to the
    // Steam backend, so Coins are never touched here (see coins_legit).
    __try { pc->Cash = 999999999; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_grant_coins(SDK::ARobberController_C* pc) {
    __try { pc->GrantCoinPacks(); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_check_coins(SDK::ARobberController_C* pc) {
    __try { pc->CheckCoinPacks(); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_promo(SDK::ARobberController_C* pc) {
    __try { pc->AddCurrentPromoItem(); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static int seh_coins(SDK::ARobberController_C* pc, bool& ok) {
    __try { ok = true; return pc->Coins; }
    __except (EXCEPTION_EXECUTE_HANDLER) { ok = false; return -1; }
}
static bool seh_patch_cdo(SDK::AShopItem_Base_C* it) {
    __try { it->UnlockLevel = 0; it->CashCost = 1; it->CoinCost = 1; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_patch_skill(SDK::AShop_SkillBase_C* s) {
    __try { s->CashCost = 1; s->RequiredProgress = 1.0f; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
// Read-only array snapshot helpers (raw buffer copy, no TArray mutation).
static std::string safe_name(SDK::UObject* o) {
    // VEH-guarded: the GObjects walk can race GC teardown, and std::string
    // locals forbid SEH __try here (C2712).
    return veh::get_name(o);
}
static bool seh_arr_contains_uclass(const SDK::TArray<SDK::UClass*>& arr, SDK::UClass* cls, bool& out) {
    __try {
        int n = arr.Num();
        if (n <= 0 || n > 100000) { out = false; return true; }
        SDK::UClass* const* d = arr.GetDataPtr();
        if (!d) { out = false; return true; }
        for (int i = 0; i < n; ++i) {
            if (d[i] == cls) { out = true; return true; }
        }
        out = false; return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// ---- Steam-inventory sync -----------------------------------------------
// Why this exists: AddMap/AddInventoryItem only touch the controller's local
// arrays. The shop UI + Steam backend read the Steam mirror
// (InventoryResultItems / FSteamInventoryResult). The pre-patch game bridged
// the two via MapsToInventory + Steam refresh events; we replay that bridge:
//   1. GenerateItems (one batch for all collected SteamItemDefIDs)
//   2. UpdateSteamInventory(handle) -> blueprint consumes the result
//   3. GetResultItems -> UpdateItemsInInventory -> OnSteamInventoryUpdated
//   4. MapsToInventory (maps -> inventory) + OnUpdateAllSteamInventory
//   5. DestroyResult (no handle leak)
// If Steam refuses generation, controller grants still stand and the status
// says so explicitly.
struct RawArr { void* d; int num; int mx; };
template<typename T>
static SDK::TArray<T> wrap_buf(T* buf, int n) {
    static_assert(sizeof(SDK::TArray<T>) == 16, "TArray layout");
    SDK::TArray<T> a{};
    RawArr* r = reinterpret_cast<RawArr*>(&a);
    r->d = buf; r->num = n; r->mx = n;
    return a; // shallow copy of Data/Num/Max
}
static int seh_defid(SDK::UClass* cls) {
    __try {
        SDK::UObject* cdo = cls->DefaultObject;
        if (!cdo) return 0;
        return static_cast<SDK::AShopItem_Base_C*>(cdo)->SteamItemDefID;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
static bool seh_generate(SDK::FSteamInventoryResult* res,
        const SDK::TArray<SDK::FSteamItemDef>& defs, const SDK::TArray<int>& qtys) {
    __try { return SDK::UInventory::GenerateItems(res, defs, qtys); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_get_items(const SDK::FSteamInventoryResult* res,
        SDK::TArray<SDK::FSteamItemDetails>* out) {
    __try { return SDK::UInventory::GetResultItems(*res, out); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_feed_result(SDK::ARobberController_C* pc, const SDK::FSteamInventoryResult& res) {
    __try { pc->UpdateSteamInventory(res); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_merge_items(SDK::ARobberController_C* pc, SDK::TArray<SDK::FSteamItemDetails>& det) {
    __try { pc->UpdateItemsInInventory(det); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_fire_updated(SDK::ARobberController_C* pc, const SDK::TArray<SDK::FSteamItemDetails>& det) {
    __try { pc->OnSteamInventoryUpdated(det); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_maps_to_inv(SDK::ARobberController_C* pc) {
    __try { pc->MapsToInventory(); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_refresh_all(SDK::ARobberController_C* pc) {
    __try { pc->OnUpdateAllSteamInventory(); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_destroy(SDK::FSteamInventoryResult* res) {
    __try { SDK::UInventory::DestroyResult(*res); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
struct DetailsSnap { SDK::FSteamItemDetails const* d; int n; int mx; bool ok; };
static DetailsSnap seh_details_snap(const SDK::TArray<SDK::FSteamItemDetails>& det) {
    __try {
        int n = det.Num();
        if (n <= 0 || n > 100000) return DetailsSnap{ nullptr, 0, 0, false };
        auto* d = det.GetDataPtr();
        if (!d) return DetailsSnap{ nullptr, 0, 0, false };
        return DetailsSnap{ d, n, det.Max(), true };
    } __except (EXCEPTION_EXECUTE_HANDLER) { return DetailsSnap{ nullptr, 0, 0, false }; }
}

// Grants every collected class into the Steam mirror. Returns granted detail count.
static int sync_steam(SDK::ARobberController_C* pc, const std::vector<SDK::UClass*>& classes,
        bool mapsToInv, bool& outOk, int& outDefs, std::string& outNote) {
    outOk = false; outDefs = 0;
    std::vector<int> ids;
    ids.reserve(classes.size());
    for (auto* cls : classes) {
        if (!IS_VALID_PTR(cls)) continue;
        int id = seh_defid(cls);
        if (id <= 0) continue;
        bool dup = false;
        for (int e : ids) { if (e == id) { dup = true; break; } }
        if (!dup) ids.push_back(id);
    }
    if (ids.empty()) { outNote = "no SteamItemDefIDs on CDOs"; return 0; }
    outDefs = (int)ids.size();

    std::vector<SDK::FSteamItemDef> defs(ids.size());
    std::vector<int> qtys(ids.size(), 1);
    for (size_t i = 0; i < ids.size(); ++i) defs[i].Value = ids[i];

    SDK::TArray<SDK::FSteamItemDef> aDefs = wrap_buf(defs.data(), (int)defs.size());
    SDK::TArray<int> aQtys = wrap_buf(qtys.data(), (int)qtys.size());
    SDK::FSteamInventoryResult res{};
    if (!seh_generate(&res, aDefs, aQtys)) {
        outNote = "Steam GenerateItems refused (controller grants kept)";
        return 0;
    }
    seh_feed_result(pc, res);
    SDK::TArray<SDK::FSteamItemDetails> details{};
    int granted = 0;
    if (seh_get_items(&res, &details)) {
        DetailsSnap s = seh_details_snap(details);
        if (s.ok) {
            granted = s.n;
            // Hand the blueprint a buffer WE own with slack: game-side code
            // may append in place, and Steam-sized buffers have no room.
            // Deliberately leaked: the controller may retain the pointer.
            SDK::FSteamItemDetails* buf = new SDK::FSteamItemDetails[(size_t)s.n + 64];
            memcpy(buf, s.d, sizeof(*buf) * (size_t)s.n);
            SDK::TArray<SDK::FSteamItemDetails> owned = wrap_buf(buf, s.n);
            struct RawMx { void* d; int num; int mx; };
            reinterpret_cast<RawMx*>(&owned)->mx = s.n + 64;
            seh_merge_items(pc, owned);
            seh_fire_updated(pc, owned);
        }
    }
    if (mapsToInv) seh_maps_to_inv(pc);
    seh_refresh_all(pc);
    seh_destroy(&res);
    outOk = granted > 0;
    outNote = outOk ? "steam mirror synced"
        : "generated but 0 details returned (controller grants kept)";
    return granted;
}

// ---- UClass collection (no IsDefaultObject filter: classes are the target) ----
static std::vector<SDK::UClass*> collect_subclasses(SDK::UClass* base, const char* prefix) {
    std::vector<SDK::UClass*> out;
    int n = seh_gnum();
    if (n <= 0 || n > 600000) return out;
    out.reserve(256);
    SDK::UClass* uclass = seh_uclass_static();
    for (int i = 0; i < n; ++i) {
        SDK::UObject* o = seh_gat(i);
        if (!IS_VALID_PTR(o)) continue;
        if (IS_VALID_PTR(uclass)) {
            bool isa = false;
            if (!seh_is_uclass(o, uclass, isa) || !isa) continue;
        }
        // Name check works even if StaticClass()/IsA path is degraded.
        std::string nm = safe_name(o);
        if (nm.empty()) continue;
        if (nm.rfind(prefix, 0) != 0) continue;
        if (nm.find("_base") != std::string::npos || nm.find("Base_") != std::string::npos) {
            // keep ShopItem_Base itself out, but allow real items
            if (nm == "ShopItem_Base_C" || nm == "ShopItem_Map_base_C") continue;
        }
        auto* cls = static_cast<SDK::UClass*>(o);
        if (IS_VALID_PTR(base)) {
            bool sub = false;
            if (!seh_issub(cls, base, sub) || !sub) continue;
        }
        out.push_back(cls);
    }
    return out;
}

static SDK::UClass* base_of(const char* shortName) {
    // Prefer SDK StaticClass; fall back to name walk.
    if (strcmp(shortName, "map") == 0) {
        SDK::UClass* b = nullptr;
        __try { b = SDK::AShopItem_Map_base_C::StaticClass(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { b = nullptr; }
        if (IS_VALID_PTR(b)) return b;
    } else if (strcmp(shortName, "item") == 0) {
        SDK::UClass* b = nullptr;
        __try { b = SDK::AShopItem_Base_C::StaticClass(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { b = nullptr; }
        if (IS_VALID_PTR(b)) return b;
    } else if (strcmp(shortName, "skill") == 0) {
        SDK::UClass* b = nullptr;
        __try { b = SDK::AShop_SkillBase_C::StaticClass(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { b = nullptr; }
        if (IS_VALID_PTR(b)) return b;
    }
    return nullptr; // name-prefix filter still applies
}

void free_all_maps() {
    Status st{};
    auto* pc = seh_local_rc();
    st.hasController = IS_VALID_PTR(pc);
    if (!st.hasController) {
        st.note = "No local RobberController (join a lobby first)";
        set_last(st);
        if (g_logger) g_logger->send(levels::error, "Maps: {}", st.note);
        return;
    }
    auto classes = collect_subclasses(base_of("map"), "ShopItem_Map_");
    st.mapClasses = (int)classes.size();
    for (auto* cls : classes) {
        if (!IS_VALID_PTR(cls)) continue;
        bool has = false;
        bool readOk = seh_arr_contains_uclass(pc->UnlockedMaps, cls, has);
        if (readOk && has) continue;
        if (seh_addmap(pc, cls)) ++st.mapsGranted;
    }
    seh_save_maps(pc);
    // Bridge to the Steam mirror (this is what the old method hit implicitly).
    {
        bool ok = false; int defs = 0; std::string note;
        st.steamGranted = sync_steam(pc, classes, true, ok, defs, note);
        st.steamDefs = defs;
        st.steamOk = ok;
        if (g_logger) g_logger->send(levels::debug, "[steam] maps sync: {} ({})",
            st.steamGranted, note);
    }
    // Ground truth: re-read UnlockedMaps (Steam result handles are async and
    // may report 0 while the grant still lands).
    for (auto* cls : classes) {
        if (!IS_VALID_PTR(cls)) continue;
        bool has = false;
        if (seh_arr_contains_uclass(pc->UnlockedMaps, cls, has) && has)
            ++st.mapsVerified;
    }
    // Cheap-shop pass so unlocked maps are purchasable for $1.
    auto items = collect_subclasses(base_of("item"), "ShopItem_");
    st.itemClasses = (int)items.size();
    for (auto* cls : items) {
        if (!IS_VALID_PTR(cls)) continue;
        SDK::UObject* cdo = seh_cdo(cls);
        if (!IS_VALID_PTR(cdo)) continue;
        bool d = true;
        seh_isdef(cdo, d);
        auto* base = static_cast<SDK::AShopItem_Base_C*>(cdo);
        if (seh_patch_cdo(base)) ++st.itemsPatched;
    }
    seh_save_inv(pc);
    st.note = st.mapsVerified > 0 ? "verified present in UnlockedMaps[]"
        : (st.mapClasses == 0 ? "no map classes enumerated (offsets stale?)" : "already unlocked");
    set_last(st);
    if (g_logger) g_logger->send(levels::success,
        "Maps: {} classes, {} granted | Shop CDOs patched: {} | Steam: {}/{} ({})",
        st.mapClasses, st.mapsGranted, st.itemsPatched,
        st.steamGranted, st.steamDefs, st.note);
}

void free_all_shop() {
    Status st = g_last;
    auto* pc = seh_local_rc();
    st.hasController = IS_VALID_PTR(pc);
    if (!st.hasController) {
        st.note = "No local RobberController (join a lobby first)";
        set_last(st);
        if (g_logger) g_logger->send(levels::error, "Shop: {}", st.note);
        return;
    }
    auto items = collect_subclasses(base_of("item"), "ShopItem_");
    st.itemClasses = (int)items.size();
    for (auto* cls : items) {
        if (!IS_VALID_PTR(cls)) continue;
        SDK::UObject* cdo = seh_cdo(cls);
        if (!IS_VALID_PTR(cdo)) continue;
        auto* base = static_cast<SDK::AShopItem_Base_C*>(cdo);
        if (seh_patch_cdo(base)) ++st.itemsPatched;
        bool has = false;
        bool readOk = seh_arr_contains_uclass(pc->ItemInventory, cls, has);
        if (readOk && has) continue;
        if (seh_additem(pc, cls)) ++st.itemsGranted;
    }
    seh_grant_cash(pc);
    seh_save_cash(pc);
    seh_save_inv(pc);
    {
        bool ok = false; int defs = 0; std::string note;
        st.steamGranted = sync_steam(pc, items, false, ok, defs, note);
        st.steamDefs = defs;
        st.steamOk = ok;
        if (g_logger) g_logger->send(levels::debug, "[steam] shop sync: {} items ({})",
            st.steamGranted, note);
    }
    for (auto* cls : items) {
        if (!IS_VALID_PTR(cls)) continue;
        bool has = false;
        if (seh_arr_contains_uclass(pc->ItemInventory, cls, has) && has)
            ++st.itemsVerified;
    }
    st.note = st.itemsVerified > 0 ? "verified present in ItemInventory[]"
        : (st.itemsGranted > 0 || st.itemsPatched > 0 ? "CDO prices $1 + inventory granted + cash topped up"
        : "nothing enumerated (offsets stale?)");
    set_last(st);
    if (g_logger) g_logger->send(levels::success,
        "Shop: {} classes, {} CDOs patched, {} granted, {} verified | Steam: {}/{} ({})",
        st.itemClasses, st.itemsPatched, st.itemsGranted, st.itemsVerified,
        st.steamGranted, st.steamDefs, st.note);
}

void free_all_skills() {
    Status st = g_last;
    auto* pc = seh_local_rc();
    st.hasController = IS_VALID_PTR(pc);
    if (!st.hasController) {
        st.note = "No local RobberController (join a lobby first)";
        set_last(st);
        if (g_logger) g_logger->send(levels::error, "Skills: {}", st.note);
        return;
    }
    auto skills = collect_subclasses(base_of("skill"), "Shop_Skill");
    if (skills.empty())
        skills = collect_subclasses(base_of("skill"), "ShopItem_");
    int patched = 0, granted = 0;
    for (auto* cls : skills) {
        if (!IS_VALID_PTR(cls)) continue;
        SDK::UObject* cdo = seh_cdo(cls);
        if (IS_VALID_PTR(cdo)) {
            if (seh_patch_skill(static_cast<SDK::AShop_SkillBase_C*>(cdo))) ++patched;
        }
        bool has = false;
        bool readOk = seh_arr_contains_uclass(pc->ItemInventory, cls, has);
        if (readOk && has) continue;
        if (seh_additem(pc, cls)) ++granted;
    }
    seh_save_inv(pc);
    {
        bool ok = false; int defs = 0; std::string note;
        int sg = sync_steam(pc, skills, false, ok, defs, note);
        if (g_logger) g_logger->send(levels::debug, "[steam] skills sync: {} items ({})", sg, note);
    }
    set_last(st);
    if (g_logger) g_logger->send(levels::success,
        "Skills: {} classes, {} CDOs patched, {} granted", (int)skills.size(), patched, granted);
}

// ---- Player-tab helpers: full catalog + current inventory + drag-grant ----
static bool seh_cdo_shop_vals(SDK::UObject* cdo, int& cash, int& coin, int& lvl, int& defid, int& cat) {
    __try {
        auto* b = static_cast<SDK::AShopItem_Base_C*>(cdo);
        cash = b->CashCost; coin = b->CoinCost; lvl = b->UnlockLevel;
        defid = b->SteamItemDefID; cat = (int)b->Item_category;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
struct InvSnap { SDK::UClass* const* d; int n; bool ok; };
static InvSnap seh_inv_snap(SDK::ARobberController_C* pc) {
    InvSnap r{ nullptr, 0, false };
    __try {
        int n = pc->ItemInventory.Num();
        if (n <= 0 || n > 100000) return r;
        auto* d = pc->ItemInventory.GetDataPtr();
        if (!d) return r;
        r.d = d; r.n = n; r.ok = true;
        return r;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return r; }
}
static SDK::UClass* seh_snap_at(SDK::UClass* const* d, int i) {
    __try { return d[i]; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

std::vector<ItemInfo> all_items() {
    std::vector<ItemInfo> out;
    auto classes = collect_subclasses(base_of("item"), "ShopItem_");
    out.reserve(classes.size());
    for (auto* cls : classes) {
        if (!IS_VALID_PTR(cls)) continue;
        ItemInfo info{};
        info.cls = cls;
        SDK::UObject* cdo = seh_cdo(cls);
        if (IS_VALID_PTR(cdo)) {
            seh_cdo_shop_vals(cdo, info.cash, info.coin, info.level, info.defid, info.cat);
            std::string nm = safe_name(cdo);
            if (!nm.empty()) {
                auto us = nm.find("_Default");
                info.name = (us != std::string::npos) ? nm.substr(0, us) : nm;
            }
        }
        if (info.name.empty()) info.name = safe_name(cls);
        out.push_back(std::move(info));
    }
    return out;
}

std::vector<std::string> inventory_names() {
    std::vector<std::string> out;
    auto* pc = seh_local_rc();
    if (!IS_VALID_PTR(pc)) return out;
    InvSnap s = seh_inv_snap(pc);
    if (!s.ok) return out;
    out.reserve((size_t)s.n);
    for (int i = 0; i < s.n; ++i) {
        SDK::UClass* cls = seh_snap_at(s.d, i);
        if (!IS_VALID_PTR(cls)) continue;
        out.push_back(safe_name(cls));
    }
    return out;
}

static std::atomic<int> g_grants{ 0 };
int grant_count() { return g_grants.load(); }

const std::vector<ItemInfo>& catalog(bool force) {
    static std::vector<ItemInfo> cat;
    static uint64_t ts = 0;
    uint64_t now = GetTickCount64();
    if (force || cat.empty() || now - ts > 8000) {
        cat = all_items();
        ts = now;
    }
    return cat;
}

static std::atomic<int> g_sel{ -1 };
int selected() { return g_sel.load(); }
void set_selected(int i) { g_sel.store(i); }

bool grant_class(void* uclass) {
    auto* pc = seh_local_rc();
    auto* cls = static_cast<SDK::UClass*>(uclass);
    if (!IS_VALID_PTR(pc) || !IS_VALID_PTR(cls)) return false;
    bool sub = false;
    SDK::UClass* mapBase = base_of("map");
    if (IS_VALID_PTR(mapBase)) seh_issub(cls, mapBase, sub);
    bool ok = seh_additem(pc, cls);
    if (sub) { seh_addmap(pc, cls); seh_save_maps(pc); }
    seh_save_inv(pc);
    if (ok) g_grants.fetch_add(1);
    return ok;
}

void coins_legit() {
    Status st{};
    auto* pc = seh_local_rc();
    st.hasController = IS_VALID_PTR(pc);
    if (!st.hasController) {
        st.note = "No local RobberController (join a lobby first)";
        set_last(st);
        if (g_logger) g_logger->send(levels::error, "Coins: {}", st.note);
        return;
    }
    bool ok = false;
    st.coinsBefore = seh_coins(pc, ok);
    seh_grant_coins(pc);
    seh_check_coins(pc);
    seh_promo(pc);
    seh_refresh_all(pc);
    st.coinsAfter = seh_coins(pc, ok);
    st.note = "coin packs via Steam grant flow (GrantCoinPacks)";
    set_last(st);
    if (g_logger) g_logger->send(levels::success,
        "Coins: {} -> {} (Steam packs; direct writes don't replicate)", st.coinsBefore, st.coinsAfter);
}


} // namespace shop

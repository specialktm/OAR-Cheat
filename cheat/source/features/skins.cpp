#include "pch/pch.h"
#include "features/skins.hpp"
#include "features/crates.hpp"
#include "features/shop.hpp"
#include "world/actor_iter.hpp"
#include "util/veh.hpp"
#include "util/files.hpp"

#include "SDK/Basic.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/GunBase_classes.hpp"
#include "SDK/WeaponSkinLibrary_classes.hpp"
#include "SDK/WeaponSkinStruct_structs.hpp"
#include "SDK/RobberController_classes.hpp"
#include "SDK/SteamCore_classes.hpp"
#include "SDK/SteamCore_structs.hpp"
#include "SDK/E_WeaponSkinCat_structs.hpp"

#include <mutex>

namespace skins {

static std::vector<SkinEntry> s_skins;
static std::mutex s_mtx;
static int s_selected = -1;
static char s_status[128] = "not scanned";

static void set_status(const char* fmt, int n) {
    std::lock_guard<std::mutex> lk(s_mtx);
    snprintf(s_status, sizeof(s_status), fmt, n);
}

template<typename T>
static bool seh_isa(SDK::AActor* a, bool& out) {
    __try { out = a->IsA(T::StaticClass()); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool seh_lookup(int id, SDK::FWeaponSkinStruct* st, bool* found) {
    __try {
        auto* world = static_cast<SDK::UObject*>(actors::game_world());
        if (!IS_VALID_PTR(world)) return false;
        SDK::UWeaponSkinLibrary_C::GetDefaultObj()->WeaponSkinLookup(id, world, st, found);
        return *found;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool lookup_id(int id, SkinEntry& out) {
    SDK::FWeaponSkinStruct st{};
    bool found = false;
    if (!seh_lookup(id, &st, &found) || !found) return false;
    std::string nm;
    bool ok = veh::try_call([&] {
        nm = st.Name_2_5C3EFC9840D99D71E80AE2AAA8AC6754.ToString();
    });
    if (!ok || nm.empty()) nm = "Skin";
    bool charm = IS_VALID_PTR(st.CharmBP_22_CA952EB441FC2B5B7BFCE19DBB0F0385);
    out.id = id;
    out.name = nm;
    out.hasCharm = charm;
    if (charm) {
        std::string cn;
        bool cok = veh::try_call([&] {
            cn = veh::get_name(st.CharmBP_22_CA952EB441FC2B5B7BFCE19DBB0F0385);
        });
        if (cok && !cn.empty()) {
            const char* cp = strstr(cn.c_str(), "Charm_");
            std::string tok = cp ? (cp + 6) : cn;
            auto up = tok.rfind("_C");
            if (up != std::string::npos) tok.erase(up);
            for (auto& ch : tok) ch = (ch == '_' ? ' ' : ch);
            if (!tok.empty()) out.charmName = tok;
        }
    }
    int steam = 0;
    veh::try_call([&] {
        steam = st.SteamID_10_600466014F11F08532ED01A7636FC70F;
    });
    out.steamId = steam;
    return true;
}

static bool seh_apply_skin(SDK::AActor* a, SDK::FWeaponSkinStruct* st, bool* charmed) {
    __try {
        if (!IS_VALID_PTR(a) || !st) return false;
        auto* gun = reinterpret_cast<SDK::AGunBase_C*>(a);
        gun->SetupGunSkin(*st, true);
        SDK::UClass* charm = st->CharmBP_22_CA952EB441FC2B5B7BFCE19DBB0F0385;
        if (IS_VALID_PTR(charm)) {
            gun->SetupCharmServer(charm);
            if (charmed) *charmed = true;
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static SDK::ARobberController_C* seh_local_rc() {
    __try {
        auto* pc = reinterpret_cast<SDK::ARobberController_C*>(actors::local_player_controller());
        if (!IS_VALID_PTR(pc)) return nullptr;
        SDK::UClass* rc = SDK::ARobberController_C::StaticClass();
        if (!IS_VALID_PTR(rc)) return pc;
        return pc->IsA(rc) ? pc : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

struct RawArr { void* d; int num; int mx; };

template<typename T>
static SDK::TArray<T> wrap_buf(T* buf, int n) {
    static_assert(sizeof(SDK::TArray<T>) == 16, "TArray layout");
    SDK::TArray<T> a{};
    RawArr* r = reinterpret_cast<RawArr*>(&a);
    r->d = buf; r->num = n; r->mx = n;
    return a;
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

static bool seh_refresh_all(SDK::ARobberController_C* pc) {
    __try { pc->OnUpdateAllSteamInventory(); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool seh_destroy(SDK::FSteamInventoryResult* res) {
    __try { SDK::UInventory::DestroyResult(*res); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

struct DetailsSnap { SDK::FSteamItemDetails const* d; int n; bool ok; };

static DetailsSnap seh_details_snap(const SDK::TArray<SDK::FSteamItemDetails>& det) {
    __try {
        int n = det.Num();
        if (n <= 0 || n > 100000) return DetailsSnap{ nullptr, 0, false };
        auto* d = det.GetDataPtr();
        if (!d) return DetailsSnap{ nullptr, 0, false };
        return DetailsSnap{ d, n, true };
    } __except (EXCEPTION_EXECUTE_HANDLER) { return DetailsSnap{ nullptr, 0, false }; }
}

static int pipe_result(SDK::ARobberController_C* pc, SDK::FSteamInventoryResult* res) {
    int granted = 0;
    seh_feed_result(pc, *res);
    SDK::TArray<SDK::FSteamItemDetails> details{};
    if (seh_get_items(res, &details)) {
        DetailsSnap s = seh_details_snap(details);
        if (s.ok) {
            granted = s.n;
            size_t cap = (size_t)s.n + 64;
            SDK::FSteamItemDetails* buf = new SDK::FSteamItemDetails[cap];
            memcpy(buf, s.d, sizeof(*buf) * (size_t)s.n);
            SDK::TArray<SDK::FSteamItemDetails> owned = wrap_buf(buf, s.n);
            reinterpret_cast<RawArr*>(&owned)->mx = (int)cap;
            seh_merge_items(pc, owned);
            seh_fire_updated(pc, owned);
        }
    }
    seh_refresh_all(pc);
    seh_destroy(res);
    return granted;
}

struct GunSnap { SDK::UClass* const* d; int n; bool ok; };

static GunSnap seh_guns(SDK::ARobberController_C* pc) {
    __try {
        int n = pc->Weapons_Equipped.Num();
        if (n <= 0 || n > 64) return GunSnap{ nullptr, 0, false };
        auto* d = pc->Weapons_Equipped.GetDataPtr();
        if (!d) return GunSnap{ nullptr, 0, false };
        return GunSnap{ d, n, true };
    } __except (EXCEPTION_EXECUTE_HANDLER) { return GunSnap{ nullptr, 0, false }; }
}

static SDK::UClass* seh_gun_at(SDK::UClass* const* d, int i) {
    __try { return d[i]; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

static bool seh_equip(SDK::ARobberController_C* pc, int rowId,
        SDK::UClass* weapon, int cat) {
    __try {
        if (!IS_VALID_PTR(pc) || !IS_VALID_PTR(weapon)) return false;
        pc->EquipSkin(rowId, weapon, (SDK::E_WeaponSkinCat)cat);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

int equip_row(int rowId) {
    SDK::FWeaponSkinStruct st{};
    bool found = false;
    if (!seh_lookup(rowId, &st, &found) || !found) return 0;
    int cat = 0;
    veh::try_call([&] {
        cat = (int)st.Category_25_015332554026C0835714BA9A31C47CD9;
    });
    SDK::ARobberController_C* pc = seh_local_rc();
    if (!IS_VALID_PTR(pc)) return 0;
    GunSnap g = seh_guns(pc);
    if (!g.ok) return 0;
    int n = 0;
    for (int i = 0; i < g.n; ++i) {
        SDK::UClass* gun = seh_gun_at(g.d, i);
        if (!IS_VALID_PTR(gun)) continue;
        if (seh_equip(pc, rowId, gun, cat)) ++n;
    }
    return n;
}

bool equip_single(int rowId, void* weapon) {
    SDK::FWeaponSkinStruct st{};
    bool found = false;
    if (!seh_lookup(rowId, &st, &found) || !found) return false;
    int cat = 0;
    veh::try_call([&] {
        cat = (int)st.Category_25_015332554026C0835714BA9A31C47CD9;
    });
    SDK::ARobberController_C* pc = seh_local_rc();
    if (!IS_VALID_PTR(pc)) return false;
    return seh_equip(pc, rowId, static_cast<SDK::UClass*>(weapon), cat);
}

bool row_info(int rowId, std::string& name, int& cat, std::string& charm, int& def) {
    SDK::FWeaponSkinStruct st{};
    bool found = false;
    if (!seh_lookup(rowId, &st, &found) || !found) return false;
    veh::try_call([&] {
        name = st.Name_2_5C3EFC9840D99D71E80AE2AAA8AC6754.ToString();
    });
    veh::try_call([&] {
        cat = (int)st.Category_25_015332554026C0835714BA9A31C47CD9;
    });
    veh::try_call([&] {
        def = st.SteamID_10_600466014F11F08532ED01A7636FC70F;
    });
    SDK::UClass* cb = nullptr;
    veh::try_call([&] {
        cb = st.CharmBP_22_CA952EB441FC2B5B7BFCE19DBB0F0385;
    });
    if (IS_VALID_PTR(cb)) {
        veh::try_call([&] {
            charm = veh::get_name(cb);
        });
        const char* cp = strstr(charm.c_str(), "Charm_");
        charm = cp ? (cp + 6) : charm;
        auto up = charm.rfind("_C");
        if (up != std::string::npos) charm.erase(up);
    }
    return true;
}

std::vector<CharmEntry> charm_shop_items() {
    std::vector<CharmEntry> out;
    static const char* keys[] = {
        "charm", "keycard", "lucky", "goldwatch", "goldbar", "vault",
        "security", "casino", "chemical", "sandwich", "traffic", "wine", "keys"
    };
    for (auto& it : shop::catalog(false)) {
        if (it.defid <= 0) continue;
        std::string n;
        for (char ch : it.name) {
            if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'))
                n += (char)tolower(ch);
        }
        bool hit = false;
        for (auto k : keys) {
            if (n.find(k) != std::string::npos) { hit = true; break; }
        }
        if (!hit) continue;
        bool dup = false;
        for (auto& e : out) if (e.def == it.defid) { dup = true; break; }
        if (dup) continue;
        CharmEntry ce;
        ce.def = it.defid;
        ce.name = it.name;
        ce.coin = it.coin;
        out.push_back(std::move(ce));
    }
    return out;
}

static bool seh_unlockedSkins(SDK::TArray<SDK::FSteamItemDetails>* inv, int cat,
        SDK::TArray<SDK::FWeaponSkinStruct>* out) {
    __try {
        auto* world = static_cast<SDK::UObject*>(actors::game_world());
        if (!IS_VALID_PTR(world)) return false;
        SDK::UWeaponSkinLibrary_C::GetDefaultObj()->GetUnlockedWeaponSkins(
            *inv, (SDK::E_WeaponSkinCat)cat, world, out);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static int seh_tnumSkins(const SDK::TArray<SDK::FWeaponSkinStruct>* o) {
    __try {
        int n = o->Num();
        if (n < 0 || n > 100000) return 0;
        return n;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static const SDK::FWeaponSkinStruct* seh_tptrSkins(const SDK::TArray<SDK::FWeaponSkinStruct>* o) {
    __try { return o->GetDataPtr(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

std::vector<OwnedSkin> unlocked_skins() {
    std::vector<OwnedSkin> out;
    std::vector<crates::SteamItem> items;
    if (!crates::snapshot(items)) return out;
    std::vector<SDK::FSteamItemDetails> v;
    v.reserve(items.size());
    for (auto& s : items) {
        SDK::FSteamItemDetails d{};
        memcpy(&d.InstanceID, &s.instance, 8);
        d.Definition.Value = s.def;
        d.Quantity = s.qty;
        v.push_back(d);
    }
    std::lock_guard<std::mutex> lk(s_mtx);
    for (int cat = 0; cat < 2; ++cat) {
        SDK::TArray<SDK::FSteamItemDetails> arr = wrap_buf(v.data(), (int)v.size());
        SDK::TArray<SDK::FWeaponSkinStruct> unlocked{};
        if (!seh_unlockedSkins(&arr, cat, &unlocked)) continue;
        int n = seh_tnumSkins(&unlocked);
        const SDK::FWeaponSkinStruct* d = seh_tptrSkins(&unlocked);
        if (!d || n <= 0) continue;
        for (int i = 0; i < n; ++i) {
            int sd = -1;
            veh::try_call([&] {
                sd = d[i].SteamID_10_600466014F11F08532ED01A7636FC70F;
            });
            if (sd <= 0) continue;
            OwnedSkin os;
            os.def = sd;
            os.cat = cat;
            veh::try_call([&] {
                os.name = d[i].Name_2_5C3EFC9840D99D71E80AE2AAA8AC6754.ToString();
            });
            for (auto& e : s_skins) {
                if (e.steamId == sd) { os.rowId = e.id; break; }
            }
            out.push_back(std::move(os));
        }
    }
    return out;
}

void unlock_everything() {
    if (s_skins.empty()) scan();
    std::vector<SkinEntry> copy;
    {
        std::lock_guard<std::mutex> lk(s_mtx);
        copy = s_skins;
    }
    if (copy.empty()) {
        set_status("no skins found", 0);
        return;
    }
    auto* pc = seh_local_rc();
    if (!IS_VALID_PTR(pc)) {
        set_status("join a lobby first", 0);
        return;
    }
    std::vector<int> defs;
    auto push_def = [&](int d) {
        if (d <= 0) return;
        for (int e : defs) if (e == d) return;
        defs.push_back(d);
    };
    int skinDefs = 0;
    for (auto& e : copy) {
        if (e.steamId > 0) { push_def(e.steamId); ++skinDefs; }
    }
    std::vector<CharmEntry> charms = charm_shop_items();
    int charmEntries = (int)charms.size();
    std::string charmNames;
    for (auto& ce : charms) {
        if (charmNames.size() < 220) {
            if (!charmNames.empty()) charmNames += "|";
            charmNames += ce.name;
        }
        push_def(ce.def);
    }
    const std::vector<shop::ItemInfo>& cat = shop::catalog(false);
    std::vector<int> steamDefs;
    int steamKnown = crates::steam_def_ids(steamDefs);
    int before0 = crates::count_unlocked_skins(0);
    int before1 = crates::count_unlocked_skins(1);
    int selIds = (int)defs.size();
    int sel = selected();
    int eq = 0;
    if (sel >= 0 && sel < (int)copy.size()) eq = equip_row(copy[sel].id);
    set_status("grant pending...", 0);
    crates::grant_async(defs, "all", [selIds, skinDefs, charmEntries, steamKnown, eq, before0, before1](int granted) {
        int after0 = crates::count_unlocked_skins(0);
        int after1 = crates::count_unlocked_skins(1);
        char buf[160];
        auto shown = [](int v, char* o) {
            if (v < 0) snprintf(o, 32, "n/a");
            else snprintf(o, 32, "%d", v);
        };
        char b0[32], b1[32], a0[32], a1[32];
        shown(before0, b0); shown(before1, b1);
        shown(after0, a0); shown(after1, a1);
        snprintf(buf, sizeof(buf), "all:%d+%d defs:%d got:%d %s/%s->%s/%s eq:%d",
            skinDefs, charmEntries, steamKnown, granted, b0, b1, a0, a1, eq);
        std::lock_guard<std::mutex> lk(s_mtx);
        strncpy_s(s_status, buf, _TRUNCATE);
    });
    {
        std::string dump;
        dump += "skins:\n";
        char ln[160];
        for (auto& e : copy) {
            snprintf(ln, sizeof(ln), "%d def=%d %s\n", e.id, e.steamId, e.name.c_str());
            dump += ln;
        }
        dump += "charms:\n";
        dump += charmNames.empty() ? "(none matched)\n" : charmNames + "\n";
        dump += "shop catalog:\n";
        for (auto& it : cat) {
            snprintf(ln, sizeof(ln), "def=%d coin=%d %s\n", it.defid, it.coin, it.name.c_str());
            dump += ln;
            if (dump.size() > 60000) break;
        }
        fileManager::writeText(fileManager::dumpPath("sku_dump.txt"), dump);
    }
}

void scan() {
    std::vector<SkinEntry> foundList;
    std::vector<int> tried;
    auto probe = [&](int id) {
        for (int t : tried) if (t == id) return;
        tried.push_back(id);
        SkinEntry e;
        if (lookup_id(id, e)) foundList.push_back(std::move(e));
    };
    int misses = 0;
    for (int id = 0; id < 4096 && misses < 256; ++id) {
        SkinEntry e;
        if (lookup_id(id, e)) {
            misses = 0;
            tried.push_back(id);
            foundList.push_back(std::move(e));
        } else {
            ++misses;
        }
    }
    std::vector<int> defs;
    if (crates::steam_def_ids(defs) > 0) {
        for (int d : defs) probe(d);
    }
    for (auto& it : shop::catalog(false)) {
        if (it.defid > 0) probe(it.defid);
    }
    int withDef = 0;
    for (auto& e : foundList) if (e.steamId > 0) ++withDef;
    std::lock_guard<std::mutex> lk(s_mtx);
    s_skins.swap(foundList);
    s_selected = s_skins.empty() ? -1 : 0;
    snprintf(s_status, sizeof(s_status), "found %d skins (%d defs)",
        (int)s_skins.size(), withDef);
}

const std::vector<SkinEntry>& list() { return s_skins; }
int selected() { return s_selected; }
void setSelected(int i) { s_selected = i; }
const char* status() { return s_status; }

void apply_selected() {
    int id = -1;
    {
        std::lock_guard<std::mutex> lk(s_mtx);
        if (s_selected >= 0 && s_selected < (int)s_skins.size())
            id = s_skins[s_selected].id;
    }
    if (id < 0) {
        set_status("pick a skin first", 0);
        return;
    }
    SDK::FWeaponSkinStruct st{};
    bool found = false;
    if (!seh_lookup(id, &st, &found) || !found) {
        set_status("skin %d gone", id);
        return;
    }
    int n = 0, charms = 0;
    auto snap = actors::copy_level_actors();
    for (void* raw : snap.list) {
        auto* a = reinterpret_cast<SDK::AActor*>(raw);
        if (!IS_VALID_PTR(a)) continue;
        bool isg = false;
        if (!seh_isa<SDK::AGunBase_C>(a, isg) || !isg) continue;
        bool ch = false;
        if (seh_apply_skin(a, &st, &ch)) {
            ++n;
            if (ch) ++charms;
        }
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "skinned %d guns (%d charms)", n, charms);
    std::lock_guard<std::mutex> lk(s_mtx);
    strncpy_s(s_status, buf, _TRUNCATE);
}

void unlock_all() {
    if (s_skins.empty()) scan();
    std::vector<SkinEntry> copy;
    {
        std::lock_guard<std::mutex> lk(s_mtx);
        copy = s_skins;
    }
    if (copy.empty()) {
        set_status("no skins found", 0);
        return;
    }
    std::vector<int> ids;
    ids.reserve(copy.size());
    int charms = 0;
    for (size_t i = 0; i < copy.size(); ++i) {
        if (copy[i].hasCharm) ++charms;
        int sid = copy[i].steamId;
        if (sid <= 0) continue;
        bool dup = false;
        for (size_t j = 0; j < ids.size(); ++j) {
            if (ids[j] == sid) { dup = true; break; }
        }
        if (!dup) ids.push_back(sid);
    }
    SDK::ARobberController_C* pc = seh_local_rc();
    int sel = selected();
    if (sel < 0 || sel >= (int)copy.size()) sel = 0;
    int selRow = copy[sel].id;
    int selDef = copy[sel].steamId;
    int selCat = 0;
    if (!IS_VALID_PTR(pc)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "unlocked %d skins, join lobby for steam sync", (int)copy.size());
        std::lock_guard<std::mutex> lk(s_mtx);
        strncpy_s(s_status, buf, _TRUNCATE);
    } else if (!ids.empty()) {
        set_status("grant pending...", 0);
        int nCopy = (int)copy.size();
        crates::grant_async(ids, "skins", [nCopy, charms, selDef, selRow](int granted) {
            int eq = equip_row(selRow);
            int verified = 0;
            if (selDef > 0)
                verified = crates::count_unlocked_def(0, selDef) + crates::count_unlocked_def(1, selDef);
            char buf[128];
            snprintf(buf, sizeof(buf), "unlocked %d (%d steam, %d charms, eq %d, owned %d)",
                nCopy, granted, charms, eq, verified);
            std::lock_guard<std::mutex> lk(s_mtx);
            strncpy_s(s_status, buf, _TRUNCATE);
        });
    }
    {
        SDK::FWeaponSkinStruct st{};
        bool found = false;
        if (seh_lookup(selRow, &st, &found) && found) {
            veh::try_call([&] {
                selCat = (int)st.Category_25_015332554026C0835714BA9A31C47CD9;
            });
            auto snap = actors::copy_level_actors();
            for (void* raw : snap.list) {
                auto* a = reinterpret_cast<SDK::AActor*>(raw);
                if (!IS_VALID_PTR(a)) continue;
                bool isg = false;
                if (!seh_isa<SDK::AGunBase_C>(a, isg) || !isg) continue;
                bool ch = false;
                seh_apply_skin(a, &st, &ch);
            }
        }
    }
    int eq = equip_row(selRow);
    if (!IS_VALID_PTR(pc) || ids.empty()) {
        char buf[128];
        if (ids.empty())
            snprintf(buf, sizeof(buf), "unlocked %d skins, no steam defs (eq %d)",
                (int)copy.size(), eq);
        else
            snprintf(buf, sizeof(buf), "unlocked %d skins, join lobby (eq %d)",
                (int)copy.size(), eq);
        std::lock_guard<std::mutex> lk(s_mtx);
        strncpy_s(s_status, buf, _TRUNCATE);
    }
}

}

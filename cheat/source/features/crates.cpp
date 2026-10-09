#include "pch/pch.h"
#include "features/crates.hpp"
#include "features/skins.hpp"
#include "features/shop.hpp"
#include "world/actor_iter.hpp"
#include "util/veh.hpp"

#include "SDK/Basic.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/RobberController_classes.hpp"
#include "SDK/WeaponSkinLibrary_classes.hpp"
#include "SDK/WeaponSkinStruct_structs.hpp"
#include "SDK/CrateStruct_structs.hpp"
#include "SDK/WinUI_classes.hpp"
#include "SDK/SteamCore_classes.hpp"
#include "SDK/SteamCore_structs.hpp"
#include "SDK/E_WeaponSkinCat_structs.hpp"
#include "logger/logger.hpp"

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "util/files.hpp"

namespace crates {

struct CrateEntry {
    int id = -1;
    std::string name;
    int steamId = 0;
    void* crateBp = nullptr;
    std::vector<int> skinIds;
};

static std::vector<CrateEntry> s_crates;
static std::vector<int> s_dropList;
static std::vector<int> s_genList;
static std::vector<int> s_itemList;
static int s_sniffIdx = 0;
static int s_sniffTotal = 0;
static uint64_t s_sniffIdleTs = 0;
static std::mutex s_mtx;
static char g_line[160] = "idle";

static void set_line(const char* fmt, int a, int b, int c) {
    std::lock_guard<std::mutex> lk(s_mtx);
    snprintf(g_line, sizeof(g_line), fmt, a, b, c);
}

static void set_text(const char* s) {
    std::lock_guard<std::mutex> lk(s_mtx);
    strncpy_s(g_line, s, _TRUNCATE);
}

const char* last_line() {
    thread_local char buf[160] = "idle";
    std::lock_guard<std::mutex> lk(s_mtx);
    memcpy(buf, g_line, sizeof(buf));
    buf[sizeof(buf) - 1] = 0;
    return buf;
}

static bool seh_crate_lookup(int id, SDK::FCrateStruct* st, bool* found) {
    __try {
        auto* world = static_cast<SDK::UObject*>(actors::game_world());
        if (!IS_VALID_PTR(world)) return false;
        SDK::UWeaponSkinLibrary_C::GetDefaultObj()->CrateLookup(id, world, st, found);
        return *found;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static int seh_skin_ids(SDK::FCrateStruct* st, int* out, int cap) {
    __try {
        int n = st->SkinsID_21_E56A84D548D4D4691C6730B0364C496C.Num();
        if (n <= 0 || n > 4096) return 0;
        auto* d = st->SkinsID_21_E56A84D548D4D4691C6730B0364C496C.GetDataPtr();
        if (!d) return 0;
        if (n > cap) n = cap;
        memcpy(out, d, sizeof(int) * (size_t)n);
        return n;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static bool seh_skin_lookup(int id, SDK::FWeaponSkinStruct* st, bool* found) {
    __try {
        auto* world = static_cast<SDK::UObject*>(actors::game_world());
        if (!IS_VALID_PTR(world)) return false;
        SDK::UWeaponSkinLibrary_C::GetDefaultObj()->WeaponSkinLookup(id, world, st, found);
        return *found;
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

static bool seh_getall(SDK::FSteamInventoryResult* res) {
    __try { return SDK::UInventory::GetAllItems(res); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool seh_result_items(const SDK::FSteamInventoryResult* res,
        SDK::TArray<SDK::FSteamItemDetails>* out) {
    __try { return SDK::UInventory::GetResultItems(*res, out); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

struct DetSnap { SDK::FSteamItemDetails const* d; int n; bool ok; };

static DetSnap seh_det_snap(const SDK::TArray<SDK::FSteamItemDetails>* det) {
    __try {
        int n = det->Num();
        if (n <= 0 || n > 100000) return DetSnap{ nullptr, 0, false };
        auto* d = det->GetDataPtr();
        if (!d) return DetSnap{ nullptr, 0, false };
        return DetSnap{ d, n, true };
    } __except (EXCEPTION_EXECUTE_HANDLER) { return DetSnap{ nullptr, 0, false }; }
}

static int seh_copy(const SDK::FSteamItemDetails* d, int n,
        SDK::FSteamItemDetails* out, int cap) {
    __try {
        if (n > cap) n = cap;
        memcpy(out, d, sizeof(*out) * (size_t)n);
        return n;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static bool seh_generate(SDK::FSteamInventoryResult* res,
        const SDK::TArray<SDK::FSteamItemDef>& defs, const SDK::TArray<int>& qtys) {
    __try { return SDK::UInventory::GenerateItems(res, defs, qtys); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool seh_exchange(SDK::FSteamInventoryResult* res,
        const SDK::TArray<SDK::FSteamItemDef>& gen, const SDK::TArray<int>& genQ,
        const SDK::TArray<SDK::FSteamItemInstanceID>& dst, const SDK::TArray<int>& dstQ) {
    __try { return SDK::UInventory::ExchangeItems(res, gen, genQ, dst, dstQ); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static int seh_status(const SDK::FSteamInventoryResult* res) {
    __try { return (int)SDK::UInventory::GetResultStatus(*res); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return -999; }
}

static bool seh_feed(SDK::ARobberController_C* pc, const SDK::FSteamInventoryResult& res) {
    __try { pc->UpdateSteamInventory(res); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool seh_merge(SDK::ARobberController_C* pc, SDK::TArray<SDK::FSteamItemDetails>& det) {
    __try { pc->UpdateItemsInInventory(det); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool seh_fire(SDK::ARobberController_C* pc, const SDK::TArray<SDK::FSteamItemDetails>& det) {
    __try { pc->OnSteamInventoryUpdated(det); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool seh_refresh(SDK::ARobberController_C* pc) {
    __try { pc->OnUpdateAllSteamInventory(); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool seh_destroy(SDK::FSteamInventoryResult* res) {
    __try { SDK::UInventory::DestroyResult(*res); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool seh_grant_promo(SDK::FSteamInventoryResult* res) {
    __try { return SDK::UInventory::GrantPromoItems(res); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool seh_unlocked(SDK::TArray<SDK::FSteamItemDetails>* inv, int cat,
        SDK::TArray<SDK::FWeaponSkinStruct>* out) {
    __try {
        auto* world = static_cast<SDK::UObject*>(actors::game_world());
        if (!IS_VALID_PTR(world)) return false;
        SDK::UWeaponSkinLibrary_C::GetDefaultObj()->GetUnlockedWeaponSkins(
            *inv, (SDK::E_WeaponSkinCat)cat, world, out);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static int seh_tnum(const SDK::TArray<SDK::FWeaponSkinStruct>* o) {
    __try {
        int n = o->Num();
        if (n < 0 || n > 100000) return 0;
        return n;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static const SDK::FWeaponSkinStruct* seh_tptr(const SDK::TArray<SDK::FWeaponSkinStruct>* o) {
    __try { return o->GetDataPtr(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

static int pipe_result(SDK::ARobberController_C* pc, SDK::FSteamInventoryResult* res) {
    int granted = 0;
    seh_feed(pc, *res);
    SDK::TArray<SDK::FSteamItemDetails> det{};
    if (seh_result_items(res, &det)) {
        DetSnap s = seh_det_snap(&det);
        if (s.ok) {
            granted = s.n;
            size_t cap = (size_t)s.n + 64;
            SDK::FSteamItemDetails* buf = new SDK::FSteamItemDetails[cap];
            int m = seh_copy(s.d, s.n, buf, (int)cap);
            if (m > 0) {
                SDK::TArray<SDK::FSteamItemDetails> owned = wrap_buf(buf, m);
                reinterpret_cast<RawArr*>(&owned)->mx = (int)cap;
                seh_merge(pc, owned);
                seh_fire(pc, owned);
            } else {
                delete[] buf;
                granted = 0;
            }
        }
    }
    seh_refresh(pc);
    seh_destroy(res);
    return granted;
}

bool snapshot(std::vector<SteamItem>& out) {
    out.clear();
    SDK::FSteamInventoryResult res{};
    if (!seh_getall(&res)) return false;
    SDK::TArray<SDK::FSteamItemDetails> det{};
    bool got = seh_result_items(&res, &det);
    seh_destroy(&res);
    if (!got) return false;
    DetSnap s = seh_det_snap(&det);
    if (!s.ok) return true;
    std::vector<SDK::FSteamItemDetails> buf((size_t)s.n);
    int m = seh_copy(s.d, s.n, buf.data(), s.n);
    for (int i = 0; i < m; ++i) {
        SteamItem it;
        memcpy(&it.instance, &buf[(size_t)i].InstanceID, 8);
        it.def = buf[(size_t)i].Definition.Value;
        it.qty = buf[(size_t)i].Quantity;
        out.push_back(it);
    }
    return true;
}

static bool seh_getdefids(SDK::TArray<SDK::FSteamItemDef>* out) {
    __try { return SDK::UInventory::GetItemDefinitionIDs(out); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static DetSnap seh_defs_snap(const SDK::TArray<SDK::FSteamItemDef>* arr) {
    __try {
        int n = arr->Num();
        if (n <= 0 || n > 100000) return DetSnap{ nullptr, 0, false };
        auto* d = arr->GetDataPtr();
        if (!d) return DetSnap{ nullptr, 0, false };
        return DetSnap{ reinterpret_cast<const SDK::FSteamItemDetails*>(d), n, true };
    } __except (EXCEPTION_EXECUTE_HANDLER) { return DetSnap{ nullptr, 0, false }; }
}

static int seh_def_copy(const SDK::FSteamItemDef* d, int n, int* out, int cap) {
    __try {
        if (n > cap) n = cap;
        for (int i = 0; i < n; ++i) out[i] = d[i].Value;
        return n;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

int steam_def_ids(std::vector<int>& out) {
    out.clear();
    SDK::TArray<SDK::FSteamItemDef> arr{};
    if (!seh_getdefids(&arr)) return -1;
    DetSnap s = seh_defs_snap(&arr);
    if (!s.ok) return 0;
    std::vector<int> buf((size_t)s.n);
    int m = seh_def_copy(reinterpret_cast<const SDK::FSteamItemDef*>(s.d), s.n,
        buf.data(), s.n);
    out.assign(buf.begin(), buf.begin() + m);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return (int)out.size();
}

static bool try_crate_id(int id, std::vector<CrateEntry>& found) {
    SDK::FCrateStruct st{};
    bool f = false;
    if (!seh_crate_lookup(id, &st, &f) || !f) return false;
    CrateEntry e;
    e.id = id;
    bool ok = veh::try_call([&] {
        e.name = st.Name_4_0FD93890480FD9EE6421B3BE673E7A56.ToString();
    });
    if (!ok || e.name.empty()) e.name = "Crate";
    int sid = 0;
    veh::try_call([&] {
        sid = st.SteamID_15_E9937E0E4AC4D383F0166DACBAE0A4CB;
    });
    e.steamId = sid;
    veh::try_call([&] {
        e.crateBp = st.CrateBP_11_78B2CC7A4BD999615C5CB3B8FAA476FD;
    });
    int ids[256];
    int n = seh_skin_ids(&st, ids, 256);
    for (int i = 0; i < n; ++i) e.skinIds.push_back(ids[i]);
    found.push_back(std::move(e));
    return true;
}

static void scan_crates() {
    std::vector<CrateEntry> found;
    std::vector<int> tried;
    auto probe = [&](int id) {
        for (int t : tried) if (t == id) return;
        tried.push_back(id);
        try_crate_id(id, found);
    };
    int misses = 0;
    for (int id = 0; id < 4096 && misses < 256; ++id) {
        if (try_crate_id(id, found)) { misses = 0; tried.push_back(id); }
        else ++misses;
    }
    std::vector<int> defs;
    if (steam_def_ids(defs) > 0) {
        for (int d : defs) probe(d);
    }
    for (auto& it : shop::catalog(false)) {
        if (it.defid > 0) probe(it.defid);
    }
    std::lock_guard<std::mutex> lk(s_mtx);
    s_crates.swap(found);
}

std::vector<CrateInfo> list_crates() {
    if (s_crates.empty()) scan_crates();
    std::lock_guard<std::mutex> lk(s_mtx);
    std::vector<CrateInfo> out;
    for (auto& e : s_crates) {
        CrateInfo ci;
        ci.name = e.name;
        ci.cls = e.crateBp;
        out.push_back(std::move(ci));
    }
    return out;
}

static int build_details(const std::vector<SteamItem>& items,
        std::vector<SDK::FSteamItemDetails>& out) {
    out.clear();
    out.reserve(items.size());
    for (auto& s : items) {
        SDK::FSteamItemDetails d{};
        memcpy(&d.InstanceID, &s.instance, 8);
        d.Definition.Value = s.def;
        d.Quantity = s.qty;
        out.push_back(d);
    }
    return (int)out.size();
}

int count_unlocked_skins(int category) {
    std::vector<SteamItem> items;
    if (!snapshot(items)) return -1;
    std::vector<SDK::FSteamItemDetails> v;
    build_details(items, v);
    SDK::TArray<SDK::FSteamItemDetails> arr = wrap_buf(v.data(), (int)v.size());
    SDK::TArray<SDK::FWeaponSkinStruct> unlocked{};
    if (!seh_unlocked(&arr, category, &unlocked)) return -2;
    return seh_tnum(&unlocked);
}

int count_unlocked_def(int category, int skinDef) {
    std::vector<SteamItem> items;
    if (!snapshot(items)) return -1;
    std::vector<SDK::FSteamItemDetails> v;
    build_details(items, v);
    SDK::TArray<SDK::FSteamItemDetails> arr = wrap_buf(v.data(), (int)v.size());
    SDK::TArray<SDK::FWeaponSkinStruct> unlocked{};
    if (!seh_unlocked(&arr, category, &unlocked)) return -2;
    int n = seh_tnum(&unlocked);
    const SDK::FWeaponSkinStruct* d = seh_tptr(&unlocked);
    if (!d || n <= 0) return 0;
    int hits = 0;
    for (int i = 0; i < n; ++i) {
        int sd = -1;
        veh::try_call([&] {
            sd = d[i].SteamID_10_600466014F11F08532ED01A7636FC70F;
        });
        if (sd == skinDef) ++hits;
    }
    return hits;
}

static bool find_instance(const std::vector<SteamItem>& items, int def,
        unsigned long long& inst) {
    for (auto& s : items) {
        if (s.def == def && s.qty > 0 && s.instance != 0) {
            inst = s.instance;
            return true;
        }
    }
    return false;
}

static int skin_def_of(int skinRow, int& catOut, std::string& nameOut) {
    SDK::FWeaponSkinStruct st{};
    bool f = false;
    if (!seh_skin_lookup(skinRow, &st, &f) || !f) return -1;
    int def = 0;
    veh::try_call([&] {
        def = st.SteamID_10_600466014F11F08532ED01A7636FC70F;
    });
    veh::try_call([&] {
        catOut = (int)st.Category_25_015332554026C0835714BA9A31C47CD9;
    });
    veh::try_call([&] {
        nameOut = st.Name_2_5C3EFC9840D99D71E80AE2AAA8AC6754.ToString();
    });
    return def;
}

struct PendingOp {
    SDK::FSteamInventoryResult res{};
    uint64_t t0 = 0;
    std::string tag;
    std::function<void(int)> onDone;
};

static std::vector<PendingOp> s_pending;

void grant_async(const std::vector<int>& defs, const std::string& tag,
        std::function<void(int)> onDone) {
    struct Agg {
        int total = 0;
        int left = 0;
        std::function<void(int)> cb;
    };
    auto agg = std::make_shared<Agg>();
    agg->cb = std::move(onDone);
    int pushed = 0;
    for (size_t o = 0; o < defs.size(); o += 64) {
        size_t n = defs.size() - o;
        if (n > 64) n = 64;
        std::vector<SDK::FSteamItemDef> d(n);
        std::vector<int> q(n, 1);
        for (size_t i = 0; i < n; ++i) d[i].Value = defs[o + i];
        SDK::TArray<SDK::FSteamItemDef> aD = wrap_buf(d.data(), (int)n);
        SDK::TArray<int> aQ = wrap_buf(q.data(), (int)n);
        SDK::FSteamInventoryResult res{};
        if (!seh_generate(&res, aD, aQ)) continue;
        PendingOp op;
        op.res = res;
        op.t0 = GetTickCount64();
        op.tag = tag;
        op.onDone = [agg](int g) {
            if (g > 0) agg->total += g;
            if (--agg->left == 0 && agg->cb) agg->cb(agg->total);
        };
        {
            std::lock_guard<std::mutex> lk(s_mtx);
            if (s_pending.size() >= 24) continue;
            s_pending.push_back(std::move(op));
        }
        ++pushed;
    }
    if (pushed == 0) {
        if (agg->cb) agg->cb(0);
        return;
    }
    agg->left = pushed;
}

void exchange_async(int skinDef, unsigned long long inst, const std::string& tag,
        std::function<void(int)> onDone) {
    SDK::FSteamItemDef gd{};
    gd.Value = skinDef;
    SDK::FSteamItemInstanceID iid{};
    memcpy(&iid, &inst, 8);
    int gq = 1, dq = 1;
    SDK::TArray<SDK::FSteamItemDef> aG = wrap_buf(&gd, 1);
    SDK::TArray<int> aGQ = wrap_buf(&gq, 1);
    SDK::TArray<SDK::FSteamItemInstanceID> aD = wrap_buf(&iid, 1);
    SDK::TArray<int> aDQ = wrap_buf(&dq, 1);
    SDK::FSteamInventoryResult res{};
    if (!seh_exchange(&res, aG, aGQ, aD, aDQ)) {
        if (onDone) onDone(-100);
        return;
    }
    PendingOp op;
    op.res = res;
    op.t0 = GetTickCount64();
    op.tag = tag;
    op.onDone = std::move(onDone);
    std::lock_guard<std::mutex> lk(s_mtx);
    if (s_pending.size() >= 24) {
        seh_destroy(&res);
        if (op.onDone) op.onDone(-100);
        return;
    }
    s_pending.push_back(std::move(op));
}

static int seh_gnum() {
    __try { return SDK::UObject::GObjects->Num(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static SDK::UObject* seh_gobj(int i) {
    __try { return SDK::UObject::GObjects->GetByIndex(i); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

static bool seh_trigger(SDK::FSteamInventoryResult* res, const SDK::FSteamItemDef& listDef) {    __try { return SDK::UInventory::TriggerItemDrop(res, listDef); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool seh_defprop(int def, const wchar_t* prop, std::string& out) {
    bool got = false;
    std::string tmp;
    bool ok = veh::try_call([&] {
        SDK::FSteamItemDef ld{};
        ld.Value = def;
        SDK::FString val;
        if (SDK::UInventory::GetItemDefinitionProperty(ld, prop, &val)) {
            tmp = val.ToString();
            got = true;
        }
    });
    if (!ok || !got) return false;
    out = tmp;
    return true;
}

void scan_lists(bool force) {
    static bool done = false;
    if (done && !force) return;
    done = true;
    std::vector<int> defs;
    if (steam_def_ids(defs) <= 0) {
        set_text("no steam defs");
        return;
    }
    set_text("scanning lists...");
    std::string dump;
    int picked = 0, named = 0;
    for (int d : defs) {
        if (d <= 0) continue;
        std::string nm;
        if (!seh_defprop(d, L"name", nm) || nm.empty()) continue;
        ++named;
        std::string tp;
        seh_defprop(d, L"type", tp);
        {
            char row[224];
            snprintf(row, sizeof(row), "def %d: %s | type=%s\n", d, nm.c_str(),
                tp.empty() ? "?" : tp.c_str());
            dump += row;
        }
        std::string n;
        for (char ch : nm) {
            if (ch >= 'A' && ch <= 'Z') n += (char)(ch + 32);
            else if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) n += ch;
        }
        std::string t;
        for (char ch : tp) {
            if (ch >= 'A' && ch <= 'Z') t += (char)(ch + 32);
            else if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) t += ch;
        }
        bool gen = t.find("generat") != std::string::npos;
        {
            std::lock_guard<std::mutex> lk(s_mtx);
            if (t == "item") {
                bool dup = false;
                for (int e : s_itemList) {
                    if (e == d) { dup = true; break; }
                }
                if (!dup) s_itemList.push_back(d);
            }
        }
        if (!gen && n.find("drop") == std::string::npos && n.find("list") == std::string::npos &&
                n.find("crate") == std::string::npos && n.find("case") == std::string::npos &&
                n.find("reward") == std::string::npos)
            continue;
        {
            std::lock_guard<std::mutex> lk(s_mtx);
            bool dup = false;
            for (int e : s_dropList) {
                if (e == d) { dup = true; break; }
            }
            if (!dup) s_dropList.push_back(d);
            if (gen) {
                bool gd = false;
                for (int e : s_genList) {
                    if (e == d) { gd = true; break; }
                }
                if (!gd) s_genList.push_back(d);
            }
        }
        ++picked;
        if (g_logger) g_logger->send(levels::success, "Crates: list candidate {} ({} type={})",
            d, nm, tp.empty() ? "?" : tp);
    }
    {
        char head[128];
        snprintf(head, sizeof(head), "steam defs: %d, named %d\n", (int)defs.size(), named);
        dump = head + dump;
    }
    fileManager::writeText(fileManager::dumpPath("drop_defs.txt"), dump);
    char buf[160];
    snprintf(buf, sizeof(buf), "lists: %d picked of %d defs (%d named)", picked, (int)defs.size(), named);
    set_text(buf);
    if (g_logger) g_logger->send(levels::success, "Crates: {}", buf);
}

static const char* result_name(int st) {
    switch (st) {
    case 1: return "OK";
    case 2: return "Fail";
    case 8: return "InvalidParam";
    case 15: return "AccessDenied";
    case 16: return "Timeout";
    case 22: return "Pending";
    case 25: return "LimitExceeded";
    case 42: return "NoMatch";
    case 84: return "RateLimitExceeded";
    case 104: return "InvalidItemType";
    default: return "?";
    }
}

void drop_async(const std::vector<int>& defs, const std::string& tag,
        std::function<void(int)> onDone) {
    struct Agg {
        int total = 0;
        int left = 0;
        std::function<void(int)> cb;
    };
    auto agg = std::make_shared<Agg>();
    agg->cb = std::move(onDone);
    int pushed = 0;
    for (int def : defs) {
        if (def <= 0) continue;
        SDK::FSteamItemDef ld{};
        ld.Value = def;
        SDK::FSteamInventoryResult res{};
        if (!seh_trigger(&res, ld)) continue;
        PendingOp op;
        op.res = res;
        op.t0 = GetTickCount64();
        op.tag = tag;
        op.onDone = [agg, def](int g) {
            if (g > 0) agg->total += g;
            if (g_logger) {
                char lb[96];
                if (g == -1)
                    snprintf(lb, sizeof(lb), "drop def %d: Timeout (-1)", def);
                else if (g < 0)
                    snprintf(lb, sizeof(lb), "drop def %d: %s (%d)", def, result_name(-g), g);
                else
                    snprintf(lb, sizeof(lb), "drop def %d: got %d", def, g);
                g_logger->send(g > 0 ? levels::success : levels::error, "Crates: {}", lb);
            }
            if (--agg->left == 0 && agg->cb) agg->cb(agg->total);
        };
        {
            std::lock_guard<std::mutex> lk(s_mtx);
            if (s_pending.size() >= 24) continue;
            s_pending.push_back(std::move(op));
        }
        ++pushed;
    }
    if (pushed == 0) {
        if (agg->cb) agg->cb(0);
        return;
    }
    agg->left = pushed;
}

void drop_all() {    if (s_crates.empty()) scan_crates();
    std::vector<CrateEntry> copy;
    {
        std::lock_guard<std::mutex> lk(s_mtx);
        copy = s_crates;
    }
    if (copy.empty()) {
        set_text("found 0 crate types");
        return;
    }
    auto* pc = seh_local_rc();
    if (!IS_VALID_PTR(pc)) {
        set_text("join a lobby first");
        return;
    }
    bool needScan = false;
    {
        std::lock_guard<std::mutex> lk(s_mtx);
        if (s_dropList.empty()) needScan = true;
    }
    if (needScan) scan_lists();
    std::vector<int> defs;
    {
        std::lock_guard<std::mutex> lk(s_mtx);
        for (int d : s_dropList) defs.push_back(d);
        for (int d : s_itemList) {
            bool dup = false;
            for (int e : defs) {
                if (e == d) { dup = true; break; }
            }
            if (!dup) defs.push_back(d);
        }
    }
    for (auto& e : copy) {
        if (e.steamId <= 0) continue;
        bool dup = false;
        for (int d : defs) {
            if (d == e.steamId) { dup = true; break; }
        }
        if (!dup) defs.push_back(e.steamId);
    }
    set_text("drop pending...");
    struct Chain {
        std::vector<int> defs;
        std::vector<int> gens;
        std::vector<SteamItem> before;
        size_t i = 0;
        int got = 0;
    };
    auto ch = std::make_shared<Chain>();
    ch->defs = defs;
    snapshot(ch->before);
    {
        std::lock_guard<std::mutex> lk(s_mtx);
        ch->gens = s_genList;
    }
    auto step = std::make_shared<std::function<void()>>();
    *step = [ch, step]() {
        if (ch->i >= ch->defs.size()) {
            std::vector<SteamItem> items;
            snapshot(items);
            int owned = 0;
            for (int d : ch->defs) {
                unsigned long long inst = 0;
                if (find_instance(items, d, inst)) ++owned;
            }
            std::vector<std::pair<int, int>> fresh;
            for (auto& it : items) {
                int have = 0;
                for (auto& b : ch->before) {
                    if (b.instance == it.instance) { have = 1; break; }
                }
                if (have) continue;
                bool aggd = false;
                for (auto& f : fresh) {
                    if (f.first == it.def) { f.second += it.qty; aggd = true; break; }
                }
                if (!aggd) fresh.push_back({ it.def, it.qty });
            }
            for (size_t k = 0; k < fresh.size() && k < 10; ++k) {
                if (g_logger) g_logger->send(levels::success,
                    "Crates: new arrival def {} x{}", fresh[k].first, fresh[k].second);
            }
            char buf[160];
            snprintf(buf, sizeof(buf), "drop: %d types, owned %d (got %d, new %d)",
                (int)ch->defs.size(), owned, ch->got, (int)fresh.size());
            set_text(buf);
            if (g_logger) g_logger->send(levels::success, "Crates: {}", buf);
            return;
        }
        int def = ch->defs[ch->i++];
        bool gen = false;
        for (int g : ch->gens) {
            if (g == def) { gen = true; break; }
        }
        char pb[96];
        snprintf(pb, sizeof(pb), "%s %d/%d def %d...",
            gen ? "gen" : "drop", (int)ch->i, (int)ch->defs.size(), def);
        set_text(pb);
        std::vector<int> one{ def };
        if (gen) {
            grant_async(one, "gen", [ch, step](int got) {
                if (got > 0) ch->got += got;
                if (g_logger) {
                    char lb[96];
                    if (got < 0)
                        snprintf(lb, sizeof(lb), "gen def %d: %s (%d)",
                            ch->defs[ch->i - 1], result_name(-got), got);
                    else
                        snprintf(lb, sizeof(lb), "gen def %d: got %d",
                            ch->defs[ch->i - 1], got);
                    g_logger->send(got > 0 ? levels::success : levels::error, "Crates: {}", lb);
                }
                (*step)();
            });
        } else {
            drop_async(one, "drop", [ch, step](int got) {
                if (got > 0) ch->got += got;
                (*step)();
            });
        }
    };
    (*step)();
}

static bool seh_steam_id(SDK::FSteamID* out) {
    __try {
        *out = SDK::UUser::GetSteamID();
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool seh_eligible(const SDK::FSteamID& sid, SDK::TArray<SDK::FSteamItemDef>* out) {
    __try { return SDK::UInventory::GetEligiblePromoItemDefinitionIDs(sid, out); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool seh_addpromo(SDK::FSteamInventoryResult* res,
        const SDK::TArray<SDK::FSteamItemDef>& defs) {
    __try { return SDK::UInventory::AddPromoItems(res, defs); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void promo_all() {
    if (s_crates.empty()) scan_crates();
    std::vector<CrateEntry> copy;
    {
        std::lock_guard<std::mutex> lk(s_mtx);
        copy = s_crates;
    }
    auto* pc = seh_local_rc();
    if (!IS_VALID_PTR(pc)) {
        set_text("join a lobby first");
        return;
    }
    SDK::FSteamID sid{};
    if (!seh_steam_id(&sid)) {
        set_text("no steam id");
        return;
    }
    SDK::TArray<SDK::FSteamItemDef> arr{};
    if (!seh_eligible(sid, &arr)) {
        set_text("promo query refused");
        return;
    }
    DetSnap s = seh_defs_snap(&arr);
    if (!s.ok) {
        set_text("promo query empty");
        return;
    }
    std::vector<int> buf((size_t)s.n);
    int m = seh_def_copy(reinterpret_cast<const SDK::FSteamItemDef*>(s.d), s.n,
        buf.data(), s.n);
    std::vector<int> gens;
    {
        std::lock_guard<std::mutex> lk(s_mtx);
        gens = s_genList;
    }
    std::vector<int> match;
    for (int i = 0; i < m; ++i) {
        int d = buf[(size_t)i];
        if (d <= 0) continue;
        bool want = false;
        for (auto& e : copy) {
            if (e.steamId == d) { want = true; break; }
        }
        if (!want) {
            for (int g : gens) {
                if (g == d) { want = true; break; }
            }
        }
        if (want) match.push_back(d);
    }
    if (match.empty()) {
        char b2[96];
        snprintf(b2, sizeof(b2), "promo eligible %d, no crates among them", m);
        set_text(b2);
        if (g_logger) g_logger->send(levels::error, "Crates: {}", b2);
        return;
    }
    std::vector<SDK::FSteamItemDef> dl(match.size());
    for (size_t i = 0; i < match.size(); ++i) dl[i].Value = match[i];
    SDK::TArray<SDK::FSteamItemDef> aD = wrap_buf(dl.data(), (int)dl.size());
    SDK::FSteamInventoryResult res{};
    if (!seh_addpromo(&res, aD)) {
        set_text("promo claim refused");
        return;
    }
    set_text("promo pending...");
    PendingOp op;
    op.res = res;
    op.t0 = GetTickCount64();
    op.tag = "promo";
    op.onDone = [match](int got) {
        std::vector<SteamItem> items;
        snapshot(items);
        int owned = 0;
        for (int d : match) {
            unsigned long long inst = 0;
            if (find_instance(items, d, inst)) ++owned;
        }
        char b3[160];
        snprintf(b3, sizeof(b3), "promo: %d crates, owned %d (got %d)",
            (int)match.size(), owned, got);
        set_text(b3);
        if (g_logger) g_logger->send(levels::success, "Crates: {}", b3);
    };
    std::lock_guard<std::mutex> lk(s_mtx);
    if (s_pending.size() >= 24) {
        seh_destroy(&res);
        set_text("promo queue full");
        return;
    }
    s_pending.push_back(std::move(op));
}

void promo_coins() {
    auto* pc = seh_local_rc();
    if (!IS_VALID_PTR(pc)) {
        set_text("join a lobby first");
        return;
    }
    SDK::FSteamID sid{};
    if (!seh_steam_id(&sid)) {
        set_text("no steam id");
        return;
    }
    SDK::TArray<SDK::FSteamItemDef> arr{};
    if (!seh_eligible(sid, &arr)) {
        set_text("promo query refused");
        return;
    }
    DetSnap s = seh_defs_snap(&arr);
    if (!s.ok) {
        set_text("promo query empty");
        return;
    }
    std::vector<int> buf((size_t)s.n);
    int m = seh_def_copy(reinterpret_cast<const SDK::FSteamItemDef*>(s.d), s.n,
        buf.data(), s.n);
    static const int coinDefs[] = { 1501, 3151, 8501, 20001 };
    std::vector<int> match;
    for (int i = 0; i < m; ++i) {
        int d = buf[(size_t)i];
        if (d <= 0) continue;
        for (int c : coinDefs) {
            if (c == d) { match.push_back(d); break; }
        }
    }
    if (match.empty()) {
        char b2[96];
        snprintf(b2, sizeof(b2), "promo eligible %d, no coin packs among them", m);
        set_text(b2);
        if (g_logger) g_logger->send(levels::error, "Crates: {}", b2);
        return;
    }
    std::vector<SDK::FSteamItemDef> dl(match.size());
    for (size_t i = 0; i < match.size(); ++i) dl[i].Value = match[i];
    SDK::TArray<SDK::FSteamItemDef> aD = wrap_buf(dl.data(), (int)dl.size());
    SDK::FSteamInventoryResult res{};
    if (!seh_addpromo(&res, aD)) {
        set_text("coin claim refused");
        return;
    }
    set_text("coin claim pending...");
    PendingOp op;
    op.res = res;
    op.t0 = GetTickCount64();
    op.tag = "promo-coins";
    op.onDone = [match](int got) {
        char b3[160];
        snprintf(b3, sizeof(b3), "coin packs: %d claimed (got %d), check balance",
            (int)match.size(), got);
        set_text(b3);
        if (g_logger) g_logger->send(levels::success, "Crates: {}", b3);
    };
    std::lock_guard<std::mutex> lk(s_mtx);
    if (s_pending.size() >= 24) {
        seh_destroy(&res);
        set_text("promo queue full");
        return;
    }
    s_pending.push_back(std::move(op));
}

static std::unordered_map<void*, bool> s_winWas;
static std::unordered_set<void*> s_winSeen;

static bool note_winui(void* w, bool was, int sid, bool countFirst) {
    auto it = s_winWas.find(w);
    bool fresh = false;
    if (it == s_winWas.end()) {
        fresh = countFirst && was && sid > 0;
    } else {
        fresh = !it->second && was && sid > 0;
    }
    s_winWas[w] = was;
    return fresh;
}

static void merge_drops(const std::vector<int>& fresh, const std::string& freshName,
        const char* verb) {
    if (fresh.empty()) return;
    {
        std::lock_guard<std::mutex> lk(s_mtx);
        for (int d : fresh) {
            bool dup = false;
            for (int e : s_dropList) {
                if (e == d) { dup = true; break; }
            }
            if (!dup) s_dropList.push_back(d);
        }
    }
    char buf[160];
    snprintf(buf, sizeof(buf), "%s drop def %d (%s)", verb, fresh[0],
        freshName.empty() ? "?" : freshName.c_str());
    set_text(buf);
    if (g_logger) g_logger->send(levels::success, "Crates: {}", buf);
}

static void sniff_tick() {
    if (!actors::world_stable(5000)) {
        s_sniffIdx = 0;
        s_sniffTotal = 0;
        return;
    }
    auto* pc = seh_local_rc();
    if (!IS_VALID_PTR(pc)) {
        s_sniffIdx = 0;
        s_sniffTotal = 0;
        return;
    }
    uint64_t now = GetTickCount64();
    if (s_sniffTotal == 0) {
        if (now - s_sniffIdleTs < 15000) return;
        int n = seh_gnum();
        if (n <= 0 || n > 600000) {
            s_sniffIdleTs = now;
            return;
        }
        s_sniffTotal = n;
        s_sniffIdx = 0;
        s_winSeen.clear();
    }
    std::vector<int> fresh;
    std::string freshName;
    int budget = 8000;
    int i = s_sniffIdx;
    for (; i < s_sniffTotal && budget > 0; ++i, --budget) {
        SDK::UObject* o = seh_gobj(i);
        if (!IS_VALID_PTR(o)) continue;
        std::string nm;
        if (!veh::try_call([&] { nm = veh::get_name(o); })) continue;
        if (nm.find("WinUI_C") == std::string::npos) continue;
        auto* w = reinterpret_cast<SDK::UWinUI_C*>(o);
        s_winSeen.insert(w);
        bool was = false;
        veh::try_call([&] { was = w->WasCrateDropped_; });
        int sid = 0;
        std::string cnm;
        if (was) veh::try_call([&] {
            sid = w->DroppedCrate.SteamID_15_E9937E0E4AC4D383F0166DACBAE0A4CB;
            cnm = w->DroppedCrate.Name_4_0FD93890480FD9EE6421B3BE673E7A56.ToString();
        });
        if (note_winui(w, was, sid, false)) {
            fresh.push_back(sid);
            if (freshName.empty() && !cnm.empty()) freshName = cnm;
        }
    }
    s_sniffIdx = i;
    if (i >= s_sniffTotal) {
        s_sniffTotal = 0;
        s_sniffIdleTs = now;
        for (auto it = s_winWas.begin(); it != s_winWas.end();) {
            if (s_winSeen.find(it->first) == s_winSeen.end()) it = s_winWas.erase(it);
            else ++it;
        }
        s_winSeen.clear();
    }
    merge_drops(fresh, freshName, "auto-captured");
}

void tick_steam() {
    sniff_tick();
    size_t n = 0;
    {
        std::lock_guard<std::mutex> lk(s_mtx);
        n = s_pending.size();
    }
    for (size_t i = 0; i < n; ++i) {
        PendingOp op;
        {
            std::lock_guard<std::mutex> lk(s_mtx);
            if (s_pending.empty()) return;
            op = std::move(s_pending.front());
            s_pending.erase(s_pending.begin());
        }
        auto* pc = seh_local_rc();
        int st = seh_status(&op.res);
        uint64_t age = GetTickCount64() - op.t0;
        if (st == 1 && IS_VALID_PTR(pc)) {
            int g = pipe_result(pc, &op.res);
            if (op.onDone) op.onDone(g);
        } else if (st == 1 || st == 22 || st == 0 || st < 0) {
            if (age > 12000) {
                seh_destroy(&op.res);
                if (op.onDone) op.onDone(-1);
            } else {
                std::lock_guard<std::mutex> lk(s_mtx);
                s_pending.push_back(std::move(op));
            }
        } else {
            int code = -st;
            seh_destroy(&op.res);
            if (op.onDone) op.onDone(code);
        }
    }
}

void grant_all() {
    if (s_crates.empty()) scan_crates();
    std::vector<CrateEntry> copy;
    {
        std::lock_guard<std::mutex> lk(s_mtx);
        copy = s_crates;
    }
    if (copy.empty()) {
        set_text("found 0 crate types");
        return;
    }
    auto* pc = seh_local_rc();
    if (!IS_VALID_PTR(pc)) {
        set_text("join a lobby first");
        return;
    }
    {
        SDK::FSteamInventoryResult res{};
        if (seh_grant_promo(&res)) {
            PendingOp op;
            op.res = res;
            op.t0 = GetTickCount64();
            op.tag = "promo";
            std::lock_guard<std::mutex> lk(s_mtx);
            if (s_pending.size() < 24) s_pending.push_back(std::move(op));
            else seh_destroy(&res);
        }
    }
    std::vector<int> defs;
    for (auto& e : copy) {
        if (e.steamId > 0) defs.push_back(e.steamId);
    }
    static const int coinDefs[] = { 1500, 1501, 3150, 3151, 8500, 8501, 20000, 20001 };
    for (int c : coinDefs) defs.push_back(c);
    set_text("grant pending...");
    grant_async(defs, "crates", [defs](int got) {
        std::vector<SteamItem> items;
        snapshot(items);
        int owned = 0;
        for (int d : defs) {
            unsigned long long inst = 0;
            if (find_instance(items, d, inst)) ++owned;
        }
        char buf[160];
        snprintf(buf, sizeof(buf), "grant: %d types, owned %d (got %d)",
            (int)defs.size(), owned, got);
        set_text(buf);
        if (g_logger) g_logger->send(levels::success, "Crates: {}", buf);
    });
}

void sniff_drop() {
    auto* pc = seh_local_rc();
    if (!IS_VALID_PTR(pc)) {
        set_text("join a lobby first");
        return;
    }
    int n = seh_gnum();
    if (n <= 0 || n > 600000) {
        set_text("bad gobjects");
        return;
    }
    set_text("sniffing winui...");
    int found = 0;
    std::vector<int> fresh;
    std::string freshName;
    std::unordered_set<void*> seen;
    for (int i = 0; i < n; ++i) {
        SDK::UObject* o = seh_gobj(i);
        if (!IS_VALID_PTR(o)) continue;
        std::string nm;
        if (!veh::try_call([&] { nm = veh::get_name(o); })) continue;
        if (nm.find("WinUI_C") == std::string::npos) continue;
        ++found;
        auto* w = reinterpret_cast<SDK::UWinUI_C*>(o);
        seen.insert(w);
        bool was = false;
        int sid = 0;
        std::string cnm;
        veh::try_call([&] { was = w->WasCrateDropped_; });
        if (was) veh::try_call([&] {
            sid = w->DroppedCrate.SteamID_15_E9937E0E4AC4D383F0166DACBAE0A4CB;
            cnm = w->DroppedCrate.Name_4_0FD93890480FD9EE6421B3BE673E7A56.ToString();
        });
        if (note_winui(w, was, sid, true)) {
            fresh.push_back(sid);
            if (freshName.empty() && !cnm.empty()) freshName = cnm;
        }
    }
    for (auto it = s_winWas.begin(); it != s_winWas.end();) {
        if (seen.find(it->first) == seen.end()) it = s_winWas.erase(it);
        else ++it;
    }
    merge_drops(fresh, freshName, "captured");
    char buf[160];
    if (found == 0)
        snprintf(buf, sizeof(buf), "no winui alive - finish a mission first");
    else if (fresh.empty())
        snprintf(buf, sizeof(buf), "winui x%d, no new drop", found);
    else
        snprintf(buf, sizeof(buf), "winui x%d, drop def %d (%s)",
            found, fresh[0],
            freshName.empty() ? "?" : freshName.c_str());
    if (fresh.empty() || found == 0) {
        set_text(buf);
        if (g_logger) g_logger->send(levels::success, "Crates: {}", buf);
    }
}

void open_for_skin(int skinRow) {
    if (skinRow < 0) {
        set_text("pick a skin first");
        return;
    }
    int cat = 0;
    std::string skinName;
    int skinDef = skin_def_of(skinRow, cat, skinName);
    if (skinDef <= 0) {
        char buf[160];
        snprintf(buf, sizeof(buf), "skin %d has no steam def", skinRow);
        set_text(buf);
        return;
    }
    if (s_crates.empty()) scan_crates();
    std::vector<CrateEntry> copy;
    {
        std::lock_guard<std::mutex> lk(s_mtx);
        copy = s_crates;
    }
    const CrateEntry* crate = nullptr;
    for (auto& e : copy) {
        for (int sid : e.skinIds) {
            if (sid == skinRow) { crate = &e; break; }
        }
        if (crate) break;
    }
    if (!crate || crate->steamId <= 0) {
        char buf[160];
        snprintf(buf, sizeof(buf), "skin %d is in no crate (coin shop only?)", skinRow);
        set_text(buf);
        return;
    }
    auto* pc = seh_local_rc();
    if (!IS_VALID_PTR(pc)) {
        set_text("join a lobby first");
        return;
    }
    std::vector<SteamItem> items;
    snapshot(items);
    unsigned long long inst = 0;
    std::string crateName = crate->name;
    auto do_exchange = [skinRow, skinDef, cat, crateName, skinName](unsigned long long ii) {
        set_text("exchange pending...");
        exchange_async(skinDef, ii, "open", [skinRow, skinDef, cat, crateName, skinName](int got) {
            if (got < 0) {
                char buf[160];
                snprintf(buf, sizeof(buf), "exchange failed (%d)", got);
                set_text(buf);
                if (g_logger) g_logger->send(levels::error, "Crates: exchange failed {}", got);
                return;
            }
            int eq = skins::equip_row(skinRow);
            int verified = count_unlocked_def(cat, skinDef);
            char buf[160];
            snprintf(buf, sizeof(buf), "opened %s: %s (steam %d, eq %d, owned %d)",
                crateName.c_str(), skinName.empty() ? "skin" : skinName.c_str(),
                got, eq, verified);
            set_text(buf);
            if (g_logger) g_logger->send(levels::success, "Crates: {}", buf);
        });
    };
    if (!find_instance(items, crate->steamId, inst)) {
        set_text("granting crate...");
        std::vector<int> one{ crate->steamId };
        grant_async(one, "crate", [one, do_exchange](int got) {
            (void)got;
            std::vector<SteamItem> it2;
            snapshot(it2);
            unsigned long long ii = 0;
            if (!find_instance(it2, one[0], ii)) {
                set_text("crate grant gave 0");
                return;
            }
            do_exchange(ii);
        });
        return;
    }
    do_exchange(inst);
}

void open_all() {
    if (s_crates.empty()) scan_crates();
    std::vector<CrateEntry> copy;
    {
        std::lock_guard<std::mutex> lk(s_mtx);
        copy = s_crates;
    }
    if (copy.empty()) {
        set_text("found 0 crate types");
        return;
    }
    auto* pc = seh_local_rc();
    if (!IS_VALID_PTR(pc)) {
        set_text("join a lobby first");
        return;
    }
    {
        SDK::FSteamInventoryResult res{};
        if (seh_grant_promo(&res)) pipe_result(pc, &res);
        else seh_destroy(&res);
    }
    std::vector<int> missing;
    std::vector<SteamItem> items;
    snapshot(items);
    for (auto& e : copy) {
        if (e.steamId <= 0 || e.skinIds.empty()) continue;
        unsigned long long inst = 0;
        if (!find_instance(items, e.steamId, inst)) missing.push_back(e.steamId);
    }
    set_text("granting missing crates...");
    grant_async(missing, "crates", [copy](int got) {
        (void)got;
        std::vector<SteamItem> items;
        snapshot(items);
        struct Tally { int opened = 0; int refused = 0; int equipped = 0; int started = 0; int done = 0; };
        auto tally = std::make_shared<Tally>();
        for (auto& e : copy) {
            if (e.steamId <= 0 || e.skinIds.empty()) continue;
            unsigned long long inst = 0;
            if (!find_instance(items, e.steamId, inst)) { ++tally->refused; continue; }
            int cat = 0;
            std::string nm;
            int def = skin_def_of(e.skinIds[0], cat, nm);
            if (def <= 0) { ++tally->refused; continue; }
            int row = e.skinIds[0];
            ++tally->started;
            exchange_async(def, inst, "open", [tally, row](int r) {
                if (r < 0) ++tally->refused;
                else {
                    ++tally->opened;
                    tally->equipped += skins::equip_row(row);
                }
                if (++tally->done == tally->started) {
                    char buf[160];
                    snprintf(buf, sizeof(buf), "opened %d (%d refused, %d eq)",
                        tally->opened, tally->refused, tally->equipped);
                    set_text(buf);
                    if (g_logger) g_logger->send(levels::success, "Crates: {}", buf);
                }
            });
        }
        if (tally->started == 0) {
            char buf[160];
            snprintf(buf, sizeof(buf), "nothing to open (%d refused)", tally->refused);
            set_text(buf);
        }
    });
}

}

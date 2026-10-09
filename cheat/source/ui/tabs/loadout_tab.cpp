#include "pch/pch.h"
#include "ui/tabs.hpp"
#include "ui/widgets.hpp"
#include "ui/menu.hpp"
#include "config/config.hpp"
#include "world/actor_iter.hpp"
#include "util/veh.hpp"
#include "features/manager.hpp"
#include "features/skins.hpp"
#include "features/crates.hpp"
#include "features/shop.hpp"
#include "logger/logger.hpp"

#include "SDK/Basic.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/GunBase_classes.hpp"
#include "SDK/RobberController_classes.hpp"
#include "SDK/EquippedWeaponSlot_classes.hpp"

namespace {

static SDK::ARobberController_C* seh_rc() {
    __try {
        auto* pc = reinterpret_cast<SDK::ARobberController_C*>(actors::local_player_controller());
        if (!IS_VALID_PTR(pc)) return nullptr;
        SDK::UClass* rc = SDK::ARobberController_C::StaticClass();
        if (!IS_VALID_PTR(rc)) return pc;
        return pc->IsA(rc) ? pc : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

struct PtrSnap { const void* arr; int n; bool ok; };

static PtrSnap seh_guns(SDK::ARobberController_C* pc) {
    __try {
        int n = pc->Weapons_Equipped.Num();
        if (n < 0 || n > 64) return PtrSnap{ nullptr, 0, false };
        if (n == 0) return PtrSnap{ nullptr, 0, true };
        auto* d = pc->Weapons_Equipped.GetDataPtr();
        if (!d) return PtrSnap{ nullptr, 0, false };
        return PtrSnap{ reinterpret_cast<const void*>(d), n, true };
    } __except (EXCEPTION_EXECUTE_HANDLER) { return PtrSnap{ nullptr, 0, false }; }
}

static PtrSnap seh_mods(SDK::ARobberController_C* pc) {
    __try {
        int n = pc->WeaponMods_Equipped.Num();
        if (n < 0 || n > 64) return PtrSnap{ nullptr, 0, false };
        if (n == 0) return PtrSnap{ nullptr, 0, true };
        auto* d = pc->WeaponMods_Equipped.GetDataPtr();
        if (!d) return PtrSnap{ nullptr, 0, false };
        return PtrSnap{ reinterpret_cast<const void*>(d), n, true };
    } __except (EXCEPTION_EXECUTE_HANDLER) { return PtrSnap{ nullptr, 0, false }; }
}

static PtrSnap seh_eskins(SDK::ARobberController_C* pc) {
    __try {
        int n = pc->EquippedWeaponSkins.Num();
        if (n < 0 || n > 256) return PtrSnap{ nullptr, 0, false };
        if (n == 0) return PtrSnap{ nullptr, 0, true };
        auto* d = pc->EquippedWeaponSkins.GetDataPtr();
        if (!d) return PtrSnap{ nullptr, 0, false };
        return PtrSnap{ reinterpret_cast<const void*>(d), n, true };
    } __except (EXCEPTION_EXECUTE_HANDLER) { return PtrSnap{ nullptr, 0, false }; }
}

static int seh_loadout(SDK::ARobberController_C* pc, bool& ok) {
    __try { ok = true; return pc->Loadout_Equipped; }
    __except (EXCEPTION_EXECUTE_HANDLER) { ok = false; return 0; }
}

static bool seh_set_weapons(SDK::ARobberController_C* pc,
        SDK::TArray<SDK::UClass*>* w, SDK::TArray<SDK::FAttachmentArrayStructure>* m,
        SDK::TArray<SDK::FEquippedSkinStruct>* s, int loadout) {
    __try {
        if (!IS_VALID_PTR(pc)) return false;
        pc->SetWeaponsServer(*w, *m, *s, loadout);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool seh_save_weapons(SDK::ARobberController_C* pc) {
    __try { pc->SaveWeapons(); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool seh_load_weapons(SDK::ARobberController_C* pc) {
    __try { pc->LoadWeapons(); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool seh_gun_ok(SDK::UClass* gun, SDK::UClass* uclass) {
    __try {
        if (!IS_VALID_PTR(gun)) return false;
        if (IS_VALID_PTR(uclass) && !gun->IsA(uclass)) return false;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static int seh_gnum() {
    __try { return SDK::UObject::GObjects->Num(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static SDK::UObject* seh_gat(int i) {
    __try { return SDK::UObject::GObjects->GetByIndex(i); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
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

static std::string short_gun(const std::string& n) {
    std::string d = n;
    if (d.size() > 2 && d.compare(d.size() - 2, 2, "_C") == 0)
        d = d.substr(0, d.size() - 2);
    for (const char* pre : { "Default__", "ShopItem_", "BP_" }) {
        size_t L = strlen(pre);
        if (d.rfind(pre, 0) == 0) d = d.substr(L);
    }
    if (d.rfind("Weapon_", 0) == 0 && d.size() > 7) d = d.substr(7);
    while (!d.empty() && d.front() == '_') d.erase(d.begin());
    if (d.empty()) d = n;
    return d;
}

struct GunEntry { std::string name; void* cls = nullptr; };
struct WepEntry { std::string name; void* cls = nullptr; };

static std::vector<GunEntry> s_allGuns;
static std::vector<GunEntry> s_scanAcc;
static int s_scanIdx = 0;
static int s_scanTotal = 0;
static bool s_scanDone = false;

static SDK::UClass* seh_uclass_static() {
    __try { return SDK::UClass::StaticClass(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

static SDK::UClass* seh_gunbase_static() {
    __try { return SDK::AGunBase_C::StaticClass(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

static bool seh_is_a(SDK::UObject* o, SDK::UClass* cls) {
    __try { return o->IsA(cls); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool seh_is_sub(SDK::UClass* cls, SDK::UClass* base) {
    __try {
        for (SDK::UStruct* s = cls; s; s = s->Super) {
            if (s == base) return true;
        }
        return false;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static SDK::UClass* seh_slotwidget_static() {
    __try { return SDK::UEquippedWeaponSlot_C::StaticClass(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

static bool seh_widget_set(SDK::UEquippedWeaponSlot_C* w, SDK::UClass* cls) {
    __try { w->SetNewItem(cls); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool seh_widget_update(SDK::UEquippedWeaponSlot_C* w) {
    __try { w->UpdateItem(); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool seh_override(SDK::ARobberController_C* pc, SDK::EShopItemCategory cat, int idx, SDK::UClass* cls) {
    __try { pc->OverrideIndexFromCategory(cat, idx, cls); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool seh_update_new_weapon(SDK::ARobberController_C* pc, int idx) {
    __try { pc->UpdateNewWeapon(idx); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool seh_switch_loadout(SDK::ARobberController_C* pc, int amt) {
    __try { pc->SwitchLoadout(amt); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static std::string strip_item(const std::string& n) {
    std::string d = n;
    auto hh = d.find("##");
    if (hh != std::string::npos) d.erase(hh);
    const char* pre = "ShopItem_";
    if (d.rfind(pre, 0) == 0) d = d.substr(9);
    const char* pre2 = "Shop_";
    if (d.rfind(pre2, 0) == 0) d = d.substr(5);
    if (d.size() > 2 && d.compare(d.size() - 2, 2, "_C") == 0)
        d = d.substr(0, d.size() - 2);
    if (d.empty()) d = n;
    return d;
}

static int s_armCat = 0;
static int s_armSel = -1;
static char s_armStatus[128] = "pick a category";

static int s_widgetCount = 0;

static SDK::UEquippedWeaponSlot_C* find_slot_widget(int slot, SDK::UClass* oldCls) {
    SDK::UClass* wc = seh_slotwidget_static();
    if (!IS_VALID_PTR(wc)) return nullptr;
    int n = seh_gnum();
    if (n <= 0 || n > 600000) return nullptr;
    s_widgetCount = 0;
    for (int i = 0; i < n; ++i) {
        SDK::UObject* o = seh_gat(i);
        if (!IS_VALID_PTR(o)) continue;
        if (!seh_is_a(o, wc)) continue;
        ++s_widgetCount;
        auto* w = static_cast<SDK::UEquippedWeaponSlot_C*>(o);
        bool hit = false;
        veh::try_call([&] {
            if (w->IndexReference == slot && w->Item == oldCls) hit = true;
        });
        if (hit) return w;
    }
    return nullptr;
}

static void rescan_guns() {
    s_scanDone = false;
    s_scanIdx = 0;
    s_scanTotal = 0;
    s_scanAcc.clear();
    s_allGuns.clear();
}

static void refresh_gun_classes() {
    if (s_scanDone) return;
    if (!actors::world_stable(5000)) return;
    int n = seh_gnum();
    if (n <= 0 || n > 600000) return;
    if (s_scanTotal != n) {
        s_scanTotal = n;
        s_scanIdx = 0;
        s_scanAcc.clear();
    }
    SDK::UClass* uclass = seh_uclass_static();
    SDK::UClass* base = seh_gunbase_static();
    int budget = 12000;
    int i = s_scanIdx;
    for (; i < n && budget > 0 && (int)s_scanAcc.size() < 200; ++i, --budget) {
        SDK::UObject* o = seh_gat(i);
        if (!IS_VALID_PTR(o)) continue;
        if (IS_VALID_PTR(uclass) && !seh_is_a(o, uclass)) continue;
        GunEntry e;
        bool ok = veh::try_call([&] {
            std::string nm = veh::get_name(o);
            if (nm.empty() || nm == "GunBase_C" || nm == "AGunBase_C") return;
            e.name = short_gun(nm);
            e.cls = static_cast<SDK::UClass*>(o);
        });
        if (!ok || e.name.empty() || !IS_VALID_PTR(e.cls)) continue;
        if (IS_VALID_PTR(base) && !seh_is_sub(static_cast<SDK::UClass*>(e.cls), base)) continue;
        s_scanAcc.push_back(std::move(e));
    }
    s_scanIdx = i;
    if (i >= n || (int)s_scanAcc.size() >= 200) {
        s_allGuns = s_scanAcc;
        s_scanDone = true;
    }
}

static std::string norm_tok(const std::string& s) {
    std::string o;
    for (char ch : s) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) o += ch;
        else if (ch >= 'A' && ch <= 'Z') o += (char)(ch + 32);
    }
    return o;
}

static int selWep = 0;
static int availSel = 0;
static int skinSel = 0;
static int charmSel = 0;
static std::vector<WepEntry> s_weps;
static std::vector<skins::OwnedSkin> s_owned;
static std::vector<skins::CharmEntry> s_charms;
static uint64_t s_dataTs = 0;
static char s_wstatus[128] = "idle";
static bool s_verifyActive = false;
static uint64_t s_verifyT0 = 0;
static uint64_t s_verifyNext = 0;
static int s_verifyTries = 0;
static int s_verifyHits = 0;
static int s_phase = 0;
static std::atomic<int> s_rpcDone{ 0 };
static std::atomic<int> s_rbDone{ 0 };
static bool s_viaWidget = false;
static int s_verifySlot = -1;
static void* s_verifyCls = nullptr;
static char s_verifyName[64] = { 0 };
static SDK::UClass* s_origCls = nullptr;
static int s_origLoadout = 0;
static char s_sstatus[128] = "idle";
static char s_cstatus[128] = "idle";
static std::string s_curSkin = "not scanned";
static std::string s_curCharm = "not scanned";

static void refresh_data(bool force) {
    uint64_t now = GetTickCount64();
    if (!force && !s_weps.empty() && now - s_dataTs < 5000) return;
    s_dataTs = now;
    s_weps.clear();
    auto* pc = seh_rc();
    if (!IS_VALID_PTR(pc)) return;
    PtrSnap g = seh_guns(pc);
    if (!g.ok) return;
    std::vector<void*> buf((size_t)g.n);
    bool cok = veh::try_call([&] {
        auto* d = reinterpret_cast<SDK::UClass* const*>(g.arr);
        for (int i = 0; i < g.n; ++i) buf[(size_t)i] = d[i];
    });
    if (!cok) return;
    for (int i = 0; i < g.n; ++i) {
        WepEntry e;
        if (IS_VALID_PTR(buf[(size_t)i])) {
            veh::try_call([&] {
                e.name = short_gun(veh::get_name(static_cast<SDK::UObject*>(buf[(size_t)i])));
            });
        }
        if (e.name.empty()) { char b[32]; snprintf(b, sizeof(b), "slot %d", i); e.name = b; }
        e.cls = buf[(size_t)i];
        s_weps.push_back(std::move(e));
    }
    if (selWep < 0 || selWep >= (int)s_weps.size()) selWep = 0;
    s_owned = skins::unlocked_skins();
    if (skinSel < 0 || skinSel >= (int)s_owned.size()) skinSel = 0;
    s_charms = skins::charm_shop_items();
    {
        std::vector<skins::CharmEntry> owned;
        std::vector<crates::SteamItem> items;
        if (crates::snapshot(items)) {
            for (auto& c : s_charms) {
                for (auto& s : items) {
                    if (s.def == c.def && s.qty > 0) { owned.push_back(c); break; }
                }
            }
        }
        if (!owned.empty()) s_charms.swap(owned);
    }
    if (charmSel < 0 || charmSel >= (int)s_charms.size()) charmSel = 0;
}

static void scan_current() {
    auto* pc = seh_rc();
    if (!IS_VALID_PTR(pc)) {
        s_curSkin = "join a lobby first";
        s_curCharm = "join a lobby first";
        return;
    }
    if (selWep < 0 || selWep >= (int)s_weps.size() || !IS_VALID_PTR(s_weps[selWep].cls)) {
        s_curSkin = "pick a weapon first";
        s_curCharm = "pick a weapon first";
        return;
    }
    void* gun = s_weps[selWep].cls;
    PtrSnap e = seh_eskins(pc);
    bool hit = false;
    if (e.ok && e.n > 0) {
        std::vector<SDK::FEquippedSkinStruct> buf((size_t)e.n);
        veh::try_call([&] {
            auto* d = reinterpret_cast<SDK::FEquippedSkinStruct const*>(e.arr);
            memcpy(buf.data(), d, sizeof(buf[0]) * (size_t)e.n);
        });
        for (int i = 0; i < e.n; ++i) {
            SDK::UClass* w = buf[(size_t)i].Weapon_6_9CBF531647150BA4D5CE9997A2C4AF18;
            int row = buf[(size_t)i].SkinID_5_DA96374A47E0224BD1785591CB5E713D;
            if (!IS_VALID_PTR(w) || w != gun) continue;
            std::string nm, ch;
            int cat = 0, def = 0;
            if (skins::row_info(row, nm, cat, ch, def)) {
                char b[128];
                snprintf(b, sizeof(b), "%.80s (cat %d)", nm.empty() ? "?" : nm.c_str(), cat);
                s_curSkin = b;
                s_curCharm = ch.empty() ? "(no charm on skin)" : ch;
            } else {
                char b[64];
                snprintf(b, sizeof(b), "row %d (not in table)", row);
                s_curSkin = b;
                s_curCharm = "?";
            }
            hit = true;
            break;
        }
    }
    if (!hit) {
        s_curSkin = "no skin equipped";
        s_curCharm = "no skin equipped";
    }
}

static void request_replace() {
    feat::queue_keyed("replace_weapon", [] {
        if (s_verifyActive) {
            snprintf(s_wstatus, sizeof(s_wstatus), "verify in progress...");
            return;
        }
        if (!actors::world_stable(5000)) {
            snprintf(s_wstatus, sizeof(s_wstatus), "world busy - try again");
            return;
        }
        auto* p = seh_rc();
        if (!IS_VALID_PTR(p)) {
            snprintf(s_wstatus, sizeof(s_wstatus), "no controller");
            return;
        }
        if (selWep < 0 || selWep >= (int)s_weps.size() ||
                availSel < 0 || availSel >= (int)s_allGuns.size() ||
                !IS_VALID_PTR(s_allGuns[availSel].cls)) {
            snprintf(s_wstatus, sizeof(s_wstatus), "pick both slots");
            return;
        }
        PtrSnap g = seh_guns(p);
        PtrSnap m = seh_mods(p);
        PtrSnap s = seh_eskins(p);
        bool lok = false;
        int loadout = seh_loadout(p, lok);
        if (!g.ok || !m.ok || !s.ok || g.n <= 0) {
            snprintf(s_wstatus, sizeof(s_wstatus), "read failed - abort");
            return;
        }
        if (!lok || loadout < 0 || loadout > 9) {
            snprintf(s_wstatus, sizeof(s_wstatus), "bad loadout idx %d",
                lok ? loadout : -999);
            return;
        }
        if (selWep >= g.n) {
            snprintf(s_wstatus, sizeof(s_wstatus), "stale list - wait refresh");
            return;
        }
        SDK::UClass* newCls = static_cast<SDK::UClass*>(s_allGuns[availSel].cls);
        SDK::UClass* cur0 = nullptr;
        veh::try_call([&] {
            auto* d0 = reinterpret_cast<SDK::UClass* const*>(g.arr);
            if (selWep >= 0 && selWep < g.n) cur0 = d0[selWep];
        });
        s_origCls = cur0;
        if (!feat::ensure_gamehook(p)) {
            snprintf(s_wstatus, sizeof(s_wstatus), "pe hook unavailable");
            return;
        }
        int slot = selWep, lo = loadout;
        s_verifySlot = slot;
        s_verifyCls = newCls;
        s_origLoadout = lo;
        snprintf(s_verifyName, sizeof(s_verifyName), "%s",
            s_allGuns[availSel].name.c_str());
        s_rpcDone.store(0);
        s_phase = 0;
        feat::queue_game([slot, lo, newCls] {
            int res = -1;
            bool rok = veh::try_call([&] {
                auto* p2 = seh_rc();
                if (!IS_VALID_PTR(p2)) return;
                SDK::UClass* uclass = seh_uclass_static();
                int n2 = p2->Weapons_Equipped.Num();
                auto* d2 = const_cast<SDK::UClass**>(p2->Weapons_Equipped.GetDataPtr());
                if (!d2 || n2 <= 0 || n2 > 64 || slot < 0 || slot >= n2) return;
                for (int i = 0; i < n2; ++i) {
                    if (!seh_gun_ok(d2[i], uclass)) return;
                }
                int m2 = p2->WeaponMods_Equipped.Num();
                auto* e2 = const_cast<SDK::FAttachmentArrayStructure*>(
                    p2->WeaponMods_Equipped.GetDataPtr());
                if (m2 < 0 || m2 > 64) return;
                if (m2 > 0 && !e2) return;
                int k2 = p2->EquippedWeaponSkins.Num();
                auto* f2 = const_cast<SDK::FEquippedSkinStruct*>(
                    p2->EquippedWeaponSkins.GetDataPtr());
                if (k2 < 0 || k2 > 256) return;
                if (k2 > 0 && !f2) return;
                s_origCls = d2[slot];
                if (g_logger) g_logger->send(levels::debug, "Loadout: widget scan start");
                SDK::UEquippedWeaponSlot_C* w = find_slot_widget(slot, d2[slot]);
                if (g_logger) g_logger->send(levels::debug, "Loadout: scan done, {} slot widgets",
                    s_widgetCount);
                if (IS_VALID_PTR(w)) {
                    if (g_logger) g_logger->send(levels::debug, "Loadout: slot matched, override");
                    SDK::EShopItemCategory wcat = (SDK::EShopItemCategory)0;
                    veh::try_call([&] { wcat = w->Category; });
                    if ((uint8_t)wcat < 16) seh_override(p2, wcat, slot, newCls);
                    if (g_logger) g_logger->send(levels::debug, "Loadout: override done, SetNewItem");
                    if (!seh_widget_set(w, newCls)) return;
                    if (g_logger) g_logger->send(levels::debug, "Loadout: SetNewItem ok, UpdateItem");
                    seh_widget_update(w);
                    if (g_logger) g_logger->send(levels::debug, "Loadout: UpdateItem ok, UpdateNewWeapon");
                    if (!seh_update_new_weapon(p2, slot)) return;
                    if (g_logger) g_logger->send(levels::debug, "Loadout: UpdateNewWeapon done");
                    s_viaWidget = true;
                    res = 1;
                    return;
                }
                if (g_logger) g_logger->send(levels::debug, "Loadout: no widget, rpc fallback");
                d2[slot] = newCls;
                if (d2[slot] != newCls) return;
                SDK::TArray<SDK::UClass*> aw = wrap_buf(d2, n2);
                SDK::TArray<SDK::FAttachmentArrayStructure> am = wrap_buf(e2, m2);
                SDK::TArray<SDK::FEquippedSkinStruct> as = wrap_buf(f2, k2);
                bool lok2 = false;
                int lo2 = seh_loadout(p2, lok2);
                if (!lok2 || lo2 < 0 || lo2 > 9) return;
                if (!seh_set_weapons(p2, &aw, &am, &as, lo2)) return;
                seh_save_weapons(p2);
                res = 1;
            });
            if (!rok) res = -1;
            s_rpcDone.store(res);
        });
        s_verifyActive = true;
        s_verifyT0 = GetTickCount64();
        s_verifyNext = s_verifyT0 + 600;
        s_verifyTries = 0;
        s_verifyHits = 0;
        snprintf(s_wstatus, sizeof(s_wstatus), "sent %.50s, verifying...",
            s_verifyName);
        s_dataTs = 0;
    });
}

void draw_loadout() {
    widgets::header_card("Loadout", "guns, skins and charms");
    auto* pc = seh_rc();
    if (!IS_VALID_PTR(pc)) {
        ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.30f, 1.0f), "No local controller - join a lobby first.");
        return;
    }
    refresh_gun_classes();
    refresh_data(false);
    if (s_verifyActive) {
        uint64_t now = GetTickCount64();
        if (now >= s_verifyNext) {
            int rpc = (s_phase == 0) ? s_rpcDone.load() : s_rbDone.load();
            void* want = (s_phase == 0) ? s_verifyCls : (void*)s_origCls;
            if (rpc < 0) {
                snprintf(s_wstatus, sizeof(s_wstatus), s_phase == 0 ?
                    "set refused" : "restore FAILED");
                s_verifyActive = false;
                s_dataTs = 0;
            } else if (rpc == 0) {
                s_verifyNext = now + 900;
                snprintf(s_wstatus, sizeof(s_wstatus), "waiting game thread...");
            } else {
                bool done = false;
                auto* vp = seh_rc();
                if (IS_VALID_PTR(vp)) {
                    PtrSnap vg = seh_guns(vp);
                    if (vg.ok && s_verifySlot >= 0 && s_verifySlot < vg.n) {
                        SDK::UClass* cur = nullptr;
                        veh::try_call([&] {
                            cur = reinterpret_cast<SDK::UClass* const*>(vg.arr)[s_verifySlot];
                        });
                        if (cur == want && IS_VALID_PTR(cur)) {
                            if (++s_verifyHits >= 2) {
                                if (s_phase == 0) {
                                    snprintf(s_wstatus, sizeof(s_wstatus), "slot %d is now %.40s (%s)",
                                        s_verifySlot, s_verifyName, s_viaWidget ? "saved, widget" : "saved, rpc");
                                } else {
                                    snprintf(s_wstatus, sizeof(s_wstatus), "server refused %.40s, restored",
                                        s_verifyName);
                                }
                                s_verifyActive = false;
                                done = true;
                                s_dataTs = 0;
                            } else {
                                s_verifyNext = now + 900;
                                snprintf(s_wstatus, sizeof(s_wstatus), "confirming %.50s...",
                                    s_verifyName);
                            }
                        } else {
                            s_verifyHits = 0;
                        }
                    }
                }
                if (!done) {
                    if (++s_verifyTries >= 4) {
                        if (s_phase == 0 && IS_VALID_PTR(s_origCls)) {
                            int rslot = s_verifySlot;
                            SDK::UClass* rcls = s_origCls;
                            int rlo = s_origLoadout;
                            s_rbDone.store(0);
                            feat::queue_game([rslot, rcls, rlo] {
                                int res = -1;
                                bool rok = veh::try_call([&] {
                                    auto* rp = seh_rc();
                                    if (!IS_VALID_PTR(rp)) return;
                                    SDK::UClass* uclass2 = seh_uclass_static();
                                    int n2 = rp->Weapons_Equipped.Num();
                                    auto* d2 = const_cast<SDK::UClass**>(rp->Weapons_Equipped.GetDataPtr());
                                    if (!d2 || n2 <= 0 || n2 > 64) return;
                                    if (rslot < 0 || rslot >= n2) return;
                                    for (int i = 0; i < n2; ++i) {
                                        if (i != rslot && !seh_gun_ok(d2[i], uclass2)) return;
                                    }
                                    int m2 = rp->WeaponMods_Equipped.Num();
                                    auto* e2 = const_cast<SDK::FAttachmentArrayStructure*>(
                                        rp->WeaponMods_Equipped.GetDataPtr());
                                    if (m2 < 0 || m2 > 64) return;
                                    if (m2 > 0 && !e2) return;
                                    int k2 = rp->EquippedWeaponSkins.Num();
                                    auto* f2 = const_cast<SDK::FEquippedSkinStruct*>(
                                        rp->EquippedWeaponSkins.GetDataPtr());
                                    if (k2 < 0 || k2 > 256) return;
                                    if (k2 > 0 && !f2) return;
                                    d2[rslot] = rcls;
                                    if (d2[rslot] != rcls) return;
                                    SDK::TArray<SDK::UClass*> aw = wrap_buf(d2, n2);
                                    SDK::TArray<SDK::FAttachmentArrayStructure> am = wrap_buf(e2, m2);
                                    SDK::TArray<SDK::FEquippedSkinStruct> as = wrap_buf(f2, k2);
                                    if (!seh_set_weapons(rp, &aw, &am, &as, rlo)) return;
                                    seh_save_weapons(rp);
                                    res = 1;
                                });
                                if (!rok) res = -1;
                                s_rbDone.store(res);
                            });
                            s_phase = 1;
                            s_verifyTries = 0;
                            s_verifyHits = 0;
                            s_verifyNext = now + 900;
                            snprintf(s_wstatus, sizeof(s_wstatus), "verifying %.50s (%d)...",
                                s_verifyName, s_verifyTries);
                        } else {
                            if (s_phase == 0 && !IS_VALID_PTR(s_origCls)) {
                                snprintf(s_wstatus, sizeof(s_wstatus), "server refused %.40s",
                                    s_verifyName);
                            } else {
                                snprintf(s_wstatus, sizeof(s_wstatus), "server refused %.40s, restore FAILED",
                                    s_verifyName);
                            }
                            s_verifyActive = false;
                            s_dataTs = 0;
                        }
                    } else {
                        s_verifyNext = now + 900;
                        snprintf(s_wstatus, sizeof(s_wstatus), "verifying %.50s (%d)...",
                            s_verifyName, s_verifyTries);
                    }
                }
            }
        }
    }

    widgets::os_section("Weapons", ImVec2(800, 400));
    {
        ImGui::SetWindowFontScale(dpi_scale);
        {
            PtrSnap dg = seh_guns(pc);
            PtrSnap dm = seh_mods(pc);
            PtrSnap de = seh_eskins(pc);
            char eb[32], mb[32], kb[32];
            if (dg.ok) snprintf(eb, sizeof(eb), "%d", dg.n);
            else snprintf(eb, sizeof(eb), "err");
            if (dm.ok) snprintf(mb, sizeof(mb), "%d", dm.n);
            else snprintf(mb, sizeof(mb), "err");
            if (de.ok) snprintf(kb, sizeof(kb), "%d", de.n);
            else snprintf(kb, sizeof(kb), "err");
            char d[160];
            snprintf(d, sizeof(d), "pc ok | equipped %s | mods %s | skins %s | classes %d",
                eb, mb, kb, (int)s_allGuns.size());
            ImGui::TextDisabled("%s", d);
        }
        if (!s_scanDone) {
            char p[128];
            snprintf(p, sizeof(p), "scanning classes %d/%d found %d",
                s_scanIdx, s_scanTotal, (int)s_scanAcc.size());
            ImGui::TextDisabled("%s", p);
        }
        {
            bool lokS = false;
            int loS = seh_loadout(pc, lokS);
            if (ImGui::Button("<", ImVec2(40, 28))) {
                feat::queue_game([] {
                    auto* p2 = seh_rc();
                    if (IS_VALID_PTR(p2)) seh_switch_loadout(p2, -1);
                });
            }
            ImGui::SameLine(0, 8);
            char lb[64];
            if (lokS) snprintf(lb, sizeof(lb), "Loadout %d", loS + 1);
            else snprintf(lb, sizeof(lb), "Loadout ?");
            ImGui::Text("%s", lb);
            ImGui::SameLine(0, 8);
            if (ImGui::Button(">", ImVec2(40, 28))) {
                feat::queue_game([] {
                    auto* p2 = seh_rc();
                    if (IS_VALID_PTR(p2)) seh_switch_loadout(p2, 1);
                });
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("switch active loadout");
        }
        if (!s_weps.empty()) {
            if (selWep < 0 || selWep >= (int)s_weps.size()) selWep = 0;
            float sw = ImGui::GetContentRegionAvail().x;
            float bw = (sw - 8.0f * (float)((int)s_weps.size() - 1)) / (float)(int)s_weps.size();
            if (bw < 60.0f) bw = 60.0f;
            for (int i = 0; i < (int)s_weps.size(); ++i) {
                if (i > 0) ImGui::SameLine(0, 8);
                char b[96];
                snprintf(b, sizeof(b), "%s%d: %s", selWep == i ? "> " : "",
                    i, s_weps[i].name.c_str());
                if (ImGui::Button(b, ImVec2(bw, 28))) selWep = i;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("pick target slot");
        } else {
            ImGui::TextDisabled("no weapons read - refresh in a lobby");
        }
        if (availSel >= 0 && availSel < (int)s_allGuns.size() &&
                selWep >= 0 && selWep < (int)s_weps.size())
            ImGui::TextDisabled("replace slot %d (%s) with %s", selWep,
                s_weps[selWep].name.c_str(), s_allGuns[availSel].name.c_str());
        else
            ImGui::TextDisabled("pick a gun in Armory below");
        if (ImGui::Button("REPLACE WEAPON", ImVec2(-1, 32))) {
            request_replace();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("swap one gun, verify, rollback on refuse");
        if (ImGui::Button("SCAN GUN CLASSES", ImVec2(-1, 28)))
            rescan_guns();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("re-enumerate gun classes");
        ImGui::TextDisabled("%s", s_wstatus);
    }
    widgets::os_section_end();

    ImGui::SetCursorPos(ImVec2(0, 530));
    widgets::os_section("Armory", ImVec2(800, 470));
    {
        ImGui::SetWindowFontScale(dpi_scale);
        if (widgets::os_subtab("Weapons", s_armCat == 0, 185.0f)) { s_armCat = 0; s_armSel = -1; }
        ImGui::SameLine(0, 8);
        if (widgets::os_subtab("Tools", s_armCat == 1, 185.0f)) { s_armCat = 1; s_armSel = -1; }
        ImGui::SameLine(0, 8);
        if (widgets::os_subtab("Appearance", s_armCat == 2, 185.0f)) { s_armCat = 2; s_armSel = -1; }
        ImGui::SameLine(0, 8);
        if (widgets::os_subtab("Skills", s_armCat == 3, 185.0f)) { s_armCat = 3; s_armSel = -1; }
        struct ArmItem { std::string label; int gun; void* cls; };
        std::vector<ArmItem> items;
        if (s_armCat == 0) {
            for (int i = 0; i < (int)s_allGuns.size(); ++i) {
                ArmItem a;
                a.label = s_allGuns[i].name;
                a.gun = i;
                a.cls = s_allGuns[i].cls;
                bool eq = false;
                for (auto& w : s_weps) {
                    if (w.cls == a.cls) { eq = true; break; }
                }
                if (eq) a.label += " [E]";
                items.push_back(std::move(a));
            }
        } else {
            std::vector<shop::ItemInfo> cat = shop::catalog(false);
            for (auto& it : cat) {
                bool hit = false;
                if (s_armCat == 1) hit = it.name.find("Equipment") != std::string::npos;
                else if (s_armCat == 2) hit = it.name.find("Mask") != std::string::npos ||
                    it.name.find("Outfit") != std::string::npos ||
                    it.name.find("Armor") != std::string::npos;
                else if (s_armCat == 3) hit = it.name.find("Skill") != std::string::npos;
                if (!hit || !IS_VALID_PTR(it.cls)) continue;
                ArmItem a;
                a.label = strip_item(it.name);
                a.gun = -1;
                a.cls = it.cls;
                items.push_back(std::move(a));
            }
        }
        ImGui::BeginChild("ArmoryGrid", ImVec2(0, 268), true);
        {
            float avail = ImGui::GetContentRegionAvail().x;
            float bw = (avail - 8.0f) * 0.5f;
            if (bw < 60.0f) bw = avail;
            if (items.empty()) ImGui::TextDisabled("nothing here - run Shop scans");
            for (int i = 0; i < (int)items.size(); ++i) {
                ImGui::PushID(i);
                bool sel = (s_armSel == i);
                if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.0f, 0.55f, 1.0f));
                if (ImGui::Button(items[i].label.c_str(), ImVec2(bw, 26))) {
                    s_armSel = i;
                    if (s_armCat == 0 && items[i].gun >= 0) availSel = items[i].gun;
                }
                if (sel) ImGui::PopStyleColor();
                ImGui::PopID();
                if (i % 2 == 0 && i + 1 < (int)items.size()) ImGui::SameLine(0, 8);
            }
        }
        ImGui::EndChild();
        if (s_armCat == 0) {
            if (ImGui::Button("EQUIP TO SLOT", ImVec2(-1, 32))) {
                if (s_armSel >= 0 && s_armSel < (int)items.size() && items[s_armSel].gun >= 0) {
                    availSel = items[s_armSel].gun;
                    request_replace();
                } else {
                    snprintf(s_armStatus, sizeof(s_armStatus), "pick a gun first");
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("replace target slot");
        } else {
            const char* act = s_armCat == 1 ? "GRANT TOOL" :
                s_armCat == 2 ? "UNLOCK ITEM" : "GRANT SKILL";
            if (ImGui::Button(act, ImVec2(-1, 32))) {
                if (s_armSel >= 0 && s_armSel < (int)items.size() && IS_VALID_PTR(items[s_armSel].cls)) {
                    void* cls = items[s_armSel].cls;
                    feat::queue_keyed("grant_class", [cls] { shop::grant_class(cls); });
                    snprintf(s_armStatus, sizeof(s_armStatus), "grant queued");
                } else {
                    snprintf(s_armStatus, sizeof(s_armStatus), "pick an item first");
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("add to inventory");
        }
        ImGui::TextDisabled("%s", s_armStatus);
    }
    widgets::os_section_end();

    ImGui::SetCursorPos(ImVec2(0, 1020));
    widgets::os_section("Skin", ImVec2(800, 330));
    {
        ImGui::SetWindowFontScale(dpi_scale);
        if (selWep >= 0 && selWep < (int)s_weps.size())
            ImGui::TextDisabled("target: %s", s_weps[selWep].name.c_str());
        else
            ImGui::TextDisabled("target: none");
        if (ImGui::Button("SCAN CURRENT", ImVec2(-1, 30))) {
            refresh_data(true);
            scan_current();
            snprintf(s_sstatus, sizeof(s_sstatus), "scanned");
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("read equipped skin + charm");
        ImGui::TextDisabled("skin: %s", s_curSkin.c_str());
        ImGui::TextDisabled("charm: %s", s_curCharm.c_str());
        ImGui::TextDisabled("owned skins: %d", (int)s_owned.size());
        static std::vector<std::string> snames;
        static std::vector<const char*> sptrs;
        snames.clear();
        for (auto& o : s_owned) snames.push_back(o.name.empty() ? "?" : o.name);
        sptrs.clear();
        for (auto& s : snames) sptrs.push_back(s.c_str());
        if (!sptrs.empty()) {
            if (skinSel < 0 || skinSel >= (int)sptrs.size()) skinSel = 0;
            ImGui::Combo("Owned skins", &skinSel, sptrs.data(), (int)sptrs.size(), 9);
            if (ImGui::Button("EQUIP SKIN", ImVec2(-1, 30))) {
                feat::queue_keyed("equip_skin", [] {
                    if (skinSel < 0 || skinSel >= (int)s_owned.size() ||
                            selWep < 0 || selWep >= (int)s_weps.size() ||
                            !IS_VALID_PTR(s_weps[selWep].cls)) {
                        snprintf(s_sstatus, sizeof(s_sstatus), "pick skin + weapon");
                        return;
                    }
                    if (s_owned[skinSel].rowId < 0) {
                        snprintf(s_sstatus, sizeof(s_sstatus), "no table row");
                        return;
                    }
                    if (skins::equip_single(s_owned[skinSel].rowId, s_weps[selWep].cls))
                        snprintf(s_sstatus, sizeof(s_sstatus), "equipped %s",
                            s_owned[skinSel].name.c_str());
                    else
                        snprintf(s_sstatus, sizeof(s_sstatus), "equip refused");
                    scan_current();
                });
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("equip on target gun");
        } else {
            ImGui::TextDisabled("no owned skins - unlock in Shop tab");
        }
        ImGui::TextDisabled("%s", s_sstatus);
    }
    widgets::os_section_end();

    ImGui::SetCursorPos(ImVec2(0, 1360));
    widgets::os_section("Charm", ImVec2(800, 340));
    {
        ImGui::SetWindowFontScale(dpi_scale);
        ImGui::TextDisabled("charm: %s", s_curCharm.c_str());
        ImGui::TextDisabled("owned charms: %d", (int)s_charms.size());
        static std::vector<std::string> cnames;
        static std::vector<const char*> cptrs;
        cnames.clear();
        for (auto& c : s_charms) {
            char b[96];
            snprintf(b, sizeof(b), "%s", c.name.c_str());
            cnames.push_back(b);
        }
        cptrs.clear();
        for (auto& c : cnames) cptrs.push_back(c.c_str());
        if (!cptrs.empty()) {
            if (charmSel < 0 || charmSel >= (int)cptrs.size()) charmSel = 0;
            ImGui::Combo("Owned charms", &charmSel, cptrs.data(), (int)cptrs.size(), 9);
            if (ImGui::Button("EQUIP VIA SKIN", ImVec2(-1, 30))) {
                feat::queue_keyed("equip_charm", [] {
                    if (charmSel < 0 || charmSel >= (int)s_charms.size() ||
                            selWep < 0 || selWep >= (int)s_weps.size() ||
                            !IS_VALID_PTR(s_weps[selWep].cls)) {
                        snprintf(s_cstatus, sizeof(s_cstatus), "pick charm + weapon");
                        return;
                    }
                    std::string want = norm_tok(s_charms[charmSel].name);
                    const skins::SkinEntry* found = nullptr;
                    for (auto& e : skins::list()) {
                        if (e.charmName.empty()) continue;
                        if (norm_tok(e.charmName).find(want) != std::string::npos ||
                                want.find(norm_tok(e.charmName)) != std::string::npos) {
                            found = &e;
                            break;
                        }
                    }
                    if (!found) {
                        snprintf(s_cstatus, sizeof(s_cstatus), "no skin carries it");
                        return;
                    }
                    if (skins::equip_single(found->id, s_weps[selWep].cls))
                        snprintf(s_cstatus, sizeof(s_cstatus), "via %.60s", found->name.c_str());
                    else
                        snprintf(s_cstatus, sizeof(s_cstatus), "equip refused");
                    scan_current();
                });
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("equip a skin carrying it");
        } else {
            ImGui::TextDisabled("no owned charms - unlock in Shop tab");
        }
        ImGui::TextDisabled("%s", s_cstatus);
    }
    widgets::os_section_end();
}
struct Reg { Reg() { tabs::add({ "loadout", "Loadout", draw_loadout }); } };
static Reg g_reg;
}

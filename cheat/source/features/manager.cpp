#include "pch/pch.h"
#include "features/manager.hpp"
#include "config/config.hpp"
#include "features/shop.hpp"
#include "features/crates.hpp"
#include "features/exploits.hpp"
#include "core/hook.hpp"
#include "core/offsets.hpp"
#include "core/memory.hpp"
#include "util/veh.hpp"
#include "logger/logger.hpp"
#include "SDK/Basic.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/RobberController_classes.hpp"
#include "SDK/RobberController_parameters.hpp"

#include <cstring>
#include <unordered_map>
#include <unordered_set>

namespace feat {

static std::vector<std::unique_ptr<Entry>>& reg() {
    static std::vector<std::unique_ptr<Entry>> v;
    return v;
}
static bool g_registered = false;

void add(Entry e) {
    auto p = std::make_unique<Entry>();
    *p = Entry{ e.id, e.name, e.desc, e.hotkey, e.tab, e.run, e.autoFlag, e.armFlag };
    reg().push_back(std::move(p));
}

void register_builtin() {
    if (g_registered) return;
    g_registered = true;
    add({ "maps", "UNLOCK ALL MAPS", "AddMap + SaveMaps", "F8", "shop",
        [] { shop::free_all_maps(); }, nullptr, nullptr });
    add({ "cheap_shop", "CHEAP SHOP", "CDO prices $1 + inventory", "", "shop",
        [] { shop::free_all_shop(); }, nullptr, nullptr });
    add({ "cheap_skills", "CHEAP SKILLS", "progress 1.0", "NUM3", "shop",
        [] { shop::free_all_skills(); }, nullptr, nullptr });
    add({ "coins", "GRANT COIN PACKS", "Steam grant flow (real coins)", "F7", "shop",
        [] { shop::coins_legit(); }, nullptr, nullptr });
    add({ "crates", "OPEN ALL CRATES", "SpawnCrate + OpenCrate", "F6", "shop",
        [] { crates::open_all(); }, nullptr, nullptr });
    add({ "vacuum", "MONEY VACUUM + GODMODE", "teleport bags to you", "F9", "world",
        [] { exploits::money_vacuum(); }, nullptr, nullptr });
    add({ "phones", "PHONES OFF", "GuardPhone.Active = false", "F10", "world",
        [] { exploits::disable_phones(); }, nullptr, nullptr });
    add({ "cameras", "CAMERAS OFF", "IgnoreCamera", "NUM0", "world",
        [] { exploits::disable_cameras(); }, nullptr, nullptr });
    add({ "doors", "UNLOCK DOORS", "UnlockDoor", "NUM1", "world",
        [] { exploits::unlock_doors(); }, nullptr, nullptr });
    add({ "guns", "BUFF GUNS", "dmg / mag / no recoil", "NUM2", "world",
        [] { exploits::buff_weapons(); }, nullptr, nullptr });
    add({ "complete", "INSTANT COMPLETE HEIST", "crew + loot to truck", "", "world",
        [] { exploits::complete_heist(); }, nullptr, nullptr });
    add({ "wingame", "INSTANT WIN", "WinGame multicast", "", "world",
        [] { exploits::win_game(); }, nullptr, nullptr });
    add({ "ammo", "INFINITE AMMO", "reserve bullets 99999", "", "world",
        [] { exploits::infinite_ammo(); }, nullptr, nullptr });
    add({ "xp", "+100K XP", "AddEXP once", "", "world",
        [] { exploits::add_xp(); }, nullptr, nullptr });
    add({ "immunity", "IMMUNITY", "GiveImmunityLevel", "", "world",
        [] { exploits::immunity(); }, nullptr, nullptr });
    add({ "dance", "DANCE", "emote on/off", "", "world",
        [] { exploits::dance(); }, nullptr, nullptr });
    add({ "kickself", "KICK SELF", "leave lobby instantly", "", "world",
        [] { exploits::kick_self(); }, nullptr, nullptr });
    add({ "floppy", "FLOPPY ARM", "ragdoll arm", "", "world",
        [] { exploits::floppy(); }, nullptr, nullptr });
    add({ "cops", "TOGGLE COPS", "alert / calm police", "", "world",
        [] { exploits::toggle_cops(); }, nullptr, nullptr });
    add({ "randmask", "RANDOM MASK", "reroll mask", "", "world",
        [] { exploits::random_mask(); }, nullptr, nullptr });
    add({ "endtimer", "END HEIST TIMER", "truck check + countdown", "", "world",
        [] { exploits::end_timer(); }, nullptr, nullptr });
    add({ "cheats", "ENABLE CHEATS", "spawn cheat manager", "", "world",
        [] { exploits::enable_cheats(); }, nullptr, nullptr });
    add({ "ghost", "GHOST NOCLIP", "noclip, rejoin to undo", "", "world",
        [] { exploits::ghost(); }, nullptr, nullptr });
    add({ "playersonly", "FREEZE WORLD", "toggle world freeze", "", "world",
        [] { exploits::players_only(); }, nullptr, nullptr });
    add({ "debugcam", "DEBUG CAMERA", "toggle freecam", "", "world",
        [] { exploits::debug_camera(); }, nullptr, nullptr });
    add({ "depthchams", "DEPTH CHAMS", "custom-depth meshes", "", "world",
        [] { exploits::depth_chams(); }, nullptr, nullptr });
}

static std::vector<const Entry*> g_empty;

const std::vector<const Entry*>& by_tab(const char* tab) {
    static std::unordered_map<std::string, std::vector<const Entry*>> cache;

    static bool built = false;
    if (!built) {
        built = true;
        for (auto& p : reg()) cache[p->tab].push_back(p.get());
    }
    auto it = cache.find(tab ? tab : "");
    return it != cache.end() ? it->second : g_empty;
}

const std::vector<const Entry*>& all() {
    static std::vector<const Entry*> v;
    static bool built = false;
    if (!built) {
        built = true;
        for (auto& p : reg()) v.push_back(p.get());
    }
    return v;
}

std::atomic<int>* key_for(const char* id) {
    if (!id) return nullptr;
    if (!strcmp(id, "maps")) return &g_cfg.keyMaps;
    if (!strcmp(id, "cheap_shop")) return &g_cfg.keyShop;
    if (!strcmp(id, "cheap_skills")) return &g_cfg.keySkills;
    if (!strcmp(id, "coins")) return &g_cfg.keyCoins;
    if (!strcmp(id, "crates")) return &g_cfg.keyCrates;
    if (!strcmp(id, "vacuum")) return &g_cfg.keyVacuum;
    if (!strcmp(id, "phones")) return &g_cfg.keyPhones;
    if (!strcmp(id, "cameras")) return &g_cfg.keyCameras;
    if (!strcmp(id, "doors")) return &g_cfg.keyDoors;
    if (!strcmp(id, "guns")) return &g_cfg.keyGuns;
    if (!strcmp(id, "complete")) return &g_cfg.keyComplete;
    if (!strcmp(id, "wingame")) return &g_cfg.keyWingame;
    if (!strcmp(id, "ammo")) return &g_cfg.keyAmmo;
    if (!strcmp(id, "xp")) return &g_cfg.keyXp;
    if (!strcmp(id, "immunity")) return &g_cfg.keyImmunity;
    if (!strcmp(id, "dance")) return &g_cfg.keyDance;
    if (!strcmp(id, "kickself")) return &g_cfg.keyKickself;
    if (!strcmp(id, "floppy")) return &g_cfg.keyFloppy;
    if (!strcmp(id, "cops")) return &g_cfg.keyCops;
    if (!strcmp(id, "randmask")) return &g_cfg.keyRandmask;
    if (!strcmp(id, "endtimer")) return &g_cfg.keyEndtimer;
    if (!strcmp(id, "cheats")) return &g_cfg.keyCheats;
    if (!strcmp(id, "ghost")) return &g_cfg.keyGhost;
    if (!strcmp(id, "playersonly")) return &g_cfg.keyPlayersonly;
    if (!strcmp(id, "debugcam")) return &g_cfg.keyDebugcam;
    if (!strcmp(id, "depthchams")) return &g_cfg.keyDepthchams;
    return nullptr;
}

std::atomic<int>* mode_for(const char* id) {
    if (!id) return nullptr;
    if (!strcmp(id, "maps")) return &g_cfg.keyModeMaps;
    if (!strcmp(id, "cheap_shop")) return &g_cfg.keyModeShop;
    if (!strcmp(id, "cheap_skills")) return &g_cfg.keyModeSkills;
    if (!strcmp(id, "coins")) return &g_cfg.keyModeCoins;
    if (!strcmp(id, "crates")) return &g_cfg.keyModeCrates;
    if (!strcmp(id, "vacuum")) return &g_cfg.keyModeVacuum;
    if (!strcmp(id, "phones")) return &g_cfg.keyModePhones;
    if (!strcmp(id, "cameras")) return &g_cfg.keyModeCameras;
    if (!strcmp(id, "doors")) return &g_cfg.keyModeDoors;
    if (!strcmp(id, "guns")) return &g_cfg.keyModeGuns;
    if (!strcmp(id, "complete")) return &g_cfg.keyModeComplete;
    if (!strcmp(id, "wingame")) return &g_cfg.keyModeWingame;
    if (!strcmp(id, "ammo")) return &g_cfg.keyModeAmmo;
    if (!strcmp(id, "xp")) return &g_cfg.keyModeXp;
    if (!strcmp(id, "immunity")) return &g_cfg.keyModeImmunity;
    if (!strcmp(id, "dance")) return &g_cfg.keyModeDance;
    if (!strcmp(id, "kickself")) return &g_cfg.keyModeKickself;
    if (!strcmp(id, "floppy")) return &g_cfg.keyModeFloppy;
    if (!strcmp(id, "cops")) return &g_cfg.keyModeCops;
    if (!strcmp(id, "randmask")) return &g_cfg.keyModeRandmask;
    if (!strcmp(id, "endtimer")) return &g_cfg.keyModeEndtimer;
    if (!strcmp(id, "cheats")) return &g_cfg.keyModeCheats;
    if (!strcmp(id, "ghost")) return &g_cfg.keyModeGhost;
    if (!strcmp(id, "playersonly")) return &g_cfg.keyModePlayersonly;
    if (!strcmp(id, "debugcam")) return &g_cfg.keyModeDebugcam;
    if (!strcmp(id, "depthchams")) return &g_cfg.keyModeDepthchams;
    return nullptr;
}

bool run(const char* id) {
    if (!id) return false;
    for (auto& p : reg()) {
        if (strcmp(p->id, id) == 0) {

            p->run();
            return true;
        }
    }
    return false;
}

static std::mutex g_qmtx;
static std::vector<std::function<void()>> g_q;
static std::unordered_map<std::string, uint64_t> g_qlast;
static std::unordered_set<std::string> g_qpending;

void queue_fn(std::function<void()> fn) {
    std::lock_guard<std::mutex> lk(g_qmtx);
    if (g_q.size() >= 16) g_q.erase(g_q.begin());
    g_q.push_back(std::move(fn));
}

void queue_keyed(const char* key, std::function<void()> fn) {
    if (!key || !key[0]) {
        queue_fn(std::move(fn));
        return;
    }
    uint64_t now = GetTickCount64();
    std::lock_guard<std::mutex> lk(g_qmtx);
    auto it = g_qlast.find(key);
    if (it != g_qlast.end() && now - it->second < 750) return;
    if (g_qpending.find(key) != g_qpending.end()) return;
    g_qlast[key] = now;
    g_qpending.insert(key);
    if (g_q.size() >= 16) { g_q.erase(g_q.begin()); }
    g_q.push_back(std::move(fn));
}

void queue_run(const char* id) {
    if (!id || !id[0]) return;
    uint64_t now = GetTickCount64();
    std::lock_guard<std::mutex> lk(g_qmtx);
    auto it = g_qlast.find(id);
    if (it != g_qlast.end() && now - it->second < 750) return;
    if (g_qpending.find(id) != g_qpending.end()) return;
    g_qlast[id] = now;
    g_qpending.insert(id);
    std::string key = id;
    if (g_q.size() >= 16) { g_q.erase(g_q.begin()); }
    g_q.push_back([key] { run(key.c_str()); });
}

static void seh_call(const std::function<void()>& fn) {
    __try { fn(); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static std::mutex g_gameMtx;
static std::vector<std::function<void()>> g_gameQ;
static hook::Detour g_peHook;
static std::atomic<bool> s_draining{ false };

static void* s_fnSetWeapons = nullptr;
static void* s_fnEquipSkin = nullptr;
static void* s_fnUpdateNewWeapon = nullptr;
static bool s_fnResolved = false;
static char s_spyLast[192] = "spy: waiting for game loadout edit...";
static int s_spyCount = 0;
static int s_spySig = 0;

const char* pespy_status() {
    return s_spyLast;
}

static void pespy_note(const char* s) {
    strncpy_s(s_spyLast, s, _TRUNCATE);
    if (g_logger) g_logger->send(levels::success, "Spy: {}", s_spyLast);
}

static void pespy_resolve() {
    if (s_fnResolved) return;
    veh::try_call([] {
        SDK::UClass* rc = SDK::ARobberController_C::StaticClass();
        if (!IS_VALID_PTR(rc)) return;
        void* a = rc->GetFunction("RobberController_C", "SetWeaponsServer");
        void* b = rc->GetFunction("RobberController_C", "EquipSkin");
        void* c = rc->GetFunction("RobberController_C", "UpdateNewWeapon");
        if (a) s_fnSetWeapons = a;
        if (b) s_fnEquipSkin = b;
        if (c) s_fnUpdateNewWeapon = c;
        s_fnResolved = true;
    });
}

static void pespy_watch(void* func, void* parms) {
    pespy_resolve();
    if (!parms) return;
    if (func == s_fnSetWeapons && s_fnSetWeapons) {
        int gn = -1, mn = -1, sn = -1, lo = -999;
        bool ok = veh::try_call([&] {
            auto* p = reinterpret_cast<SDK::Params::RobberController_C_SetWeaponsServer*>(parms);
            gn = p->Weapons.Num();
            mn = p->Mods.Num();
            sn = p->WeaponSkins.Num();
            lo = p->Loadout_Equipped_0;
        });
        if (!ok) return;
        if (gn < 0 || gn > 64 || mn < 0 || mn > 64 || sn < 0 || sn > 512) return;
        int sig = ((gn & 0xFF) << 24) | ((mn & 0xFF) << 16) | ((sn & 0x1FF) << 7) | (lo & 0x7F);
        if (sig != s_spySig || s_spyCount < 3) {
            s_spySig = sig;
            ++s_spyCount;
            char buf[192];
            snprintf(buf, sizeof(buf), "game SetWeaponsServer #%d: guns=%d mods=%d skins=%d loadout=%d",
                s_spyCount, gn, mn, sn, lo);
            pespy_note(buf);
        }
    } else if (func == s_fnEquipSkin && s_fnEquipSkin) {
        int id = -1, cat = -1;
        bool ok = veh::try_call([&] {
            auto* p = reinterpret_cast<SDK::Params::RobberController_C_EquipSkin*>(parms);
            id = p->ID;
            cat = (int)p->Category;
        });
        if (!ok) return;
        ++s_spyCount;
        char buf[192];
        snprintf(buf, sizeof(buf), "game EquipSkin #%d: id=%d cat=%d", s_spyCount, id, cat);
        pespy_note(buf);
    } else if (func == s_fnUpdateNewWeapon && s_fnUpdateNewWeapon) {
        int idx = -999;
        bool ok = veh::try_call([&] {
            auto* p = reinterpret_cast<SDK::Params::RobberController_C_UpdateNewWeapon*>(parms);
            idx = p->Index_0;
        });
        if (!ok) return;
        ++s_spyCount;
        char buf[192];
        snprintf(buf, sizeof(buf), "game UpdateNewWeapon #%d: index=%d", s_spyCount, idx);
        pespy_note(buf);
    }
}

static void hkProcessEvent(void* obj, void* func, void* parms) {
    auto orig = g_peHook.orig<void(*)(void*, void*, void*)>();
    bool expected = false;
    if (s_draining.compare_exchange_strong(expected, true)) {
        std::vector<std::function<void()>> local;
        {
            std::lock_guard<std::mutex> lk(g_gameMtx);
            local.swap(g_gameQ);
        }
        for (auto& fn : local) seh_call(fn);
        s_draining.store(false);
        pespy_watch(func, parms);
    }
    orig(obj, func, parms);
}

void queue_game(std::function<void()> fn) {
    std::lock_guard<std::mutex> lk(g_gameMtx);
    if (g_gameQ.size() >= 8) g_gameQ.erase(g_gameQ.begin());
    g_gameQ.push_back(std::move(fn));
}

bool ensure_gamehook(void* obj) {
    if (g_peHook.valid()) return true;
    void** vt = nullptr;
    __try { vt = *reinterpret_cast<void***>(obj); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    if (!IS_VALID_PTR(vt)) return false;
    void* target = nullptr;
    __try { target = vt[offsets::ProcessEventIdx]; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    if (!IS_VALID_PTR(target)) return false;
    uintptr_t base = mem::image_base();
    uintptr_t t = reinterpret_cast<uintptr_t>(target);
    uintptr_t viaOff = base + (uintptr_t)offsets::ProcessEvent;
    if (t != viaOff && (t < base || t > base + 0x40000000u)) return false;
    if (!g_peHook.create(target, (void*)hkProcessEvent)) return false;
    return g_peHook.enable();
}

void shutdown_gamehook() { g_peHook.remove(); }

void run_queued() {
    static std::atomic<bool> s_busy{ false };
    bool expected = false;
    if (!s_busy.compare_exchange_strong(expected, true)) return;
    std::vector<std::function<void()>> local;
    {
        std::lock_guard<std::mutex> lk(g_qmtx);
        if (g_q.empty()) { s_busy.store(false); return; }
        local.swap(g_q);
        g_qpending.clear();
    }
    for (auto& fn : local) seh_call(fn);
    s_busy.store(false);
}

}

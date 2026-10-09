#include "pch/pch.h"
#include "features/manager.hpp"
#include "config/config.hpp"
#include "features/shop.hpp"
#include "features/crates.hpp"
#include "features/exploits.hpp"
#include "core/hook.hpp"
#include "core/offsets.hpp"
#include "core/memory.hpp"

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
    *p = Entry{ e.id, e.name, e.desc, e.hotkey, e.tab, e.run, e.autoFlag };
    reg().push_back(std::move(p));
}

void register_builtin() {
    if (g_registered) return;
    g_registered = true;
    add({ "maps", "UNLOCK ALL MAPS", "AddMap + SaveMaps", "F8", "shop",
        [] { shop::free_all_maps(); }, nullptr });
    add({ "cheap_shop", "CHEAP SHOP", "CDO prices $1 + inventory", "", "shop",
        [] { shop::free_all_shop(); }, nullptr });
    add({ "cheap_skills", "CHEAP SKILLS", "progress 1.0", "NUM3", "shop",
        [] { shop::free_all_skills(); }, nullptr });
    add({ "coins", "GRANT COIN PACKS", "Steam grant flow (real coins)", "F7", "shop",
        [] { shop::coins_legit(); }, nullptr });
    add({ "crates", "OPEN ALL CRATES", "SpawnCrate + OpenCrate", "F6", "shop",
        [] { crates::open_all(); }, nullptr });
    add({ "vacuum", "MONEY VACUUM + GODMODE", "teleport bags to you", "F9", "world",
        [] { exploits::money_vacuum(); }, nullptr });
    add({ "phones", "PHONES OFF", "GuardPhone.Active = false", "F10", "world",
        [] { exploits::disable_phones(); }, &g_cfg.autoPhones });
    add({ "cameras", "CAMERAS OFF", "IgnoreCamera", "NUM0", "world",
        [] { exploits::disable_cameras(); }, &g_cfg.autoCameras });
    add({ "doors", "UNLOCK DOORS", "UnlockDoor", "NUM1", "world",
        [] { exploits::unlock_doors(); }, &g_cfg.autoDoors });
    add({ "guns", "BUFF GUNS", "dmg / mag / no recoil", "NUM2", "world",
        [] { exploits::buff_weapons(); }, nullptr });
    add({ "complete", "INSTANT COMPLETE HEIST", "crew + loot to truck", "", "world",
        [] { exploits::complete_heist(); }, nullptr });
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

static void seh_run(void (*fn)()) {
    __try { if (fn) fn(); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
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

void tick_autos() {
    static uint64_t lastMs = 0;
    bool any = false;
    for (auto& p : reg()) {
        if (p->autoFlag && p->autoFlag->load()) { any = true; break; }
    }
    if (!any) return;
    uint64_t now = GetTickCount64();
    float interval = g_cfg.autoIntervalSec.load();
    if (interval < 1.0f) interval = 1.0f;
    if (now - lastMs < (uint64_t)(interval * 1000.0f)) return;
    lastMs = now;
    for (auto& p : reg()) {
        if (p->autoFlag && p->autoFlag->load()) seh_run(p->run);
    }
}

}

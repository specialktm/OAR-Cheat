#include "pch/pch.h"
#include "cheat.h"
#include "base/Initialize.hpp"
#include "config/config.hpp"
#include "world/diagnostics.hpp"
#include "util/veh.hpp"
#include "world/actor_iter.hpp"
#include "render/dx11_hook.hpp"
#include "features/manager.hpp"
#include "features/shop.hpp"
#include "features/crates.hpp"
#include "features/capture.hpp"
#include "features/exploits.hpp"
#include "logger/logger.hpp"
#include "core/loading.hpp"
#include "steam/steam_user.hpp"

#include "SDK/Engine_classes.hpp"

using namespace SDK;

static bool edge(int vk) { return (GetAsyncKeyState(vk) & 1) != 0; }

void cheat::Initialize()
{
    feat::register_builtin();
    veh::ensure();

    if (!didInitDx.exchange(true)) {

        bool ok = dx11::init();
        if (g_logger) {
            if (ok) g_logger->send(levels::success, "DX11 Present hook on (INSERT = menu)");
            else g_logger->send(levels::error, "DX11 hook failed - hotkeys only");
        }
        loading::stage(1, "Signature scan");
        diag::report();
        loading::stage(2, "Steam profile");
        steamServiceHelper::fetch();
        loading::stage(3, "Overlay");
        loading::finish();
    }
}

void cheat::tick()
{
    if (dx11::is_unloading()) return;

    bool listOpen = g_cfg.menuOpen.load() && g_cfg.uiStyle.load() == 1;
    auto suppressed = [listOpen](int vk) {
        if (vk == 0) return true;
        if (listOpen && (vk == VK_NUMPAD0 || vk == VK_NUMPAD2 || vk == VK_NUMPAD4 ||
                vk == VK_NUMPAD5 || vk == VK_NUMPAD6 || vk == VK_NUMPAD8))
            return true;
        return false;
    };
    auto held = [](int vk) {
        return vk != 0 && (GetAsyncKeyState(vk) & 0x8000) != 0;
    };
    struct Job { std::atomic<int> *key, *mode; void (*fn)(); };
    static Job jobs[] = {
        { &g_cfg.keyMaps, &g_cfg.keyModeMaps, [] { shop::free_all_maps(); } },
        { &g_cfg.keyShop, &g_cfg.keyModeShop, [] { shop::free_all_shop(); } },
        { &g_cfg.keySkills, &g_cfg.keyModeSkills, [] { shop::free_all_skills(); } },
        { &g_cfg.keyCoins, &g_cfg.keyModeCoins, [] { shop::coins_legit(); } },
        { &g_cfg.keyCrates, &g_cfg.keyModeCrates, [] { crates::open_all(); } },
        { &g_cfg.keyVacuum, &g_cfg.keyModeVacuum, [] { exploits::money_vacuum(); } },
        { &g_cfg.keyPhones, &g_cfg.keyModePhones, [] { exploits::disable_phones(); } },
        { &g_cfg.keyCameras, &g_cfg.keyModeCameras, [] { exploits::disable_cameras(); } },
        { &g_cfg.keyDoors, &g_cfg.keyModeDoors, [] { exploits::unlock_doors(); } },
        { &g_cfg.keyGuns, &g_cfg.keyModeGuns, [] { exploits::buff_weapons(); } },
    };
    static uint64_t lastRun[10] = {};
    uint64_t nowMs = GetTickCount64();
    float interval = g_cfg.autoIntervalSec.load();
    if (interval < 1.0f) interval = 1.0f;
    for (int i = 0; i < 10; ++i) {
        int vk = jobs[i].key->load();
        if (suppressed(vk)) continue;
        int mode = jobs[i].mode->load();
        if (mode == 1) {
            if (held(vk) && nowMs - lastRun[i] > 800) {
                lastRun[i] = nowMs;
                jobs[i].fn();
            }
        } else if (mode == 2) {
            if (nowMs - lastRun[i] > (uint64_t)(interval * 1000.0f)) {
                lastRun[i] = nowMs;
                jobs[i].fn();
            }
        } else if (edge(vk)) {
            jobs[i].fn();
        }
    }
    if (edge(g_cfg.keyUnload.load())) programRunning.store(false);
    int menuMode = g_cfg.keyModeMenu.load();
    int menuVk = g_cfg.keyMenu.load();
    if (menuMode == 1) g_cfg.menuOpen.store(held(menuVk));
    else if (menuMode == 2) g_cfg.menuOpen.store(true);

    if (g_cfg.godMode.load()) exploits::god_tick();

    {
        static DWORD lastRetry = 0;
        static int retries = 0;
        DWORD now = GetTickCount();
        if (retries < 5 && now - lastRetry > 10000) {
            lastRetry = now;
            ++retries;
            steamServiceHelper::retry_avatar();
        }
    }

    capture::set_active(g_cfg.showModelViewer.load() ||
        GetTickCount64() - g_cfg.modelSeenMs.load() < 800);
    capture::tick();

    feat::run_queued();
    crates::tick_steam();
    feat::tick_autos();

    if (g_cfg.autoFreeShop.load()) {
        static DWORD last = 0;
        DWORD now = GetTickCount();
        if (now - last > 30000) { last = now; shop::free_all_shop(); }
    }
}

void cheat::Uninitialize()
{
    feat::shutdown_gamehook();
    steamServiceHelper::shutdown();
    capture::shutdown();
    dx11::shutdown();
}

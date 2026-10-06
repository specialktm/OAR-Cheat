#include "pch/pch.h"
#include "cheat.h"
#include "base/Initialize.hpp"
#include "core/config.hpp"
#include "core/diagnostics.hpp"
#include "core/veh.hpp"
#include "render/dx11_hook.hpp"
#include "features/manager.hpp"
#include "features/shop.hpp"
#include "features/crates.hpp"
#include "features/capture.hpp"
#include "features/exploits.hpp"
#include "logger/logger.hpp"

using namespace SDK;

static bool edge(int vk) { return (GetAsyncKeyState(vk) & 1) != 0; }

void cheat::Initialize()
{
    feat::register_builtin();
    veh::ensure();
    // DX11 overlay first: ESP + menu live in Present, hotkeys keep working headless.
    if (!didInitDx.exchange(true)) {
        bool ok = dx11::init();
        if (g_logger) {
            if (ok) g_logger->send(levels::success, "DX11 Present hook on (INSERT = menu)");
            else g_logger->send(levels::error, "DX11 hook failed - hotkeys only");
        }
        diag::report();
    }
}

void cheat::tick()
{
    if (dx11::is_unloading()) return;
    // Edge-triggered, non-blocking. All VKs editable in Settings > KEYBINDS.
    auto hit = [](std::atomic<int>& k) {
        int vk = k.load();
        return vk != 0 && edge(vk);
    };
    if (hit(g_cfg.keyMaps)) shop::free_all_maps();
    if (hit(g_cfg.keyVacuum)) exploits::money_vacuum();
    if (hit(g_cfg.keyPhones)) exploits::disable_phones();
    if (hit(g_cfg.keyCameras)) exploits::disable_cameras();
    if (hit(g_cfg.keyDoors)) exploits::unlock_doors();
    if (hit(g_cfg.keyGuns)) exploits::buff_weapons();
    if (hit(g_cfg.keySkills)) shop::free_all_skills();
    if (hit(g_cfg.keyCrates)) crates::open_all();
    if (hit(g_cfg.keyCoins)) shop::coins_legit();
    if (hit(g_cfg.keyShop)) shop::free_all_shop();

    if (g_cfg.godMode.load()) exploits::god_tick();

    capture::set_active(g_cfg.showModelViewer.load());
    capture::tick();

    feat::run_queued();
    feat::tick_autos();

    if (g_cfg.autoFreeShop.load()) {
        static DWORD last = 0;
        DWORD now = GetTickCount();
        if (now - last > 30000) { last = now; shop::free_all_shop(); }
    }
}

void cheat::Uninitialize()
{
    capture::shutdown();
    dx11::shutdown();
}

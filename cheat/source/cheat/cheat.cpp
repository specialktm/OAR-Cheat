#include "pch/pch.h"
#include "cheat.h"
#include "base/Initialize.hpp"
#include "config/config.hpp"
#include "world/diagnostics.hpp"
#include "core/memory.hpp"
#include "util/veh.hpp"
#include "world/actor_iter.hpp"
#include "render/dx11_hook.hpp"
#include "features/manager.hpp"
#include "features/takebind.hpp"
#include "features/oar_lua.hpp"
#include "features/music/music.h"
#include "util/crashlog.hpp"
#include "features/shop.hpp"
#include "features/crates.hpp"
#include "features/capture.hpp"
#include "features/exploits.hpp"
#include "logger/logger.hpp"
#include "core/loading.hpp"
#include "steam/steam_user.hpp"
#include "../../vendor/imgui/imgui.h"
#include <unordered_map>

#include "SDK/Engine_classes.hpp"
#include "SDK/CoreUObject_classes.hpp"

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
        {
            uintptr_t rva = mem::gobjects_rva();
            if (rva) SDK::UObject::GObjects.InitManually(reinterpret_cast<void*>(mem::image_base() + rva));
        }
        loading::stage(2, "Steam profile");
        steamServiceHelper::fetch();
        loading::stage(3, "Overlay");
        loading::finish();
    }
}

void cheat::tick()
{
    if (dx11::is_unloading()) return;

    HWND fg = GetForegroundWindow();
    HWND gw = (HWND)dx11::window();
    bool fgOk = (gw == nullptr) || (fg == gw);
    (void)fgOk;
    struct HState { keybind::keybind_t st; bool was = false; uint64_t last = 0; };
    static std::unordered_map<std::string, HState> hstates;
    uint64_t nowMs = GetTickCount64();
    if (ImGui::GetCurrentContext()) {
        for (const feat::Entry* e : feat::all()) {
            auto* k = feat::key_for(e->id);
            if (!k) continue;
            int vk = k->load();
            if (!vk) continue;
            auto* m = feat::mode_for(e->id);
            HState& s = hstates[e->id];
            s.st.key = vk;
            s.st.mode = m ? (keybind::activation_mode)m->load() : keybind::activation_mode::hold;
            bool active = keybind::is_active(s.st);
            int mode = m ? m->load() : 1;
            if (mode == 2) {
                if (active && nowMs - s.last > 5000) {
                    s.last = nowMs;
                    feat::queue_run(e->id);
                }
            } else if (active && !s.was) {
                feat::queue_run(e->id);
            }
            s.was = active;
        }
    }
    if (edge(g_cfg.keyUnload.load())) programRunning.store(false);
    int menuMode = g_cfg.keyModeMenu.load();
    int menuVk = g_cfg.keyMenu.load();
    if (menuMode == 1) g_cfg.menuOpen.store((menuVk != 0 && (GetAsyncKeyState(menuVk) & 0x8000) != 0));
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

    if (g_cfg.autoFreeShop.load()) {
        static DWORD last = 0;
        DWORD now = GetTickCount();
        if (now - last > 30000) { last = now; shop::free_all_shop(); }
    }
}

void cheat::Uninitialize()
{
    dx11::begin_unload();
    feat::shutdown_gamehook();
    lua::stop_all();
    music::shutdown();
    veh::remove();
    steamServiceHelper::shutdown();
    capture::shutdown();
    dx11::shutdown();
    crashlog::shutdown();
}

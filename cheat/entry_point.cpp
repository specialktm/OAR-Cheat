// dllmain: minimal, safe init/unload. Render work happens in Present hook,
// hotkey pump on a worker thread with 50ms cadence (was 400ms + per-actor sleeps).
#include "pch/pch.h"
#include "cheat/cheat.h"
#include "cheat/base/Initialize.hpp"
#include "config/config.hpp"
#include "core/loading.hpp"
#include "logger/logger.hpp"
#include "util/crashlog.hpp"

static void safe_tick() {
    __try { if (g_cheat) g_cheat->tick(); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

BOOL APIENTRY DllMain(HMODULE hModule, unsigned long reason, void* lpReserved)
{
    UNREFERENCED_PARAMETER(lpReserved);
    if (reason == DLL_PROCESS_ATTACH) {
        g_Module = hModule;
        DisableThreadLibraryCalls(g_Module);
        g_MainThread = CreateThread(nullptr, 0, [](void*) -> DWORD {
            crashlog::init();
            loading::begin(); // injection loading screen (drawn by Present)
            g_logger = new logger();
            g_Initializer = new Initializer();
            g_logger->send(levels::debug, "Adding class interface...");
            g_Initializer->AddClassInterface(&g_cheat);
            g_logger->send(levels::debug, "Calling Initializer...");
            g_Initializer->Initialize();
            g_logger->send(levels::debug, "Main loop. INSERT=menu, END=unload.");

            while (programRunning.load()) {
                safe_tick();
                int uvk = g_cfg.keyUnload.load();
                if (uvk != 0 && (GetAsyncKeyState(uvk) & 1)) {
                    g_logger->send(levels::debug, "unload key, unloading.");
                    programRunning.store(false);
                    break;
                }
                std::this_thread::sleep_for(50ms);
            }

            g_logger->send(levels::debug, "Uninitializing...");
            g_Initializer->Uninitialize();
            delete g_Initializer; g_Initializer = nullptr;
            g_logger->send(levels::debug, "Bye.");
            delete g_logger; g_logger = nullptr;

            CloseHandle(g_MainThread); g_MainThread = nullptr;
            FreeLibraryAndExitThread(g_Module, 0);
        }, nullptr, 0, nullptr);
    }
    return TRUE;
}

extern "C" __declspec(dllexport) void unload()
{
    programRunning.store(false);
}

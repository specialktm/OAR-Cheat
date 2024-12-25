// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch/pch.h"
#include "cheat/cheat.h"
#include "cheat/base/Initialize.hpp"


BOOL APIENTRY DllMain(HMODULE hModule, unsigned long callreason, void* lpReserved)
{
    g_Module = hModule;
    UNREFERENCED_PARAMETER(lpReserved);

    if (callreason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(g_Module);
        g_MainThread = CreateThread(nullptr, NULL, [](void* ) -> DWORD
        {
            g_logger = new logger();
            g_Initializer = new Initializer();

            g_logger->send(levels::debug, "Adding class interface...");
            g_Initializer->AddClassInterface(&g_cheat);

            g_logger->send(levels::debug, "Calling Initializer...");
            g_Initializer->Initialize();

            g_logger->send(levels::debug, "Running main loop. Press 'END' to exit.");

            while (programRunning)
            {
                g_cheat->tick();

                if (GetAsyncKeyState(VK_END) && 1)
                {
                    g_logger->send(levels::debug, "'END' key pressed. Exiting loop.");
                    programRunning = false;
                    std::this_thread::yield();
                }

                std::this_thread::sleep_for(400ms);
            }

            g_logger->send(levels::debug, "Uninitializing Classes...");
            g_Initializer->Uninitialize();

            g_logger->send(levels::debug, "Cleaning up resources...");
            delete g_Initializer;

            g_logger->send(levels::debug, "Exiting program.");
            delete g_logger; 

            CloseHandle(g_MainThread);
            FreeLibraryAndExitThread(g_Module, 0);
        }, nullptr, NULL, nullptr);
    }
    return true;
}
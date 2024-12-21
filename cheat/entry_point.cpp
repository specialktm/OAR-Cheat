// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch/pch.h"
#include "cheat/cheat.h"
#include "cheat/base/Initialize.hpp"


BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(lpReserved);

    g_Module = hModule;
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(g_Module);
        g_MainThread = CreateThread(nullptr, NULL, [](LPVOID lpThreadParameter)->DWORD
            {

                g_logger = std::make_unique<logger>();
                g_logger->send(levels::debug, "Initializing...");

                cheat* m_cheat;
                Initializer* m_Initializer = new Initializer;
              

                g_logger->send(levels::debug, "Adding class interface...");
                m_Initializer->AddClassInterface(&m_cheat);

                g_logger->send(levels::debug, "Calling Initializer...");
                m_Initializer->Initialize();


                g_logger->send(levels::debug, "Running main loop. Press 'END' to exit.");
                if (!programRunning)
                    programRunning = true;


                while (programRunning)
                {
                    m_cheat->tick();
        
                    if (GetAsyncKeyState(VK_END) && 1)
                    {
                        g_logger->send(levels::debug, "'END' key pressed. Exiting loop.");
                        programRunning = false;
                        std::this_thread::yield();
                    }
                    std::this_thread::sleep_for(400ms);
                }

                g_logger->send(levels::debug, "Uninitializing Classes...");
                m_Initializer->Uninitialize();

                g_logger->send(levels::debug, "Cleaning up resources...");
                delete m_Initializer;

                g_logger->send(levels::debug, "Exiting program.");
                
                g_logger.reset();
                CloseHandle(g_MainThread);
                FreeLibraryAndExitThread(g_Module, 0);

            }, nullptr, NULL, nullptr);
    }
    return TRUE;
}


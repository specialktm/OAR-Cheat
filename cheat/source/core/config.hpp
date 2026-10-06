#pragma once
#include "pch/pch.h"
#include "../../vendor/imgui/imgui.h"

// Global runtime config, driven by ImGui menu. Hotkeys still work when menu is closed.
struct Config {
    std::atomic<bool> menuOpen{ true };

    // ESP - items
    std::atomic<bool> itemEsp{ true };
    std::atomic<bool> moneyEsp{ true };
    std::atomic<bool> pickupEsp{ true };
    std::atomic<bool> keycardEsp{ false };
    std::atomic<float> itemMaxDist{ 15000.0f }; // cm (UE units). 0 = unlimited
    std::atomic<bool> itemShowDistance{ true };
    std::atomic<bool> itemShowValue{ true };

    // ESP - players
    std::atomic<bool> playerEsp{ true };
    std::atomic<bool> playerBox{ true };
    std::atomic<bool> playerName{ true };
    std::atomic<bool> playerHealth{ true };
    std::atomic<bool> playerDistance{ true };
    std::atomic<float> playerMaxDist{ 0.0f };

    // ESP - guards / police
    std::atomic<bool> guardEsp{ true };
    std::atomic<bool> guardBox{ true };
    std::atomic<bool> guardState{ true };
    std::atomic<float> guardMaxDist{ 0.0f };

    // ESP style lab (offsets in px, tuned in the preview canvas)
    std::atomic<float> espBoxW{ 0.45f };
    std::atomic<float> espNameDy{ -16.0f };
    std::atomic<float> espPrevX{ 0.5f };  // preview figure pos, normalized
    std::atomic<float> espPrevY{ 0.5f };
    std::atomic<int> espBoxStyle{ 0 };    // 0 = 2D full, 1 = corners, 2 = 3D
    std::atomic<int> espLabelAnchor{ 0 }; // 0..7 snap points around the box (name line)
    std::atomic<int> espSubAnchor{ 4 };   // 0..7 snap for distance/state line
    std::atomic<bool> hpLeft{ true };     // health bar side (drag it to flip)
    std::atomic<bool> itemBox{ true };    // mini boxes on items (same styles)
    std::atomic<bool> boxFill{ false };   // translucent fill inside boxes
    std::atomic<bool> healthGradient{ true };
    std::atomic<int> healthStyle{ 0 };    // 0 = bar+number, 1 = bar, 2 = number
    std::atomic<int> nameMode{ 0 };       // 0 = full GetName, 1 = short
    std::atomic<bool> guardDistance{ true };
    std::atomic<bool> playerSkel{ true };
    std::atomic<bool> guardSkel{ true };
    std::atomic<int> uiStyle{ 0 }; // 0 = mouse UI, 1 = numpad list UI
    // ESP palette (render thread only: UI + overlay share the Present thread)
    ImVec4 playerCol{ 0.35f, 1.0f, 0.45f, 1.0f };
    ImVec4 guardCol{ 1.0f, 0.67f, 0.24f, 1.0f };
    ImVec4 itemCol{ 0.51f, 0.78f, 1.0f, 1.0f };
    ImVec4 moneyCol{ 0.47f, 1.0f, 0.51f, 1.0f };
    ImVec4 healthCol{ 0.35f, 0.95f, 0.55f, 1.0f };

    // Watermark anchor (draggable)
    std::atomic<float> wmX{ 12.0f };
    std::atomic<float> wmY{ 12.0f };

    // Detached popout windows
    std::atomic<bool> showEspPreview{ false };
    std::atomic<bool> showItemPreview{ false };
    std::atomic<bool> showModelViewer{ false };

    // ESP shared
    std::atomic<bool> espEnabled{ true };
    std::atomic<float> espTextScale{ 1.0f };

    // Shop / unlocks
    std::atomic<bool> autoFreeShop{ false };

    // Auto actions (feature manager runs these on an interval)
    std::atomic<bool> autoPhones{ false };
    std::atomic<bool> autoCameras{ false };
    std::atomic<bool> autoDoors{ false };
    std::atomic<float> autoIntervalSec{ 5.0f };

    // Watermark overlay
    std::atomic<bool> watermark{ true };
    std::atomic<bool> clock24h{ true };

    // Backdrop / theme
    std::atomic<bool> blurBackdrop{ true };
    std::atomic<int> themeIdx{ 0 };
    // Custom theme editor (themeIdx == theme::count() means Custom)
    ImVec4 customA{ 0.55f, 0.42f, 1.0f, 1.0f };
    ImVec4 customB{ 0.25f, 0.85f, 1.0f, 1.0f };
    ImVec4 customGlow{ 0.55f, 0.45f, 1.0f, 1.0f };

    // Editable keybinds (VK codes, 0 = none). Render thread UI + worker tick.
    std::atomic<int> keyMenu{ 0x2D };    // INSERT
    std::atomic<int> keyUnload{ 0x23 };  // END
    std::atomic<int> keyMaps{ 0x77 };    // F8
    std::atomic<int> keyShop{ 0 };       // cheap shop (none by default)
    std::atomic<int> keySkills{ 0x63 };  // NUM3
    std::atomic<int> keyCoins{ 0x76 };   // F7
    std::atomic<int> keyCrates{ 0x75 };  // F6
    std::atomic<int> keyVacuum{ 0x78 };  // F9
    std::atomic<int> keyPhones{ 0x79 };  // F10
    std::atomic<int> keyCameras{ 0x60 }; // NUM0
    std::atomic<int> keyDoors{ 0x61 };  // NUM1
    std::atomic<int> keyGuns{ 0x62 };    // NUM2

    std::string configName{ "default" };
    std::string configStatus;

    // Exploits (trigger once via menu button or hotkey)
    std::atomic<bool> godMode{ false };
};

inline Config g_cfg;

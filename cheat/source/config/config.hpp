#pragma once
#include "pch/pch.h"
#include "../../vendor/imgui/imgui.h"

struct Config {
    std::atomic<bool> menuOpen{ true };

    std::atomic<bool> itemEsp{ true };
    std::atomic<bool> moneyEsp{ true };
    std::atomic<bool> pickupEsp{ true };
    std::atomic<bool> keycardEsp{ false };
    std::atomic<float> itemMaxDist{ 15000.0f };
    std::atomic<bool> itemShowDistance{ true };
    std::atomic<bool> itemShowValue{ true };

    std::atomic<bool> playerEsp{ true };
    std::atomic<bool> playerBox{ true };
    std::atomic<bool> playerName{ true };
    std::atomic<bool> playerHealth{ true };
    std::atomic<bool> playerDistance{ true };
    std::atomic<float> playerMaxDist{ 0.0f };

    std::atomic<bool> guardEsp{ true };
    std::atomic<bool> guardBox{ true };
    std::atomic<bool> guardState{ true };
    std::atomic<float> guardMaxDist{ 0.0f };

    std::atomic<float> espBoxW{ 0.45f };
    std::atomic<float> espNameDy{ -16.0f };
    std::atomic<float> espPrevX{ 0.5f };
    std::atomic<float> espPrevY{ 0.5f };
    std::atomic<int> espBoxStyle{ 0 };
    std::atomic<int> espLabelAnchor{ 0 };
    std::atomic<int> espSubAnchor{ 4 };
    std::atomic<bool> hpLeft{ true };
    std::atomic<bool> itemBox{ true };
    std::atomic<bool> boxFill{ false };
    std::atomic<int> healthStyle{ 0 };
    std::atomic<int> nameMode{ 0 };
    std::atomic<bool> guardDistance{ true };
    std::atomic<bool> playerSkel{ true };
    std::atomic<bool> guardSkel{ true };
    std::atomic<bool> playerHolding{ false };
    std::atomic<bool> guardHealth{ true };

    std::atomic<bool> visCheck{ true };
    std::atomic<bool> hideDead{ false };
    std::atomic<int> uiStyle{ 0 };

    ImVec4 playerCol{ 0.35f, 1.0f, 0.45f, 1.0f };
    ImVec4 guardCol{ 1.0f, 0.67f, 0.24f, 1.0f };
    ImVec4 itemCol{ 0.51f, 0.78f, 1.0f, 1.0f };
    ImVec4 moneyCol{ 0.47f, 1.0f, 0.51f, 1.0f };
    ImVec4 healthCol{ 0.35f, 0.95f, 0.55f, 1.0f };
    ImVec4 npcCol{ 0.71f, 0.39f, 1.0f, 1.0f };
    ImVec4 deadCol{ 0.47f, 0.47f, 0.47f, 1.0f };

    std::atomic<float> wmX{ 12.0f };
    std::atomic<float> wmY{ 12.0f };

    std::atomic<bool> showModelViewer{ false };

    std::atomic<unsigned long long> modelSeenMs{ 0 };

    std::atomic<bool> espEnabled{ true };
    std::atomic<float> espTextScale{ 1.0f };

    std::atomic<bool> autoFreeShop{ false };

    std::atomic<bool> autoPhones{ false };
    std::atomic<bool> autoCameras{ false };
    std::atomic<bool> autoDoors{ false };
    std::atomic<float> autoIntervalSec{ 5.0f };

    std::atomic<bool> watermark{ true };
    std::atomic<bool> clock24h{ true };
    std::atomic<bool> particles{ false };

    std::atomic<bool> blurBackdrop{ true };
    std::atomic<float> blurAmount{ 0.5f };
    std::atomic<int> themeIdx{ 0 };

    ImVec4 customA{ 0.55f, 0.42f, 1.0f, 1.0f };
    ImVec4 customB{ 0.25f, 0.85f, 1.0f, 1.0f };
    ImVec4 customGlow{ 0.55f, 0.45f, 1.0f, 1.0f };

    std::atomic<int> keyMenu{ 0x2D };
    std::atomic<int> keyUnload{ 0x23 };
    std::atomic<int> keyMaps{ 0x77 };
    std::atomic<int> keyShop{ 0 };
    std::atomic<int> keySkills{ 0x63 };
    std::atomic<int> keyCoins{ 0x76 };
    std::atomic<int> keyCrates{ 0x75 };
    std::atomic<int> keyVacuum{ 0x78 };
    std::atomic<int> keyPhones{ 0x79 };
    std::atomic<int> keyCameras{ 0x60 };
    std::atomic<int> keyDoors{ 0x61 };
    std::atomic<int> keyGuns{ 0x62 };
    std::atomic<int> keyModeMenu{ 0 };
    std::atomic<int> keyModeMaps{ 0 };
    std::atomic<int> keyModeShop{ 0 };
    std::atomic<int> keyModeSkills{ 0 };
    std::atomic<int> keyModeCoins{ 0 };
    std::atomic<int> keyModeCrates{ 0 };
    std::atomic<int> keyModeVacuum{ 0 };
    std::atomic<int> keyModePhones{ 0 };
    std::atomic<int> keyModeCameras{ 0 };
    std::atomic<int> keyModeDoors{ 0 };
    std::atomic<int> keyModeGuns{ 0 };

    std::string configName{ "default" };
    std::string configStatus;

    std::atomic<bool> godMode{ false };
};

inline Config g_cfg;

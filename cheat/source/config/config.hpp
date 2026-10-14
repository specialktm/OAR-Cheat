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
    std::atomic<float> timeDilation{ 1.0f };
    std::atomic<float> fov{ 90.0f };

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
    std::atomic<bool> playerChams{ false };
    std::atomic<bool> playerGlow{ false };
    std::atomic<bool> silChams{ false };
    std::atomic<bool> silChamsG{ false };
    std::atomic<bool> guardChams{ false };
    std::atomic<bool> guardGlow{ false };
    std::atomic<int> healthStyle{ 0 };
    std::atomic<int> nameMode{ 0 };
    std::atomic<bool> guardDistance{ true };
    std::atomic<bool> playerSkel{ true };
    std::atomic<bool> guardSkel{ true };
    std::atomic<bool> playerHolding{ false };
    std::atomic<bool> guardHealth{ true };

    std::atomic<bool> visCheck{ true };
    std::atomic<bool> hideDead{ false };

    ImVec4 playerCol{ 0.35f, 1.0f, 0.45f, 1.0f };
    ImVec4 guardCol{ 1.0f, 0.67f, 0.24f, 1.0f };
    ImVec4 itemCol{ 0.51f, 0.78f, 1.0f, 1.0f };
    ImVec4 moneyCol{ 0.47f, 1.0f, 0.51f, 1.0f };
    ImVec4 healthCol{ 0.35f, 0.95f, 0.55f, 1.0f };
    ImVec4 npcCol{ 0.71f, 0.39f, 1.0f, 1.0f };
    ImVec4 deadCol{ 0.47f, 0.47f, 0.47f, 1.0f };
    ImVec4 silHiddenP{ 0.45f, 0.20f, 0.90f, 1.0f };
    ImVec4 silHiddenG{ 0.90f, 0.25f, 0.20f, 1.0f };

    std::atomic<float> wmX{ -1.0f };
    std::atomic<float> wmY{ -1.0f };

    std::atomic<bool> showModelViewer{ false };

    std::atomic<unsigned long long> modelSeenMs{ 0 };

    std::atomic<bool> espEnabled{ true };
    std::atomic<float> espTextScale{ 1.0f };

    std::atomic<bool> autoFreeShop{ false };

    std::atomic<bool> watermark{ true };
    std::atomic<bool> clock24h{ true };
    std::atomic<bool> particles{ false };

    std::atomic<bool> blurBackdrop{ true };
    std::atomic<float> blurAmount{ 0.5f };
    std::atomic<bool> borderFx{ false };
    std::atomic<bool> borderRainbow{ false };
    std::atomic<bool> keybindList{ true };
    std::atomic<bool> objClassesOnly{ false };
    std::atomic<bool> playerCountBox{ true };
    std::atomic<bool> crosshair{ false };
    std::atomic<float> pcX{ -1.0f };
    std::atomic<float> pcY{ -1.0f };
    std::atomic<bool> killfeed{ true };
    std::atomic<bool> hitlog{ true };
    std::atomic<bool> glassBlur{ true };
    ImVec4 glassTint{ 0.10f, 0.13f, 0.19f, 1.0f };
    std::atomic<bool> radar{ true };
    std::atomic<float> radarRange{ 60.0f };
    std::atomic<float> kbX{ -1.0f };
    std::atomic<float> kbY{ -1.0f };
    std::atomic<float> logX{ -1.0f };
    std::atomic<float> logY{ -1.0f };
    std::atomic<float> killX{ -1.0f };
    std::atomic<float> killY{ -1.0f };

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
    std::atomic<int> keyComplete{ 0 };
    std::atomic<int> keyWingame{ 0 };
    std::atomic<int> keyAmmo{ 0 };
    std::atomic<int> keyXp{ 0 };
    std::atomic<int> keyImmunity{ 0 };
    std::atomic<int> keyDance{ 0 };
    std::atomic<int> keyKickself{ 0 };
    std::atomic<int> keyFloppy{ 0 };
    std::atomic<int> keyCops{ 0 };
    std::atomic<int> keyRandmask{ 0 };
    std::atomic<int> keyEndtimer{ 0 };
    std::atomic<int> keyCheats{ 0 };
    std::atomic<int> keyGhost{ 0 };
    std::atomic<int> keyPlayersonly{ 0 };
    std::atomic<int> keyDebugcam{ 0 };
    std::atomic<int> keyDepthchams{ 0 };
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
    std::atomic<int> keyModeComplete{ 0 };
    std::atomic<int> keyModeWingame{ 0 };
    std::atomic<int> keyModeAmmo{ 0 };
    std::atomic<int> keyModeXp{ 0 };
    std::atomic<int> keyModeImmunity{ 0 };
    std::atomic<int> keyModeDance{ 0 };
    std::atomic<int> keyModeKickself{ 0 };
    std::atomic<int> keyModeFloppy{ 0 };
    std::atomic<int> keyModeCops{ 0 };
    std::atomic<int> keyModeRandmask{ 0 };
    std::atomic<int> keyModeEndtimer{ 0 };
    std::atomic<int> keyModeCheats{ 0 };
    std::atomic<int> keyModeGhost{ 0 };
    std::atomic<int> keyModePlayersonly{ 0 };
    std::atomic<int> keyModeDebugcam{ 0 };
    std::atomic<int> keyModeDepthchams{ 0 };

    std::string configName{ "default" };
    std::string configStatus;

    std::atomic<bool> godMode{ false };
};

inline Config g_cfg;

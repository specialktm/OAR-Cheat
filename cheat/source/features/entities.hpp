#pragma once
#include "pch/pch.h"
#include <string>
#include <vector>

namespace esp {

struct PlayerInfo {
    void* actor = nullptr;
    std::string name;
    int hp = 0, maxHp = 100;
    float distM = 0.0f;
    bool downed = false, visible = true, self = false;
    float x = 0, y = 0, z = 0;
};

struct GuardInfo {
    void* actor = nullptr;
    bool guard = false;
    int hp = 0;
    float distM = 0.0f;
    bool dead = false, alerted = false, armed = false, visible = true;
    float x = 0, y = 0, z = 0;
};

struct LootInfo {
    void* actor = nullptr;
    std::string label;
    float distM = 0.0f;
    bool money = false;
    int value = 0;
    float x = 0, y = 0, z = 0;
};

class playerManager {
public:
    playerManager() = delete;
    static void rebuild(const std::vector<void*>& actors, void* selfPawn);
    static const std::vector<PlayerInfo>& players();
    static const std::vector<GuardInfo>& npcs();
    static int selected();
    static void setSelected(int i);
    static const PlayerInfo* selectedPlayer();
};

class itemManager {
public:
    itemManager() = delete;
    static void rebuild(const std::vector<void*>& actors);
    static const std::vector<LootInfo>& items();
};

}

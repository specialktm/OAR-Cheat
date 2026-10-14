#pragma once
#include <cstdint>
#include <atomic>

namespace shm {

inline constexpr uint32_t MAGIC = 0x5352414F;
inline constexpr uint32_t VERSION = 1;
inline constexpr const char* MAP_NAME = "OARCheatShmV1";
inline constexpr int MAX_P = 64;
inline constexpr int MAX_G = 96;
inline constexpr int MAX_L = 256;
inline constexpr int MAX_Q = 32;

struct Vec3 { float x = 0, y = 0, z = 0; };

struct Player {
    char name[32] = {};
    Vec3 pos{};
    int32_t hp = 0, maxhp = 100;
    float dist = 0;
    uint32_t flags = 0;
};

struct Guard {
    Vec3 pos{};
    int32_t hp = 0;
    float dist = 0;
    uint32_t flags = 0;
};

struct Loot {
    char label[48] = {};
    Vec3 pos{};
    int32_t value = 0;
    uint32_t flags = 0;
};

struct Snap {
    uint32_t magic = 0, ver = 0, frame = 0;
    uint32_t np = 0, ng = 0, nl = 0;
    uint64_t tickMs = 0;
    Player p[MAX_P];
    Guard g[MAX_G];
    Loot l[MAX_L];
};

enum CmdType : uint32_t {
    C_VACUUM = 1, C_PHONES, C_CAMERAS, C_DOORS, C_GUNS, C_GOD, C_XP,
    C_IMMUNITY, C_DANCE, C_FLOPPY, C_COPS, C_MASK, C_TIMER, C_HEIST,
    C_SLOMO, C_FOV, C_SUMMON, C_GHOST, C_FREEZE, C_DEBUGCAM, C_DEPTH
};

struct Cmd {
    uint32_t type = 0;
    int32_t a = 0;
    float b = 0;
    char text[64] = {};
};

struct CmdQ {
    std::atomic<uint32_t> head{ 0 }, tail{ 0 };
    Cmd cmds[MAX_Q];
};

struct Result {
    uint32_t frame = 0;
    char line[128] = {};
};

struct Toggles {
    uint32_t esp = 1, chamsP = 0, chamsG = 0, meshP = 0, meshG = 0;
    uint32_t glowP = 0, glowG = 0, skelP = 1, skelG = 1;
};

struct Region {
    Snap snap;
    CmdQ q;
    Result res;
    Toggles tog;
};

inline constexpr size_t REGION_SIZE = sizeof(Region);

}

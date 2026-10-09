#pragma once
#include "pch/pch.h"

namespace actors {

struct Snapshot {

    std::vector<void*> list;
};

Snapshot copy_level_actors();
void* local_player_controller();
void* local_pawn();
void* game_world();
bool world_stable(uint64_t quietMs);

template<typename T>
inline bool is_default_object(T* o);

}

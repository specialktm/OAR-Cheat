#pragma once
#include "pch/pch.h"

// Fast, correct actor access. Prefer UWorld->PersistentLevel->Actors over
// GObjects scans (old utility::FindActors iterated 39k+ GObjects every tick,
// had a by-value vector bug so it always returned empty, and touched
// non-actors). GObjects scan is kept only as fallback.
//
// Include order matters for build speed: this header is SDK-free.
// The .cpp that uses it includes only the needed SDK headers.
namespace actors {

struct Snapshot {
    // raw AActor* pointers captured under a short lock-free copy.
    // We store as void* here to stay SDK-free in headers.
    std::vector<void*> list;
};

// Implemented in actor_iter.cpp (needs Engine_classes.hpp).
Snapshot copy_level_actors();
void* local_player_controller();
void* local_pawn();
void* game_world();

template<typename T>
inline bool is_default_object(T* o);

} // namespace actors

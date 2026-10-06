#include "pch/pch.h"
#include "core/actor_iter.hpp"
#include "core/memory.hpp"

// Only heavy SDK header in this TU. Do NOT include SDK.hpp (1000+ headers).
#include "SDK/Basic.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/CoreUObject_classes.hpp"

namespace actors {

// SEH leaf helpers: no vector/string locals, so /EHsc accepts __try.
static SDK::UWorld* seh_get_world_sdk() {
    __try { return SDK::UWorld::GetWorld(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static void* seh_read_ptr(void* const* addr) {
    __try { return *addr; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static SDK::ULevel* seh_persistent(SDK::UWorld* w) {
    __try { return w->PersistentLevel; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static SDK::UGameInstance* seh_owning_gi(SDK::UWorld* w) {
    __try { return w->OwningGameInstance; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static bool seh_is_default(SDK::AActor* a, bool& out) {
    __try { out = a->IsDefaultObject(); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
struct LevelArr { SDK::AActor* const* data; int n; bool ok; };
static LevelArr seh_level_arr(SDK::ULevel* lvl) {
    LevelArr r{ nullptr, 0, false };
    __try {
        auto& arr = lvl->Actors;
        int n = arr.Num();
        if (n <= 0 || n > 20000) return r;
        SDK::AActor* const* d = arr.GetDataPtr();
        if (!d) return r;
        r.data = d; r.n = n; r.ok = true;
        return r;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return r; }
}
static SDK::AActor* seh_elem(SDK::AActor* const* d, int i) {
    __try { return d[i]; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static void* seh_lp_pc(void* lpRaw) {
    __try { return reinterpret_cast<SDK::UPlayer*>(lpRaw)->PlayerController; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static void* seh_ack_pawn(SDK::APlayerController* pc) {
    __try { return pc->AcknowledgedPawn; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

void* game_world() {
    uintptr_t addr = mem::image_base() + mem::gworld_rva();
    void* w = seh_read_ptr(reinterpret_cast<void* const*>(addr));
    if (IS_VALID_PTR(w)) return w;
    return seh_get_world_sdk();
}

Snapshot copy_level_actors() {
    Snapshot s;
    auto* world = reinterpret_cast<SDK::UWorld*>(game_world());
    if (!IS_VALID_PTR(world)) return s;
    auto* level = seh_persistent(world);
    if (!IS_VALID_PTR(level)) return s;
    LevelArr arr = seh_level_arr(level);
    if (!arr.ok) return s;
    s.list.reserve(static_cast<size_t>(arr.n));
    for (int i = 0; i < arr.n; ++i) {
        SDK::AActor* a = seh_elem(arr.data, i);
        if (!IS_VALID_PTR(a)) continue;
        bool isDef = false;
        if (!seh_is_default(a, isDef) || isDef) continue;
        s.list.push_back(a);
    }
    return s;
}

void* local_player_controller() {
    auto* world = reinterpret_cast<SDK::UWorld*>(game_world());
    if (!IS_VALID_PTR(world)) return nullptr;
    auto* gi = seh_owning_gi(world);
    if (!IS_VALID_PTR(gi)) return nullptr;
    SDK::ULocalPlayer* lp = nullptr;
    __try {
        auto& players = gi->LocalPlayers;
        if (!players.IsValid()) return nullptr;
        if (players.Num() <= 0) return nullptr;
        auto* d = players.GetDataPtr();
        if (!d) return nullptr;
        lp = d[0];
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
    if (!IS_VALID_PTR(lp)) return nullptr;
    return seh_lp_pc(lp);
}

void* local_pawn() {
    auto* pc = reinterpret_cast<SDK::APlayerController*>(local_player_controller());
    if (!IS_VALID_PTR(pc)) return nullptr;
    return seh_ack_pawn(pc);
}

} // namespace actors

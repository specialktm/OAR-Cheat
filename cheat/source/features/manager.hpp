#pragma once
#include "pch/pch.h"
// Feature manager: every one-shot/auto capability registers here once.
// UI tabs enumerate the registry instead of hardcoding feature lists,
// so adding a feature never touches menu code.
namespace feat {

struct Entry {
    const char* id;          // stable key, e.g. "maps"
    const char* name;        // button label
    const char* desc;        // sub-label shown under the button
    const char* hotkey;      // display only, e.g. "F8" ("" if none)
    const char* tab;         // owning tab id: "shop" | "world"
    void (*run)();           // one-shot action (never null)
    std::atomic<bool>* autoFlag; // nullable: when set, tick_autos runs it
};

void register_builtin();                 // idempotent; called from cheat init
void add(Entry e);
const std::vector<const Entry*>& by_tab(const char* tab);
const std::vector<const Entry*>& all();
// Editable keybind for an entry id (null if none). Render thread + worker tick.
std::atomic<int>* key_for(const char* id);
bool run(const char* id);
void tick_autos();                       // throttled; call every tick

// Deferred actions: menu buttons/drag-drop run on the render thread, but
// feature bodies issue ProcessEvent calls that can fault on stale objects.
// A fault inside an ImGui window corrupts the Begin/End stack (fatal at
// EndFrame), so UI-triggered work is queued here and drained on the worker
// tick where safe_tick's __try can absorb it without UI state present.
void queue_run(const char* id);
void queue_fn(std::function<void()> fn);
void run_queued();                       // worker tick drains (each SEH-guarded)

} // namespace feat

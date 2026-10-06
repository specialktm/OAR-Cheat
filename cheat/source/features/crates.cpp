#include "pch/pch.h"
#include "features/crates.hpp"
#include "core/actor_iter.hpp"
#include "core/veh.hpp"
#include "SDK/Basic.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/BP_CrateManager_classes.hpp"
#include "SDK/BP_Crate_Base_classes.hpp"
#include "SDK/RobberController_classes.hpp"
#include "logger/logger.hpp"

namespace crates {

static char g_line[160] = "idle";
static std::mutex g_mtx;
static void set_line(const char* fmt, int a, int b, int c) {
    std::lock_guard<std::mutex> lk(g_mtx);
    snprintf(g_line, sizeof(g_line), fmt, a, b, c);
}
const char* last_line() {
    thread_local char buf[160] = "idle";
    std::lock_guard<std::mutex> lk(g_mtx);
    memcpy(buf, g_line, sizeof(buf));
    buf[sizeof(buf) - 1] = 0;
    return buf;
}

// ---- SEH leaves (no string/vector locals) ----
static int seh_gnum() {
    __try { return SDK::UObject::GObjects->Num(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
static SDK::UObject* seh_gat(int i) {
    __try { return SDK::UObject::GObjects->GetByIndex(i); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static SDK::UClass* seh_uclass_static() {
    __try { return SDK::UClass::StaticClass(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static bool seh_is_uclass(SDK::UObject* o, SDK::UClass* uclass, bool& out) {
    __try { out = o->IsA(uclass); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_isa_actor(SDK::AActor* a, SDK::UClass* cls, bool& out) {
    __try { out = a->IsA(cls); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static SDK::UClass* seh_crate_base() {
    __try { return SDK::ABP_Crate_Base_C::StaticClass(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
// needs BP_Crate_Base type only for StaticClass; forward via UClass name walk instead
static SDK::UClass* seh_mgr_static() {
    __try { return SDK::ABP_CrateManager_C::StaticClass(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static bool seh_issub(SDK::UClass* cls, SDK::UClass* base, bool& out) {
    __try {
        for (SDK::UStruct* s = cls; s; s = s->Super) {
            if (s == base) { out = true; return true; }
        }
        out = false; return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_spawn(SDK::ABP_CrateManager_C* m, SDK::UClass* cls) {
    __try { m->SpawnCrate(cls); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_open(SDK::ABP_CrateManager_C* m) {
    __try { m->OpenCrate(); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static SDK::ARobberController_C* seh_local_rc() {
    __try {
        auto* pc = reinterpret_cast<SDK::ARobberController_C*>(actors::local_player_controller());
        return IS_VALID_PTR(pc) ? pc : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static bool seh_additem(SDK::ARobberController_C* pc, SDK::UClass* cls) {
    __try { int idx = 0; pc->AddInventoryItem(cls, &idx); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_save_inv(SDK::ARobberController_C* pc) {
    __try { pc->SaveInventoryItems(); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

std::vector<CrateInfo> list_crates() {
    std::vector<CrateInfo> out;
    int n = seh_gnum();
    if (n <= 0 || n > 600000) return out;
    SDK::UClass* uclass = seh_uclass_static();
    SDK::UClass* base = seh_crate_base();
    out.reserve(8);
    for (int i = 0; i < n; ++i) {
        SDK::UObject* o = seh_gat(i);
        if (!IS_VALID_PTR(o)) continue;
        if (IS_VALID_PTR(uclass)) {
            bool isa = false;
            if (!seh_is_uclass(o, uclass, isa) || !isa) continue;
        }
        std::string nm = veh::get_name(o);
        if (nm.rfind("BP_Crate_", 0) != 0) continue;
        if (nm == "BP_Crate_Base_C" || nm == "BP_CrateManager_C") continue;
        auto* cls = static_cast<SDK::UClass*>(o);
        if (IS_VALID_PTR(base)) {
            bool sub = false;
            if (!seh_issub(cls, base, sub) || !sub) continue;
        }
        CrateInfo ci;
        ci.name = std::move(nm);
        ci.cls = cls;
        out.push_back(std::move(ci));
    }
    return out;
}

void open_all() {
    auto classes = list_crates();
    // manager actors live in the level (same fast path as doors/cams)
    std::vector<SDK::ABP_CrateManager_C*> mgrs;
    {
        auto snap = actors::copy_level_actors();
        SDK::UClass* mcls = seh_mgr_static();
        for (void* r : snap.list) {
            auto* a = reinterpret_cast<SDK::AActor*>(r);
            if (!IS_VALID_PTR(a)) continue;
            bool ok = false;
            if (IS_VALID_PTR(mcls)) {
                if (!seh_isa_actor(a, mcls, ok) || !ok) continue;
            } else continue;
            mgrs.push_back(static_cast<SDK::ABP_CrateManager_C*>(a));
        }
    }
    if (mgrs.empty()) {
        set_line("no CrateManager in level - open a heist (%d classes)", (int)classes.size(), 0, 0);
        if (g_logger) g_logger->send(levels::warn,
            "Crates: {} classes found but no CrateManager actor in this level", (int)classes.size());
        return;
    }
    int opened = 0, granted = 0;
    auto* pc = seh_local_rc();
    for (auto* m : mgrs) {
        if (!IS_VALID_PTR(m)) continue;
        for (auto& c : classes) {
            auto* cls = static_cast<SDK::UClass*>(c.cls);
            if (!IS_VALID_PTR(cls)) continue;
            // inventory first, then the visual open (matches request order)
            if (IS_VALID_PTR(pc) && seh_additem(pc, cls)) ++granted;
            if (!seh_spawn(m, cls)) continue;
            if (seh_open(m)) ++opened;
        }
    }
    if (IS_VALID_PTR(pc)) seh_save_inv(pc);
    set_line("%d granted, %d opened via %d managers", granted, opened, (int)mgrs.size());
    if (g_logger) g_logger->send(levels::success,
        "Crates: {} classes, {} granted to inventory, {} opened via {} manager(s)",
        (int)classes.size(), granted, opened, (int)mgrs.size());
}

} // namespace crates

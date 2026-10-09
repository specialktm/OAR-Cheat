#include "pch/pch.h"
#include "features/modelview.hpp"
#include "features/capture.hpp"
#include "config/config.hpp"
#include "world/actor_iter.hpp"
#include "util/veh.hpp"
#include "SDK/Basic.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/PlayerCharacter_classes.hpp"
#include "SDK/NPCBase_classes.hpp"

#include <cstdio>

#include "../../vendor/imgui/imgui.h"
#include "ui/menu.hpp"

namespace modelview {

static bool seh_loc(SDK::AActor* a, SDK::FVector& out) {
    __try { out = a->K2_GetActorLocation(); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
template<typename T>
static bool seh_isa(SDK::AActor* a, bool& out) {
    __try { out = a->IsA(T::StaticClass()); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static void* g_target = nullptr;
static uint64_t g_pickTs = 0;
static char g_name[64] = { 0 };

static void* pick_target() {
    uint64_t now = GetTickCount64();
    if (g_target && now - g_pickTs < 1000 && IS_VALID_PTR(g_target)) return g_target;
    g_target = nullptr;
    g_pickTs = now;
    auto snap = actors::copy_level_actors();
    void* self = actors::local_pawn();
    SDK::FVector me{};
    bool haveMe = IS_VALID_PTR(self) && seh_loc(static_cast<SDK::AActor*>(self), me);
    float best = 1e30f;
    for (void* r : snap.list) {
        auto* a = static_cast<SDK::AActor*>(r);
        if (!IS_VALID_PTR(a) || a == self) continue;
        bool p = false, n = false;
        if (!seh_isa<SDK::APlayerCharacter_C>(a, p)) continue;
        if (!p && !seh_isa<SDK::ANPCBase_C>(a, n)) continue;
        if (!p && !n) continue;
        SDK::FVector loc{};
        if (!seh_loc(a, loc)) continue;
        float d = haveMe ? (loc.X - me.X) * (loc.X - me.X) + (loc.Y - me.Y) * (loc.Y - me.Y)
                         : 1.0f;
        if (d < best) { best = d; g_target = a; }
    }
    if (IS_VALID_PTR(g_target)) {
        std::string nm = veh::get_name(static_cast<SDK::UObject*>(g_target));
        snprintf(g_name, sizeof(g_name), "%s", nm.empty() ? "?" : nm.c_str());
    } else g_name[0] = 0;
    return g_target;
}

void draw_live_model(ImVec2 origin, ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y),
        ui::fade(IM_COL32(8, 10, 18, 255)), 0.0f);

    void* a = pick_target();
    if (IS_VALID_PTR(a)) {
        g_cfg.modelSeenMs.store(GetTickCount64());
        SDK::FVector loc{};
        if (seh_loc(static_cast<SDK::AActor*>(a), loc))
            capture::set_target(loc.X, loc.Y, loc.Z);
    }

    if (capture::has_image() && capture::srv()) {
        dl->AddImage((ImTextureID)capture::srv(), origin,
            ImVec2(origin.x + size.x, origin.y + size.y), ImVec2(0, 0),
            ImVec2(1, 1), ui::fade(IM_COL32(255, 255, 255, 255)));
        if (g_name[0]) {
            ImVec2 nsz = ImGui::CalcTextSize(g_name);
            dl->AddText(ImVec2(origin.x + (size.x - nsz.x) / 2, origin.y + 8),
                ui::fade(IM_COL32(235, 238, 248, 255)), g_name);
        }
        ImVec2 lsz = ImGui::CalcTextSize("LIVE");
        dl->AddText(ImVec2(origin.x + size.x - lsz.x - 8, origin.y + 8),
            ui::fade(IM_COL32(140, 230, 170, 255)), "LIVE");
        return;
    }
    const char* msg = IS_VALID_PTR(a) ? "rendering from game..."
                                      : "no player/NPC in level";
    ImVec2 tsz = ImGui::CalcTextSize(msg);
    dl->AddText(ImVec2(origin.x + (size.x - tsz.x) / 2, origin.y + size.y / 2),
        ui::fade(IM_COL32(150, 156, 175, 255)), msg);
    dl->AddRect(origin, ImVec2(origin.x + size.x, origin.y + size.y),
        ui::fade(IM_COL32(60, 70, 100, 255)), 0.0f, 0, 1.0f);
}

}

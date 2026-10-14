#include "pch/pch.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "features/silchams.hpp"
#include "features/esp.hpp"
#include "features/entities.hpp"
#include "config/config.hpp"
#include "SDK/Engine_classes.hpp"
#include "../../vendor/imgui/imgui.h"

namespace silchams {

static ImU32 raw(ImVec4 c) {
    return IM_COL32((int)(c.x * 255.0f), (int)(c.y * 255.0f), (int)(c.z * 255.0f), 255);
}

static bool joints_of(void* actor, SDK::FVector w[18]) {
    if (!IS_VALID_PTR(actor)) return false;
    __try {
        return esp::skeleton_points(actor, w, 18) == 18;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static void draw_actor(ImDrawList* dl, void* actor, ImVec4 visCol, ImVec4 hidCol, bool visible) {
    SDK::FVector w[18];
    if (!joints_of(actor, w)) return;
    esp::ScreenPos s[18];
    for (int i = 0; i < 18; ++i) {
        if (!esp::world_to_screen(&w[i], s[i])) return;
    }
    ImVec2 p[18];
    for (int i = 0; i < 18; ++i) p[i] = ImVec2(s[i].x, s[i].y);
    float hs = sqrtf((p[5].x - p[4].x) * (p[5].x - p[4].x) + (p[5].y - p[4].y) * (p[5].y - p[4].y));
    if (!(hs > 1.0f && hs < 800.0f)) return;
    float torso = hs * 0.42f;
    if (torso < 5.0f) torso = 5.0f;
    if (torso > 16.0f) torso = 16.0f;
    float limb = torso * 0.72f;
    ImU32 fill = raw(visible ? visCol : hidCol);
    ImU32 rgb = fill & 0x00FFFFFFu;
    ImU32 glow = rgb | (26u << 24);
    ImU32 outl = IM_COL32(0, 0, 0, 170);
    static const int chains[5][6] = {
        { 0, 1, 2, 3, 4, 5 },
        { 3, 6, 7, 8, -1, -1 },
        { 3, 9, 10, 11, -1, -1 },
        { 0, 12, 13, 14, -1, -1 },
        { 0, 15, 16, 17, -1, -1 },
    };
    static const int lens[5] = { 6, 4, 4, 4, 4 };
    for (int ci = 0; ci < 5; ++ci) {
        float wd = (ci == 0) ? torso : limb;
        for (int k = 0; k + 1 < lens[ci]; ++k) {
            int a = chains[ci][k], b = chains[ci][k + 1];
            if (a < 0 || b < 0) continue;
            float dx = p[b].x - p[a].x, dy = p[b].y - p[a].y;
            if (dx * dx + dy * dy < 4.0f) continue;
            dl->AddLine(p[a], p[b], glow, wd + 7.0f);
        }
    }
    for (int ci = 0; ci < 5; ++ci) {
        float wd = (ci == 0) ? torso : limb;
        for (int k = 0; k + 1 < lens[ci]; ++k) {
            int a = chains[ci][k], b = chains[ci][k + 1];
            if (a < 0 || b < 0) continue;
            float dx = p[b].x - p[a].x, dy = p[b].y - p[a].y;
            if (dx * dx + dy * dy < 4.0f) continue;
            dl->AddLine(p[a], p[b], outl, wd + 2.5f);
        }
    }
    for (int ci = 0; ci < 5; ++ci) {
        float wd = (ci == 0) ? torso : limb;
        for (int k = 0; k + 1 < lens[ci]; ++k) {
            int a = chains[ci][k], b = chains[ci][k + 1];
            if (a < 0 || b < 0) continue;
            float dx = p[b].x - p[a].x, dy = p[b].y - p[a].y;
            if (dx * dx + dy * dy < 4.0f) continue;
            dl->AddLine(p[a], p[b], fill, wd);
        }
    }
    float hr = hs * 0.5f;
    if (hr < 4.0f) hr = 4.0f;
    if (hr > 18.0f) hr = 18.0f;
    dl->AddCircleFilled(p[5], hr + 3.5f, glow, 20);
    dl->AddCircleFilled(p[5], hr + 1.2f, outl, 20);
    dl->AddCircleFilled(p[5], hr, fill, 20);
    dl->AddCircleFilled(p[8], limb * 0.45f, fill, 10);
    dl->AddCircleFilled(p[11], limb * 0.45f, fill, 10);
    dl->AddCircleFilled(p[14], limb * 0.45f, fill, 10);
    dl->AddCircleFilled(p[17], limb * 0.45f, fill, 10);
}

void render() {
    if (!g_cfg.espEnabled.load()) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;
    float pmax = g_cfg.playerMaxDist.load();
    float gmax = g_cfg.guardMaxDist.load();
    if (g_cfg.silChams.load() && g_cfg.playerEsp.load()) {
        for (auto& e : esp::playerManager::players()) {
            if (e.self || !IS_VALID_PTR(e.actor)) continue;
            if (pmax > 0.0f && e.distM > pmax) continue;
            __try {
                draw_actor(dl, e.actor, g_cfg.playerCol, g_cfg.silHiddenP, e.visible);
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    }
    if (g_cfg.silChamsG.load() && g_cfg.guardEsp.load()) {
        for (auto& e : esp::playerManager::npcs()) {
            if (!IS_VALID_PTR(e.actor)) continue;
            if (e.dead && g_cfg.hideDead.load()) continue;
            if (gmax > 0.0f && e.distM > gmax) continue;
            ImVec4 vc = e.guard ? g_cfg.guardCol : g_cfg.npcCol;
            if (e.dead) vc = g_cfg.deadCol;
            __try {
                draw_actor(dl, e.actor, vc, g_cfg.silHiddenG, e.visible && !e.dead);
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    }
}

}

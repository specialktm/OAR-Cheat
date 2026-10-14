#include "pch/pch.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "ui/overlays.hpp"
#include "ui/take_shell.hpp"
#include "config/config.hpp"
#include "features/entities.hpp"
#include "../../vendor/imgui/imgui.h"

namespace overlays {

static ImU32 accent_raw() {
    ImVec4 acc = takeui::accentA(1.0f);
    return IM_COL32((int)(acc.x * 255.0f), (int)(acc.y * 255.0f), (int)(acc.z * 255.0f), 255);
}

void render_crosshair() {
    if (!g_cfg.crosshair.load()) return;
    if (g_cfg.menuOpen.load()) return;
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 c(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImU32 col = accent_raw();
    ImU32 dark = IM_COL32(0, 0, 0, 160);
    float gap = 6.0f, len = 9.0f;
    dl->AddLine(ImVec2(c.x - gap - len, c.y), ImVec2(c.x - gap, c.y), dark, 3.0f);
    dl->AddLine(ImVec2(c.x + gap, c.y), ImVec2(c.x + gap + len, c.y), dark, 3.0f);
    dl->AddLine(ImVec2(c.x, c.y - gap - len), ImVec2(c.x, c.y - gap), dark, 3.0f);
    dl->AddLine(ImVec2(c.x, c.y + gap), ImVec2(c.x, c.y + gap + len), dark, 3.0f);
    dl->AddLine(ImVec2(c.x - gap - len, c.y), ImVec2(c.x - gap, c.y), col, 1.5f);
    dl->AddLine(ImVec2(c.x + gap, c.y), ImVec2(c.x + gap + len, c.y), col, 1.5f);
    dl->AddLine(ImVec2(c.x, c.y - gap - len), ImVec2(c.x, c.y - gap), col, 1.5f);
    dl->AddLine(ImVec2(c.x, c.y + gap), ImVec2(c.x, c.y + gap + len), col, 1.5f);
    dl->AddCircleFilled(c, 1.6f, col, 8);
}

void render_watermark() {
}

void render_radar() {
    if (!g_cfg.radar.load()) return;
    float sx = 0, sy = 0;
    bool haveSelf = false;
    __try {
        for (auto& p : esp::playerManager::players()) {
            if (p.self && IS_VALID_PTR(p.actor)) { sx = p.x; sy = p.y; haveSelf = true; break; }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    if (!haveSelf) return;
    ImGuiIO& io = ImGui::GetIO();
    float range = g_cfg.radarRange.load();
    if (range < 10.0f) range = 10.0f;
    if (range > 200.0f) range = 200.0f;
    float sz = 170.0f;
    ImVec2 p0(io.DisplaySize.x - sz - 14.0f, 64.0f);
    ImVec2 p1(p0.x + sz, p0.y + sz);
    ImVec2 c((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);
    float half = sz * 0.5f - 6.0f;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(p0, p1, IM_COL32(22, 25, 30, 235), 8.0f);
    dl->AddRect(p0, p1, IM_COL32(46, 55, 66, 255), 8.0f);
    dl->AddLine(ImVec2(c.x - half, c.y), ImVec2(c.x + half, c.y), IM_COL32(46, 55, 66, 160), 1.0f);
    dl->AddLine(ImVec2(c.x, c.y - half), ImVec2(c.x, c.y + half), IM_COL32(46, 55, 66, 160), 1.0f);
    dl->AddCircle(c, half, IM_COL32(46, 55, 66, 200), 48, 1.0f);
    auto dot = [&](float wx, float wy, ImU32 col, float r) {
        float dx = (wx - sx) / range * half;
        float dy = (wy - sy) / range * half;
        float d2 = dx * dx + dy * dy;
        if (d2 > half * half) {
            float d = sqrtf(d2);
            dx = dx / d * half;
            dy = dy / d * half;
        }
        dl->AddCircleFilled(ImVec2(c.x + dx, c.y + dy), r, col, 10);
    };
    __try {
        for (auto& e : esp::playerManager::players()) {
            if (e.self || !IS_VALID_PTR(e.actor)) continue;
            dot(e.x, e.y, e.downed ? IM_COL32(120, 120, 120, 255) : IM_COL32(80, 255, 120, 255), 3.0f);
        }
        for (auto& e : esp::playerManager::npcs()) {
            if (!IS_VALID_PTR(e.actor) || e.dead) continue;
            dot(e.x, e.y, e.alerted ? IM_COL32(255, 70, 70, 255) : IM_COL32(255, 180, 80, 255), 3.0f);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    dl->AddRectFilled(ImVec2(c.x - 4, c.y - 4), ImVec2(c.x + 4, c.y + 4), IM_COL32(255, 255, 255, 255), 1.0f);
}

void render_all() {
    render_crosshair();
    render_radar();
}

}

#include "pch/pch.h"
#include "ui/theme.hpp"
#include "config/config.hpp"

namespace theme {

static const Theme kThemes[] = {
    { "Mono Purple",  ImVec4(0.64f, 0.00f, 1.00f, 1.0f), ImVec4(0.38f, 0.00f, 0.75f, 1.0f), ImVec4(0.64f, 0.10f, 1.00f, 1.0f) },
    { "Gold",         ImVec4(0.91f, 0.72f, 0.14f, 1.0f), ImVec4(0.85f, 0.45f, 0.10f, 1.0f), ImVec4(0.95f, 0.78f, 0.25f, 1.0f) },
    { "OS Blue",      ImVec4(0.21f, 0.66f, 0.96f, 1.0f), ImVec4(0.12f, 0.42f, 0.85f, 1.0f), ImVec4(0.25f, 0.70f, 1.0f, 1.0f) },
    { "OAR Violet",   ImVec4(0.55f, 0.42f, 1.0f, 1.0f), ImVec4(0.25f, 0.85f, 1.0f, 1.0f), ImVec4(0.55f, 0.45f, 1.0f, 1.0f) },
    { "Cyan Ops",     ImVec4(0.20f, 0.75f, 1.0f, 1.0f), ImVec4(0.35f, 1.0f, 0.85f, 1.0f), ImVec4(0.30f, 0.80f, 1.0f, 1.0f) },
    { "Emerald",      ImVec4(0.20f, 0.80f, 0.45f, 1.0f), ImVec4(0.55f, 1.0f, 0.45f, 1.0f), ImVec4(0.30f, 0.90f, 0.55f, 1.0f) },
    { "Crimson",      ImVec4(1.0f, 0.30f, 0.38f, 1.0f), ImVec4(1.0f, 0.60f, 0.25f, 1.0f), ImVec4(1.0f, 0.35f, 0.40f, 1.0f) },
    { "Sunset Amber", ImVec4(1.0f, 0.62f, 0.20f, 1.0f), ImVec4(1.0f, 0.35f, 0.55f, 1.0f), ImVec4(1.0f, 0.65f, 0.30f, 1.0f) },
    { "Ghost Mono",   ImVec4(0.75f, 0.78f, 0.88f, 1.0f), ImVec4(0.45f, 0.50f, 0.62f, 1.0f), ImVec4(0.80f, 0.82f, 0.92f, 1.0f) },
};

int count() { return (int)(sizeof(kThemes) / sizeof(kThemes[0])) + 1; }

static Theme g_custom{ "Custom", {}, {}, {} };

const Theme& get(int i) {
    if (i < 0) i = 0;
    if (i >= count()) i = 0;
    if (i == (int)(sizeof(kThemes) / sizeof(kThemes[0]))) {
        g_custom.a = g_cfg.customA;
        g_custom.b = g_cfg.customB;
        g_custom.glow = g_cfg.customGlow;
        return g_custom;
    }
    return kThemes[i];
}

int current() {
    int i = g_cfg.themeIdx.load();
    if (i < 0 || i >= count()) return 0;
    return i;
}

void set_current(int i) {
    if (i < 0 || i >= count()) return;
    g_cfg.themeIdx.store(i);
    apply_current();
}

static ImVec4 with_a(ImVec4 c, float a) { c.w = a; return c; }

ImVec4 accentA(float a) { return with_a(get(current()).a, a); }
ImVec4 accentB(float a) { return with_a(get(current()).b, a); }

void apply_current() {
    const Theme& t = get(current());
    auto& s = ImGui::GetStyle();

    s.Colors[ImGuiCol_WindowBg] = ImColor(60, 65, 80, 60);
    s.Colors[ImGuiCol_ChildBg] = ImColor(20, 20, 20, 255);
    s.Colors[ImGuiCol_PopupBg] = ImColor(26, 26, 26, 255);
    s.Colors[ImGuiCol_Text] = ImColor(120, 120, 120, 255);
    s.Colors[ImGuiCol_TextDisabled] = ImColor(100, 100, 100, 255);
    s.Colors[ImGuiCol_Border] = ImColor(28, 28, 28, 255);
    s.Colors[ImGuiCol_TextSelectedBg] = ImColor(25, 22, 33, 100);

    s.Colors[ImGuiCol_ScrollbarGrab] = ImColor(80, 80, 90, 255);
    s.Colors[ImGuiCol_ScrollbarGrabHovered] = ImColor(120, 120, 135, 255);
    s.Colors[ImGuiCol_ScrollbarGrabActive] = ImColor(140, 140, 155, 255);
    s.Colors[ImGuiCol_FrameBg] = ImColor(16, 16, 16, 255);
    s.Colors[ImGuiCol_FrameBgHovered] = ImColor(18, 18, 18, 255);
    s.Colors[ImGuiCol_FrameBgActive] = ImColor(18, 18, 18, 255);
    s.Colors[ImGuiCol_Button] = ImColor(26, 26, 26, 255);
    s.Colors[ImGuiCol_ButtonHovered] = ImColor(30, 30, 30, 255);
    s.Colors[ImGuiCol_ButtonActive] = ImColor(33, 33, 33, 255);
    s.Colors[ImGuiCol_Header] = ImColor(26, 26, 26, 255);
    s.Colors[ImGuiCol_HeaderHovered] = ImColor(38, 38, 38, 255);
    s.Colors[ImGuiCol_HeaderActive] = ImColor(48, 48, 48, 255);
    s.Colors[ImGuiCol_Separator] = ImColor(28, 28, 28, 255);
    s.Colors[ImGuiCol_CheckMark] = ImColor(255, 255, 255, 255);

    s.WindowBorderSize = 0;
    s.WindowPadding = ImVec2(0, 0);
    s.WindowRounding = 5.f;
    s.PopupBorderSize = 0.f;
    s.PopupRounding = 5.f;
    s.ChildRounding = 7;
    s.ChildBorderSize = 1.f;
    s.FrameBorderSize = 1.0f;
    s.ScrollbarSize = 3.0f;
    s.FrameRounding = 5.f;
    s.ItemSpacing = ImVec2(0, 12);
    s.ItemInnerSpacing = ImVec2(10, 0);

    ImVec4 a = get(current()).a;
    ImVec4 b = get(current()).b;
    (void)a;
    (void)b;
    (void)t;
}

}

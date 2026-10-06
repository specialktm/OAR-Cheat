#include "pch/pch.h"
#include "ui/theme.hpp"
#include "core/config.hpp"

namespace theme {

static const Theme kThemes[] = {
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
    s.WindowRounding = 6.0f;
    s.ChildRounding = 5.0f;
    s.FrameRounding = 4.0f;
    s.GrabRounding = 4.0f;
    s.ScrollbarRounding = 5.0f;
    s.TabRounding = 0.0f;
    s.WindowBorderSize = 1.0f;
    s.ChildBorderSize = 1.0f;
    s.FrameBorderSize = 0.0f;
    s.WindowPadding = ImVec2(0, 0);
    s.FramePadding = ImVec2(12, 8);
    s.ItemSpacing = ImVec2(10, 8);
    s.ItemInnerSpacing = ImVec2(8, 6);
    s.ScrollbarSize = 10.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = ImVec4(0.93f, 0.94f, 0.97f, 1.0f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.58f, 0.62f, 0.72f, 1.0f);
    c[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.06f, 0.085f, 0.0f); // transparent: blur image IS the bg
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = ImVec4(0.09f, 0.10f, 0.14f, 1.0f);
    c[ImGuiCol_Border] = ImVec4(t.a.x * 0.6f, t.a.y * 0.6f, t.a.z * 0.6f, 0.45f);
    c[ImGuiCol_FrameBg] = ImVec4(0.13f, 0.14f, 0.20f, 1.0f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.17f, 0.18f, 0.27f, 1.0f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.21f, 0.32f, 1.0f);
    c[ImGuiCol_CheckMark] = t.b;
    c[ImGuiCol_SliderGrab] = t.a;
    c[ImGuiCol_SliderGrabActive] = t.b;
    c[ImGuiCol_Button] = ImVec4(t.a.x * 0.45f, t.a.y * 0.45f, t.a.z * 0.7f, 1.0f);
    c[ImGuiCol_ButtonHovered] = ImVec4(t.a.x * 0.7f, t.a.y * 0.7f, t.a.z * 0.95f, 1.0f);
    c[ImGuiCol_ButtonActive] = t.b;
    c[ImGuiCol_Header] = ImVec4(t.a.x * 0.5f, t.a.y * 0.5f, t.a.z * 0.8f, 0.9f);
    c[ImGuiCol_HeaderHovered] = ImVec4(t.a.x * 0.7f, t.a.y * 0.7f, t.a.z, 1.0f);
    c[ImGuiCol_HeaderActive] = t.b;
    c[ImGuiCol_Separator] = ImVec4(t.a.x * 0.6f, t.a.y * 0.6f, t.a.z * 0.6f, 0.35f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(t.a.x * 0.6f, t.a.y * 0.6f, t.a.z * 0.8f, 0.8f);
    c[ImGuiCol_ScrollbarGrabHovered] = t.a;
    c[ImGuiCol_ScrollbarGrabActive] = t.b;
}

} // namespace theme

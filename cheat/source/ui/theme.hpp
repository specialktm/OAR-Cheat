#pragma once
#include "pch/pch.h"
#include "../../vendor/imgui/imgui.h"
// Theme system: named palettes applied as a whole (not just colors -
// rounding, accents, glows). Selected from the Settings tab.
namespace theme {

struct Theme {
    const char* name;
    ImVec4 a;      // primary accent (violet slot)
    ImVec4 b;      // secondary accent (cyan slot)
    ImVec4 glow;   // hover glow tint
};

int count();
const Theme& get(int i);
int current();
void set_current(int i);
void apply_current();   // apply_current reads g_cfg.themeIdx
ImVec4 accentA(float a = 1.0f);
ImVec4 accentB(float a = 1.0f);

} // namespace theme

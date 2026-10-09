#pragma once
#include "pch/pch.h"
#include "../../vendor/imgui/imgui.h"

namespace theme {

struct Theme {
    const char* name;
    ImVec4 a;
    ImVec4 b;
    ImVec4 glow;
};

int count();
const Theme& get(int i);
int current();
void set_current(int i);
void apply_current();
ImVec4 accentA(float a = 1.0f);
ImVec4 accentB(float a = 1.0f);

}

#pragma once
#include "pch/pch.h"
#include "../../vendor/imgui/imgui.h"

extern ImFont* tab_text2;
extern ImFont* tab_text3;
extern float dpi_scale;

namespace ui {
void apply_modern_style();
void render();
void render_backdrop();
float visibility();

inline ImU32 fade(ImU32 col) {
    float v = visibility();
    unsigned a = (unsigned)(((col >> 24) & 0xFFu) * v);
    return (col & 0x00FFFFFFu) | (a << 24);
}
}

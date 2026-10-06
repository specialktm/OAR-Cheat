#pragma once
#include "pch/pch.h"
namespace ui {
void apply_modern_style(); // = theme::apply_current (kept for hook compat)
void render();
void render_backdrop();    // blur pass or gradient fallback, called first
} // namespace ui

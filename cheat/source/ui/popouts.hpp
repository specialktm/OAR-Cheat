#pragma once
#include "pch/pch.h"
// Detached popout windows (used instead of embedded previews).
namespace popouts {
void render_all(); // called after the main window; each popout self-gates
} // namespace popouts

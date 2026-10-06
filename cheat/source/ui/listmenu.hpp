#pragma once
#include "pch/pch.h"
// GTA-style list UI: narrow column, NUM8/2 navigate, NUM5 select,
// NUM4/6 adjust, NUM0 back. Rendered on the Present thread; actions are
// queued for the worker tick so heavy scans never hitch the frame.
namespace listui {
void render();        // draw panel (menu open + list style)
void handle_keys();   // edge + repeat nav (call every Present, render thread)
} // namespace listui

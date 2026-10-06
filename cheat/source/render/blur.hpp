#pragma once
#include "pch/pch.h"
// Real frosted-glass backdrop: quarter-res separable gaussian blur of the
// live backbuffer, composited under the menu. Any failure returns false and
// the menu falls back to the gradient backdrop. Zero cost when menu closed.
namespace blur {
void on_resize();                        // release sized targets (resize path)
bool backdrop(void* swapchain);          // copy + blur + AddImage fullscreen
bool backdrop_region(void* swapchain, float x, float y, float w, float h);
bool ready();
void shutdown();
} // namespace blur

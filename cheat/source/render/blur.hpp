#pragma once
#include "pch/pch.h"

namespace blur {
void on_resize();
bool backdrop(void* swapchain);
bool backdrop_region(void* swapchain, float x, float y, float w, float h);
bool ready();
void set_radius(float r);
float radius();
void set_alpha(float a);
float alpha();
void shutdown();
}

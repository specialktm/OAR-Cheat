#pragma once
#include "pch/pch.h"

namespace dx11 {
bool init();
void shutdown();
bool is_ready();
void* swapchain();
void* device();
void* title_font();
void* icon_font();
void* get_rtv();
void* ctx();
void* present_target();
bool present_hooked();
void* resize_target();
bool resize_hooked();
bool is_unloading();
}

#pragma once
#include "pch/pch.h"
// DX11 Present hook (evidence: dump exe has d3d11/DXGI/Present/SwapChain strings).
// Dummy-device vtable steal -> MinHook on Present[8] / ResizeBuffers[13].
namespace dx11 {
bool init();
void shutdown();
bool is_ready();
void* swapchain();
void* present_target();
bool present_hooked();
void* resize_target();
bool resize_hooked();
bool is_unloading();
} // namespace dx11

#pragma once
#include "pch/pch.h"
// Live textured 3D capture: a hidden SceneCapture2D renders the target,
// ExportRenderTarget drops a PNG, WIC decodes it on the worker thread, and
// the render thread uploads it for AddImage. Zero asset ripping, all runtime.
// Degrades gracefully (null SRV) if any engine call is missing at runtime.
namespace capture {
void set_active(bool on);                 // any thread
void set_target(float x, float y, float z); // render thread (rig pick)
void tick();                              // worker thread (throttled inside)
void present_upload();                    // render thread in hkPresent
void* srv();                              // render thread, may be null
bool has_image();
void shutdown();                          // worker thread at unload
} // namespace capture

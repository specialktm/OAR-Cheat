#pragma once
#include "pch/pch.h"

namespace capture {
void set_active(bool on);
void set_target(float x, float y, float z);
void tick();
void present_upload();
void* srv();
bool has_image();
void shutdown();
}

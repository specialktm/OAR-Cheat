#pragma once
#include "pch/pch.h"
#include <string>
#include <functional>

namespace veh {
void ensure();
std::string get_name(void* uobject);

bool try_call(std::function<void()>&& fn);
}

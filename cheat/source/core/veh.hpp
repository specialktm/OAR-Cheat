#pragma once
#include "pch/pch.h"
#include <string>
// Scoped AV guard for string-building reads from game memory (e.g.
// UObject::GetName during a GObjects walk the GC can mutate mid-read).
// SEH __try is illegal here (C2712: C++ object + __try under /EHsc), so this
// uses a vectored handler + setjmp instead. On fault the partial string is
// discarded (tiny leak, rare path) and "" is returned. Only
// EXCEPTION_ACCESS_VIOLATION inside a guarded section is swallowed; everything
// else (and every other thread) chains to EXCEPTION_CONTINUE_SEARCH.
namespace veh {
void ensure();                        // install handler once (call at init)
std::string get_name(void* uobject);  // AV-safe UObject::GetName
} // namespace veh

#pragma once
#include "pch/pch.h"
#include "core/pattern.hpp"
#include "core/offsets.hpp"

// Central memory helpers: image base, GWorld/GObjects resolution with
// sig-scan primary + hardcoded RVA fallback from the SDK dump.
namespace mem {

inline uintptr_t image_base() {
    static uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
    return base;
}

inline uintptr_t resolve_rip(const char* sigstr, int fallbackRva) {
    uintptr_t at = sig::scan_module(sigstr);
    if (at) {
        // assume disp at +3, len 7 (mov reg, [rip+disp32])
        __try {
            return sig::rip(at, 3, 7) - image_base(); // return as RVA
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    return static_cast<uintptr_t>(fallbackRva);
}

inline uintptr_t gworld_rva() {
    static uintptr_t rva = resolve_rip(offsets::SIG_GWORLD, offsets::GWorld);
    return rva;
}
inline uintptr_t gobjects_rva() {
    static uintptr_t rva = resolve_rip(offsets::SIG_GOBJECTS, offsets::GObjects);
    return rva;
}

template<typename T>
inline T* gworld() {
    uintptr_t addr = image_base() + gworld_rva();
    __try { return *reinterpret_cast<T**>(addr); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

inline bool valid_uobject(void* p) {
    return IS_VALID_PTR(p);
}

} // namespace mem

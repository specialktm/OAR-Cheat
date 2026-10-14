#pragma once
#include "pch/pch.h"
#include "core/pattern.hpp"
#include "core/offsets.hpp"
#include <cstring>

namespace mem {

inline uintptr_t image_base() {
    static uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
    return base;
}

inline uintptr_t decode_rip(uintptr_t insn, uint32_t dispOff, uint32_t insnLen) {
    __try {
        int32_t disp = *reinterpret_cast<int32_t*>(insn + dispOff);
        return insn + insnLen + (int64_t)disp;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

inline bool probe_gobjects(uintptr_t arr) {
    __try {
        if (!IS_VALID_PTR((void*)arr)) return false;
        int32_t mx = 0, n = 0, nc = 0;
        memcpy(&mx, (void*)(arr + 0x10), sizeof(mx));
        memcpy(&n, (void*)(arr + 0x14), sizeof(n));
        memcpy(&nc, (void*)(arr + 0x1C), sizeof(nc));
        return n > 5000 && n < 600000 && mx >= n && nc >= 1 && nc <= 64;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

inline bool probe_gworld(uintptr_t addr) {
    __try {
        void* w = nullptr;
        memcpy(&w, (void*)addr, sizeof(w));
        return IS_VALID_PTR(w);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

inline uintptr_t resolve_rip(const char* sigstr, int fallbackRva) {
    uintptr_t at = sig::scan_module(sigstr);
    if (at) {
        __try {
            return sig::rip(at, 3, 7) - image_base();
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    return static_cast<uintptr_t>(fallbackRva);
}

inline uintptr_t resolve_gworld_rva() {
    uintptr_t at = sig::scan_module(offsets::SIG_GWORLD);
    if (at) {
        uintptr_t a = decode_rip(at + 8, 3, 7);
        if (probe_gworld(a)) return a - image_base();
    }
    return static_cast<uintptr_t>(offsets::GWorld);
}

inline uintptr_t resolve_gobjects_rva() {
    uintptr_t at = sig::scan_module(offsets::SIG_GOBJECTS);
    if (at) {
        uintptr_t a = decode_rip(at + 26, 3, 7);
        if (probe_gobjects(a)) return a - image_base();
    }
    uintptr_t fb = image_base() + static_cast<uintptr_t>(offsets::GObjects);
    if (probe_gobjects(fb)) return static_cast<uintptr_t>(offsets::GObjects);
    return static_cast<uintptr_t>(offsets::GObjects);
}

namespace detail {
inline std::atomic<uintptr_t> g_gw{ 0 };
inline std::atomic<uintptr_t> g_go{ 0 };
inline std::atomic<bool> g_gwInit{ false };
inline std::atomic<bool> g_goInit{ false };
}

inline uintptr_t gworld_rva() {
    if (!detail::g_gwInit.exchange(true))
        detail::g_gw.store(resolve_gworld_rva());
    return detail::g_gw.load();
}
inline uintptr_t gobjects_rva() {
    if (!detail::g_goInit.exchange(true))
        detail::g_go.store(resolve_gobjects_rva());
    return detail::g_go.load();
}
inline void rescan() {
    detail::g_gw.store(resolve_gworld_rva());
    detail::g_go.store(resolve_gobjects_rva());
    detail::g_gwInit.store(true);
    detail::g_goInit.store(true);
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

}

#pragma once
#include "pch/pch.h"

// Thin RAII wrapper over vendored MinHook (cheat/vendor/minhook).
// MinHook sources are compiled directly into the DLL (see vcxproj), no .lib needed.
#include "../../vendor/minhook/include/MinHook.h"

namespace hook {

inline bool g_init = false;

inline bool init() {
    if (g_init) return true;
    if (MH_Initialize() == MH_OK) { g_init = true; return true; }
    return false;
}
inline void shutdown() {
    if (g_init) { MH_Uninitialize(); g_init = false; }
}

class Detour {
public:
    Detour() = default;
    Detour(void* target, void* detour) { create(target, detour); }
    ~Detour() { remove(); }

    Detour(const Detour&) = delete;
    Detour& operator=(const Detour&) = delete;

    bool create(void* target, void* detourFn) {
        if (!g_init && !init()) return false;
        target_ = target; detour_ = detourFn;
        MH_STATUS s = MH_CreateHook(target, detourFn, &orig_);
        return s == MH_OK || s == MH_ERROR_ALREADY_CREATED;
    }
    bool enable()  { return MH_EnableHook(target_) == MH_OK; }
    bool disable() { return MH_DisableHook(target_) == MH_OK; }
    void remove() {
        if (target_) { MH_DisableHook(target_); MH_RemoveHook(target_); target_ = nullptr; orig_ = nullptr; }
    }
    template<typename T> T orig() const { return reinterpret_cast<T>(orig_); }
    void* target() const { return target_; }
    bool valid() const { return target_ && orig_; }
private:
    void* target_ = nullptr;
    void* detour_ = nullptr;
    void* orig_ = nullptr;
};

// Simple VMT swap (used for DXGI Present/ResizeBuffers - no trampoline needed).
class Vmt {
public:
    static bool swap(void** vtable, int idx, void* detour, void** origOut) {
        if (!IS_VALID_PTR(vtable) || idx < 0) return false;
        DWORD old = 0;
        void** entry = &vtable[idx];
        if (!IS_VALID_PTR(entry)) return false;
        __try {
            if (origOut) *origOut = *entry;
            if (!VirtualProtect(entry, sizeof(void*), PAGE_EXECUTE_READWRITE, &old)) return false;
            *entry = detour;
            VirtualProtect(entry, sizeof(void*), old, &old);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
};

} // namespace hook

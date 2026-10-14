#pragma once
#include "pch/pch.h"
#include "core/offsets.hpp"
#include "core/pattern.hpp"

namespace ptr {
inline uintptr_t base() {
    static uintptr_t b = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
    return b;
}
template<typename T>
inline bool read(void* obj, uintptr_t off, T& out) {
    __try {
        if (!IS_VALID_PTR(obj)) return false;
        out = *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(obj) + off);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
template<typename T>
inline bool write(void* obj, uintptr_t off, const T& v) {
    __try {
        if (!IS_VALID_PTR(obj)) return false;
        *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(obj) + off) = v;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
inline void* vfunc(void* obj, size_t idx) {
    __try {
        if (!IS_VALID_PTR(obj)) return nullptr;
        void** vt = *reinterpret_cast<void***>(obj);
        if (!IS_VALID_PTR(vt)) return nullptr;
        return vt[idx];
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
namespace off {
inline constexpr uintptr_t PC_Health = 0x6B0;
inline constexpr uintptr_t PC_MaxHealth = 0x834;
inline constexpr uintptr_t PC_Armor = 0x83C;
inline constexpr uintptr_t Money_Value = 0x260;
inline constexpr uintptr_t Phone_Active = 0x290;
inline constexpr uintptr_t Door_Locked = 0x2B8;
inline constexpr uintptr_t Door_PowerLocked = 0x2C1;
inline constexpr uintptr_t Truck_Take = 0x410;
inline constexpr uintptr_t Truck_Min = 0x414;
inline constexpr uintptr_t Truck_Players = 0x418;
inline constexpr uintptr_t Truck_CanEnd = 0x41C;
}
}

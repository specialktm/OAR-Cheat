#pragma once
#include "pch/pch.h"
#include "SDK/Basic.hpp"
#include "SDK/Engine_classes.hpp"
#include <vector>

// Fixed legacy helper (old DoWork passed vector by value -> always empty).
// GObjects scan is SLOW (39k objects) - use only for shop/data actors that are
// not in ULevel. ESP uses actors::copy_level_actors() instead.
//
// NOTE: No __try here (vector + SEH is C2712 under /EHsc). Each raw read goes
// through a tiny SEH leaf with no C++ objects.
namespace util_seh {
inline int gobjects_num() {
    __try { return SDK::UObject::GObjects->Num(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
inline SDK::UObject* gobjects_at(int i) {
    __try { return SDK::UObject::GObjects->GetByIndex(i); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
inline bool is_default(SDK::UObject* o, bool& out) {
    __try { out = o->IsDefaultObject(); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
template<typename T>
inline bool is_a(SDK::UObject* o, bool& out) {
    __try { out = o->IsA(T::StaticClass()); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
} // namespace util_seh

class utility
{
public:
    utility() = default;
    ~utility() = default;

    template <typename T>
    static std::vector<T*> FindActors()
    {
        std::vector<T*> found;
        found.reserve(128);
        int n = util_seh::gobjects_num();
        if (n <= 0 || n > 600000) return found;
        for (int i = 0; i < n; ++i) {
            SDK::UObject* obj = util_seh::gobjects_at(i);
            if (!IS_VALID_PTR(obj)) continue;
            bool d = false;
            if (!util_seh::is_default(obj, d) || d) continue;
            bool isa = false;
            if (!util_seh::is_a<T>(obj, isa) || !isa) continue;
            found.push_back(static_cast<T*>(obj));
        }
        return found;
    }
};

inline utility g_util;

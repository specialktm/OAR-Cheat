#include "pch/pch.h"
#include "core/veh.hpp"
#include "SDK/Basic.hpp"
#include "SDK/CoreUObject_classes.hpp"

#include <csetjmp>

namespace veh {

static thread_local int t_guard = 0;
static thread_local jmp_buf t_jb;
static thread_local void* t_obj = nullptr;
static thread_local std::string* t_out = nullptr;

static LONG WINAPI handler(EXCEPTION_POINTERS* p) {
    if (t_guard > 0 &&
        p->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        longjmp(t_jb, 1);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static bool g_installed = false;
static std::mutex g_mtx;

void ensure() {
    std::lock_guard<std::mutex> lk(g_mtx);
    if (!g_installed) {
        AddVectoredExceptionHandler(1, handler);
        g_installed = true;
    }
}

// No C++ locals: longjmp-safe by construction (nothing to unwind).
static void do_call() {
    SDK::UObject* o = static_cast<SDK::UObject*>(t_obj);
    *t_out = o->GetName();
}

std::string get_name(void* uobject) {
    ensure();
    std::string out; // leaked on the fault path only (documented, rare)
    if (!IS_VALID_PTR(uobject)) return out;
    t_obj = uobject;
    t_out = &out;
    if (setjmp(t_jb)) {
        t_guard = 0; t_obj = nullptr; t_out = nullptr;
        return std::string();
    }
    ++t_guard;
    do_call();
    --t_guard;
    t_obj = nullptr; t_out = nullptr;
    return out;
}

} // namespace veh

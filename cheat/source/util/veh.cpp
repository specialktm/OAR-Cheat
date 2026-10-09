#include "pch/pch.h"
#include "util/veh.hpp"
#include "SDK/Basic.hpp"
#include "SDK/CoreUObject_classes.hpp"

#include <csetjmp>

namespace veh {

static thread_local int t_guard = 0;
static thread_local jmp_buf t_jb;
static thread_local void* t_obj = nullptr;
static thread_local std::string* t_out = nullptr;
static thread_local int t_try_guard = 0;
static thread_local jmp_buf t_try_jb;

static LONG WINAPI handler(EXCEPTION_POINTERS* p) {
    if ((t_guard > 0 || t_try_guard > 0) &&
        p->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        if (t_try_guard > 0) {
            longjmp(t_try_jb, 1);
        } else {
            longjmp(t_jb, 1);
        }
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

static void do_call() {
    SDK::UObject* o = static_cast<SDK::UObject*>(t_obj);
    *t_out = o->GetName();
}

std::string get_name(void* uobject) {
    ensure();
    std::string out;
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

bool try_call(std::function<void()>&& fn) {
    ensure();
    static thread_local int t_try_guard = 0;
    static thread_local jmp_buf t_try_jb;

    if (setjmp(t_try_jb)) {
        t_try_guard = 0;
        return false;
    }
    ++t_try_guard;
    __try {
        fn();
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        --t_try_guard;
        return false;
    }
    --t_try_guard;
    return true;
}

}

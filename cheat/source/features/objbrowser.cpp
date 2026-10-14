#include "pch/pch.h"
#include "features/objbrowser.hpp"
#include "core/memory.hpp"
#include "core/offsets.hpp"
#include "util/veh.hpp"
#include "SDK/Basic.hpp"
#include "SDK/CoreUObject_classes.hpp"
#include <mutex>
#include <atomic>
#include <thread>
#include <vector>
#include <cstring>

namespace objbrowser {
namespace {
std::mutex mtx;
std::vector<std::pair<int, void*>> found;
std::atomic<bool> busy{ false };
std::atomic<bool> cancelFlag{ false };
std::atomic<float> prog{ 0.0f };
std::atomic<int> totalCount{ 0 };

bool read_obj(int i, void*& obj) {
    __try {
        uintptr_t base = mem::image_base() + mem::gobjects_rva();
        auto* arr = reinterpret_cast<SDK::TUObjectArray*>(base);
        if (!IS_VALID_PTR(arr)) return false;
        int32_t n = 0;
        __try { n = arr->Num(); } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        if (i < 0 || i >= n) return false;
        SDK::UObject* o = nullptr;
        __try { o = arr->GetByIndex(i); } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        if (!IS_VALID_PTR(o)) return false;
        obj = o;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool obj_name(void* o, std::string& out) {
    if (!IS_VALID_PTR(o)) return false;
    return veh::try_call([&] { out = veh::get_name(o); });
}

static bool obj_class(void* o, std::string& out) {
    if (!IS_VALID_PTR(o)) return false;
    return veh::try_call([&] {
        auto* uo = reinterpret_cast<SDK::UObject*>(o);
        auto* c = uo->Class;
        if (!IS_VALID_PTR(c)) return;
        out = veh::get_name(c);
    });
}

bool has_sub(const std::string& hay, const std::string& ndl) {
    if (ndl.empty()) return true;
    if (hay.size() < ndl.size()) return false;
    for (size_t i = 0; i + ndl.size() <= hay.size(); ++i) {
        bool ok = true;
        for (size_t j = 0; j < ndl.size(); ++j) {
            if (tolower((unsigned char)hay[i + j]) != tolower((unsigned char)ndl[j])) { ok = false; break; }
        }
        if (ok) return true;
    }
    return false;
}
}

static bool seh_gonum(int& n) {
    __try {
        uintptr_t base = mem::image_base() + mem::gobjects_rva();
        auto* arr = reinterpret_cast<SDK::TUObjectArray*>(base);
        if (!IS_VALID_PTR(arr)) return false;
        __try { n = arr->Num(); } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void scan() {
    bool expected = false;
    if (!busy.compare_exchange_strong(expected, true)) return;
    cancelFlag.store(false);
    prog.store(0.0f);
    std::thread([] {
        int n = 0;
        if (!seh_gonum(n) || n <= 0) {
            totalCount.store(0);
            prog.store(1.0f);
            busy.store(false);
            return;
        }
        totalCount.store(n);
        std::vector<std::pair<int, void*>> out;
        out.reserve((size_t)n);
        for (int i = 0; i < n; ++i) {
            if (cancelFlag.load()) break;
            void* o = nullptr;
            if (read_obj(i, o)) out.push_back({ i, o });
            if ((i & 1023) == 0) prog.store((float)i / (float)n);
        }
        {
            std::lock_guard<std::mutex> lk(mtx);
            found.swap(out);
        }
        prog.store(1.0f);
        busy.store(false);
    }).detach();
}

void cancel() {
    cancelFlag.store(true);
}

bool running() {
    return busy.load();
}

float progress() {
    return prog.load();
}

int total() {
    int n = 0;
    {
        std::lock_guard<std::mutex> lk(mtx);
        n = (int)found.size();
    }
    int t = totalCount.load();
    return t > 0 ? t : n;
}

std::vector<Row> rows(const char* filter, int cap) {
    std::vector<Row> out;
    std::string f = filter ? filter : "";
    std::vector<std::pair<int, void*>> copy;
    {
        std::lock_guard<std::mutex> lk(mtx);
        copy = found;
    }
    for (auto& pr : copy) {
        if ((int)out.size() >= cap) break;
        std::string nm;
        obj_name(pr.second, nm);
        if (!has_sub(nm, f) && !has_sub(std::to_string(pr.first), f)) continue;
        Row r;
        r.index = pr.first;
        r.obj = pr.second;
        r.name = nm;
        obj_class(pr.second, r.cls);
        out.push_back(r);
    }
    return out;
}

bool resolve(int index, Row& out) {
    void* o = nullptr;
    if (!read_obj(index, o)) return false;
    out.index = index;
    out.obj = o;
    obj_name(o, out.name);
    obj_class(o, out.cls);
    return true;
}

}

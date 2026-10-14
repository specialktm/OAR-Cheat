#include "pch/pch.h"
#include "features/propeditor.hpp"
#include "util/veh.hpp"
#include "core/addrs.hpp"
#include "SDK/Basic.hpp"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/PlayerCharacter_classes.hpp"

#include <unordered_map>
#include <cstring>
#include <cstdlib>
#include <cctype>

namespace proped {
namespace {

std::unordered_map<void*, std::vector<Prop>> g_cache;
bool g_verified = false;
bool g_verifyTried = false;

int kind_for(const std::string& t) {
    if (t == "IntProperty" || t == "UInt32Property") return 1;
    if (t == "Int64Property" || t == "UInt64Property") return 2;
    if (t == "FloatProperty") return 3;
    if (t == "DoubleProperty") return 4;
    if (t == "BoolProperty") return 5;
    if (t == "ByteProperty") return 6;
    if (t == "ObjectProperty" || t == "ClassProperty" || t == "WeakObjectProperty" || t == "LazyObjectProperty" || t == "SoftObjectProperty") return 7;
    if (t == "StrProperty") return 8;
    return 0;
}

bool read_prop_head(void* f, int32_t& arrayDim, int32_t& elemSize, int32_t& off) {
    __try {
        if (!IS_VALID_PTR(f)) return false;
        uintptr_t b = reinterpret_cast<uintptr_t>(f);
        memcpy(&arrayDim, (void*)(b + 0x30), sizeof(arrayDim));
        memcpy(&elemSize, (void*)(b + 0x34), sizeof(elemSize));
        memcpy(&off, (void*)(b + 0x44), sizeof(off));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool read_bool_extra(void* f, uint8_t& byteOff, uint8_t& mask) {
    __try {
        if (!IS_VALID_PTR(f)) return false;
        uintptr_t b = reinterpret_cast<uintptr_t>(f);
        memcpy(&byteOff, (void*)(b + 0x71), sizeof(byteOff));
        memcpy(&mask, (void*)(b + 0x73), sizeof(mask));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool field_next(void* f, void*& next) {
    __try {
        if (!IS_VALID_PTR(f)) return false;
        auto* uf = reinterpret_cast<SDK::UField*>(f);
        next = uf->Next;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool field_names(void* f, std::string& clsName, std::string& propName) {
    if (!IS_VALID_PTR(f)) return false;
    return veh::try_call([&] {
        auto* uf = reinterpret_cast<SDK::UField*>(f);
        if (!IS_VALID_PTR(uf)) return;
        if (IS_VALID_PTR(uf->Class)) clsName = veh::get_name(uf->Class);
        propName = veh::get_name(uf);
    });
}

static bool struct_info(void* cls, void*& children, int32_t& structSize) {
    if (!IS_VALID_PTR(cls)) return false;
    return veh::try_call([&] {
        auto* us = reinterpret_cast<SDK::UStruct*>(cls);
        if (!IS_VALID_PTR(us)) return;
        children = us->Children;
        structSize = us->Size;
    });
}

void scan_class(void* cls, std::vector<Prop>& out) {
    out.clear();
    void* children = nullptr;
    int32_t structSize = 0;
    if (!struct_info(cls, children, structSize)) return;
    if (structSize <= 0 || structSize > 0x10000) structSize = 0x10000;
    void* f = children;
    for (int i = 0; i < 4096 && IS_VALID_PTR(f); ++i) {
        void* next = nullptr;
        field_next(f, next);
        std::string clsName, propName;
        if (field_names(f, clsName, propName) && !propName.empty() && propName.size() <= 128) {
            int kind = kind_for(clsName);
            int32_t arrayDim = 0, elemSize = 0, off = -1;
            if (read_prop_head(f, arrayDim, elemSize, off) && arrayDim == 1 && off >= 0) {
                bool sizeOk = false;
                if (kind == 1 || kind == 3) sizeOk = (elemSize == 4);
                else if (kind == 2 || kind == 4) sizeOk = (elemSize == 8);
                else if (kind == 5 || kind == 6) sizeOk = (elemSize == 1);
                else if (kind == 7 || kind == 8) sizeOk = (elemSize >= 8 && elemSize <= 32);
                if (sizeOk && off + elemSize <= structSize) {
                    Prop p;
                    p.name = propName;
                    p.type = clsName.empty() ? "?" : clsName;
                    p.kind = kind;
                    p.offset = off;
                    p.size = elemSize;
                    if (kind == 5) {
                        uint8_t byteOff = 0, mask = 0;
                        if (!read_bool_extra(f, byteOff, mask) || mask == 0) {
                            f = next;
                            continue;
                        }
                        p.offset = off + byteOff;
                        p.mask = mask;
                    }
                    if (kind != 0) out.push_back(std::move(p));
                }
            }
        }
        f = next;
        if (!IS_VALID_PTR(f)) break;
    }
}

static void verify_layout() {
    if (g_verifyTried) return;
    g_verifyTried = true;
    veh::try_call([&] {
        SDK::UClass* pc = SDK::APlayerCharacter_C::StaticClass();
        if (!IS_VALID_PTR(pc)) return;
        auto* us = reinterpret_cast<SDK::UStruct*>(pc);
        if (!IS_VALID_PTR(us)) return;
        void* f = us->Children;
        for (int i = 0; i < 4096 && IS_VALID_PTR(f); ++i) {
            void* next = nullptr;
            field_next(f, next);
            std::string clsName, propName;
            if (field_names(f, clsName, propName) && propName == "Health") {
                int32_t arrayDim = 0, elemSize = 0, off = -1;
                if (read_prop_head(f, arrayDim, elemSize, off) && off == (int32_t)ptr::off::PC_Health)
                    g_verified = true;
                break;
            }
            f = next;
            if (!IS_VALID_PTR(f)) break;
        }
    });
}

}

bool layout_verified() { return g_verified; }

const std::vector<Prop>& props_for(void* cls) {
    static std::vector<Prop> empty;
    if (!IS_VALID_PTR(cls)) return empty;
    verify_layout();
    auto it = g_cache.find(cls);
    if (it != g_cache.end()) return it->second;
    std::vector<Prop>& v = g_cache[cls];
    scan_class(cls, v);
    return v;
}

bool read_value(void* obj, const Prop& p, std::string& out) {
    if (!IS_VALID_PTR(obj) || p.offset < 0) return false;
    char b[128];
    if (p.kind == 1) {
        int32_t v = 0;
        if (!ptr::read(obj, (uintptr_t)p.offset, v)) return false;
        snprintf(b, sizeof(b), "%d", v);
        out = b;
        return true;
    }
    if (p.kind == 2) {
        int64_t v = 0;
        if (!ptr::read(obj, (uintptr_t)p.offset, v)) return false;
        snprintf(b, sizeof(b), "%lld", (long long)v);
        out = b;
        return true;
    }
    if (p.kind == 3) {
        float v = 0.0f;
        if (!ptr::read(obj, (uintptr_t)p.offset, v)) return false;
        snprintf(b, sizeof(b), "%g", v);
        out = b;
        return true;
    }
    if (p.kind == 4) {
        double v = 0.0;
        if (!ptr::read(obj, (uintptr_t)p.offset, v)) return false;
        snprintf(b, sizeof(b), "%g", v);
        out = b;
        return true;
    }
    if (p.kind == 5) {
        uint8_t v = 0;
        if (!ptr::read(obj, (uintptr_t)p.offset, v)) return false;
        out = (v & p.mask) ? "true" : "false";
        return true;
    }
    if (p.kind == 6) {
        uint8_t v = 0;
        if (!ptr::read(obj, (uintptr_t)p.offset, v)) return false;
        snprintf(b, sizeof(b), "%u", (unsigned)v);
        out = b;
        return true;
    }
    if (p.kind == 7) {
        void* v = nullptr;
        if (!ptr::read(obj, (uintptr_t)p.offset, v)) return false;
        if (!IS_VALID_PTR(v)) { out = "null"; return true; }
        std::string nm;
        if (!veh::try_call([&] { nm = veh::get_name(v); })) return false;
        snprintf(b, sizeof(b), "%p %s", v, nm.c_str());
        out = b;
        return true;
    }
    if (p.kind == 8) {
        wchar_t* data = nullptr;
        int32_t num = 0;
        if (!veh::try_call([&] {
            uintptr_t fbase = reinterpret_cast<uintptr_t>(obj) + (uintptr_t)p.offset;
            memcpy(&data, (void*)fbase, sizeof(data));
            memcpy(&num, (void*)(fbase + 8), sizeof(num));
        })) return false;
        if (num <= 1 || num > 256 || !IS_VALID_PTR(data)) return false;
        wchar_t buf[256] = {};
        bool ok = veh::try_call([&] {
            int n = num - 1;
            if (n > 255) n = 255;
            for (int i = 0; i < n; ++i) buf[i] = data[i];
        });
        if (!ok) return false;
        char mb[512] = {};
        int mlen = WideCharToMultiByte(CP_UTF8, 0, buf, -1, mb, (int)sizeof(mb) - 1, nullptr, nullptr);
        if (mlen <= 0) return false;
        out = mb;
        return true;
    }
    return false;
}

bool write_value(void* obj, const Prop& p, const std::string& text, std::string& err) {
    if (!g_verified) { err = "layout unverified"; return false; }
    if (!IS_VALID_PTR(obj) || p.offset < 0) { err = "bad target"; return false; }
    if (p.kind == 1) {
        long v = strtol(text.c_str(), nullptr, 0);
        int32_t w = (int32_t)v;
        if (!ptr::write(obj, (uintptr_t)p.offset, w)) { err = "write refused"; return false; }
        return true;
    }
    if (p.kind == 2) {
        long long v = strtoll(text.c_str(), nullptr, 0);
        int64_t w = (int64_t)v;
        if (!ptr::write(obj, (uintptr_t)p.offset, w)) { err = "write refused"; return false; }
        return true;
    }
    if (p.kind == 3) {
        float v = strtof(text.c_str(), nullptr);
        if (!ptr::write(obj, (uintptr_t)p.offset, v)) { err = "write refused"; return false; }
        return true;
    }
    if (p.kind == 4) {
        double v = strtod(text.c_str(), nullptr);
        if (!ptr::write(obj, (uintptr_t)p.offset, v)) { err = "write refused"; return false; }
        return true;
    }
    if (p.kind == 5) {
        std::string t = text;
        for (auto& ch : t) ch = (char)tolower((unsigned char)ch);
        bool on = (t == "1" || t == "true" || t == "on");
        bool off = (t == "0" || t == "false" || t == "off");
        if (!on && !off) { err = "use true/false"; return false; }
        uint8_t cur = 0;
        if (!ptr::read(obj, (uintptr_t)p.offset, cur)) { err = "read refused"; return false; }
        uint8_t nw = on ? (uint8_t)(cur | p.mask) : (uint8_t)(cur & ~p.mask);
        if (!ptr::write(obj, (uintptr_t)p.offset, nw)) { err = "write refused"; return false; }
        return true;
    }
    if (p.kind == 6) {
        unsigned long v = strtoul(text.c_str(), nullptr, 0);
        if (v > 255) { err = "0-255 only"; return false; }
        uint8_t w = (uint8_t)v;
        if (!ptr::write(obj, (uintptr_t)p.offset, w)) { err = "write refused"; return false; }
        return true;
    }
    err = "read-only type";
    return false;
}

}

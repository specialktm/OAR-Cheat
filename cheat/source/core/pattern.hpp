#pragma once
#include "pch/pch.h"
#include <psapi.h>

// IDA-style signature scanner: "48 8B 05 ? ? ? ? 48 8B" where ? or ?? = wildcard.
// Used to resolve GWorld / GObjects / ProcessEvent dynamically so hardcoded
// offsets from dump (Basic.hpp) are a fallback, not a hard dependency.
namespace sig {

inline uintptr_t base_of(const char* mod = nullptr) {
    HMODULE h = mod ? GetModuleHandleA(mod) : GetModuleHandleA(nullptr);
    return reinterpret_cast<uintptr_t>(h);
}

inline bool get_text_section(uintptr_t base, uintptr_t& outStart, size_t& outSize) {
    auto dos = reinterpret_cast<PIMAGE_DOS_HEADER>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto nt = reinterpret_cast<PIMAGE_NT_HEADERS64>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if (memcmp(sec->Name, ".text", 5) == 0) {
            outStart = base + sec->VirtualAddress;
            outSize = sec->Misc.VirtualSize;
            return true;
        }
    }
    return false;
}

struct Pattern {
    std::vector<uint8_t> bytes;
    std::vector<bool> mask; // true = must match
};

inline Pattern parse(const char* sig) {
    Pattern p;
    const char* cur = sig;
    while (*cur) {
        while (*cur == ' ') ++cur;
        if (!*cur) break;
        if (*cur == '?' ) {
            p.bytes.push_back(0);
            p.mask.push_back(false);
            ++cur;
            if (*cur == '?') ++cur;
        } else {
            char* end = nullptr;
            long v = strtol(cur, &end, 16);
            p.bytes.push_back(static_cast<uint8_t>(v));
            p.mask.push_back(true);
            cur = end;
        }
    }
    return p;
}

inline uintptr_t scan(uintptr_t start, size_t size, const char* signature) {
    Pattern p = parse(signature);
    if (p.bytes.empty()) return 0;
    const size_t n = p.bytes.size();
    auto* mem = reinterpret_cast<uint8_t*>(start);
    for (size_t i = 0; i + n <= size; ++i) {
        bool ok = true;
        for (size_t j = 0; j < n; ++j) {
            if (p.mask[j] && mem[i + j] != p.bytes[j]) { ok = false; break; }
        }
        if (ok) return start + i;
    }
    return 0;
}

inline uintptr_t scan_module(const char* signature, const char* mod = nullptr) {
    uintptr_t base = base_of(mod);
    if (!base) return 0;
    uintptr_t start = 0; size_t size = 0;
    if (!get_text_section(base, start, size)) {
        // fallback: scan first 120MB
        start = base + 0x1000; size = 120 * 1024 * 1024;
    }
    return scan(start, size, signature);
}

// Resolve RIP-relative address: instruction at `at` with disp at (at+off), size `insnLen`.
inline uintptr_t rip(uintptr_t at, uint32_t dispOff, uint32_t insnLen) {
    int32_t disp = *reinterpret_cast<int32_t*>(at + dispOff);
    return at + insnLen + disp;
}

} // namespace sig

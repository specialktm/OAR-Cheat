#pragma once
#include "pch/pch.h"

// Offsets from dump: 4.27.2-18319896+++UE4+Release-4.27-OAR
// File: dump/GObjects-Dump.txt (39719 objects), dump/*.idmap/*.usmap,
//       cheat/vendor/SDK/Basic.hpp (Offsets namespace).
// EXE evidence (dump/OAR-Win64-Shipping_dump.exe, 84MB):
//   d3d11.dll x2, DXGI x23, Present x24, SwapChain x37, CreateRenderTarget x8
//   => game renders on DX11 -> IDXGISwapChain::Present hook (idx 8) is valid.
namespace offsets {

// Hard fallbacks (imagebase + RVA). Prefer sig-scan at runtime.
inline constexpr int GObjects    = 0x04B0ABD0;
inline constexpr int GNames      = 0x04ACE880;
inline constexpr int GWorld      = 0x04C52570;
inline constexpr int AppendString= 0x00FF7110;
inline constexpr int ProcessEvent= 0x011E78D0;
inline constexpr int ProcessEventIdx = 0x44; // vtable idx

// Signatures reverse-engineered from dump/OAR-Win64-Shipping_dump.exe
// (ImageBase 0x7FF614020000, .text VA 0x1000 size 0x37D8000). Each was
// validated by scanning the full .text: hit count + resolved target.
// Convention: the RIP-relative mov/lea is a 7-byte `48 8B ?D [rip+disp32]`
// with the disp at pattern+3, matching mem::resolve_rip(at, 3, 7).

// GWorld: 1 hit @RVA 0x7C69CD, resolves to GWorld. Note the old pattern
// ended `41 B8` - the real byte is `41 B0` (mov r8b,1); one byte is why the
// old scan always missed. Alt site @RVA 0x7CC888 (also verified) kept here
// for reference: "80 3D ? ? ? ? 00 74 22 48 8B 05 ? ? ? ? 48 85 C0 74 16".
inline constexpr const char* SIG_GWORLD =
    "0F 57 C9 0F 2E C1 74 4E 48 8B 1D ? ? ? ? 48 85 DB 74 3B 41 B0 01";

// GObjects: 421/421 hits, every one resolves to GObjects (inlined
// TUObjectArray::GetByIndex chunk math + load). First-hit is always correct,
// and the multiplicity makes it patch-resistant.
inline constexpr const char* SIG_GOBJECTS =
    "0F B7 D2 03 C2 8B C8 0F B7 C0 2B C2 48 98 C1 F9 10 48 63 C9 48 8D 14 40 48 8B 05 ? ? ? ?";

// ProcessEvent @RVA 0x11E78D0: 18-byte prologue was shared with one twin
// (@RVA 0x2345CD0); extended form below is unique (1 hit, exact RVA).
inline constexpr const char* SIG_PROCESS_EVENT =
    "40 55 56 57 41 54 41 55 41 56 41 57 48 81 EC F0 00 00 00 48 8D 6C 24 30 48 89 9D 18 01 00 00";

// FName::AppendString @RVA 0xFF7110: unique (1 hit, exact RVA).
inline constexpr const char* SIG_APPEND_STRING =
    "48 89 5C 24 18 48 89 74 24 20 57 48 83 EC 20 8B 01 48 8B DA";

} // namespace offsets

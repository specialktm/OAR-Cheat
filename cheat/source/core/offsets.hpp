#pragma once
#include "pch/pch.h"

namespace offsets {

inline constexpr int GObjects    = 0x04B0ABD0;
inline constexpr int GNames      = 0x04ACE880;
inline constexpr int GWorld      = 0x04C52570;
inline constexpr int AppendString= 0x00FF7110;
inline constexpr int ProcessEvent= 0x011E78D0;
inline constexpr int ProcessEventIdx = 0x44;

inline constexpr const char* SIG_GWORLD =
    "0F 57 C9 0F 2E C1 74 4E 48 8B 1D ? ? ? ? 48 85 DB 74 3B 41 B0 01";

inline constexpr const char* SIG_GOBJECTS =
    "0F B7 D2 03 C2 8B C8 0F B7 C0 2B C2 48 98 C1 F9 10 48 63 C9 48 8D 14 40 48 8B 05 ? ? ? ?";

inline constexpr const char* SIG_PROCESS_EVENT =
    "40 55 56 57 41 54 41 55 41 56 41 57 48 81 EC F0 00 00 00 48 8D 6C 24 30 48 89 9D 18 01 00 00";

inline constexpr const char* SIG_APPEND_STRING =
    "48 89 5C 24 18 48 89 74 24 20 57 48 83 EC 20 8B 01 48 8B DA";

}

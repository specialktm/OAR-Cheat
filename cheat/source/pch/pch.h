#pragma once
// Light PCH - keep this minimal for fast builds. Do NOT include SDK uber-headers here.
#ifndef PCH_H
#define PCH_H

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>
#include <thread>
#include <algorithm>
#include <format>
#include <iostream>
#include <functional>

using namespace std::chrono;
using namespace std::chrono_literals;

#define IS_VALID_PTR(ptr) ((ptr) != nullptr && (reinterpret_cast<uintptr_t>(ptr) > 0x1000))
#define _s(x) #x

static HANDLE g_MainThread = nullptr;
static HMODULE g_Module = nullptr;
inline std::atomic<bool> programRunning{ true };

#endif

#ifndef PCH_H
#define PCH_H
#include <windows.h>
#include <cstdint>
#include "framework/framework.h"

#define IS_VALID_PTR(ptr) ((ptr) != nullptr && (reinterpret_cast<uintptr_t>(ptr) > 0x1000))
using namespace std::chrono;
using namespace std::chrono_literals;

#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable: 4996)

#define _s(x) #x
#endif // PCH_H
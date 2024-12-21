#pragma once
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#include <vector>
#include <functional>
#include <iostream>
#include <string>
#include <iomanip>
#include <format>
#undef max
#include <algorithm>
#include <chrono>
#include <thread>
#include "cheat/base/Baseclass.hpp"

#include "SDK/Engine_classes.hpp"
#include "SDK/Basic.hpp"
#include "SDK/ServerBrowser_Mapselect_classes.hpp"
#include "SDK/RobberController_classes.hpp"

#include "logger/logger.hpp"

static HANDLE g_MainThread;
static HMODULE g_Module;


inline bool keyDown(int key)
{
	return GetAsyncKeyState(key);
}
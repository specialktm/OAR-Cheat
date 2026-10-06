#pragma once
#include "pch/pch.h"
#include "../../vendor/imgui/imgui.h"
// Custom ImGui elements: sidebar pills, gradient action buttons, animated
// toggles, cards, stat rows. All hover states follow the active theme.
namespace widgets {

enum class SideIcon { Eye, Cart, Globe, Person, Gear };

void section(const char* t);
bool nav_pill(const char* label, bool active);
bool sidebar_button(const char* label, SideIcon icon, bool active);
bool action_button(const char* label, const char* sub, ImVec2 size);
bool toggle_ex(const char* label, std::atomic<bool>& v);
// OSGames-style checkbox row with optional color chip (opens editor popup).
bool check_row(const char* label, std::atomic<bool>& v, ImVec4* chip);
bool color_chip(const char* id, ImVec4& col);
// Segmented pill control. Returns true when selection changed.
bool segmented(const char** items, int count, int& sel);
// Styled dropdown bound to an atomic int index.
bool dropdown(const char* label, const char** items, int count, std::atomic<int>& sel);
void slider_f(const char* label, std::atomic<float>& v, float lo, float hi);
// Editable keybind: left-click to arm, next keypress binds, right-click clears.
// Render thread only (polls GetAsyncKeyState while armed).
bool keybind(const char* label, std::atomic<int>& vk);
const char* key_name(int vk);
void stat_row(const char* k, int v, ImU32 col);
void stat_text(const char* k, const char* v, ImU32 col);
bool card_begin(const char* title, const char* hint = nullptr);
void card_end();

} // namespace widgets

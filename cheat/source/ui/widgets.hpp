#pragma once
#include "pch/pch.h"
#include "../../vendor/imgui/imgui.h"

namespace widgets {

const char* key_name(int vk);
const char* key_mode_name(int mode);
bool keybind_row(const char* label, std::atomic<int>& vk, std::atomic<int>* mode = nullptr);

bool check_bridge(const char* label, std::atomic<bool>& v);
bool combo_bridge(const char* label, const char** items, int count, std::atomic<int>& sel);
bool sliderf_bridge(const char* label, std::atomic<float>& v, float lo, float hi, const char* fmt = "%.0f");
bool os_check(const char* label, bool* v);
bool os_slider(const char* label, float* v, float lo, float hi, const char* fmt = "%.1f");
void os_multi(const char* label, bool variable[], const char* labels[], int count);
bool os_section(const char* name, ImVec2 size);
void os_section_end();
bool os_subtab(const char* label, bool selected, float width);
bool os_tab(bool selected, const char* icon, const char* label, const char* hint, ImVec2 size, float openT);
bool os_circle_toggle(const char* id, ImVec2 size);
bool multi_bridge(const char* label, const char** items, std::atomic<bool>** flags, int count);
void header_card(const char* title, const char* sub);

void avatar_tile(ImVec2 p0, ImVec2 size);

void smooth_scroll(const char* id, float ease = 12.0f);

}

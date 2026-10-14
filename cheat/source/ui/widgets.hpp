#pragma once
#include "pch/pch.h"
#include "../../vendor/imgui/imgui.h"

namespace widgets {

const char* key_name(int vk);

bool check_bridge(const char* label, std::atomic<bool>& v);
bool combo_bridge(const char* label, const char** items, int count, std::atomic<int>& sel);
bool sliderf_bridge(const char* label, std::atomic<float>& v, float lo, float hi, const char* fmt = "%.0f");
bool os_check(const char* label, bool* v);
bool os_slider(const char* label, float* v, float lo, float hi, const char* fmt = "%.1f");
void os_multi(const char* label, bool variable[], const char* labels[], int count);
bool os_section(const char* name, ImVec2 size);
void os_section_end();
bool action_button(const char* id, const char* name, const char* desc, ImVec2 size);
bool os_subtab(const char* label, bool selected, float width);
bool multi_bridge(const char* label, const char** items, std::atomic<bool>** flags, int count);
void color_tail(const char* id, ImVec4& col);
void check_picker(const char* label, std::atomic<bool>& v, ImVec4& col, const char* cid = nullptr);
void key_row(const char* text, const char* uid, std::atomic<int>& key, std::atomic<int>& mode);
void key_pill(const char* uid, std::atomic<int>& key, std::atomic<int>& mode);
std::string shorten(const std::string& s, size_t n = 30);

void avatar_tile(ImVec2 p0, ImVec2 size);

void smooth_scroll(const char* id, float ease = 12.0f);

}

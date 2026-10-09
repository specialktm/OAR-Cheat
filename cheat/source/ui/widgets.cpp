#include "pch/pch.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "ui/widgets.hpp"
#include "ui/menu.hpp"
#include "ui/theme.hpp"
#include "steam/steam_user.hpp"

#include <unordered_map>
#include <cmath>
#include <cstring>
#include <string>

#include "../../vendor/imgui/imgui_internal.h"

namespace widgets {

const char* key_name(int vk) {
    static char buf[16];
    if (vk <= 0) return "None";
    switch (vk) {
    case 0x01: return "MB1";
    case 0x02: return "MB2";
    case 0x04: return "MMB";
    case 0x05: return "MB4";
    case 0x06: return "MB5";
    case 0x2D: return "INSERT";
    case 0x23: return "END";
    case 0x2E: return "DELETE";
    case 0x24: return "HOME";
    case 0x21: return "PGUP";
    case 0x22: return "PGDN";
    case 0x70: return "F1"; case 0x71: return "F2"; case 0x72: return "F3";
    case 0x73: return "F4"; case 0x74: return "F5"; case 0x75: return "F6";
    case 0x76: return "F7"; case 0x77: return "F8"; case 0x78: return "F9";
    case 0x79: return "F10"; case 0x7A: return "F11"; case 0x7B: return "F12";
    case 0x60: return "NUM0"; case 0x61: return "NUM1"; case 0x62: return "NUM2";
    case 0x63: return "NUM3"; case 0x64: return "NUM4"; case 0x65: return "NUM5";
    case 0x66: return "NUM6"; case 0x67: return "NUM7"; case 0x68: return "NUM8";
    case 0x69: return "NUM9";
    case 0x10: return "SHIFT"; case 0x11: return "CTRL"; case 0x12: return "ALT";
    case 0x20: return "SPACE"; case 0x09: return "TAB"; case 0x0D: return "ENTER";
    case 0x1B: return "ESC"; case 0x08: return "BKSP";
    default: break;
    }
    if (vk >= 'A' && vk <= 'Z' || vk >= '0' && vk <= '9') {
        buf[0] = (char)vk; buf[1] = 0;
        return buf;
    }
    snprintf(buf, sizeof(buf), "VK %02X", vk & 0xFF);
    return buf;
}

const char* key_mode_name(int mode) {
    if (mode == 1) return "Hold";
    if (mode == 2) return "Always";
    return "Toggle";
}

bool keybind_row(const char* label, std::atomic<int>& vk, std::atomic<int>* mode) {
    static std::string arming;
    static uint64_t armTs = 0;
    int cur = vk.load();
    int curMode = mode ? mode->load() : 0;
    ImGui::PushID(label);
    ImVec2 bpos = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    ImVec2 bsz(w, 30);
    bool pressed = ImGui::InvisibleButton("##kb", bsz);
    bool hovered = ImGui::IsItemHovered();
    bool isArming = (arming == label);
    bool changed = false;
    if (pressed) {
        arming = isArming ? "" : label;
        armTs = GetTickCount64();
    }
    char txt[32];
    snprintf(txt, sizeof(txt), "%s%s%s", isArming ? "-" : key_name(cur),
        mode ? "  [" : "", mode ? key_mode_name(curMode) : "", mode ? "]" : "");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(bpos, ImVec2(bpos.x + bsz.x, bpos.y + bsz.y),
        ui::fade(isArming ? IM_COL32(64, 0, 128, 255) : IM_COL32(18, 18, 20, 255)), 5.0f);
    dl->AddRect(bpos, ImVec2(bpos.x + bsz.x, bpos.y + bsz.y),
        ui::fade(hovered || isArming ? IM_COL32(100, 100, 100, 255) : IM_COL32(16, 16, 18, 255)),
        5.0f, 0, 1.0f);
    ImVec2 lsz = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(bpos.x + 10, bpos.y + (bsz.y - lsz.y) * 0.5f),
        ui::fade(IM_COL32(200, 200, 200, 255)), label);
    ImVec2 tsz = ImGui::CalcTextSize(txt);
    dl->AddText(ImVec2(bpos.x + bsz.x - tsz.x - 10, bpos.y + (bsz.y - tsz.y) * 0.5f),
        ui::fade(isArming ? IM_COL32(255, 255, 255, 255) : IM_COL32(235, 235, 235, 255)), txt);
    if (hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::SetTooltip(mode ? "click to bind, right-click for mode"
                               : "click to bind, right-click clears");
        if (mode && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            ImGui::OpenPopup("##kbmode");
        else if (!mode && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            vk.store(0);
            arming.clear();
            changed = true;
        }
    }
    if (mode && ImGui::BeginPopup("##kbmode")) {
        for (int m = 0; m < 3; ++m) {
            if (ImGui::Selectable(key_mode_name(m), curMode == m))
                mode->store(m);
        }
        if (ImGui::Selectable("Clear key", false)) {
            vk.store(0);
            arming.clear();
            changed = true;
        }
        ImGui::EndPopup();
    }
    if (isArming) {
        if (GetTickCount64() - armTs > 250) {
            for (int k = 1; k < 256; ++k) {
                if (k == 0x1B) continue;
                if (GetAsyncKeyState(k) & 1) {
                    vk.store(k);
                    arming.clear();
                    changed = true;
                    break;
                }
            }
        }
        if (GetAsyncKeyState(0x1B) & 1) arming.clear();
    }
    ImGui::PopID();
    return changed;
}

bool check_bridge(const char* label, std::atomic<bool>& v) {
    bool b = v.load();
    ImGui::PushID(label);
    ImGuiWindow* win = ImGui::GetCurrentWindow();
    if (win->SkipItems) {
        ImGui::PopID();
        return false;
    }
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = win->GetID("##os_cb");
    const ImVec2 lsz = ImGui::CalcTextSize(label, NULL, true);
    float sq = ImGui::GetFrameHeight();
    ImVec2 pos = win->DC.CursorPos;
    ImRect total_bb(pos, pos + ImVec2(sq + (lsz.x > 0.0f ? style.ItemInnerSpacing.x + lsz.x : 0.0f), lsz.y + style.FramePadding.y * 2.0f));
    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, id)) {
        ImGui::PopID();
        return false;
    }
    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
    if (pressed) {
        b = !b;
        v.store(b);
        ImGui::MarkItemEdited(id);
    }
    ImRect check_bb(pos, pos + ImVec2(sq, sq));
    float rounding = style.FrameRounding * 2.5f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(check_bb.Min, check_bb.Max, ui::fade(IM_COL32(31, 31, 31, 255)), rounding);
    if (b) {
        ImVec4 a = theme::accentA(1.0f);
        dl->AddRectFilled(check_bb.Min, check_bb.Max, ui::fade(IM_COL32((int)(a.x * 255.0f), (int)(a.y * 255.0f), (int)(a.z * 255.0f), 255)), rounding);
    }
    dl->AddRect(check_bb.Min, check_bb.Max, ui::fade(hovered ? IM_COL32(100, 100, 100, 255) : IM_COL32(80, 80, 80, 150)), rounding, 0, 1.5f);
    if (hovered)
        dl->AddRectFilled(check_bb.Min, check_bb.Max, ui::fade(IM_COL32(255, 255, 255, 20)), rounding);
    if (lsz.x > 0.0f) {
        ImVec2 lp(check_bb.Max.x + style.ItemInnerSpacing.x, check_bb.Min.y + style.FramePadding.y);
        ImU32 tc = b ? IM_COL32(164, 0, 255, 255) : IM_COL32(255, 255, 255, 255);
        dl->AddText(ImVec2(lp.x + 1, lp.y + 1), ui::fade(IM_COL32(0, 0, 0, 120)), label);
        dl->AddText(lp, ui::fade(tc), label);
    }
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ImGui::PopID();
    return pressed;
}

bool os_check(const char* label, bool* v) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
    const float square_sz = ImGui::GetFrameHeight();
    const ImVec2 pos = window->DC.CursorPos;
    const ImRect total_bb(pos, pos + ImVec2(square_sz + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f), label_size.y + style.FramePadding.y * 2.0f));
    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, id))
        return false;
    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
    if (pressed) {
        *v = !(*v);
        ImGui::MarkItemEdited(id);
    }
    const ImRect check_bb(pos, pos + ImVec2(square_sz, square_sz));
    float checkbox_rounding = style.FrameRounding * 2.5f;
    window->DrawList->AddRectFilled(check_bb.Min, check_bb.Max, ui::fade(IM_COL32(31, 31, 31, 255)), checkbox_rounding);
    if (*v) {
        ImVec4 a = theme::accentA(1.0f);
        window->DrawList->AddRectFilled(check_bb.Min, check_bb.Max, ui::fade(IM_COL32((int)(a.x * 255.0f), (int)(a.y * 255.0f), (int)(a.z * 255.0f), 255)), checkbox_rounding);
    }
    window->DrawList->AddRect(check_bb.Min, check_bb.Max, ui::fade(hovered ? IM_COL32(100, 100, 100, 255) : IM_COL32(80, 80, 80, 150)), checkbox_rounding, 0, 1.5f);
    if (hovered)
        window->DrawList->AddRectFilled(check_bb.Min, check_bb.Max, ui::fade(IM_COL32(255, 255, 255, 20)), checkbox_rounding);
    if (label_size.x > 0.0f) {
        ImVec2 label_pos = ImVec2(check_bb.Max.x + style.ItemInnerSpacing.x, check_bb.Min.y + style.FramePadding.y);
        ImU32 text_color = *v ? IM_COL32(164, 0, 255, 255) : IM_COL32(255, 255, 255, 255);
        window->DrawList->AddText(label_pos + ImVec2(1, 1), ui::fade(IM_COL32(0, 0, 0, 120)), label);
        window->DrawList->AddText(label_pos, ui::fade(text_color), label);
    }
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    return pressed;
}

bool combo_bridge(const char* label, const char** items, int count, std::atomic<int>& sel) {
    if (count <= 0) return false;
    int cur = sel.load();
    if (cur < 0 || cur >= count) cur = 0;
    if (ImGui::Combo(label, &cur, items, count, -1)) {
        sel.store(cur);
        return true;
    }
    return false;
}

bool sliderf_bridge(const char* label, std::atomic<float>& v, float lo, float hi, const char* fmt) {
    float f = v.load();
    if (os_slider(label, &f, lo, hi, fmt ? fmt : "%.0f")) {
        v.store(f);
        return true;
    }
    return false;
}

bool os_slider(const char* label, float* v, float v_min, float v_max, const char* format) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
    const float w = ImGui::CalcItemWidth();
    const float slider_height = 20.0f;
    const ImVec2 pos = window->DC.CursorPos;
    char value_buf[64];
    snprintf(value_buf, sizeof(value_buf), format, *v);
    ImVec2 value_size = ImGui::CalcTextSize(value_buf);
    const float value_space = value_size.x + 10.0f;
    const ImRect bb(pos, pos + ImVec2(w - value_space, slider_height));
    const ImRect total_bb(pos, pos + ImVec2(w + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f), slider_height));
    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, id, &bb))
        return false;
    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_PressedOnClick);
    float old_value = *v;
    if (held) {
        const float normalized = ImSaturate((g.IO.MousePos.x - bb.Min.x) / (bb.Max.x - bb.Min.x));
        *v = ImLerp(v_min, v_max, normalized);
        if (strstr(format, ".2f") != NULL) *v = roundf(*v * 100.0f) / 100.0f;
        else if (strstr(format, ".3f") != NULL) *v = roundf(*v * 1000.0f) / 1000.0f;
        else if (strstr(format, ".1f") != NULL) *v = roundf(*v * 10.0f) / 10.0f;
        else if (strstr(format, ".0f") != NULL) *v = roundf(*v);
        *v = ImClamp(*v, v_min, v_max);
    }
    bool value_changed = (*v != old_value);
    if (value_changed)
        ImGui::MarkItemEdited(id);
    snprintf(value_buf, sizeof(value_buf), format, *v);
    float t = ImSaturate((*v - v_min) / (v_max - v_min));
    float slider_rounding = style.FrameRounding * 1.5f;
    window->DrawList->AddRectFilled(bb.Min, bb.Max, ui::fade(IM_COL32(30, 30, 30, 255)), slider_rounding);
    if (t > 0.0f) {
        ImVec4 a = theme::accentA(1.0f);
        ImU32 fill_color = ui::fade(IM_COL32((int)(a.x * 255.0f), (int)(a.y * 255.0f), (int)(a.z * 255.0f), 255));
        window->DrawList->AddRectFilled(bb.Min, ImVec2(ImLerp(bb.Min.x, bb.Max.x, t), bb.Max.y), fill_color, slider_rounding);
    }
    window->DrawList->AddRect(bb.Min, bb.Max, ui::fade(hovered || held ? IM_COL32(100, 100, 100, 255) : IM_COL32(70, 70, 70, 255)), slider_rounding, 0, 1.5f);
    float indicator_x = ImLerp(bb.Min.x, bb.Max.x, t);
    window->DrawList->AddLine(ImVec2(indicator_x, bb.Min.y), ImVec2(indicator_x, bb.Max.y), ui::fade(held ? IM_COL32(200, 100, 255, 255) : IM_COL32(164, 0, 255, 255)), 3.0f);
    ImVec2 value_pos = ImVec2(bb.Max.x + 10.0f, bb.Min.y);
    window->DrawList->AddText(value_pos + ImVec2(1, 1), ui::fade(IM_COL32(0, 0, 0, 120)), value_buf);
    window->DrawList->AddText(value_pos, ui::fade(IM_COL32(255, 255, 255, 255)), value_buf);
    if (label_size.x > 0.0f) {
        ImVec2 label_pos = ImVec2(bb.Min.x, bb.Min.y - label_size.y - 2.0f);
        window->DrawList->AddText(label_pos + ImVec2(1, 1), ui::fade(IM_COL32(0, 0, 0, 120)), label);
        window->DrawList->AddText(label_pos, ui::fade(IM_COL32(255, 255, 255, 255)), label);
    }
    if (hovered || held) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    return pressed || value_changed;
}

bool os_tab(bool selected, const char* icon, const char* label, const char* hint, ImVec2 size, float openT) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 text_size = ImGui::CalcTextSize(label);
    float t = ImClamp(openT, 0.0f, 1.0f);
    float width = ImLerp(50.0f, 210.0f, t);
    ImVec2 total_size = ImVec2(width, size.y);
    const ImRect bb(pos, pos + total_size);
    ImGui::ItemSize(total_size, 0.0f);
    if (!ImGui::ItemAdd(bb, id))
        return false;
    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    ImVec4 acc = theme::accentA(1.0f);
    ImU32 col = selected ? ui::fade(ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.60f))) :
        hovered ? ui::fade(ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.50f))) :
        ui::fade(ImGui::GetColorU32(ImVec4(0.15f, 0.15f, 0.15f, 0.6f)));
    window->DrawList->AddRectFilled(bb.Min, bb.Max, col, style.FrameRounding);
    window->DrawList->AddRect(bb.Min, bb.Max, ui::fade(IM_COL32(50, 50, 50, 255)), style.FrameRounding, 0, 1.0f);
    if (selected)
        window->DrawList->AddRect(bb.Min, bb.Max, ui::fade(ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 1.0f))), style.FrameRounding);
    float center_y = bb.Min.y + total_size.y * 0.5f;
    ImVec2 icon_pos(bb.Min.x + 10.0f, center_y - 10.0f);
    window->DrawList->AddText(icon_pos, ui::fade(IM_COL32_WHITE), icon);
    if (t * 160.0f > 100.0f) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImClamp((t * 160.0f - 100.0f) / 60.0f, 0.0f, 1.0f) * ImGui::GetStyle().Alpha);
        ImVec2 label_pos = pos + ImVec2(50, size.y / 2 - text_size.y / 2);
        window->DrawList->AddText(label_pos, ui::fade(ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f))), label);
        ImGui::PopStyleVar();
    }
    if (hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (hint && hint[0] != '\0')
            ImGui::SetTooltip("%s", hint);
    }
    return pressed;
}

bool os_circle_toggle(const char* id, ImVec2 size) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;
    const ImGuiID bid = window->GetID(id);
    ImVec2 pos = window->DC.CursorPos;
    const ImRect bb(pos, pos + size);
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, bid))
        return false;
    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, bid, &hovered, &held);
    ImVec4 acc = theme::accentA(1.0f);
    ImU32 bg = hovered ? ui::fade(ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.5f))) :
        ui::fade(ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.4f)));
    ImVec2 center = ImVec2((bb.Min.x + bb.Max.x) * 0.5f, (bb.Min.y + bb.Max.y) * 0.5f);
    float radius = (bb.Max.x - bb.Min.x) * 0.5f;
    window->DrawList->AddCircleFilled(center, radius, bg, 64);
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    return pressed;
}

void os_multi(const char* label, bool variable[], const char* labels[], int count) {
    std::string preview = "None";
    int picked = 0;
    for (int i = 0; i < count; ++i) {
        if (!variable[i]) continue;
        if (picked >= 3) {
            preview += "...";
            break;
        }
        if (picked == 0) preview = labels[i];
        else preview += std::string(", ") + labels[i];
        ++picked;
    }
    if (ImGui::BeginCombo(label, preview.c_str(), 0)) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 7));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 7));
        for (int i = 0; i < count; ++i)
            ImGui::Selectable(labels[i], &variable[i], ImGuiSelectableFlags_DontClosePopups);
        ImGui::PopStyleVar(2);
        ImGui::EndCombo();
    }
}

bool multi_bridge(const char* label, const char** items, std::atomic<bool>** flags, int count) {
    int n = count < 16 ? count : 16;
    if (n <= 0) return false;
    bool tmp[16];
    bool before[16];
    for (int i = 0; i < n; ++i) tmp[i] = before[i] = flags[i]->load();
    os_multi(label, tmp, items, n);
    bool changed = false;
    for (int i = 0; i < n; ++i) {
        if (tmp[i] != before[i]) {
            flags[i]->store(tmp[i]);
            changed = true;
        }
    }
    return changed;
}

bool os_section(const char* name, ImVec2 size) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (name && name[0]) {
        ImVec2 tsz = ImGui::CalcTextSize(name);
        dl->AddText(ImVec2(pos.x + 14.0f, pos.y + 9.0f),
            ui::fade(IM_COL32(255, 255, 255, 255)), name);
        (void)tsz;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 36));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 12));
    return ImGui::BeginChild(name && name[0] ? name : "##card", size, true);
}

void os_section_end() {
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}

bool os_subtab(const char* label, bool selected, float width) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size(width, 31.0f);
    const ImRect bb(pos, pos + size);
    ImGui::ItemSize(size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id))
        return false;
    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    ImVec4 acc = theme::accentA(1.0f);
    ImU32 bg = selected ? ui::fade(ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.65f))) :
        hovered ? ui::fade(ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.35f))) :
        ui::fade(IM_COL32(26, 26, 26, 255));
    window->DrawList->AddRectFilled(bb.Min, bb.Max, bg, style.FrameRounding);
    window->DrawList->AddRect(bb.Min, bb.Max,
        ui::fade(selected ? ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 1.0f)) : IM_COL32(50, 50, 50, 255)),
        style.FrameRounding, 0, 1.0f);
    ImVec2 tp(pos.x + (width - label_size.x) * 0.5f, pos.y + (31.0f - label_size.y) * 0.5f);
    window->DrawList->AddText(tp, ui::fade(IM_COL32(255, 255, 255, 255)), label);
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    return pressed;
}

void header_card(const char* title, const char* sub) {
    float cw = ImGui::GetContentRegionAvail().x;
    if (cw < 200.0f) cw = 200.0f;
    os_section("", ImVec2(cw, 100));
    {
        ImVec2 c0 = ImGui::GetCursorScreenPos();
        float inner = cw - 16.0f;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (tab_text3) {
            ImVec2 tsz = tab_text3->CalcTextSizeA(20.0f, 10000.0f, 0.0f, title);
            dl->AddText(tab_text3, 20.0f,
                ImVec2(c0.x + (inner - tsz.x) * 0.5f, c0.y + 4.0f),
                IM_COL32(255, 255, 255, 255), title);
            ImVec2 ssz = tab_text3->CalcTextSizeA(13.0f, 10000.0f, 0.0f, sub);
            dl->AddText(tab_text3, 13.0f,
                ImVec2(c0.x + (inner - ssz.x) * 0.5f, c0.y + 30.0f),
                IM_COL32(150, 150, 150, 255), sub);
        } else {
            ImVec2 tsz = ImGui::CalcTextSize(title);
            dl->AddText(ImVec2(c0.x + (inner - tsz.x) * 0.5f, c0.y + 4.0f),
                IM_COL32(255, 255, 255, 255), title);
            ImVec2 ssz = ImGui::CalcTextSize(sub);
            dl->AddText(ImVec2(c0.x + (inner - ssz.x) * 0.5f, c0.y + 30.0f),
                IM_COL32(150, 150, 150, 255), sub);
        }
    }
    os_section_end();
    ImGui::SetCursorPosY(120);
}

void avatar_tile(ImVec2 p0, ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p1(p0.x + size.x, p0.y + size.y);
    void* av = steamServiceHelper::avatar();
    if (av) {
        dl->AddImage(av, p0, p1);
        return;
    }
    dl->AddRectFilled(p0, p1, ui::fade(IM_COL32(30, 33, 52, 255)), 0.0f);
    if (!steamServiceHelper::avatar_failed()) {
        ImVec2 c(p0.x + size.x * 0.5f, p0.y + size.y * 0.5f);
        float r = (size.x < size.y ? size.x : size.y) * 0.32f;
        static float rot = 0.0f;
        rot += ImGui::GetIO().DeltaTime * 4.0f;
        dl->PathArcTo(c, r, rot, rot + IM_PI * 1.6f, 24);
        dl->PathStroke(ui::fade(IM_COL32(100, 160, 255, 255)), false, 2.8f);
        dl->PathClear();
        dl->PathArcTo(c, r, rot + IM_PI * 1.6f, rot + IM_PI * 2.0f, 24);
        dl->PathStroke(ui::fade(IM_COL32(100, 160, 255, 80)), false, 2.0f);
    } else {
        const char* un = steamServiceHelper::name();
        char initial[2] = { (char)toupper(un[0] ? un[0] : '?'), 0 };
        ImVec2 isz = ImGui::CalcTextSize(initial);
        dl->AddText(ImVec2(p0.x + (size.x - isz.x) * 0.5f, p0.y + (size.y - isz.y) * 0.5f),
            ui::fade(IM_COL32(190, 200, 230, 255)), initial);
    }
    dl->AddRect(p0, p1, ui::fade(IM_COL32(90, 98, 130, 255)), 0.0f, 0, 1.0f);
}

void smooth_scroll(const char* id, float ease) {
    struct State { float shown = 0, target = 0; bool init = false; };
    static std::unordered_map<ImGuiID, State> g_scroll;
    static int g_wheelFrame = -1;
    ImGuiWindow* win = ImGui::GetCurrentWindow();
    if (!win) return;
    ImGuiID hid = ImGui::GetID(id);
    State& s = g_scroll[hid];
    float raw = win->Scroll.y;
    float maxY = win->ScrollMax.y;
    if (!s.init) { s.shown = s.target = raw; s.init = true; }
    float dt = ImGui::GetIO().DeltaTime;
    if (dt <= 0.0f) return;
    s.target += raw - s.shown;
    ImGuiContext& g = *GImGui;
    int frame = ImGui::GetFrameCount();
    float wheel = g.IO.MouseWheel;
    if (wheel != 0.0f && g_wheelFrame != frame) {
        bool inside = false;
        for (ImGuiWindow* w = g.HoveredWindow; w; w = w->ParentWindow)
            if (w == win) { inside = true; break; }
        if (inside) {
            float a = 5.0f * ImGui::GetFontSize();
            float b = win->InnerRect.GetHeight() * 0.67f;
            float step = (a < b ? a : b);
            step = (float)((int)step);
            if (step < 1.0f) step = 1.0f;
            s.target -= wheel * step;
            g_wheelFrame = frame;
        }
    }
    if (s.target < 0.0f) s.target = 0.0f;
    if (s.target > maxY) s.target = maxY;
    float k = 1.0f - expf(-dt * (ease > 0.0f ? ease : 12.0f));
    s.shown += (s.target - s.shown) * k;
    if (fabsf(s.target - s.shown) < 0.25f) s.shown = s.target;
    if (s.shown < 0.0f) s.shown = 0.0f;
    if (s.shown > maxY) s.shown = maxY;
    win->Scroll.y = s.shown;
}

}

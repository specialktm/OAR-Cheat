#pragma once
#include "pch/pch.h"
#include "../../vendor/imgui/imgui.h"
#include "../../vendor/imgui/imgui_internal.h"
#include "../render/blur.hpp"
#include <vector>
#include <string>
#include <algorithm>
#include <mutex>
#include <cmath>
#include <ctime>
#include <chrono>
namespace takeui {
inline ImU32 accent() { return ImGui::GetColorU32(ImGuiCol_SliderGrab); }
inline ImU32 accent_hov() { return ImGui::GetColorU32(ImGuiCol_SliderGrabActive); }
inline ImVec4 accentA(float a = 1.0f) { return ImVec4(0.0f, 0.75f, 1.0f, a); }
inline ImVec4 accentB(float a = 1.0f) { return ImVec4(0.20f, 0.82f, 1.0f, a); }
inline void particles() {
    ImVec2 screen = { (float)GetSystemMetrics(SM_CXSCREEN), (float)GetSystemMetrics(SM_CYSCREEN) };
    static ImVec2 ppos[40]; static ImVec2 ptgt[40]; static float pspd[40]; static float prad[40];
    for (int i = 1; i < 36; i++) {
        if (ppos[i].x == 0 || ppos[i].y == 0) {
            ppos[i].x = (float)(rand() % (int)screen.x + 1); ppos[i].y = 15.f;
            pspd[i] = (float)(1 + rand() % 18); prad[i] = (float)(rand() % 3);
            ptgt[i].x = (float)(rand() % (int)screen.x); ptgt[i].y = screen.y * 2;
        }
        ppos[i] = ImLerp(ppos[i], ptgt[i], ImGui::GetIO().DeltaTime * (pspd[i] / 60));
        if (ppos[i].y > screen.y) { ppos[i].x = 0; ppos[i].y = 0; }
        ImGui::GetBackgroundDrawList()->AddCircleFilled(ppos[i], prad[i], ImGui::GetColorU32(ImGuiCol_SliderGrab, 0.27f));
    }
}
inline void CenterSeparator(const char* label) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;
    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID(label);
    ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImGui::CalcItemSize(ImVec2(-0.1f, g.FontSize), label_size.x, g.FontSize);
    const ImRect bb(pos, pos + size);
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id)) return;
    float cy = pos.y + size.y * 0.5f;
    float gap = g.Style.ItemInnerSpacing.x * 2.0f;
    float text_x0 = pos.x + (size.x - label_size.x) * 0.5f;
    float text_x1 = text_x0 + label_size.x;
    ImU32 line = ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_Border));
    if (text_x0 - gap > pos.x)
        window->DrawList->AddLine(ImVec2(pos.x, cy), ImVec2(text_x0 - gap, cy), line, 1.0f);
    const ImU32 title_col = ImGui::GetColorU32(ImGuiCol_Text);
    window->DrawList->AddText(ImVec2(text_x0 + 1.0f, pos.y), title_col, label);
    window->DrawList->AddText(ImVec2(text_x0, pos.y), title_col, label);
    if (text_x1 + gap < pos.x + size.x)
        window->DrawList->AddLine(ImVec2(text_x1 + gap, cy), ImVec2(pos.x + size.x, cy), line, 1.0f);
}
inline void TabIcon(ImDrawList* dl, ImVec2 c, int idx, ImU32 col) {
    switch (idx) {
    case 0:
        dl->AddCircle(c, 7.0f, col, 20, 1.5f);
        dl->AddLine(c - ImVec2(11, 0), c - ImVec2(4, 0), col, 1.5f);
        dl->AddLine(c + ImVec2(4, 0), c + ImVec2(11, 0), col, 1.5f);
        dl->AddLine(c - ImVec2(0, 11), c - ImVec2(0, 4), col, 1.5f);
        dl->AddLine(c + ImVec2(0, 4), c + ImVec2(0, 11), col, 1.5f);
        dl->AddCircleFilled(c, 1.6f, col, 8);
        break;
    case 1:
        dl->AddLine(c + ImVec2(-9, 0), c + ImVec2(0, -5), col, 1.5f);
        dl->AddLine(c + ImVec2(0, -5), c + ImVec2(9, 0), col, 1.5f);
        dl->AddLine(c + ImVec2(9, 0), c + ImVec2(0, 5), col, 1.5f);
        dl->AddLine(c + ImVec2(0, 5), c + ImVec2(-9, 0), col, 1.5f);
        dl->AddCircleFilled(c, 1.8f, col, 10);
        break;
    case 2:
        dl->AddCircle(c - ImVec2(0, 5), 3.4f, col, 14, 1.5f);
        dl->AddLine(c + ImVec2(-6, 8), c + ImVec2(6, 8), col, 1.5f);
        dl->AddLine(c + ImVec2(-6, 8), c + ImVec2(-6, 3), col, 1.5f);
        dl->AddLine(c + ImVec2(6, 8), c + ImVec2(6, 3), col, 1.5f);
        break;
    case 3:
        dl->AddLine(c + ImVec2(-9, -5), c + ImVec2(9, -5), col, 1.5f);
        dl->AddLine(c + ImVec2(-9, 0), c + ImVec2(5, 0), col, 1.5f);
        dl->AddLine(c + ImVec2(-9, 5), c + ImVec2(9, 5), col, 1.5f);
        dl->AddLine(c + ImVec2(5, -4), c + ImVec2(9, 0), col, 1.5f);
        dl->AddLine(c + ImVec2(5, 4), c + ImVec2(9, 0), col, 1.5f);
        break;
    case 4:
        dl->AddRect(c + ImVec2(-8, -5), c + ImVec2(8, 6), col, 1.0f, 0, 1.5f);
        dl->AddRectFilled(c + ImVec2(-8, -8), c + ImVec2(-1, -5), col, 1.0f);
        dl->AddLine(c + ImVec2(-4, -1), c + ImVec2(4, -1), col, 1.0f);
        dl->AddLine(c + ImVec2(-4, 2), c + ImVec2(1, 2), col, 1.0f);
        break;
    case 5:
        dl->AddLine(c + ImVec2(-2, -7), c + ImVec2(-8, 0), col, 1.8f);
        dl->AddLine(c + ImVec2(-8, 0), c + ImVec2(-2, 7), col, 1.8f);
        dl->AddLine(c + ImVec2(2, -7), c + ImVec2(8, 0), col, 1.8f);
        dl->AddLine(c + ImVec2(8, 0), c + ImVec2(2, 7), col, 1.8f);
        dl->AddLine(c + ImVec2(3, 8), c + ImVec2(-3, -8), col, 1.5f);
        break;
    case 6:
        dl->AddCircle(c, 4.5f, col, 16, 1.5f);
        for (int k = 0; k < 4; k++) {
            float a = k * 3.14159f / 4.0f;
            ImVec2 d(cosf(a), sinf(a));
            dl->AddLine(c + d * 4.5f, c + d * 8.0f, col, 1.5f);
        }
        break;
    default:
        for (int k = -1; k <= 1; k++) {
            float y = c.y + k * 5.0f;
            dl->AddLine(ImVec2(c.x - 8, y), ImVec2(c.x + 8, y), col, 1.2f);
            dl->AddCircleFilled(ImVec2(c.x + k * 3.0f, y), 2.2f, col, 10);
        }
        break;
    }
}
inline bool TabBtn(bool selected, int idx, const char* label, const char* hint, float tab_size, const ImVec2& size = ImVec2(40, 40)) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;
    ImGuiContext& g = *GImGui; const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 total = size; if (tab_size > 10) total.x = size.x + tab_size - 10;
    const ImRect bb(pos, pos + total);
    ImGui::ItemSize(total, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id)) return false;
    bool hovered, held; bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    float ba = blur::shell_alpha();
    ImU32 bg = selected ? IM_COL32(0, 120, 170, 160) : hovered ? IM_COL32(0, 90, 130, 130) : IM_COL32(35, 40, 48, (int)(160 * ba));
    if (ba < 1.0f && !selected) blur::glass_rect(bb.Min.x, bb.Min.y, bb.GetWidth(), bb.GetHeight(), style.FrameRounding);
    window->DrawList->AddRectFilled(bb.Min, bb.Max, bg, style.FrameRounding);
    if (selected) window->DrawList->AddRect(bb.Min, bb.Max, accent(), style.FrameRounding);
    ImU32 fg = selected ? IM_COL32(255, 255, 255, 255) : hovered ? IM_COL32(220, 240, 250, 255) : IM_COL32(160, 180, 195, 255);
    TabIcon(window->DrawList, pos + ImVec2(20, 20), idx, fg);
    if (tab_size > 100 && label) {
        ImVec2 tsz = ImGui::CalcTextSize(label);
        const ImVec2 tp = pos + ImVec2(50, size.y / 2 - tsz.y / 2);
        window->DrawList->AddText(tp + ImVec2(1, 0), IM_COL32(255, 255, 255, 255), label);
        window->DrawList->AddText(tp, IM_COL32(255, 255, 255, 255), label);
    }
    if (hovered) { if (hint && hint[0]) ImGui::SetTooltip("%s - %s", label ? label : "", hint); else if (label) ImGui::SetTooltip("%s", label); }
    return pressed;
}
inline const char* rank_name(int r) { return r == 2 ? "PRO" : r == 1 ? "Member" : r == 4 ? "Free" : "DEV"; }
inline void clock_str(char* b, size_t n, bool h24) {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{}; localtime_s(&tm, &t);
    std::strftime(b, n, h24 ? "%H:%M:%S" : "%I:%M:%S %p", &tm);
}
inline void date_str(char* b, size_t n) {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{}; localtime_s(&tm, &t);
    std::strftime(b, n, "%d/%m/%Y", &tm);
}
}

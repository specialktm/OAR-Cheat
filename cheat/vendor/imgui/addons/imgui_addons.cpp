//============ Copyright KiwiHax, All rights reserved ============//
//
//  Purpose: Custom modern widget set
//
//================================================================//

#include "../imgui_internal.h"
#include "imgui_addons.h"
#include "../../../source/render/blur.hpp"

#include <map>
#include <string>
#include <cstdio>
#include <cstring>
#include <algorithm>

using namespace ImGui;

// ------------------------------------------------------------------ //
//  Palette helpers
// ------------------------------------------------------------------ //
static ImVec4 Pal(const ImU32 hex, const float alpha = 1.0f)
{
    return ImAdd::HexToColorVec4(hex, alpha);
}

static ImU32 PalU(const ImU32 hex, const float alpha = 1.0f)
{
    ImVec4 c = ImAdd::HexToColorVec4(hex, alpha);
    return ImGui::ColorConvertFloat4ToU32(c);
}

static float PalLerp(const float a, const float b, const float t)
{
    return a + (b - a) * t;
}

// ------------------------------------------------------------------ //

ImVec4 ImAdd::HexToColorVec4(unsigned int hex_color, float alpha) {
    ImVec4 color;

    color.x = ((hex_color >> 16) & 0xFF) / 255.0f;
    color.y = ((hex_color >> 8) & 0xFF) / 255.0f;
    color.z = (hex_color & 0xFF) / 255.0f;
    color.w = alpha;

    return color;
}

void ImAdd::SeparatorText(const char* label, float thickness)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(ImVec2(-0.1f, g.FontSize), label_size.x, g.FontSize);

    const ImRect total_bb(pos, pos + size);
    ItemSize(total_bb);
    if (!ItemAdd(total_bb, id)) {
        return;
    }

    const float dot_radius = 2.5f;
    const float text_offset = dot_radius * 2.0f + style.ItemInnerSpacing.x;

    window->DrawList->AddCircleFilled(pos + ImVec2(dot_radius + 1.0f, size.y / 2.0f), dot_radius + 3.0f, PalU(PAL_ACCENT, 0.20f));
    window->DrawList->AddCircleFilled(pos + ImVec2(dot_radius + 1.0f, size.y / 2.0f), dot_radius, PalU(PAL_ACCENT));

    PushStyleColor(ImGuiCol_Text, Pal(PAL_TEXT_DIM));
    RenderText(pos + ImVec2(text_offset, 0), label);
    ImGui::PopStyleColor();

    if (thickness > 0)
    {
        window->DrawList->AddLine(pos + ImVec2(text_offset + label_size.x + style.ItemInnerSpacing.x * 2.0f, size.y / 2.0f), pos + ImVec2(size.x - thickness, size.y / 2.0f), PalU(PAL_BORDER), thickness);
    }
}

void ImAdd::VSeparator(float margin, float thickness)
{
    if (thickness <= 0)
        return;

    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(ImVec2(thickness, -0.1f), thickness, thickness);

    const ImRect bb(pos, pos + size);
    const ImRect bb_rect(pos + ImVec2(0, margin), pos + size - ImVec2(0, margin));

    ItemSize(ImVec2(thickness, 0.0f));
    if (!ItemAdd(bb, 0))
        return;

    window->DrawList->AddRectFilled(bb_rect.Min, bb_rect.Max, PalU(PAL_BORDER));
}

void ImAdd::DoubleText(ImVec4 color1, ImVec4 color2, const char* label1, const char* label2)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(std::string(label1 + std::string(label2)).c_str());
    const ImVec2 label1_size = CalcTextSize(label1, NULL, true);
    const ImVec2 label2_size = CalcTextSize(label2, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(ImVec2(-0.1f, g.FontSize), label1_size.x + label2_size.x, g.FontSize);

    const ImRect total_bb(pos, pos + size);
    ItemSize(total_bb);
    if (!ItemAdd(total_bb, id)) {
        return;
    }

    PushStyleColor(ImGuiCol_Text, color1);
    RenderText(pos, label1);
    PopStyleColor();

    PushStyleColor(ImGuiCol_Text, color2);
    RenderText(pos + ImVec2(size.x - ImGui::CalcTextSize(label2).x, 0), label2);
    PopStyleColor();
}

bool ImAdd::SelectableLabel(const char* label, bool selected, bool centered, const ImVec2& size_arg)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    
    // Calculate size: use full available width if size_arg.x is 0 or small
    float item_width = size_arg.x;
    if (item_width <= 1.0f)
        item_width = GetContentRegionAvail().x;
    float item_height = size_arg.y > 1.0f ? size_arg.y : label_size.y + style.FramePadding.y * 2.0f;
    
    ImVec2 size = ImVec2(item_width, item_height);

    const ImRect total_bb(pos, pos + size);
    ItemSize(size);
    if (!ItemAdd(total_bb, id))
        return false;

    // Behaviors
    bool hovered, held;
    bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);

    // Animations
    struct stColors_State {
        ImColor Label;
        ImColor Hover;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.Label = selected ? Pal(PAL_ACCENT_HOV) : Pal(PAL_TEXT_DIM);
        it_anim->second.Hover = ImColor(0, 0, 0, 0);
    }

    ImColor target_label = selected ? ImColor(Pal(PAL_ACCENT_HOV)) : ImColor(hovered ? Pal(PAL_TEXT) : Pal(PAL_TEXT_DIM));
    ImColor target_hover = ImColor(0, 0, 0, 0);
    if (hovered) target_hover = ImColor(255, 255, 255, 10);
    if (selected) target_hover = ImColor(255, 255, 255, 14);

    const float t = 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime;
    it_anim->second.Label.Value = ImLerp(it_anim->second.Label.Value, target_label.Value, t);
    it_anim->second.Hover.Value = ImLerp(it_anim->second.Hover.Value, target_hover.Value, t);

    RenderNavCursor(total_bb, id);

    if (it_anim->second.Hover.Value.w > 0.01f)
    {
        window->DrawList->AddRectFilled(total_bb.Min, total_bb.Max, it_anim->second.Hover, 3.0f);
    }

    // Full outline when selected
    if (selected)
    {
        window->DrawList->AddRect(total_bb.Min, total_bb.Max, PalU(PAL_ACCENT), 3.0f, 0, 1.5f);
    }

    ImVec2 text_pos = pos + ImTrunc(ImVec2(centered ? (size.x / 2 - label_size.x / 2) : (style.FramePadding.x + (selected || hovered ? 2.0f : 0.0f)), size.y / 2 - label_size.y / 2));
    PushStyleColor(ImGuiCol_Text, it_anim->second.Label.Value);
    RenderText(text_pos, label);
    PopStyleColor();

    if (selected)
    {
        const float check_sz = g.FontSize * 0.6f;
        ImVec2 check_pos = total_bb.Max - ImVec2(check_sz + style.FramePadding.x * 2.0f, 0.0f);
        check_pos.y = total_bb.Min.y + (size.y - check_sz) * 0.5f;
        ImAdd::RenderCheckMark(window->DrawList, check_pos, PalU(PAL_ACCENT), check_sz);
    }

    return pressed;
}

bool ImAdd::SelectableFrame(const char* label, bool selected, bool centered, const ImVec2& size_arg)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(size_arg, label_size.x, label_size.y);

    const ImRect total_bb(pos, pos + size);
    ItemSize(size);
    if (!ItemAdd(total_bb, id))
        return false;

    // Behaviors
    bool hovered, held;
    bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);

    // Colors
    ImVec4 colLabel = GetStyleColorVec4(hovered || selected ? ImGuiCol_Text : ImGuiCol_TextDisabled);
    ImVec4 colFrame = selected ? Pal(PAL_ACCENT_ACT) : Pal(hovered ? PAL_FRAME_HOV : PAL_FRAME);
    ImVec4 colBorder = selected ? Pal(PAL_ACCENT_HOV) : Pal(hovered ? PAL_BORDER_HI : PAL_BORDER);

    // Animations
    struct stColors_State {
        ImColor Label;
        ImColor Frame;
        ImColor Border;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.Label = colLabel;
        it_anim->second.Frame = colFrame;
        it_anim->second.Border = colBorder;
    }

    const float t = 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime;
    it_anim->second.Label.Value = ImLerp(it_anim->second.Label.Value, colLabel, t);
    it_anim->second.Frame.Value = ImLerp(it_anim->second.Frame.Value, colFrame, t);
    it_anim->second.Border.Value = ImLerp(it_anim->second.Border.Value, colBorder, t);

    RenderNavCursor(total_bb, id);

    window->DrawList->AddRectFilled(pos, pos + size, it_anim->second.Frame, 4.0f);

    window->DrawList->AddRect(pos, pos + size, it_anim->second.Border, 4.0f, 0, 1.0f);

    PushStyleColor(ImGuiCol_Text, it_anim->second.Label.Value);
    RenderText(pos + ImTrunc(ImVec2(centered ? (size.x / 2 - label_size.x / 2) : style.FramePadding.x, size.y / 2 - label_size.y / 2)), label);
    PopStyleColor();

    return pressed;
}

bool ImAdd::Tab(const char* label, int* v, int current_id, ImDrawFlags draw_flags, const ImVec2& size_arg)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

    const ImRect total_bb(pos, pos + size);
    ItemSize(size);
    if (!ItemAdd(total_bb, id))
        return false;

    // Behaviors
    bool hovered, held;
    bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);
    if (pressed)
        *v = current_id;

    bool active = (*v == current_id);

    // Animations
    struct stColors_State {
        ImColor Label;
        ImColor Frame;
        ImColor Border;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.Label = active ? ImColor(Pal(PAL_TEXT)) : ImColor(Pal(PAL_TEXT_DIM));
        it_anim->second.Frame = active ? ImColor(Pal(PAL_ACCENT)) : ImColor(Pal(PAL_FRAME));
        it_anim->second.Border = active ? ImColor(Pal(PAL_ACCENT_HOV)) : ImColor(Pal(PAL_BORDER));
    }

    ImColor target_label = active ? ImColor(Pal(PAL_TEXT)) : ImColor(hovered ? Pal(PAL_TEXT) : Pal(PAL_TEXT_DIM));
    ImColor target_frame = active ? ImColor(Pal(PAL_ACCENT)) : ImColor(hovered ? Pal(PAL_FRAME_HOV) : Pal(PAL_FRAME));
    ImColor target_border = active ? ImColor(Pal(PAL_ACCENT_HOV)) : ImColor(hovered ? Pal(PAL_BORDER_HI) : Pal(PAL_BORDER));

    const float t = 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime;
    it_anim->second.Label.Value = ImLerp(it_anim->second.Label.Value, target_label.Value, t);
    it_anim->second.Frame.Value = ImLerp(it_anim->second.Frame.Value, target_frame.Value, t);
    it_anim->second.Border.Value = ImLerp(it_anim->second.Border.Value, target_border.Value, t);

    RenderNavCursor(total_bb, id);

    float ta = blur::shell_alpha();
    ImColor tfr = it_anim->second.Frame;
    if (!active && ta < 1.0f)
    {
        tfr.Value.w *= ta;
        blur::glass_rect(total_bb.Min.x, total_bb.Min.y, total_bb.GetWidth(), total_bb.GetHeight(), style.FrameRounding + 1.0f);
    }

    if (active)
    {
        window->DrawList->AddRectFilled(total_bb.Min - ImVec2(1, 1), total_bb.Max + ImVec2(1, 1), PalU(PAL_ACCENT, 0.22f), style.FrameRounding + 2.0f, draw_flags);
    }

    window->DrawList->AddRectFilled(total_bb.Min, total_bb.Max, tfr, style.FrameRounding + 1.0f, draw_flags);
    window->DrawList->AddRect(total_bb.Min, total_bb.Max, it_anim->second.Border, style.FrameRounding + 1.0f, draw_flags, 1.0f);

    PushStyleColor(ImGuiCol_Text, it_anim->second.Label.Value);
    RenderText(pos + size / 2 - label_size / 2 - ImVec2(0, 1), label);
    ImGui::PopStyleColor();

    return pressed;
}

bool ImAdd::SideTab(const char* label, int* v, int current_id, const ImVec2& size_arg)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

    const ImRect total_bb(pos, pos + size);
    ItemSize(size);
    if (!ItemAdd(total_bb, id))
        return false;

    // Behaviors
    bool hovered, held;
    bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);
    if (pressed)
        *v = current_id;

    bool active = (*v == current_id);

    // Animations
    struct stColors_State {
        ImColor Label;
        ImColor Frame;
        ImColor Bar;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.Label = active ? ImColor(Pal(PAL_ACCENT_HOV)) : ImColor(Pal(PAL_TEXT_DIM));
        it_anim->second.Frame = ImColor(0, 0, 0, 0);
        it_anim->second.Bar = active ? ImColor(Pal(PAL_ACCENT)) : ImColor(Pal(PAL_ACCENT, 0.0f));
    }

    ImColor target_label = active ? ImColor(Pal(PAL_ACCENT_HOV)) : ImColor(hovered ? Pal(PAL_TEXT) : Pal(PAL_TEXT_DIM));
    ImColor target_frame = ImColor(0, 0, 0, 0);
    if (hovered) target_frame = ImColor(255, 255, 255, 8);
    if (active) target_frame = ImColor(255, 255, 255, 22);
    ImColor target_bar = active ? ImColor(Pal(PAL_ACCENT)) : ImColor(Pal(PAL_ACCENT, 0.0f));

    const float t = 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime;
    it_anim->second.Label.Value = ImLerp(it_anim->second.Label.Value, target_label.Value, t);
    it_anim->second.Frame.Value = ImLerp(it_anim->second.Frame.Value, target_frame.Value, t);
    it_anim->second.Bar.Value = ImLerp(it_anim->second.Bar.Value, target_bar.Value, t);

    RenderNavCursor(total_bb, id);

    window->DrawList->AddRectFilled(total_bb.Min, total_bb.Max, it_anim->second.Frame, 2.0f);

    const ImVec4 bar_v4 = it_anim->second.Bar.Value;
    if (bar_v4.w > 0.01f)
    {
        window->DrawList->AddRectFilled(ImVec2(total_bb.Min.x, total_bb.Min.y + 6.0f), ImVec2(total_bb.Min.x + 2.0f, total_bb.Max.y - 6.0f), it_anim->second.Bar, 1.0f);
    }

    PushStyleColor(ImGuiCol_Text, it_anim->second.Label.Value);
    RenderText(pos + ImVec2(style.FramePadding.x + 6.0f, size.y / 2 - label_size.y / 2), label);
    ImGui::PopStyleColor();

    return pressed;
}

bool ImAdd::Button(const char* label, const ImVec2& size_arg, ImDrawFlags draw_flags, ImGuiButtonFlags button_flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

    const ImRect total_bb(pos, pos + size);
    ItemSize(size);
    if (!ItemAdd(total_bb, id))
        return false;

    // Behaviors
    bool hovered, held;
    bool pressed = ButtonBehavior(total_bb, id, &hovered, &held, button_flags);

    // Colors
    ImVec4 colFrame = (hovered && held) ? Pal(PAL_ACCENT_ACT) : hovered ? Pal(PAL_ACCENT) : Pal(PAL_FRAME);
    ImVec4 colBorder = hovered ? Pal(PAL_ACCENT_HOV) : Pal(PAL_BORDER);

    // Animations
    struct stColors_State {
        ImColor Frame;
        ImColor Border;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.Frame = colFrame;
        it_anim->second.Border = colBorder;
    }

    const float t = 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime;
    it_anim->second.Frame.Value = ImLerp(it_anim->second.Frame.Value, colFrame, t);
    it_anim->second.Border.Value = ImLerp(it_anim->second.Border.Value, colBorder, t);

    RenderNavCursor(total_bb, id);

    float ba = blur::shell_alpha();
    if (ba < 1.0f) blur::glass_rect(total_bb.Min.x, total_bb.Min.y, total_bb.GetWidth(), total_bb.GetHeight(), 5.0f);
    ImColor fr = it_anim->second.Frame;
    fr.Value.w *= ba;
    if (hovered)
    {
        window->DrawList->AddRectFilled(total_bb.Min - ImVec2(2, 2), total_bb.Max + ImVec2(2, 2), PalU(PAL_ACCENT, (held ? 0.10f : 0.18f) * ba), 7.0f, draw_flags);
    }

    window->DrawList->AddRectFilled(total_bb.Min, total_bb.Max, fr, 5.0f, draw_flags);
    window->DrawList->AddLine(total_bb.Min + ImVec2(6.0f, 1.0f), ImVec2(total_bb.Max.x - 6.0f, total_bb.Min.y + 1.0f), IM_COL32(255, 255, 255, 18), 1.0f);
    window->DrawList->AddRect(total_bb.Min, total_bb.Max, it_anim->second.Border, 5.0f, draw_flags, 1.0f);

    PushStyleColor(ImGuiCol_Text, hovered ? Pal(PAL_TEXT) : Pal(PAL_TEXT_DIM));
    RenderTextClipped(total_bb.Min + style.FramePadding + ImVec2(4.0f, 0.0f), total_bb.Max - style.FramePadding, label, NULL, &label_size, ImVec2(0.0f, 0.5f), &total_bb);
    ImGui::PopStyleColor();

    return pressed;
}

bool ImAdd::CheckBox(const char* label, bool* checked)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    // Calamari look: fixed 16px box, scheme fill, dark centered check.
    const float box_sz = 16.0f;

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(box_sz + style.ItemInnerSpacing.x + label_size.x, box_sz);

    const ImRect box_bb(pos, pos + ImVec2(box_sz, box_sz));
    const ImRect total_bb(pos, pos + size);
    ItemSize(size);
    if (!ItemAdd(total_bb, id))
        return false;

    // Behaviors
    bool hovered, held;
    bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);

    if (pressed) *checked = !*checked;

    // Animations
    struct stColors_State {
        ImColor Accent;
        ImColor Label;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.Accent = Pal(PAL_ACCENT, 0.0f);
        it_anim->second.Label = Pal(PAL_TEXT_DIM);
    }

    ImColor target_accent = *checked ? ImColor(Pal(PAL_ACCENT)) : ImColor(Pal(PAL_ACCENT, 0.0f));
    ImColor target_label = *checked ? ImColor(Pal(PAL_TEXT)) : ImColor(Pal(PAL_TEXT_DIM));

    const float t = 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime;
    it_anim->second.Accent.Value = ImLerp(it_anim->second.Accent.Value, target_accent.Value, t);
    it_anim->second.Label.Value = ImLerp(it_anim->second.Label.Value, target_label.Value, t);

    RenderNavCursor(total_bb, id);

    const float alpha = ImClamp(it_anim->second.Accent.Value.w, 0.0f, 1.0f);

    // Border + scheme fill (fill fades in with the enabled animation).
    window->DrawList->AddRect(box_bb.Min, box_bb.Max, PalU(PAL_BORDER), 2.0f);
    window->DrawList->AddRectFilled(box_bb.Min, box_bb.Max, PalU(PAL_ACCENT, alpha), 2.0f);
    // Dark check mark centered in the filled box, like calamari's ChildBg check.
    if (alpha > 0.02f)
    {
        ImU32 check_col = PalU(PAL_BG_CHILD, alpha);
        ImAdd::RenderCheckMark(window->DrawList, box_bb.GetCenter() - ImVec2(4, 4), check_col, 8.0f);
    }

    PushStyleColor(ImGuiCol_Text, it_anim->second.Label.Value);
    RenderText(pos + ImVec2(box_sz + style.ItemInnerSpacing.x, (box_sz - label_size.y) * 0.5f), label);
    PopStyleColor();

    return pressed;
}

bool ImAdd::Combo(const char* label, int* selected_index, std::vector<const char*> items)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    const float width = CalcItemWidth();
    const float height = GetFrameHeight();

    int items_count = items.size();

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(width, height + (label_size.x > 0 ? (g.FontSize + style.ItemInnerSpacing.y) : 0.0f));

    const ImRect total_bb(pos, pos + size);
    const ImRect combo_bb(pos + ImVec2(0.0f, label_size.x > 0 ? (g.FontSize + style.ItemInnerSpacing.y) : 0.0f), pos + size);
    ItemSize(size);
    if (!ItemAdd(total_bb, id))
        return false;

    // Behaviors
    bool hovered, held;
    bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);

    std::string popup_str_id = std::string(std::string(label) + "::combo_popup");

    if (pressed)
    {
        OpenPopup(popup_str_id.c_str());
    }

    const bool popup_open = IsPopupOpen(popup_str_id.c_str());

    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
    PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 2.0f));
    if (BeginPopupEx(GetID(popup_str_id.c_str()), ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove))
    {
        SetWindowPos(combo_bb.Min + ImVec2(0, height + style.FramePadding.y), ImGuiCond_Always);
        SetWindowSize(ImVec2(width, ImGui::GetFontSize() * items_count + 12.0f + 2.0f * items_count), ImGuiCond_Always);

        for (int i = 0; i < items_count; i++)
        {
            if (ImAdd::SelectableLabel(items[i], i == *selected_index, false, ImVec2(GetContentRegionAvail().x, GetFontSize() + 2.0f)))
            {
                *selected_index = i;
                CloseCurrentPopup();
            }
        }

        EndPopup();
    }
    PopStyleVar(2);

    // Animations
    struct stColors_State {
        ImColor Frame;
        ImColor Border;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.Frame = Pal(PAL_FRAME);
        it_anim->second.Border = Pal(PAL_BORDER);
    }

    ImColor target_frame = (hovered && held) ? ImColor(Pal(PAL_FRAME_ACT)) : hovered ? ImColor(Pal(PAL_FRAME_HOV)) : ImColor(Pal(PAL_FRAME));
    ImColor target_border = popup_open ? ImColor(Pal(PAL_ACCENT)) : hovered ? ImColor(Pal(PAL_BORDER_HI)) : ImColor(Pal(PAL_BORDER));

    const float t = 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime;
    it_anim->second.Frame.Value = ImLerp(it_anim->second.Frame.Value, target_frame.Value, t);
    it_anim->second.Border.Value = ImLerp(it_anim->second.Border.Value, target_border.Value, t);

    RenderNavCursor(total_bb, id);

    window->DrawList->AddRectFilled(combo_bb.Min, combo_bb.Max, it_anim->second.Frame, 4.0f);
    window->DrawList->AddRect(combo_bb.Min, combo_bb.Max, it_anim->second.Border, 4.0f, 0, 1.0f);

    std::string preview_item;
    if (*selected_index > items.size()) {
        preview_item = "*unknown item*";
    }
    else
    {
        preview_item = items[*selected_index];
    }

    window->DrawList->AddText(combo_bb.Min + style.FramePadding + ImVec2(2.0f, 0.0f), popup_open ? PalU(PAL_ACCENT_HOV) : PalU(PAL_TEXT), preview_item.c_str());

    ImVec2 arrow_pos(combo_bb.Max.x - GetFontSize() - style.FramePadding.x - 2.0f, combo_bb.Min.y + style.FramePadding.y);
    ImAdd::RenderArrow(window->DrawList, arrow_pos, popup_open ? PalU(PAL_ACCENT_HOV) : PalU(PAL_TEXT_DIM), ImGui::GetFontSize(), ImGuiDir_Down);

    if (label_size.x > 0)
    {
        PushStyleColor(ImGuiCol_Text, popup_open ? Pal(PAL_ACCENT_HOV) : Pal(PAL_TEXT_DIM));
        RenderText(pos, label);
        PopStyleColor();
    }

    return pressed;
}

bool ImAdd::MultiCombo(const char* label, int* selected_mask, std::vector<const char*> items)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    const float width = CalcItemWidth();
    const float height = GetFrameHeight();

    int items_count = items.size();

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(width, height + (label_size.x > 0 ? (g.FontSize + style.ItemInnerSpacing.y) : 0.0f));

    const ImRect total_bb(pos, pos + size);
    const ImRect combo_bb(pos + ImVec2(0.0f, label_size.x > 0 ? (g.FontSize + style.ItemInnerSpacing.y) : 0.0f), pos + size);
    ItemSize(size);
    if (!ItemAdd(total_bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);

    std::string popup_str_id = std::string(std::string(label) + "::multicombo_popup");

    if (pressed)
    {
        OpenPopup(popup_str_id.c_str());
    }

    const bool popup_open = IsPopupOpen(popup_str_id.c_str());

    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
    PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 2.0f));
    if (BeginPopupEx(GetID(popup_str_id.c_str()), ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove))
    {
        SetWindowPos(combo_bb.Min + ImVec2(0, height + style.FramePadding.y), ImGuiCond_Always);
        SetWindowSize(ImVec2(width, ImGui::GetFontSize() * items_count + 12.0f + 2.0f * items_count), ImGuiCond_Always);

        bool value_changed = false;
        for (int i = 0; i < items_count; i++)
        {
            bool is_selected = (*selected_mask & (1 << i)) != 0;
            if (ImAdd::SelectableLabel(items[i], is_selected, false, ImVec2(GetContentRegionAvail().x, GetFontSize() + 2.0f)))
            {
                if (is_selected)
                {
                    *selected_mask &= ~(1 << i);
                }
                else
                {
                    *selected_mask |= (1 << i);
                }
                value_changed = true;
            }
        }

        EndPopup();
        PopStyleVar(2);

        if (value_changed)
        {
            return true;
        }
    }
    else
    {
        PopStyleVar(2);
    }

    // Animations
    struct stColors_State {
        ImColor Frame;
        ImColor Border;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.Frame = Pal(PAL_FRAME);
        it_anim->second.Border = Pal(PAL_BORDER);
    }

    ImColor target_frame = (hovered && held) ? ImColor(Pal(PAL_FRAME_ACT)) : hovered ? ImColor(Pal(PAL_FRAME_HOV)) : ImColor(Pal(PAL_FRAME));
    ImColor target_border = popup_open ? ImColor(Pal(PAL_ACCENT)) : hovered ? ImColor(Pal(PAL_BORDER_HI)) : ImColor(Pal(PAL_BORDER));

    const float t = 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime;
    it_anim->second.Frame.Value = ImLerp(it_anim->second.Frame.Value, target_frame.Value, t);
    it_anim->second.Border.Value = ImLerp(it_anim->second.Border.Value, target_border.Value, t);

    RenderNavCursor(total_bb, id);

    window->DrawList->AddRectFilled(combo_bb.Min, combo_bb.Max, it_anim->second.Frame, 4.0f);
    window->DrawList->AddRect(combo_bb.Min, combo_bb.Max, it_anim->second.Border, 4.0f, 0, 1.0f);

    std::string preview_item;
    int selected_count = 0;
    for (int i = 0; i < items_count; i++)
    {
        if (*selected_mask & (1 << i))
        {
            selected_count++;
        }
    }

    if (selected_count == 0)
    {
        preview_item = "None";
    }
    else
    {
        for (int i = 0; i < items_count; i++)
        {
            if (*selected_mask & (1 << i))
            {
                if (!preview_item.empty()) preview_item += ", ";
                preview_item += items[i];
            }
        }
        float avail_w = combo_bb.GetWidth() - style.FramePadding.x * 2.0f - GetFontSize() - 8.0f;
        if (avail_w > 20.0f)
        {
            bool cut = false;
            while (preview_item.size() > 3 && CalcTextSize(preview_item.c_str()).x > avail_w)
            {
                preview_item.pop_back();
                cut = true;
            }
            if (cut && preview_item.size() > 3)
                preview_item.replace(preview_item.size() - 3, 3, "...");
        }
    }

    window->DrawList->AddText(combo_bb.Min + style.FramePadding + ImVec2(2.0f, 0.0f), popup_open ? PalU(PAL_ACCENT_HOV) : PalU(PAL_TEXT), preview_item.c_str());

    ImVec2 arrow_pos(combo_bb.Max.x - GetFontSize() - style.FramePadding.x - 2.0f, combo_bb.Min.y + style.FramePadding.y);
    ImAdd::RenderArrow(window->DrawList, arrow_pos, popup_open ? PalU(PAL_ACCENT_HOV) : PalU(PAL_TEXT_DIM), ImGui::GetFontSize(), ImGuiDir_Down);

    if (label_size.x > 0)
    {
        PushStyleColor(ImGuiCol_Text, popup_open ? Pal(PAL_ACCENT_HOV) : Pal(PAL_TEXT_DIM));
        RenderText(pos, label);
        PopStyleColor();
    }

    return false;
}

bool ImAdd::InputText(ImTextureRef icon_texture, const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags)
{
    IM_ASSERT(!(flags & ImGuiInputTextFlags_Multiline)); // call InputTextMultiline()

    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    ImVec2 pos = GetCursorScreenPos();
    bool has_icon = icon_texture != nullptr;

    ImGui::BeginChild(label, ImVec2(CalcItemWidth(), GetFrameHeight()), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);

    const bool is_active = (g.ActiveId != 0) && ImGui::IsItemActive();

    ImVec4 frame_bg_color = GetStyleColorVec4(ImGuiCol_FrameBg);
    if (frame_bg_color.w > 0.0f) {
        window->DrawList->AddRectFilled(pos, pos + GetWindowSize(), GetColorU32(ImGuiCol_FrameBg), 4.0f);
    }

    ImU32 border_col = is_active ? PalU(PAL_ACCENT) : PalU(PAL_BORDER);
    window->DrawList->AddRect(pos, pos + GetWindowSize(), border_col, 4.0f, 0, 1.0f);

    if (has_icon) {
        window->DrawList->AddImage(icon_texture, pos + style.FramePadding, pos + ImVec2(GetWindowHeight(), GetWindowHeight()) - style.FramePadding, ImVec2(0, 0), ImVec2(1, 1), ImGui::GetColorU32(ImGuiCol_Text));
    }

    PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
    PushItemWidth(has_icon ? (GetWindowWidth() - GetFontSize() - style.FramePadding.x) : GetWindowWidth());
    if (has_icon) {
        SetCursorScreenPos(pos + ImVec2(GetFontSize() + style.FramePadding.x, 0));
    }
    bool input = InputTextEx(std::string("##" + std::string(label)).c_str(), NULL, buf, (int)buf_size, ImVec2(0, 0), flags);
    PopItemWidth();
    PopStyleVar();
    PopStyleColor();

    if (has_icon) {
        GetForegroundDrawList()->AddRectFilledMultiColor(pos + ImVec2(GetFontSize() + style.FramePadding.x, style.FrameBorderSize), pos + ImVec2(GetWindowHeight(), GetWindowHeight() - style.FrameBorderSize), GetColorU32(ImGuiCol_FrameBg), GetColorU32(ImGuiCol_FrameBg, 0.0f), GetColorU32(ImGuiCol_FrameBg, 0.0f), GetColorU32(ImGuiCol_FrameBg));
    }

    if (!IsItemActive() && strlen(buf) == 0) {
        window->DrawList->AddText(pos + ImVec2(has_icon ? GetWindowHeight() : style.FramePadding.x, style.FramePadding.y), GetColorU32(ImGuiCol_TextDisabled), label);
    }

    ImGui::EndChild();

    return input;
}

bool ImAdd::KeyBind(const char* str_id, ImGuiKey* k, const ImVec2& size_arg)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    ImGuiIO& io = g.IO;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(str_id);

    ImVec2 pos = window->DC.CursorPos;

    char buf_display[32] = "Unbinded";

    bool is_selecing = false;

    if (*k != 0 && g.ActiveId != id)
    {
        strcpy_s(buf_display, sizeof buf_display, GetKeyName(*k));
    }
    else if (g.ActiveId == id)
    {
        is_selecing = true;
        strcpy_s(buf_display, sizeof buf_display, "...");
    }

    ImVec2 size = CalcItemSize(size_arg, CalcTextSize(GetKeyName(*k)).x + style.FramePadding.x * 2.0f, g.FontSize + style.FramePadding.y * 2.0f);
    ImRect frame_bb(pos, pos + size);
    ImRect total_bb(pos, frame_bb.Max);

    ImGui::ItemSize(total_bb);
    if (!ImGui::ItemAdd(total_bb, id))
        return false;

    const bool hovered = ImGui::ItemHoverable(frame_bb, id, 0);

    // Colors
    ImVec4 colLabel = is_selecing ? Pal(PAL_ACCENT_HOV) : hovered ? Pal(PAL_TEXT) : Pal(PAL_TEXT_DIM);

    // Animations
    struct stColors_State {
        ImColor Label;
        ImColor Frame;
        ImColor Border;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.Label = colLabel;
        it_anim->second.Frame = Pal(PAL_FRAME);
        it_anim->second.Border = Pal(PAL_BORDER);
    }

    const float t = 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime;
    it_anim->second.Label.Value = ImLerp(it_anim->second.Label.Value, colLabel, t);
    it_anim->second.Frame.Value = ImLerp(it_anim->second.Frame.Value, is_selecing ? ImColor(Pal(PAL_ACCENT, 0.15f)) : ImColor(hovered ? Pal(PAL_FRAME_HOV) : Pal(PAL_FRAME)), t);
    it_anim->second.Border.Value = ImLerp(it_anim->second.Border.Value, is_selecing ? ImColor(Pal(PAL_ACCENT)) : ImColor(hovered ? Pal(PAL_BORDER_HI) : Pal(PAL_BORDER)), t);

    if (hovered)
    {
        ImGui::SetHoveredID(id);
        g.MouseCursor = ImGuiMouseCursor_Hand;
    }

    const bool user_clicked = hovered && IsMouseClicked(ImGuiMouseButton_Left);

    if (user_clicked)
    {
        ImGui::SetActiveID(id, window);
        ImGui::FocusWindow(window);
    }
    else if (IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (g.ActiveId == id)
            ImGui::ClearActiveID();
    }

    bool value_changed = false;
    int key = *k;

    if (hovered && IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (g.ActiveId != id)
        {
            memset(io.MouseDown, 0, sizeof(io.MouseDown));
            SetActiveID(id, window);
            FocusWindow(window);
        }
    }

    if (IsMouseClicked(ImGuiMouseButton_Left) && g.ActiveId == id && !hovered)
    {
        ClearActiveID();
    }

    static std::map<ImGuiID, int> keybind_frame_count;
    static std::map<ImGuiID, bool> keybind_just_activated;

    if (g.ActiveId == id)
    {
        auto it = keybind_frame_count.find(id);
        if (it == keybind_frame_count.end())
        {
            keybind_frame_count[id] = 0;
            keybind_just_activated[id] = true;
            memset(io.MouseDown, 0, sizeof(io.MouseDown));
        }

        if (keybind_just_activated[id])
        {
            keybind_frame_count[id]++;
            if (keybind_frame_count[id] > 3)
            {
                keybind_just_activated[id] = false;
            }
        }

        if (!keybind_just_activated[id])
        {
            for (int btn = 0; btn < 5; btn++)
            {
                if (ImGui::IsMouseClicked(btn))
                {
                    *k = (ImGuiKey)(ImGuiKey_MouseLeft + btn);
                    value_changed = true;
                    ImGui::ClearActiveID();
                    keybind_frame_count.erase(id);
                    keybind_just_activated.erase(id);
                    break;
                }
            }

            if (!value_changed)
            {
                for (int i = ImGuiKey_NamedKey_BEGIN; i < ImGuiKey_NamedKey_END; i++)
                {
                    ImGuiKey key_test = (ImGuiKey)i;

                    if ((key_test >= ImGuiKey_MouseLeft && key_test <= ImGuiKey_MouseWheelY) || key_test == ImGuiKey_Escape)
                        continue;

                    if (IsKeyPressed(key_test))
                    {
                        *k = key_test;
                        value_changed = true;
                        ClearActiveID();
                        keybind_frame_count.erase(id);
                        keybind_just_activated.erase(id);
                        break;
                    }
                }
            }

            if (IsKeyPressed(ImGuiKey_Escape))
            {
                ClearActiveID();
                keybind_frame_count.erase(id);
                keybind_just_activated.erase(id);
            }
        }
    }
    else
    {
        keybind_frame_count.erase(id);
        keybind_just_activated.erase(id);
    }

    ImGui::RenderNavHighlight(total_bb, id);

    window->DrawList->AddRectFilled(frame_bb.Min, frame_bb.Max, it_anim->second.Frame, 4.0f);
    window->DrawList->AddRect(frame_bb.Min, frame_bb.Max, it_anim->second.Border, 4.0f, 0, 1.0f);

    ImVec2 buf_display_size = ImGui::CalcTextSize(buf_display, NULL, true);
    window->DrawList->AddText(pos + ImVec2(size.x - buf_display_size.x - style.FramePadding.x, size.y / 2 - buf_display_size.y / 2), it_anim->second.Label, buf_display);

    return value_changed;
}

bool ImAdd::SliderAlpha(const char* str_id, ImVec4& col)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    ImGuiIO& io = g.IO;
    const ImGuiID id = window->GetID(str_id);

    float width = CalcItemWidth();
    float height = GetFrameHeight();
    float half_height = ImTrunc(height / 2.0f);

    ImVec2 pos = window->DC.CursorPos;
    const ImVec2 size(width, height);

    const ImRect bb(pos, pos + size);

    ItemSize(bb);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    const float slider_min_x = bb.Min.x + half_height;
    const float slider_max_x = bb.Max.x - half_height;

    if (held)
    {
        const float slider_width = slider_max_x - slider_min_x;

        if (slider_width > 0.0f)
        {
            float mouse_x = ImClamp(io.MousePos.x, slider_min_x, slider_max_x);
            col.w = 1.0f - ((mouse_x - slider_min_x) / slider_width);
            col.w = ImClamp(col.w, 0.0f, 1.0f);
        }
    }

    ImColor col_rgb = ImVec4(col.x, col.y, col.z, 1.0f);
    ImColor col_rgb_width_alpha = ImVec4(col.x, col.y, col.z, col.w);

    window->DrawList->AddRectFilled(pos, pos + ImVec2(half_height, height), col_rgb, 4.0f, ImDrawFlags_RoundCornersLeft);
    RenderColorRectWithAlphaCheckerboard(window->DrawList, pos + ImVec2(half_height, 0), pos + size, 0, 1.0f, half_height, ImVec2(0.0f, 0.0f), 4.0f, ImDrawFlags_RoundCornersRight);
    window->DrawList->AddRectFilledMultiColor(pos + ImVec2(half_height, 0), pos + ImVec2(width - half_height, height), col_rgb, col_rgb & ~IM_COL32_A_MASK, col_rgb & ~IM_COL32_A_MASK, col_rgb);

    const float circle_radius = ImMax(ImTrunc(g.FontSize / 2.0f) - 2.0f, 1.0f);
    const float slider_width = slider_max_x - slider_min_x;
    float circle_x = slider_min_x + slider_width * (1.0f - col.w);

    RenderColorRectWithAlphaCheckerboard(window->DrawList, ImVec2(circle_x - circle_radius, pos.y + half_height - circle_radius), ImVec2(circle_x + circle_radius, pos.y + half_height + circle_radius), col_rgb_width_alpha, 1.0f, circle_radius, ImVec2(0.0f, 0.0f), circle_radius + 1.0f);
    window->DrawList->AddCircle(ImVec2(circle_x, pos.y + half_height), circle_radius + 1.0f, IM_COL32_BLACK, 0, 1.0f);

    return pressed;
}

bool ImAdd::SliderHue(const char* str_id, ImVec4& col)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    ImGuiIO& io = g.IO;
    const ImGuiID id = window->GetID(str_id);

    float width = CalcItemWidth();
    float height = GetFrameHeight();
    float half_height = ImTrunc(height / 2.0f);

    ImVec2 pos = window->DC.CursorPos;
    const ImVec2 size(width, height);

    const ImRect bb(pos, pos + size);

    ItemSize(bb);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    const float slider_min_x = bb.Min.x + half_height;
    const float slider_max_x = bb.Max.x - half_height;

    float h, s, v;
    ImGui::ColorConvertRGBtoHSV(col.x, col.y, col.z, h, s, v);

    if (held)
    {
        const float slider_width = slider_max_x - slider_min_x;

        if (slider_width > 0.0f)
        {
            float mouse_x = ImClamp(io.MousePos.x, slider_min_x, slider_max_x);
            h = (mouse_x - slider_min_x) / slider_width;
            h = ImClamp(h, 0.0f, 1.0f);

            ImGui::ColorConvertHSVtoRGB(h, s, v, col.x, col.y, col.z);
        }
    }

    const ImVec2 gradient_start = pos + ImVec2(half_height, 0);
    const ImVec2 gradient_end = pos + ImVec2(width - half_height, height);

    const int num_segments = 6;
    const float segment_width = (width - height) / num_segments;

    for (int i = 0; i < num_segments; i++)
    {
        float hue_start = i / (float)num_segments;
        float hue_end = (i + 1) / (float)num_segments;

        ImVec4 color_start, color_end;
        ImGui::ColorConvertHSVtoRGB(hue_start, 1.0f, 1.0f, color_start.x, color_start.y, color_start.z);
        ImGui::ColorConvertHSVtoRGB(hue_end, 1.0f, 1.0f, color_end.x, color_end.y, color_end.z);
        color_start.w = color_end.w = 1.0f;

        ImVec2 seg_start = gradient_start + ImVec2(segment_width * i, 0);
        ImVec2 seg_end = gradient_start + ImVec2(segment_width * (i + 1), height);

        window->DrawList->AddRectFilledMultiColor(
            seg_start, seg_end,
            ImColor(color_start), ImColor(color_end), ImColor(color_end), ImColor(color_start)
        );
    }

    ImVec4 left_color, right_color;
    ImGui::ColorConvertHSVtoRGB(0.0f, 1.0f, 1.0f, left_color.x, left_color.y, left_color.z);
    ImGui::ColorConvertHSVtoRGB(1.0f, 1.0f, 1.0f, right_color.x, right_color.y, right_color.z);
    left_color.w = right_color.w = 1.0f;

    window->DrawList->AddRectFilled(pos, pos + ImVec2(half_height, height), ImColor(left_color), 4.0f, ImDrawFlags_RoundCornersLeft);
    window->DrawList->AddRectFilled(pos + ImVec2(width - half_height, 0), pos + size, ImColor(right_color), 4.0f, ImDrawFlags_RoundCornersRight);

    const float circle_radius = ImMax(ImTrunc(g.FontSize / 2.0f) - 2.0f, 1.0f);
    const float slider_width = slider_max_x - slider_min_x;
    float circle_x = slider_min_x + slider_width * h;

    ImVec4 hue_color;
    ImGui::ColorConvertHSVtoRGB(h, 1.0f, 1.0f, hue_color.x, hue_color.y, hue_color.z);
    hue_color.w = 1.0f;

    window->DrawList->AddCircleFilled(ImVec2(circle_x, pos.y + half_height), circle_radius, GetColorU32(hue_color));
    window->DrawList->AddCircle(ImVec2(circle_x, pos.y + half_height), circle_radius + 1.0f, IM_COL32_BLACK, 0, 1.0f);

    return pressed;
}

bool ImAdd::ColorPalette(const char* str_id, ImVec4& col, const ImVec2& size_arg)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    ImGuiIO& io = g.IO;
    const ImGuiID id = window->GetID(str_id);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(size_arg, CalcItemWidth(), CalcItemWidth());

    const ImRect bb(pos, pos + size);

    ItemSize(bb);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    float h, s, v;
    ImGui::ColorConvertRGBtoHSV(col.x, col.y, col.z, h, s, v);

    if (held || hovered)
    {
        if (held)
        {
            s = ImClamp((io.MousePos.x - bb.Min.x) / size.x, 0.0f, 1.0f);
            v = 1.0f - ImClamp((io.MousePos.y - bb.Min.y) / size.y, 0.0f, 1.0f);

            ImGui::ColorConvertHSVtoRGB(h, s, v, col.x, col.y, col.z);
        }
    }

    ImVec4 hue_color;
    ImGui::ColorConvertHSVtoRGB(h, 1.0f, 1.0f, hue_color.x, hue_color.y, hue_color.z);
    hue_color.w = 1.0f;
    ImU32 hue_color32 = ImGui::ColorConvertFloat4ToU32(hue_color);

    window->DrawList->AddRectFilledMultiColor(bb.Min, bb.Max,
        IM_COL32_WHITE, hue_color32, hue_color32, IM_COL32_WHITE
    );

    window->DrawList->AddRectFilledMultiColor(bb.Min, bb.Max,
        IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0), IM_COL32_BLACK, IM_COL32_BLACK
    );

    window->DrawList->AddRect(bb.Min, bb.Max, PalU(PAL_BORDER), style.FrameRounding, 0, 1.0f);

    float circle_x = bb.Min.x + s * size.x;
    float circle_y = bb.Min.y + (1.0f - v) * size.y;
    const float circle_radius = ImMax(ImTrunc(g.FontSize / 2.0f) - 2.0f, 1.0f);

    ImVec4 col_rgb_without_alpha(col.x, col.y, col.z, 1.0f);

    window->DrawList->AddCircleFilled(ImVec2(circle_x, circle_y), circle_radius, GetColorU32(col_rgb_without_alpha));
    window->DrawList->AddCircle(ImVec2(circle_x, circle_y), circle_radius + 1.0f, IM_COL32_BLACK, 0, 1.0f);
    window->DrawList->AddCircle(ImVec2(circle_x, circle_y), circle_radius, IM_COL32_WHITE, 0, 1.0f);

    return pressed;
}

bool ImAdd::ColorButton(const char* desc_id, const ImVec4& col, bool has_alpha, float square_sz)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(desc_id);

    ImVec2 pos = window->DC.CursorPos;
    const ImVec2 size(CalcItemSize(ImVec2(square_sz, square_sz), square_sz, square_sz));
    const ImRect bb(pos, pos + size);

    ItemSize(bb);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    ImVec4 col_rgb = col;
    ImVec4 col_rgb_without_alpha(col_rgb.x, col_rgb.y, col_rgb.z, 1.0f);

    ImVec4 col_source = has_alpha ? col_rgb : col_rgb_without_alpha;

    if (col_source.w < 1.0f && has_alpha)
        RenderColorRectWithAlphaCheckerboard(window->DrawList, pos, pos + size, GetColorU32(col_source), 1.0f, ImTrunc(square_sz / 2), ImVec2(0, 0), 4.0f);
    else
        window->DrawList->AddRectFilled(pos, pos + size, GetColorU32(col_source), 4.0f);

    window->DrawList->AddRect(pos, pos + size, hovered ? PalU(PAL_ACCENT_HOV) : PalU(PAL_BORDER), 4.0f, 0, 1.0f);

    RenderNavCursor(bb, id);

    return pressed;
}

void ImAdd::ColorPicker4(const char* label, float col[4], bool has_alpha)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    ImGuiStyle& style = g.Style;

    ImVec4 col_v4(col[0], col[1], col[2], has_alpha ? col[3] : 1.0F);

    float width = 200;

    PushID(label);
    BeginGroup();

    std::string str_label = label;
    std::string str_colp_id = str_label + "::color_palette";
    std::string str_hue_id = str_label + "::hue";

    ImAdd::ColorPalette(str_colp_id.c_str(), col_v4, ImVec2(width, ImTrunc(width / 2)));

    PushItemWidth(width);
    PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2());

    ImAdd::SliderHue(str_hue_id.c_str(), col_v4);

    if (has_alpha)
    {
        std::string str_alpha_id = str_label + "::alpha";
        ImAdd::SliderAlpha(str_alpha_id.c_str(), col_v4);
    }

    PopStyleVar();
    PopItemWidth();

    EndGroup();
    PopID();

    col[0] = col_v4.x;
    col[1] = col_v4.y;
    col[2] = col_v4.z;
    col[3] = col_v4.w;
}

bool ImAdd::ColorEdit4(const char* name, float col[4])
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImVec4 col_v4(col[0], col[1], col[2], col[3]);

    ImVec2 pos = window->DC.CursorPos;

    bool result = ImAdd::ColorButton(name, col_v4, true, g.FontSize);

    std::string popup_str_id = std::string(std::string(name) + "::color_edit_4");

    if (result)
    {
        OpenPopup(popup_str_id.c_str());
    }

    PushStyleVar(ImGuiStyleVar_WindowPadding, style.FramePadding);
    PushStyleVar(ImGuiStyleVar_ItemSpacing, style.FramePadding);

    const float picker_width = 200.0f;
    ImGui::SetNextWindowContentSize(ImVec2(picker_width, 0.0f));

    if (BeginPopupEx(GetID(popup_str_id.c_str()), ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        SetWindowPos(pos - style.WindowPadding, ImGuiCond_Always);

        ImVec4 col_rgba = ImVec4(0, 0, 0, 0);
        col_rgba.x = col[0];
        col_rgba.y = col[1];
        col_rgba.z = col[2];
        col_rgba.w = col[3];

        ImAdd::ColorButton((popup_str_id + "::color_preview").c_str(), col_rgba, true, GetFontSize());

        static std::map<std::string, char[64]> hex_buffers;
        static std::map<std::string, float[4]> prev_colors;
        char* hex_buf = hex_buffers[popup_str_id];
        float* prev_col = prev_colors[popup_str_id];

        char hex_input_id[128];
        sprintf_s(hex_input_id, sizeof(hex_input_id), "##hex_input_%s", popup_str_id.c_str());

        if (IsWindowAppearing())
        {
            int i[4] = { IM_F32_TO_INT8_UNBOUND(col[0]), IM_F32_TO_INT8_UNBOUND(col[1]), IM_F32_TO_INT8_UNBOUND(col[2]), IM_F32_TO_INT8_UNBOUND(col[3]) };
            ImFormatString(hex_buf, 64, "#%02X%02X%02X%02X", ImClamp(i[0], 0, 255), ImClamp(i[1], 0, 255), ImClamp(i[2], 0, 255), ImClamp(i[3], 0, 255));
            prev_col[0] = col[0];
            prev_col[1] = col[1];
            prev_col[2] = col[2];
            prev_col[3] = col[3];
        }

        ImGui::SameLine();
        ImGui::Text("%s", name);

        float hex_text_width = CalcTextSize(hex_buf).x;
        float hex_input_width = CalcTextSize(hex_buf).x;
        ImGui::SameLine(ImMax(ImGui::GetWindowWidth() - style.WindowPadding.x - hex_input_width, ImGui::GetCursorPosX()));
        ImGui::SetCursorPosY(style.WindowPadding.y);
        SetNextItemWidth(hex_input_width);

        ImGuiID hex_input_item_id = GetID(hex_input_id);
        bool hex_input_was_active = (g.ActiveId == hex_input_item_id);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0, 0 });
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

        if (ImAdd::InputText(nullptr, hex_input_id, hex_buf, 10, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_ElideLeft))
        {
            unsigned int hex_val = 0;
            const char* hex_str = hex_buf;
            if (*hex_str == '#')
                hex_str++;

            if (sscanf_s(hex_str, "%X", &hex_val) == 1)
            {
                size_t len = strlen(hex_str);
                if (len == 8)
                {
                    col[0] = ((hex_val >> 24) & 0xFF) / 255.0f;
                    col[1] = ((hex_val >> 16) & 0xFF) / 255.0f;
                    col[2] = ((hex_val >> 8) & 0xFF) / 255.0f;
                    col[3] = (hex_val & 0xFF) / 255.0f;
                    prev_col[0] = col[0];
                    prev_col[1] = col[1];
                    prev_col[2] = col[2];
                    prev_col[3] = col[3];
                    result = true;
                }
                else if (len == 6)
                {
                    col[0] = ((hex_val >> 16) & 0xFF) / 255.0f;
                    col[1] = ((hex_val >> 8) & 0xFF) / 255.0f;
                    col[2] = (hex_val & 0xFF) / 255.0f;
                    prev_col[0] = col[0];
                    prev_col[1] = col[1];
                    prev_col[2] = col[2];
                    prev_col[3] = col[3];
                    result = true;
                }
            }
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();

        bool hex_input_is_active = (g.ActiveId == hex_input_item_id);

        ImAdd::ColorPicker4(name, col, true);

        if (!hex_input_is_active && !IsWindowAppearing())
        {
            bool color_changed = (prev_col[0] != col[0] || prev_col[1] != col[1] || prev_col[2] != col[2] || prev_col[3] != col[3]);
            if (color_changed)
            {
                int i[4] = { IM_F32_TO_INT8_UNBOUND(col[0]), IM_F32_TO_INT8_UNBOUND(col[1]), IM_F32_TO_INT8_UNBOUND(col[2]), IM_F32_TO_INT8_UNBOUND(col[3]) };
                ImFormatString(hex_buf, 64, "#%02X%02X%02X%02X", ImClamp(i[0], 0, 255), ImClamp(i[1], 0, 255), ImClamp(i[2], 0, 255), ImClamp(i[3], 0, 255));
                prev_col[0] = col[0];
                prev_col[1] = col[1];
                prev_col[2] = col[2];
                prev_col[3] = col[3];
            }
        }

        EndPopup();
    }

    PopStyleVar(2);

    return result;
}

bool ImAdd::SliderScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    const float width = CalcItemWidth();
    const ImVec2 pos = window->DC.CursorPos;

    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    const bool has_label = label_size.x > 0;
    const float frame_pos_y = has_label ? (g.FontSize + style.ItemInnerSpacing.y) : 0.0f;
    const float frame_height = g.FontSize;

    const ImRect frame_bb(pos + ImVec2(0, frame_pos_y), pos + ImVec2(width, frame_pos_y + frame_height));
    const ImRect total_bb(pos, frame_bb.Max);

    ItemSize(total_bb);
    if (!ItemAdd(total_bb, id, &frame_bb, 0))
        return false;

    if (format == NULL)
        format = DataTypeGetInfo(data_type)->PrintFmt;

    const bool hovered = ItemHoverable(frame_bb, id, g.LastItemData.ItemFlags);
    const bool clicked = hovered && IsMouseClicked(0, ImGuiInputFlags_None, id);
    const bool held = g.ActiveId == id;
    const bool make_active = (clicked || g.NavActivateId == id);

    if (make_active)
    {
        SetActiveID(id, window);
        SetFocusID(id, window);
        FocusWindow(window);
        g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
    }

    // Grab logic
    ImRect grab_bb;
    const bool value_changed = SliderBehavior(frame_bb, id, data_type, p_data, p_min, p_max, format, 0, &grab_bb);
    if (value_changed)
        MarkItemEdited(id);

    float relative_value = 0.0f;
    if (data_type == ImGuiDataType_Float)
    {
        float val = *(float*)p_data;
        float min_val = *(float*)p_min;
        float max_val = *(float*)p_max;
        relative_value = (val - min_val) / (max_val - min_val);
    }
    else if (data_type == ImGuiDataType_S32)
    {
        int val = *(int*)p_data;
        int min_val = *(int*)p_min;
        int max_val = *(int*)p_max;
        relative_value = (float)(val - min_val) / (float)(max_val - min_val);
    }

    relative_value = ImClamp(relative_value, 0.0f, 1.0f);

    // Animations
    struct stColors_State {
        ImColor Fill;
        ImColor Frame;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.Fill = held ? Pal(PAL_ACCENT_HOV) : Pal(PAL_ACCENT);
        it_anim->second.Frame = Pal(PAL_FRAME);
    }

    ImColor target_fill = held ? ImColor(Pal(PAL_ACCENT_HOV)) : ImColor(hovered ? Pal(PAL_ACCENT_HOV) : Pal(PAL_ACCENT));
    ImColor target_frame = hovered ? ImColor(Pal(PAL_FRAME_HOV)) : ImColor(Pal(PAL_FRAME));

    const float t = 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime;
    it_anim->second.Fill.Value = ImLerp(it_anim->second.Fill.Value, target_fill.Value, t);
    it_anim->second.Frame.Value = ImLerp(it_anim->second.Frame.Value, target_frame.Value, t);

    // Draw track
    const float track_h = 6.0f;
    const float track_pad_y = (frame_height - track_h) * 0.5f;
    ImRect track_bb(frame_bb.Min + ImVec2(0.0f, track_pad_y), frame_bb.Max - ImVec2(0.0f, track_pad_y));

    window->DrawList->AddRectFilled(track_bb.Min, track_bb.Max, it_anim->second.Frame, 3.0f);
    window->DrawList->AddRect(track_bb.Min, track_bb.Max, PalU(PAL_BORDER, 0.6f), 3.0f, 0, 1.0f);

    const float grab_w = ImMax(style.GrabMinSize, 12.0f);
    const float fill_w = ImMax(0.0f, relative_value * (track_bb.GetWidth() - grab_w * 0.5f) + grab_w * 0.25f);
    ImVec2 fill_end(track_bb.Min.x + fill_w, track_bb.Max.y);
    if (fill_end.x > track_bb.Min.x)
    {
        window->DrawList->AddRectFilled(track_bb.Min, fill_end, it_anim->second.Fill, 3.0f);
    }

    float grab_x = ImClamp(track_bb.Min.x + relative_value * (track_bb.GetWidth() - grab_w), track_bb.Min.x, track_bb.Max.x - grab_w);

    ImVec2 grab_center(grab_x + grab_w * 0.5f, track_bb.Min.y + track_bb.GetHeight() * 0.5f);

    window->DrawList->AddCircleFilled(grab_center, 9.0f, PalU(PAL_ACCENT, 0.16f), 20);
    window->DrawList->AddCircleFilled(grab_center, 5.5f, IM_COL32(255, 255, 255, 255), 16);
    window->DrawList->AddCircle(grab_center, held ? 8.0f : 7.0f, it_anim->second.Fill, 16, 2.0f);

    char value_buf[64];
    const char* value_buf_end = value_buf + DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, p_data, format);

    if (has_label) {
        const char* label_end = FindRenderedTextEnd(label);
        window->DrawList->AddText(total_bb.Min, PalU(PAL_TEXT), label, label_end);
        window->DrawList->AddText(total_bb.Min + ImVec2(width - CalcTextSize(value_buf).x, 0), held ? PalU(PAL_ACCENT_HOV) : PalU(PAL_TEXT_DIM), value_buf);
    }

    return value_changed;
}

bool ImAdd::SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format)
{
    return ImAdd::SliderScalar(label, ImGuiDataType_Float, v, &v_min, &v_max, format);
}

bool ImAdd::SliderInt(const char* label, int* v, int v_min, int v_max, const char* format)
{
    return ImAdd::SliderScalar(label, ImGuiDataType_S32, v, &v_min, &v_max, format);
}

bool ImAdd::BeginChild(const char* label, const ImVec2& size_arg)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    ImGui::BeginChild(label, size_arg, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);

    window = GetCurrentWindow();

    ImVec2 pos = window->Pos;
    ImVec2 size = window->Size;

    ImDrawList* dl = window->DrawList;

    const float header_h = ImGui::GetFrameHeight();
    const float rounding = 8.0f;

    dl->AddRectFilled(pos + ImVec2(0, 4), pos + size + ImVec2(0, 4), IM_COL32(0, 0, 0, 100), rounding);
    float ga = blur::shell_alpha();
    if (ga < 1.0f) blur::glass_rect(pos.x, pos.y, size.x, size.y, rounding);
    dl->AddRectFilled(pos, pos + size, PalU(PAL_BG_CHILD, ga < 1.0f ? 0.50f : 1.0f), rounding);
    dl->AddRectFilledMultiColor(ImVec2(pos.x + rounding, pos.y), ImVec2(pos.x + size.x - rounding, pos.y + size.y * 0.35f),
        IM_COL32(255, 255, 255, 16), IM_COL32(255, 255, 255, 16),
        IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));
    dl->AddRect(pos, pos + size, PalU(PAL_BORDER), rounding, 0, 1.0f);
    dl->AddLine(pos + ImVec2(8.0f, 1.0f), pos + ImVec2(size.x - 8.0f, 1.0f), IM_COL32(255, 255, 255, 22), 1.0f);

    dl->AddRectFilled(pos + ImVec2(0.0f, 0.0f), pos + ImVec2(size.x, header_h), PalU(PAL_FRAME, 0.55f), rounding, ImDrawFlags_RoundCornersTop);

    dl->AddRectFilled(pos + ImVec2(6.0f, (header_h - 12.0f) * 0.5f), pos + ImVec2(9.0f, (header_h + 12.0f) * 0.5f), PalU(PAL_ACCENT), 2.0f);

    dl->AddText(pos + ImVec2(15.0f, header_h / 2.0f - ImGui::GetFontSize() / 2.0f), PalU(PAL_TEXT), label, FindRenderedTextEnd(label));

    dl->AddLine(pos + ImVec2(6.0f, header_h), pos + ImVec2(size.x - 6.0f, header_h), PalU(PAL_BORDER), 1.0f);

    SetCursorScreenPos(pos + ImVec2(0.0f, header_h));
    ImGui::BeginChild(std::string(std::string(label) + "body").c_str(), ImVec2(ImGui::GetWindowWidth(), ImGui::GetWindowHeight() - header_h), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground);
    PushItemWidth(GetContentRegionAvail().x);

    return true;
}

void ImAdd::EndChild()
{
    PopItemWidth();
    ImGui::EndChild();
    ImGui::EndChild();
}

void ImAdd::RenderArrow(ImDrawList* draw_list, ImVec2 pos, ImU32 col, float sz, ImGuiDir direction)
{
    if (direction < 0 || direction >= ImGuiDir_COUNT) return;

    float thickness = 1.5f;

    float half = ImTrunc(sz / 2.0f);
    float pad_y = ImTrunc(sz / 2.4f);
    float pad_x = ImTrunc(sz / 4.0f);

    if (direction == ImGuiDir_Down)
    {
        draw_list->PathLineTo(pos + ImVec2(pad_x, pad_y));
        draw_list->PathLineTo(pos + ImVec2(half, sz - pad_y));
        draw_list->PathLineTo(pos + ImVec2(sz - pad_x, pad_y));
    }
    else if (direction == ImGuiDir_Up)
    {
        draw_list->PathLineTo(pos + ImVec2(pad_x, sz - pad_y));
        draw_list->PathLineTo(pos + ImVec2(half, pad_y));
        draw_list->PathLineTo(pos + ImVec2(sz - pad_x, sz - pad_y));
    }
    else if (direction == ImGuiDir_Right)
    {
        draw_list->PathLineTo(pos + ImVec2(pad_y, pad_x));
        draw_list->PathLineTo(pos + ImVec2(sz - pad_y, half));
        draw_list->PathLineTo(pos + ImVec2(pad_y, sz - pad_x));
    }
    else if (direction == ImGuiDir_Left)
    {
        draw_list->PathLineTo(pos + ImVec2(sz - pad_y, pad_x));
        draw_list->PathLineTo(pos + ImVec2(pad_y, half));
        draw_list->PathLineTo(pos + ImVec2(sz - pad_y, sz - pad_x));
    }

    draw_list->PathStroke(col, 0, thickness);
}

void ImAdd::RenderCheckMark(ImDrawList* draw_list, ImVec2 pos, ImU32 col, float sz)
{
    float thickness = 1.5f;

    sz -= thickness * 0.5f;
    pos += ImVec2(thickness * 0.25f, thickness * 0.25f);

    float third = sz / 3.0f;
    float bx = pos.x + third;
    float by = pos.y + sz - third * 0.5f;
    draw_list->PathLineTo(ImVec2(bx - third, by - third));
    draw_list->PathLineTo(ImVec2(bx, by));
    draw_list->PathLineTo(ImVec2(bx + third * 2.0f, by - third * 2.0f));
    draw_list->PathStroke(col, 0, thickness);
}

void ImAdd::RenderSubTabs(const std::vector<const char*>& subtabs, int& current_subtab)
{
    ImGuiStyle& style = GetStyle();
    PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

    float subtab_width = (GetContentRegionAvail().x + style.FrameBorderSize * subtabs.size()) / subtabs.size();

    for (int i = 0; i < subtabs.size(); i++)
    {
        ImVec2 subtab_size(i == (subtabs.size() - 1) ? GetContentRegionAvail().x : ImTrunc(subtab_width), GetFontSize() + style.FramePadding.y * 2.0f);
        ImDrawFlags draw_flags = i == 0 ? ImDrawFlags_RoundCornersTopLeft | ImDrawFlags_RoundCornersBottomLeft : i == (subtabs.size() - 1) ? ImDrawFlags_RoundCornersTopRight | ImDrawFlags_RoundCornersBottomRight : ImDrawFlags_RoundCornersNone;

        Tab(subtabs[i], &current_subtab, i, draw_flags, subtab_size);
        if (i < subtabs.size() - 1) SameLine();
    }

    PopStyleVar(2);
}

bool ImAdd::ColorEdit4WithHex(const char* label, float col[4])
{
    return ColorEdit4(label, col);
}

#include "pch/pch.h"
#include "ui/widgets.hpp"
#include "ui/theme.hpp"

#include <unordered_map>
#include <cmath>

#include "../../vendor/imgui/imgui_internal.h"

namespace widgets {

static ImU32 to_u32(const ImVec4& c) {
    return IM_COL32((int)(c.x * 255), (int)(c.y * 255), (int)(c.z * 255), (int)(c.w * 255));
}

void section(const char* t) {
    ImGui::Spacing();
    ImGui::TextColored(theme::accentB(0.9f), "%s", t);
    ImGui::Separator();
    ImGui::Spacing();
}

bool nav_pill(const char* label, bool active) {
    ImVec2 cur = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    ImVec2 sz(w, 34);
    ImGui::InvisibleButton(label, sz);
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec4 a = theme::accentA(), b = theme::accentB();
    if (active) {
        dl->AddRectFilledMultiColor(cur, ImVec2(cur.x + sz.x, cur.y + sz.y),
            to_u32(ImVec4(a.x * 0.55f, a.y * 0.55f, a.z * 0.85f, 1.0f)),
            to_u32(ImVec4(b.x * 0.35f, b.y * 0.35f, b.z * 0.6f, 1.0f)),
            to_u32(ImVec4(b.x * 0.35f, b.y * 0.35f, b.z * 0.6f, 1.0f)),
            to_u32(ImVec4(a.x * 0.55f, a.y * 0.55f, a.z * 0.85f, 1.0f)));
        dl->AddRectFilled(cur, ImVec2(cur.x + 3, cur.y + sz.y), to_u32(b), 2.0f);
    } else if (hovered) {
        dl->AddRectFilled(cur, ImVec2(cur.x + sz.x, cur.y + sz.y),
            IM_COL32(48, 44, 92, 255), 9.0f);
        dl->AddRect(cur, ImVec2(cur.x + sz.x, cur.y + sz.y), to_u32(ImVec4(a.x, a.y, a.z, 0.5f)), 9.0f, 0, 1.0f);
    }
    dl->AddText(ImVec2(cur.x + 14, cur.y + 9),
        active ? IM_COL32(255, 255, 255, 255)
        : hovered ? IM_COL32(220, 225, 255, 255)
                  : IM_COL32(150, 155, 175, 255),
        label);
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    return clicked;
}

bool action_button(const char* label, const char* sub, ImVec2 size) {
    ImGui::InvisibleButton(label, size);
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();
    ImVec2 p0 = ImGui::GetItemRectMin(), p1 = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec4 a = theme::accentA(), b = theme::accentB(), g = theme::get(theme::current()).glow;
    p0.y += 0.0f; p1.y += 0.0f;
    if (hovered)
        dl->AddRectFilled(ImVec2(p0.x - 2, p0.y - 2), ImVec2(p1.x + 2, p1.y + 2),
            to_u32(ImVec4(g.x, g.y, g.z, 0.18f)), 5.0f);
    float m = hovered ? 1.0f : 0.62f;
    dl->AddRectFilled(p0, p1,
        to_u32(ImVec4(a.x * m * 0.55f, a.y * m * 0.55f, a.z * m * 0.75f, 1.0f)), 4.0f);
    dl->AddRectFilled(ImVec2(p0.x, p0.y), ImVec2(p0.x + 3, p1.y),
        to_u32(hovered ? b : ImVec4(a.x, a.y, a.z, 0.6f)), 1.0f);
    dl->AddRect(p0, p1,
        hovered ? IM_COL32(200, 225, 255, 220) : to_u32(ImVec4(a.x, a.y, a.z, 0.35f)),
        4.0f, 0, 1.0f);
    ImVec2 tsz = ImGui::CalcTextSize(label);
    float hgt = p1.y - p0.y;
    if (sub && hgt >= 44.0f) {
        dl->AddText(ImVec2((p0.x + p1.x - tsz.x) * 0.5f, p0.y + 7),
            IM_COL32(255, 255, 255, 255), label);
        ImVec2 ssz = ImGui::CalcTextSize(sub);
        dl->AddText(ImVec2((p0.x + p1.x - ssz.x) * 0.5f, p0.y + 26),
            IM_COL32(210, 215, 235, 200), sub);
    } else {
        dl->AddText(ImVec2((p0.x + p1.x - tsz.x) * 0.5f, p0.y + (hgt - tsz.y) * 0.5f),
            IM_COL32(255, 255, 255, 255), label);
        if (sub && hovered) ImGui::SetTooltip("%s", sub);
    }
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    return clicked;
}

bool toggle_ex(const char* label, std::atomic<bool>& v) {
    bool b = v.load();
    ImGui::PushID(label);
    ImVec2 cur = ImGui::GetCursorScreenPos();
    ImVec2 sz(40, 22);
    ImGui::InvisibleButton("##tgl", sz);
    bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) { b = !b; v.store(b); }
    static std::unordered_map<ImGuiID, float> anim;
    ImGuiID id = ImGui::GetID("##tgl");
    float& t = anim[id];
    float target = b ? 1.0f : 0.0f;
    t += (target - t) * ImMin(1.0f, ImGui::GetIO().DeltaTime * 12.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec4 a = theme::accentA(), bb = theme::accentB();
    ImU32 track = to_u32(ImVec4(
        0.20f + t * a.x * 0.55f, 0.19f + t * a.y * 0.55f, 0.36f + t * a.z * 0.45f, 1.0f));
    if (hovered)
        dl->AddCircle(ImVec2(cur.x + 20, cur.y + 11), 14.0f,
            to_u32(ImVec4(bb.x, bb.y, bb.z, 0.35f)), 20, 1.5f);
    dl->AddRectFilled(cur, ImVec2(cur.x + sz.x, cur.y + sz.y), track, 11.0f);
    float kx = cur.x + 3 + t * (sz.x - 6 - 16);
    dl->AddCircleFilled(ImVec2(kx + 8, cur.y + 11), 8.0f, IM_COL32(255, 255, 255, 255));
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ImGui::SameLine(0, 10);
    ImGui::TextColored(b ? ImVec4(0.93f, 0.94f, 0.97f, 1.0f) : ImVec4(0.58f, 0.62f, 0.72f, 1.0f),
        "%s", label);
    ImGui::PopID();
    return b;
}

void slider_f(const char* label, std::atomic<float>& v, float lo, float hi) {
    float f = v.load();
    ImGui::PushItemWidth(-1);
    if (ImGui::SliderFloat(label, &f, lo, hi, "%.0f")) v.store(f);
    ImGui::PopItemWidth();
}

void stat_row(const char* k, int v, ImU32 col) {
    char buf[32]; snprintf(buf, sizeof(buf), "%d", v);
    stat_text(k, buf, col);
}

void stat_text(const char* k, const char* v, ImU32 col) {
    ImVec2 cur = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(cur, ImVec2(cur.x + w, cur.y + 26), IM_COL32(20, 22, 34, 255), 8.0f);
    dl->AddText(ImVec2(cur.x + 10, cur.y + 6), IM_COL32(170, 175, 195, 255), k);
    ImVec2 vsz = ImGui::CalcTextSize(v);
    dl->AddText(ImVec2(cur.x + w - vsz.x - 10, cur.y + 6), col, v);
    ImGui::Dummy(ImVec2(w, 26));
}

bool card_begin(const char* title, const char* hint) {
    ImVec2 cur = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    // NOTE: ImGuiWindowFlags_AlwaysAutoResize is illegal here (fatal assert);
    // auto-height comes from ImGuiChildFlags_AutoResizeY instead.
    ImGui::BeginChild(title, ImVec2(w, 0),
        ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImVec2 c0 = ImGui::GetWindowPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // accent spine
    ImVec4 b = theme::accentB();
    dl->AddRectFilled(ImVec2(c0.x, cur.y + 2), ImVec2(c0.x + 3, cur.y + 24), to_u32(b), 1.0f);
    ImGui::Indent(10);
    ImGui::TextColored(ImVec4(0.93f, 0.94f, 0.97f, 1.0f), "%s", title);
    if (hint) {
        ImGui::SameLine(0, 8);
        ImGui::TextDisabled("%s", hint);
    }
    ImGui::Separator();
    ImGui::Spacing();
    (void)cur;
    return true;
}

void card_end() {
    ImGui::Unindent(10);
    ImGui::EndChild();
    ImGui::Spacing();
}


static void draw_side_icon(ImDrawList* dl, SideIcon icon, ImVec2 c, float s, ImU32 col) {
    switch (icon) {
    case SideIcon::Eye:
        dl->AddEllipse(ImVec2(c.x, c.y), ImVec2(9.0f * s, 6.0f * s), col, 0.0f, 20, 1.6f);
        dl->AddCircleFilled(ImVec2(c.x, c.y), 2.6f * s, col);
        break;
    case SideIcon::Cart:
        dl->AddRect(ImVec2(c.x - 8 * s, c.y - 6 * s), ImVec2(c.x + 8 * s, c.y + 3 * s), col, 1.0f, 0, 1.6f);
        dl->AddCircleFilled(ImVec2(c.x - 4 * s, c.y + 7 * s), 1.8f * s, col);
        dl->AddCircleFilled(ImVec2(c.x + 4 * s, c.y + 7 * s), 1.8f * s, col);
        break;
    case SideIcon::Globe:
        dl->AddCircle(ImVec2(c.x, c.y), 8.5f * s, col, 20, 1.6f);
        dl->AddLine(ImVec2(c.x - 8.5f * s, c.y), ImVec2(c.x + 8.5f * s, c.y), col, 1.2f);
        dl->AddEllipse(ImVec2(c.x, c.y), ImVec2(4.0f * s, 8.5f * s), col, 0.0f, 16, 1.2f);
        break;
    case SideIcon::Person:
        dl->AddCircleFilled(ImVec2(c.x, c.y - 4 * s), 3.6f * s, col);
        dl->AddLine(ImVec2(c.x - 6 * s, c.y + 8 * s), ImVec2(c.x + 6 * s, c.y + 8 * s), col, 2.2f);
        dl->AddLine(ImVec2(c.x, c.y), ImVec2(c.x, c.y + 8 * s), col, 2.2f);
        break;
    case SideIcon::Gear:
        dl->AddCircle(ImVec2(c.x, c.y), 4.5f * s, col, 16, 1.8f);
        for (int i = 0; i < 8; ++i) {
            float a = (float)i * 0.7853982f;
            dl->AddLine(ImVec2(c.x + cosf(a) * 6.5f * s, c.y + sinf(a) * 6.5f * s),
                ImVec2(c.x + cosf(a) * 9.0f * s, c.y + sinf(a) * 9.0f * s), col, 1.8f);
        }
        break;
    }
}

bool sidebar_button(const char* label, SideIcon icon, bool active) {
    ImVec2 cur = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    ImVec2 sz(w, 36);
    ImGui::InvisibleButton(label, sz);
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec4 a = theme::accentA(), b = theme::accentB();
    if (active) {
        dl->AddRectFilledMultiColor(cur, ImVec2(cur.x + sz.x, cur.y + sz.y),
            ImU32(to_u32(ImVec4(a.x * 0.75f, a.y * 0.75f, a.z * 1.0f, 1.0f))),
            ImU32(to_u32(ImVec4(b.x * 0.6f, b.y * 0.6f, b.z * 0.9f, 1.0f))),
            ImU32(to_u32(ImVec4(b.x * 0.6f, b.y * 0.6f, b.z * 0.9f, 1.0f))),
            ImU32(to_u32(ImVec4(a.x * 0.75f, a.y * 0.75f, a.z * 1.0f, 1.0f))));
        dl->AddRectFilled(cur, ImVec2(cur.x + 3, cur.y + sz.y), to_u32(b), 2.0f);
    } else if (hovered) {
        dl->AddRectFilled(cur, ImVec2(cur.x + sz.x, cur.y + sz.y),
            IM_COL32(34, 40, 58, 255), 4.0f);
    }
    ImU32 ic = active ? IM_COL32(255, 255, 255, 255)
        : hovered ? IM_COL32(210, 220, 245, 255) : IM_COL32(130, 140, 165, 255);
    draw_side_icon(dl, icon, ImVec2(cur.x + 18, cur.y + sz.y / 2), 1.0f, ic);
    dl->AddText(ImVec2(cur.x + 36, cur.y + 10), ic, label);
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    return clicked;
}

bool color_chip(const char* id, ImVec4& col) {
    ImVec2 cur = ImGui::GetCursorScreenPos();
    ImVec2 sz(16, 16);
    ImGui::InvisibleButton(id, sz);
    bool clicked = ImGui::IsItemClicked();
    bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(cur, ImVec2(cur.x + sz.x, cur.y + sz.y), to_u32(col), 4.0f);
    dl->AddRect(cur, ImVec2(cur.x + sz.x, cur.y + sz.y),
        hovered ? IM_COL32(255, 255, 255, 220) : IM_COL32(0, 0, 0, 160), 4.0f, 0, 1.0f);
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (clicked) ImGui::OpenPopup(id);
    if (ImGui::BeginPopup(id)) {
        ImGui::ColorPicker4("##pick", (float*)&col,
            ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview);
        ImGui::EndPopup();
    }
    return clicked;
}

bool check_row(const char* label, std::atomic<bool>& v, ImVec4* chip) {
    bool b = v.load();
    ImGui::PushID(label);
    ImVec2 cur = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float chipW = chip ? 26.0f : 0.0f;
    ImGui::InvisibleButton("##row", ImVec2(w, 24));
    bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) { b = !b; v.store(b); }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec4 a = theme::accentA();
    float bs = 15.0f, by = cur.y + (24.0f - bs) / 2;
    if (b) {
        dl->AddRectFilled(ImVec2(cur.x, by), ImVec2(cur.x + bs, by + bs),
            to_u32(ImVec4(a.x * 0.85f, a.y * 0.85f, a.z, 1.0f)), 4.0f);
        dl->AddText(ImVec2(cur.x + 2.5f, by - 3.5f), IM_COL32(255, 255, 255, 255), "x");
    } else {
        dl->AddRect(ImVec2(cur.x, by), ImVec2(cur.x + bs, by + bs),
            hovered ? IM_COL32(170, 190, 230, 255) : IM_COL32(110, 120, 145, 255),
            4.0f, 0, 1.2f);
    }
    dl->AddText(ImVec2(cur.x + bs + 9, cur.y + 4),
        b ? IM_COL32(235, 238, 248, 255) : IM_COL32(150, 156, 175, 255), label);
    if (chip) {
        ImGui::SetCursorScreenPos(ImVec2(cur.x + w - 16, by - 1));
        char cid[64]; snprintf(cid, sizeof(cid), "##chip_%s", label);
        color_chip(cid, *chip);
    }
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ImGui::PopID();
    return b;
}

bool segmented(const char** items, int count, int& sel) {
    bool changed = false;
    float w = ImGui::GetContentRegionAvail().x;
    float bw = (w - (count - 1) * 6.0f) / count;
    ImVec4 a = theme::accentA(), b = theme::accentB();
    for (int i = 0; i < count; ++i) {
        if (i > 0) ImGui::SameLine(0, 6);
        ImVec2 cur = ImGui::GetCursorScreenPos();
        ImVec2 sz(bw, 30);
        char id[64]; snprintf(id, sizeof(id), "##seg%d", i);
        ImGui::InvisibleButton(id, sz);
        bool hovered = ImGui::IsItemHovered();
        bool active = (sel == i);
        if (ImGui::IsItemClicked()) { sel = i; changed = true; }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (active) {
            dl->AddRectFilledMultiColor(cur, ImVec2(cur.x + sz.x, cur.y + sz.y),
                to_u32(ImVec4(a.x * 0.9f, a.y * 0.9f, a.z, 1.0f)),
                to_u32(ImVec4(b.x * 0.75f, b.y * 0.75f, b.z, 1.0f)),
                to_u32(ImVec4(b.x * 0.75f, b.y * 0.75f, b.z, 1.0f)),
                to_u32(ImVec4(a.x * 0.9f, a.y * 0.9f, a.z, 1.0f)));
        } else {
            dl->AddRectFilled(cur, ImVec2(cur.x + sz.x, cur.y + sz.y),
                hovered ? IM_COL32(40, 46, 66, 255) : IM_COL32(26, 31, 46, 255), 7.0f);
        }
        ImVec2 tsz = ImGui::CalcTextSize(items[i]);
        dl->AddText(ImVec2(cur.x + (sz.x - tsz.x) / 2, cur.y + (sz.y - tsz.y) / 2),
            active ? IM_COL32(255, 255, 255, 255)
            : hovered ? IM_COL32(215, 222, 242, 255) : IM_COL32(140, 148, 170, 255),
            items[i]);
        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    return changed;
}

bool dropdown(const char* label, const char** items, int count, std::atomic<int>& sel) {
    int cur = sel.load();
    if (cur < 0 || cur >= count) cur = 0;
    ImGui::TextDisabled("%s", label);
    char pid[96]; snprintf(pid, sizeof(pid), "##dd_%s", label);
    ImGui::PushItemWidth(-1);
    bool changed = false;
    if (ImGui::BeginCombo(pid, items[cur])) {
        for (int i = 0; i < count; ++i) {
            bool isSel = (i == cur);
            if (ImGui::Selectable(items[i], isSel)) { sel.store(i); changed = true; }
            if (isSel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    return changed;
}

const char* key_name(int vk) {
    static char buf[16];
    if (vk <= 0) return "None";
    // common names first (GetKeyNameText needs a scan code; keep it simple)
    switch (vk) {
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

bool keybind(const char* label, std::atomic<int>& vk) {
    static std::string arming; // label currently capturing (render thread only)
    int cur = vk.load();
    ImGui::PushID(label);
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine(0, 8);
    bool isArming = (arming == label);
    const char* txt = isArming ? "..." : key_name(cur);
    char btn[64];
    snprintf(btn, sizeof(btn), "%s##kb", txt);
    bool changed = false;
    if (ImGui::SmallButton(btn)) {
        arming = isArming ? "" : label;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("click to bind, right-click clears");
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            vk.store(0);
            arming.clear();
            changed = true;
        }
    }
    if (isArming) {
        // first press wins; ESC cancels without clearing
        for (int k = 1; k < 256; ++k) {
            if (k == 0x1B) continue; // ESC = cancel
            if (GetAsyncKeyState(k) & 1) {
                vk.store(k);
                arming.clear();
                changed = true;
                break;
            }
        }
        if (GetAsyncKeyState(0x1B) & 1) arming.clear();
    }
    ImGui::PopID();
    return changed;
}

} // namespace widgets

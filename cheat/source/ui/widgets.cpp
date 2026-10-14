#include "pch/pch.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "ui/widgets.hpp"
#include "ui/menu.hpp"
#include "ui/take_shell.hpp"
#include "steam/steam_user.hpp"
#include "config/config.hpp"
#include "features/takebind.hpp"
#include "features/manager.hpp"

#include <unordered_map>
#include <map>
#include <cmath>
#include <cstring>
#include <string>

#include "../../vendor/imgui/imgui_internal.h"

namespace widgets {

const char* key_name(int vk) {
    static char buf[64];
    if (vk <= 0) return "None";
    if (vk >= 'A' && vk <= 'Z' || vk >= '0' && vk <= '9') {
        buf[0] = (char)vk; buf[1] = 0;
        return buf;
    }
    switch (vk) {
    case 0x01: return "LMB";
    case 0x02: return "RMB";
    case 0x04: return "MMB";
    case 0x05: return "X1";
    case 0x06: return "X2";
    case 0x08: return "Backspace";
    case 0x09: return "Tab";
    case 0x0D: return "Enter";
    case 0x10: return "Shift";
    case 0x11: return "Ctrl";
    case 0x12: return "Alt";
    case 0x13: return "Pause";
    case 0x14: return "Caps";
    case 0x1B: return "Esc";
    case 0x20: return "Space";
    case 0x21: return "Page Up";
    case 0x22: return "Page Down";
    case 0x23: return "End";
    case 0x24: return "Home";
    case 0x25: return "Left";
    case 0x26: return "Up";
    case 0x27: return "Right";
    case 0x28: return "Down";
    case 0x2D: return "Insert";
    case 0x2E: return "Delete";
    case 0x5B: return "LWin";
    case 0x5C: return "RWin";
    case 0x60: return "Numpad 0";
    case 0x61: return "Numpad 1";
    case 0x62: return "Numpad 2";
    case 0x63: return "Numpad 3";
    case 0x64: return "Numpad 4";
    case 0x65: return "Numpad 5";
    case 0x66: return "Numpad 6";
    case 0x67: return "Numpad 7";
    case 0x68: return "Numpad 8";
    case 0x69: return "Numpad 9";
    case 0x6A: return "Numpad *";
    case 0x6B: return "Numpad +";
    case 0x6D: return "Numpad -";
    case 0x6E: return "Numpad .";
    case 0x6F: return "Numpad /";
    case 0x70: return "F1"; case 0x71: return "F2"; case 0x72: return "F3";
    case 0x73: return "F4"; case 0x74: return "F5"; case 0x75: return "F6";
    case 0x76: return "F7"; case 0x77: return "F8"; case 0x78: return "F9";
    case 0x79: return "F10"; case 0x7A: return "F11"; case 0x7B: return "F12";
    default: break;
    }
    {
        char nm[64] = {};
        UINT sc = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
        if (sc && GetKeyNameTextA((LONG)(sc << 16), nm, sizeof(nm)) && nm[0]) {
            snprintf(buf, sizeof(buf), "%s", nm);
            return buf;
        }
    }
    snprintf(buf, sizeof(buf), "VK %02X", vk & 0xFF);
    return buf;
}

bool check_bridge(const char* label, std::atomic<bool>& v) {
    bool b = v.load();
    if (ImAdd::CheckBox(label, &b)) {
        v.store(b);
        return true;
    }
    return false;
}

bool os_check(const char* label, bool* v) {
    return ImAdd::CheckBox(label, v);
}

bool combo_bridge(const char* label, const char** items, int count, std::atomic<int>& sel) {
    if (count <= 0) return false;
    int cur = sel.load();
    if (cur < 0 || cur >= count) cur = 0;
    std::vector<const char*> list;
    list.reserve(count);
    for (int i = 0; i < count; ++i) list.push_back(items[i]);
    if (ImAdd::Combo(label, &cur, list)) {
        sel.store(cur);
        return true;
    }
    return false;
}

bool sliderf_bridge(const char* label, std::atomic<float>& v, float lo, float hi, const char* fmt) {
    float f = v.load();
    if (ImAdd::SliderFloat(label, &f, lo, hi, fmt ? fmt : "%.0f")) {
        v.store(f);
        return true;
    }
    return false;
}

bool os_slider(const char* label, float* v, float lo, float hi, const char* fmt) {
    return ImAdd::SliderFloat(label, v, lo, hi, fmt ? fmt : "%.1f");
}

void os_multi(const char* label, bool variable[], const char* labels[], int count) {
    if (count <= 0) return;
    int mask = 0;
    for (int i = 0; i < count && i < 32; ++i)
        if (variable[i]) mask |= (1 << i);
    std::vector<const char*> list;
    list.reserve(count);
    for (int i = 0; i < count; ++i) list.push_back(labels[i]);
    if (ImAdd::MultiCombo(label, &mask, list)) {
        for (int i = 0; i < count && i < 32; ++i)
            variable[i] = (mask & (1 << i)) != 0;
    }
}

bool multi_bridge(const char* label, const char** items, std::atomic<bool>** flags, int count) {
    int n = count < 16 ? count : 16;
    if (n <= 0) return false;
    int mask = 0, before = 0;
    for (int i = 0; i < n; ++i) {
        bool b = flags[i]->load();
        if (b) mask |= (1 << i);
    }
    before = mask;
    std::vector<const char*> list;
    list.reserve(n);
    for (int i = 0; i < n; ++i) list.push_back(items[i]);
    ImAdd::MultiCombo(label, &mask, list);
    if (mask == before) return false;
    for (int i = 0; i < n; ++i)
        flags[i]->store((mask & (1 << i)) != 0);
    return true;
}

bool os_section(const char* name, ImVec2 size) {
    return ImAdd::BeginChild(name && name[0] ? name : "##card", size);
}

void os_section_end() {
    ImAdd::EndChild();
}

bool os_subtab(const char* label, bool selected, float width) {
    int v = selected ? 1 : 0;
    return ImAdd::Tab(label, &v, 1, 0, ImVec2(width, 0));
}

static std::string g_capId;
static int g_capFrame = -100;

bool action_button(const char* id, const char* name, const char* desc, ImVec2 size) {
    auto* k = feat::key_for(id);
    auto* m = feat::mode_for(id);
    bool armed = (k && m);
    int vk = armed ? k->load() : 0;
    ImGui::PushID(id);
    bool ran = false;
    if (ImAdd::Button(name, size)) {
        feat::queue_run(id);
        ran = true;
    }
    if (desc && desc[0] && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", desc);
    if (vk > 0) {
        ImVec2 mn = ImGui::GetItemRectMin();
        ImVec2 mx = ImGui::GetItemRectMax();
        std::string kn = keybind::get_key_name(vk);
        ImVec2 ksz = ImGui::CalcTextSize(kn.c_str());
        ImVec2 nsz = ImGui::CalcTextSize(name);
        if (nsz.x + ksz.x + 28.0f < (mx.x - mn.x) && (mx.y - mn.y) > ksz.y) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddText(ImVec2(mx.x - ksz.x - 10.0f, mn.y + ((mx.y - mn.y) - ksz.y) * 0.5f),
                IM_COL32(139, 151, 165, 255), kn.c_str());
        }
    }
    if (armed) {
        char ctx[96];
        snprintf(ctx, sizeof(ctx), "keyctx_%s", id);
        ImGui::SetNextWindowSize(ImVec2(190, 0), ImGuiCond_Always);
        if (ImGui::BeginPopupContextItem(ctx)) {
            bool capturing = (g_capId == id);
            if (!capturing) {
                char cur[64];
                if (vk > 0)
                    snprintf(cur, sizeof(cur), "Bound: %s", keybind::get_key_name(vk).c_str());
                else
                    snprintf(cur, sizeof(cur), "Unbound");
                ImGui::TextDisabled("%s", cur);
                ImGui::Separator();
                if (ImAdd::Button(vk > 0 ? "REBIND" : "SET KEYBIND", ImVec2(-1, 26))) {
                    g_capId = id;
                    g_capFrame = ImGui::GetFrameCount();
                }
                int md = m->load();
                if (ImGui::MenuItem("Hold", nullptr, md == 1)) m->store(1);
                if (ImGui::MenuItem("Toggle", nullptr, md == 0)) m->store(0);
                if (ImGui::MenuItem("Always", nullptr, md == 2)) m->store(2);
                ImGui::Separator();
                if (ImGui::MenuItem("Clear")) {
                    k->store(0);
                    if (g_capId == id) g_capId.clear();
                }
            } else {
                ImGui::TextDisabled("press a key...");
                if (ImGui::GetFrameCount() > g_capFrame + 2) {
                    for (int v = 5; v < 255; ++v) {
                        if (v == 7) continue;
                        if (GetAsyncKeyState(v) & 0x8000) {
                            k->store(v);
                            g_capId.clear();
                            break;
                        }
                    }
                }
                if (ImAdd::Button("CANCEL", ImVec2(-1, 26)))
                    g_capId.clear();
            }
            ImGui::EndPopup();
        } else if (g_capId == id) {
            g_capId.clear();
        }
    }
    ImGui::PopID();
    return ran;
}

void color_tail(const char* id, ImVec4& col) {
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - ImGui::GetFontSize());
    ImAdd::ColorEdit4WithHex(id, (float*)&col);
}

void check_picker(const char* label, std::atomic<bool>& v, ImVec4& col, const char* cid) {
    check_bridge(label, v);
    char pid[64];
    snprintf(pid, sizeof(pid), "Colour##%s", cid ? cid : label);
    color_tail(pid, col);
}

void key_pill(const char* uid, std::atomic<int>& key, std::atomic<int>& mode) {
    ImGuiStyle& style = ImGui::GetStyle();
    float avail = ImGui::GetContentRegionAvail().x - style.WindowPadding.x * 2.0f;
    float w = avail * 0.4f < 120.0f ? avail * 0.4f : 120.0f;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x + style.WindowPadding.x - w);
    int k = key.load();
    keybind::activation_mode m = (keybind::activation_mode)mode.load();
    if (keybind::keybind_with_mode(uid, k, m, uid, w)) {
        key.store(k);
        mode.store((int)m);
    }
}

void key_row(const char* text, const char* uid, std::atomic<int>& key, std::atomic<int>& mode) {
    ImGui::TextDisabled("%s", text);
    key_pill(uid, key, mode);
}

std::string shorten(const std::string& s, size_t n) {
    if (s.size() <= n) return s;
    return s.substr(0, n - 3) + "...";
}

void avatar_tile(ImVec2 p0, ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p1(p0.x + size.x, p0.y + size.y);
    void* av = steamServiceHelper::avatar();
    if (av) {
        dl->AddImage(av, p0, p1);
        return;
    }
    dl->AddRectFilled(p0, p1, (IM_COL32(30, 33, 52, 255)), 0.0f);
    if (!steamServiceHelper::avatar_failed()) {
        ImVec2 c(p0.x + size.x * 0.5f, p0.y + size.y * 0.5f);
        float r = (size.x < size.y ? size.x : size.y) * 0.32f;
        static float rot = 0.0f;
        rot += ImGui::GetIO().DeltaTime * 4.0f;
        dl->PathArcTo(c, r, rot, rot + IM_PI * 1.6f, 24);
        dl->PathStroke((IM_COL32(100, 160, 255, 255)), false, 2.8f);
        dl->PathClear();
        dl->PathArcTo(c, r, rot + IM_PI * 1.6f, rot + IM_PI * 2.0f, 24);
        dl->PathStroke((IM_COL32(100, 160, 255, 80)), false, 2.0f);
    } else {
        const char* un = steamServiceHelper::name();
        char initial[2] = { (char)toupper(un[0] ? un[0] : '?'), 0 };
        ImVec2 isz = ImGui::CalcTextSize(initial);
        dl->AddText(ImVec2(p0.x + (size.x - isz.x) * 0.5f, p0.y + (size.y - isz.y) * 0.5f),
            (IM_COL32(190, 200, 230, 255)), initial);
    }
    dl->AddRect(p0, p1, (IM_COL32(90, 98, 130, 255)), 0.0f, 0, 1.0f);
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

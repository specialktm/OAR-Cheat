#include "pch/pch.h"
#include "ui/takerender.hpp"
#include "ui/take_shell.hpp"
#include "ui/tabs.hpp"
#include "ui/overlays.hpp"
#include "config/config.hpp"
#include "features/entities.hpp"
#include "features/combatlog.hpp"
#include "features/silchams.hpp"
#include "features/lua_draw.hpp"
#include "features/oar_lua.hpp"
#include "features/takebind.hpp"
#include "features/manager.hpp"
#include "render/blur.hpp"
#include "render/dx11_hook.hpp"
#include "steam/steam_user.hpp"
#include "../../vendor/imgui/imgui.h"
#include <map>
#include <string>
#include <vector>
#include <chrono>
#include <cstring>

namespace takerender {
namespace {
constexpr float FADE_SEC = 5.0f;
float g_hitLeft = -1.0f;
float g_hitTop = -1.0f;

float quant_w(float w) { return ceilf(w / 8.0f) * 8.0f; }

void glass_box(ImVec2 bmin, ImVec2 bmax, float rounding, float alpha = 1.0f) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    if (g_cfg.glassBlur.load()) {
        blur::set_alpha(1.0f);
        blur::backdrop_region(dx11::swapchain(), bmin.x, bmin.y, bmax.x - bmin.x, bmax.y - bmin.y);
    }
    draw->AddRectFilled(bmin + ImVec2(0, 3), bmax + ImVec2(0, 3), IM_COL32(0, 0, 0, (int)(110 * alpha)), rounding);
    draw->AddRectFilled(bmin, bmax, IM_COL32(22, 25, 30, (int)(235 * alpha)), rounding);
    ImVec4 t = g_cfg.glassTint;
    draw->AddRectFilled(bmin, bmax, IM_COL32((int)(t.x * 255.0f), (int)(t.y * 255.0f), (int)(t.z * 255.0f), (int)(110 * alpha)), rounding);
    draw->AddRect(bmin, bmax, IM_COL32(46, 55, 66, (int)(255 * alpha)), rounding);
}

bool visuals_tab() {
    const auto& all = tabs::list();
    int cur = tabs::current();
    if (cur < 0 || cur >= (int)all.size()) return false;
    const char* id = all[cur].id;
    return id && !strcmp(id, "esp");
}

std::string steam_name() {
    std::string wun = steamServiceHelper::name();
    auto p = wun.find("##");
    if (p != std::string::npos) wun.erase(p);
    if (wun.empty()) wun = "Local";
    return wun;
}

int remote_count() {
    int n = 0;
    for (auto& p : esp::playerManager::players()) {
        if (p.self || !IS_VALID_PTR(p.actor)) continue;
        ++n;
    }
    return n;
}
}

void keybind_list() {
    if (!g_cfg.keybindList.load()) return;
    static std::map<std::string, keybind::keybind_t> states;
    struct row_t { std::string name; std::string key; bool active; };
    std::vector<row_t> rows;
    for (const feat::Entry* e : feat::all()) {
        auto* kb = feat::key_for(e->id);
        if (!kb) continue;
        int vk = kb->load();
        if (vk == 0) continue;
        auto* md = feat::mode_for(e->id);
        keybind::keybind_t& st = states[e->id];
        st.key = vk;
        st.mode = md ? (keybind::activation_mode)md->load() : keybind::activation_mode::hold;
        row_t row;
        row.name = e->name;
        row.key = keybind::get_key_name(vk);
        row.active = keybind::is_active(st);
        rows.push_back(row);
    }
    if (rows.empty()) return;
    ImFont* font = ImGui::GetFont();
    const float fs = ImGui::GetFontSize();
    const float pad_x = fs * 0.8f;
    const float pad_y = fs * 0.55f;
    const float header_h = fs + fs * 0.5f;
    const float row_h = fs + fs * 0.45f;
    const float row_gap = fs * 0.2f;
    const float gap = fs * 0.4f;
    const float rounding = 8.0f;
    const ImU32 accent_col = takeui::accent();
    float name_w = 0.0f, key_w = 0.0f;
    for (auto& row : rows) {
        float nw = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, row.name.c_str()).x;
        float kw = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, row.key.c_str()).x;
        if (nw > name_w) name_w = nw;
        if (kw > key_w) key_w = kw;
    }
    const float pill_w = key_w + fs * 0.9f;
    const float pill_h = fs + fs * 0.45f;
    const float box_w = quant_w(pad_x * 2.0f + name_w + fs * 0.8f + pill_w);
    const float box_h = quant_w(pad_y * 2.0f + header_h + gap + (float)rows.size() * row_h + (float)(rows.size() - 1) * row_gap);
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 base;
    float kx = g_cfg.kbX.load(), ky = g_cfg.kbY.load();
    if (kx >= 0.0f && ky >= 0.0f) base = ImVec2(kx, ky);
    else base = ImVec2(ImGui::GetStyle().DisplaySafeAreaPadding.x + ImGui::GetStyle().DisplayWindowPadding.x, (io.DisplaySize.y - box_h) * 0.5f);
    if (g_cfg.menuOpen.load()) {
        static bool grabbed = false;
        static ImVec2 goff(0.0f, 0.0f);
        ImVec2 hmin = base - ImVec2(4.0f, 4.0f);
        ImVec2 hmax = base + ImVec2(box_w, box_h) + ImVec2(4.0f, 4.0f);
        bool hov = io.MousePos.x >= hmin.x && io.MousePos.x <= hmax.x &&
            io.MousePos.y >= hmin.y && io.MousePos.y <= hmax.y;
        if (!grabbed && io.MouseClicked[0] && hov) {
            grabbed = true;
            goff = ImVec2(io.MousePos.x - base.x, io.MousePos.y - base.y);
        }
        if (grabbed) {
            if (io.MouseDown[0]) {
                base.x = io.MousePos.x - goff.x;
                base.y = io.MousePos.y - goff.y;
                if (base.x < 0.0f) base.x = 0.0f;
                if (base.y < 0.0f) base.y = 0.0f;
                if (base.x > io.DisplaySize.x - box_w) base.x = io.DisplaySize.x - box_w;
                if (base.y > io.DisplaySize.y - box_h) base.y = io.DisplaySize.y - box_h;
                g_cfg.kbX.store(base.x);
                g_cfg.kbY.store(base.y);
            } else grabbed = false;
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        } else if (hov) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    ImVec2 bmin = base, bmax = base + ImVec2(box_w, box_h);
    glass_box(bmin, bmax, rounding);
    float cy = bmin.y + pad_y;
    draw->AddText(font, fs, ImVec2(bmin.x + pad_x, cy + fs * 0.1f), accent_col, "KEYBINDS");
    cy += header_h;
    draw->AddLine(ImVec2(bmin.x + pad_x, cy), ImVec2(bmax.x - pad_x, cy), IM_COL32(46, 55, 66, 255), 1.0f);
    cy += gap;
    for (auto& row : rows) {
        ImU32 ncol = row.active ? IM_COL32(233, 239, 244, 255) : IM_COL32(139, 151, 165, 255);
        float ty = cy + (row_h - fs) * 0.5f;
        draw->AddText(font, fs, ImVec2(bmin.x + pad_x, ty), ncol, row.name.c_str());
        ImVec2 pmin(bmax.x - pad_x - pill_w, cy + (row_h - pill_h) * 0.5f);
        ImVec2 pmax(pmin.x + pill_w, pmin.y + pill_h);
        draw->AddRectFilled(pmin, pmax, row.active ? IM_COL32(255, 255, 255, 28) : IM_COL32(255, 255, 255, 14), pill_h * 0.3f);
        draw->AddRect(pmin, pmax, row.active ? accent_col : IM_COL32(46, 55, 66, 255), pill_h * 0.3f);
        ImVec2 ksz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, row.key.c_str());
        ImU32 kcol = row.active ? accent_col : IM_COL32(139, 151, 165, 255);
        draw->AddText(font, fs, ImVec2(pmin.x + (pill_w - ksz.x) * 0.5f, cy + (row_h - fs) * 0.5f), kcol, row.key.c_str());
        cy += row_h + row_gap;
    }
    if (g_cfg.borderFx.load()) {
        const float now = (float)ImGui::GetTime() + 5678.9f;
        ImVec4 acc = takeui::accentA(1.0f);
        ImVec4 fx1 = acc, fx2 = acc;
        if (g_cfg.borderRainbow.load()) {
            float h = fmodf(now * 0.18f, 1.0f);
            float r, g, b;
            ImGui::ColorConvertHSVtoRGB(h, 0.75f, 1.0f, r, g, b);
            fx1 = ImVec4(r, g, b, 1.0f);
            float h2 = h + 0.5f;
            if (h2 >= 1.0f) h2 -= 1.0f;
            ImGui::ColorConvertHSVtoRGB(h2, 0.75f, 1.0f, r, g, b);
            fx2 = ImVec4(r, g, b, 1.0f);
        }
        const float R = rounding;
        const float W = box_w, H = box_h;
        const float bh = W - 2.0f * R, bv = H - 2.0f * R;
        const float arc = 3.14159265f * R * 0.5f;
        const float total = 2.0f * bh + 2.0f * bv + 4.0f * arc;
        if (total > 1.0f && bh > 0.0f && bv > 0.0f) {
            auto border_point = [&](float dist) -> ImVec2 {
                float d = fmodf(dist, total);
                if (d < 0.0f) d += total;
                if (d < bh) return ImVec2(bmin.x + R + d, bmin.y);
                d -= bh;
                if (d < arc) { float t = d / R - 3.14159265f * 0.5f; return ImVec2(bmin.x + W - R + cosf(t) * R, bmin.y + R + sinf(t) * R); }
                d -= arc;
                if (d < bv) return ImVec2(bmin.x + W, bmin.y + R + d);
                d -= bv;
                if (d < arc) { float t = d / R; return ImVec2(bmin.x + W - R + cosf(t) * R, bmin.y + H - R + sinf(t) * R); }
                d -= arc;
                if (d < bh) return ImVec2(bmin.x + W - R - d, bmin.y + H);
                d -= bh;
                if (d < arc) { float t = d / R + 3.14159265f * 0.5f; return ImVec2(bmin.x + R + cosf(t) * R, bmin.y + H - R + sinf(t) * R); }
                d -= arc;
                if (d < bv) return ImVec2(bmin.x, bmin.y + H - R - d);
                d -= bv;
                float t = d / R + 3.14159265f;
                return ImVec2(bmin.x + R + cosf(t) * R, bmin.y + R + sinf(t) * R);
            };
            const float head1 = now * 100.0f + 5678.9f;
            const float head2 = head1 + total * 0.5f;
            const float ARC_LEN = 70.0f;
            const int ARC_STEPS = 12;
            for (int k = 0; k < 2; k++) {
                const float head = (k == 0) ? head1 : head2;
                const ImVec4 base = (k == 0) ? fx1 : fx2;
                ImVec2 px = border_point(head);
                for (int j = 1; j <= ARC_STEPS; j++) {
                    const ImVec2 q = border_point(head - (float)j * (ARC_LEN / (float)ARC_STEPS));
                    const float fade = 1.0f - (float)j / (float)(ARC_STEPS + 1);
                    draw->AddLine(px, q, ImGui::GetColorU32(ImVec4(base.x, base.y, base.z, base.w * fade * 0.22f)), 5.0f);
                    draw->AddLine(px, q, ImGui::GetColorU32(ImVec4(base.x, base.y, base.z, base.w * fade * 0.6f)), 2.5f);
                    draw->AddLine(px, q, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, base.w * fade * 0.35f)), 1.0f);
                    px = q;
                }
            }
        }
    }
}

void hitlog() {
    if (!g_cfg.hitlog.load()) return;
    std::vector<combat::LEntry> rows;
    {
        auto now = std::chrono::steady_clock::now();
        for (auto& e : combat::entries()) {
            if (e.kill) continue;
            float age = std::chrono::duration<float>(now - e.at).count();
            if (age > FADE_SEC) continue;
            rows.push_back(e);
        }
    }
    bool preview = rows.empty() && g_cfg.menuOpen.load() && visuals_tab();
    ImGuiIO& io = ImGui::GetIO();
    ImFont* font = ImGui::GetFont();
    const float fs = ImGui::GetFontSize();
    const float pad_x = fs * 0.7f;
    const float pad_y = fs * 0.45f;
    const float row_h = fs + fs * 0.3f;
    const float gap = fs * 0.25f;
    const float rounding = 8.0f;
    const ImU32 accent_col = takeui::accent();
    struct R { std::string text; std::string dmg; float alpha; };
    std::vector<R> draw_rows;
    auto now = std::chrono::steady_clock::now();
    if (preview) {
        draw_rows.push_back({ "Robber_07  Torso", "-34", 1.0f });
        draw_rows.push_back({ "Guard_02  Arm", "-12", 1.0f });
    } else {
        for (auto& e : rows) {
            float a = 1.0f - std::chrono::duration<float>(now - e.at).count() / FADE_SEC;
            if (a < 0.0f) a = 0.0f;
            if (a > 1.0f) a = 1.0f;
            if (a <= 0.05f) continue;
            char tb[160];
            if (!e.part.empty()) snprintf(tb, sizeof(tb), "%s  %s", e.name.c_str(), e.part.c_str());
            else snprintf(tb, sizeof(tb), "%s", e.name.c_str());
            char db[32];
            snprintf(db, sizeof(db), "-%d", e.dmg);
            draw_rows.push_back({ tb, db, a });
        }
    }
    if (draw_rows.empty()) return;
    float max_w = 0.0f, total_h = 0.0f;
    std::vector<ImVec2> sizes;
    for (auto& r : draw_rows) {
        float tw = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, r.text.c_str()).x;
        float dw = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, r.dmg.c_str()).x;
        float bw = quant_w(pad_x * 2.0f + tw + fs * 0.4f + dw);
        float bh = quant_w(row_h + pad_y * 1.6f);
        sizes.push_back(ImVec2(bw, bh));
        if (bw > max_w) max_w = bw;
        total_h += bh + gap;
    }
    total_h -= gap;
    ImVec2 base;
    float lx = g_cfg.logX.load(), ly = g_cfg.logY.load();
    if (lx >= 0.0f && ly >= 0.0f) base = ImVec2(lx, ly);
    else base = ImVec2(ImGui::GetStyle().DisplaySafeAreaPadding.x + 12.0f, (io.DisplaySize.y - total_h) * 0.5f);
    g_hitLeft = base.x;
    g_hitTop = base.y;
    if (g_cfg.menuOpen.load()) {
        static bool grabbed = false;
        static ImVec2 goff(0.0f, 0.0f);
        ImVec2 hmin = base - ImVec2(4.0f, 4.0f);
        ImVec2 hmax = base + ImVec2(max_w, total_h) + ImVec2(4.0f, 4.0f);
        bool hov = io.MousePos.x >= hmin.x && io.MousePos.x <= hmax.x &&
            io.MousePos.y >= hmin.y && io.MousePos.y <= hmax.y;
        if (!grabbed && io.MouseClicked[0] && hov) {
            grabbed = true;
            goff = ImVec2(io.MousePos.x - base.x, io.MousePos.y - base.y);
        }
        if (grabbed) {
            if (io.MouseDown[0]) {
                base.x = io.MousePos.x - goff.x;
                base.y = io.MousePos.y - goff.y;
                if (base.x < 0.0f) base.x = 0.0f;
                if (base.y < 0.0f) base.y = 0.0f;
                if (base.x > io.DisplaySize.x - max_w) base.x = io.DisplaySize.x - max_w;
                if (base.y > io.DisplaySize.y - total_h) base.y = io.DisplaySize.y - total_h;
                g_cfg.logX.store(base.x);
                g_cfg.logY.store(base.y);
                g_hitLeft = base.x;
                g_hitTop = base.y;
            } else grabbed = false;
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        } else if (hov && !io.MouseDown[0]) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    float y = base.y;
    for (size_t i = 0; i < draw_rows.size(); ++i) {
        float a = draw_rows[i].alpha;
        ImVec2 bmin(base.x, y), bmax(base.x + sizes[i].x, y + sizes[i].y);
        glass_box(bmin, bmax, rounding, a);
        float ty = bmin.y + (sizes[i].y - fs) * 0.5f;
        draw->AddText(font, fs, ImVec2(bmin.x + pad_x, ty), IM_COL32(233, 233, 239, (int)(255 * a)), draw_rows[i].text.c_str());
        float dw = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, draw_rows[i].dmg.c_str()).x;
        ImVec4 acc = takeui::accentA(1.0f);
        draw->AddText(font, fs, ImVec2(bmax.x - pad_x - dw, ty),
            IM_COL32((int)(acc.x * 255.0f), (int)(acc.y * 255.0f), (int)(acc.z * 255.0f), (int)(255 * a)), draw_rows[i].dmg.c_str());
        y += sizes[i].y + gap;
    }
}

void killfeed() {
    if (!g_cfg.killfeed.load()) return;
    std::vector<combat::LEntry> rows;
    {
        auto now = std::chrono::steady_clock::now();
        for (auto& e : combat::entries()) {
            if (!e.kill) continue;
            if (std::chrono::duration<float>(now - e.at).count() > FADE_SEC) continue;
            rows.push_back(e);
        }
    }
    bool preview = rows.empty() && g_cfg.menuOpen.load() && visuals_tab();
    ImGuiIO& io = ImGui::GetIO();
    ImFont* font = ImGui::GetFont();
    const float fs = ImGui::GetFontSize();
    const float pad_x = fs * 0.7f;
    const float pad_y = fs * 0.45f;
    const float header_h = fs + pad_y * 0.5f;
    const float row_h = fs + fs * 0.3f;
    const float lsp = fs * 0.15f;
    const float rounding = 8.0f;
    struct R { std::string name; float alpha; };
    std::vector<R> draw_rows;
    auto now = std::chrono::steady_clock::now();
    if (preview) draw_rows.push_back({ "Robber_07", 1.0f });
    else {
        for (auto& e : rows) {
            float a = 1.0f - std::chrono::duration<float>(now - e.at).count() / FADE_SEC;
            if (a < 0.0f) a = 0.0f;
            if (a > 1.0f) a = 1.0f;
            if (a <= 0.05f) continue;
            draw_rows.push_back({ e.name, a });
        }
    }
    if (draw_rows.empty()) return;
    float max_w = 0.0f;
    for (auto& r : draw_rows) {
        char buf[128];
        snprintf(buf, sizeof(buf), "KILLED  %s", r.name.c_str());
        float w = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, buf).x;
        if (w > max_w) max_w = w;
    }
    float hw = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, "KILL FEED").x;
    float width = quant_w(pad_x * 2.0f + (max_w > hw ? max_w : hw));
    float height = quant_w(header_h + pad_y * 0.5f + row_h * (float)draw_rows.size() + lsp * (float)(draw_rows.size() - 1));
    ImVec2 min_pt, max_pt;
    float kx = g_cfg.killX.load(), ky = g_cfg.killY.load();
    if (kx >= 0.0f && ky >= 0.0f) {
        min_pt = ImVec2(kx, ky);
        max_pt = ImVec2(kx + width, ky + height);
    } else {
        float dx = g_hitLeft >= 0.0f ? g_hitLeft : ImGui::GetStyle().DisplaySafeAreaPadding.x + 12.0f;
        float dy = (io.DisplaySize.y - height) * 0.5f - fs * 2.5f;
        if (g_hitTop >= 0.0f) dy = g_hitTop - height - fs * 0.35f;
        min_pt = ImVec2(dx, dy);
        max_pt = ImVec2(dx + width, dy + height);
    }
    if (g_cfg.menuOpen.load()) {
        static bool grabbed = false;
        static ImVec2 goff(0.0f, 0.0f);
        ImVec2 hmin = min_pt - ImVec2(4.0f, 4.0f);
        ImVec2 hmax = max_pt + ImVec2(4.0f, 4.0f);
        bool hov = io.MousePos.x >= hmin.x && io.MousePos.x <= hmax.x &&
            io.MousePos.y >= hmin.y && io.MousePos.y <= hmax.y;
        if (!grabbed && io.MouseClicked[0] && hov) {
            grabbed = true;
            goff = ImVec2(io.MousePos.x - min_pt.x, io.MousePos.y - min_pt.y);
        }
        if (grabbed) {
            if (io.MouseDown[0]) {
                min_pt.x = io.MousePos.x - goff.x;
                min_pt.y = io.MousePos.y - goff.y;
                if (min_pt.x < 0.0f) min_pt.x = 0.0f;
                if (min_pt.y < 0.0f) min_pt.y = 0.0f;
                if (min_pt.x > io.DisplaySize.x - width) min_pt.x = io.DisplaySize.x - width;
                if (min_pt.y > io.DisplaySize.y - height) min_pt.y = io.DisplaySize.y - height;
                g_cfg.killX.store(min_pt.x);
                g_cfg.killY.store(min_pt.y);
                max_pt = min_pt + ImVec2(width, height);
            } else grabbed = false;
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        } else if (hov && !io.MouseDown[0]) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    glass_box(min_pt, max_pt, rounding);
    draw->AddRectFilled(ImVec2(min_pt.x + 8.0f, min_pt.y), ImVec2(max_pt.x - 8.0f, min_pt.y + 1.5f), IM_COL32(255, 82, 82, 255), 1.0f);
    draw->AddText(font, fs, ImVec2(min_pt.x + pad_x, min_pt.y + pad_y * 0.5f), IM_COL32(255, 120, 120, 255), "KILL FEED");
    float y = min_pt.y + header_h + pad_y * 0.25f;
    for (auto& r : draw_rows) {
        float a = r.alpha;
        draw->AddText(font, fs, ImVec2(min_pt.x + pad_x, y), IM_COL32(255, 82, 82, (int)(255 * a)), "KILLED");
        float kw = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, "KILLED").x;
        draw->AddText(font, fs, ImVec2(min_pt.x + pad_x + kw + fs * 0.5f, y), IM_COL32(233, 233, 239, (int)(255 * a)), r.name.c_str());
        y += row_h + lsp;
    }
}

void player_count() {
    ImGuiIO& io = ImGui::GetIO();
    int n = remote_count();
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    ImFont* font = ImGui::GetFont();
    const float fs = ImGui::GetFontSize();
    const char* label = "Players:";
    char countb[16];
    snprintf(countb, sizeof(countb), "%2d", n);
    ImVec2 lsz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, label);
    ImVec2 csz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, countb);
    const float w = quant_w(16.0f + lsz.x + 6.0f + csz.x + 16.0f);
    const float h = quant_w(fs + 14.0f);
    ImVec2 p0;
    float sx = g_cfg.pcX.load(), sy = g_cfg.pcY.load();
    if (sx >= 0.0f && sy >= 0.0f) p0 = ImVec2(sx, sy);
    else p0 = ImVec2(ImGui::GetStyle().DisplaySafeAreaPadding.x + 12.0f, io.DisplaySize.y - 12.0f - h);
    if (g_cfg.menuOpen.load()) {
        static bool grabbed = false;
        static ImVec2 goff(0.0f, 0.0f);
        ImVec2 hmin = p0 - ImVec2(4.0f, 4.0f);
        ImVec2 hmax = p0 + ImVec2(w, h) + ImVec2(4.0f, 4.0f);
        bool hov = io.MousePos.x >= hmin.x && io.MousePos.x <= hmax.x &&
            io.MousePos.y >= hmin.y && io.MousePos.y <= hmax.y;
        if (!grabbed && io.MouseClicked[0] && hov) {
            grabbed = true;
            goff = ImVec2(io.MousePos.x - p0.x, io.MousePos.y - p0.y);
        }
        if (grabbed) {
            if (io.MouseDown[0]) {
                p0.x = io.MousePos.x - goff.x;
                p0.y = io.MousePos.y - goff.y;
                if (p0.x < 0.0f) p0.x = 0.0f;
                if (p0.y < 0.0f) p0.y = 0.0f;
                if (p0.x > io.DisplaySize.x - w) p0.x = io.DisplaySize.x - w;
                if (p0.y > io.DisplaySize.y - h) p0.y = io.DisplaySize.y - h;
                g_cfg.pcX.store(p0.x);
                g_cfg.pcY.store(p0.y);
            } else grabbed = false;
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        } else if (hov) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    ImVec2 p1 = p0 + ImVec2(w, h);
    glass_box(p0, p1, 8.0f);
    float tx = p0.x + 16.0f, ty = p0.y + 7.0f;
    draw->AddText(font, fs, ImVec2(tx, ty), IM_COL32(139, 151, 165, 255), label);
    draw->AddText(font, fs, ImVec2(tx + lsz.x + 6.0f, ty), takeui::accent(), countb);
}

void watermark() {
    if (!g_cfg.watermark.load()) return;
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    ImFont* font = ImGui::GetFont();
    const float fs = ImGui::GetFontSize();
    char fpsb[16]; snprintf(fpsb, sizeof(fpsb), "%3d FPS", (int)io.Framerate);
    char timeb[24]; takeui::clock_str(timeb, sizeof(timeb), g_cfg.clock24h.load());
    int wm_players = remote_count();
    std::string wm_name = steamServiceHelper::name();
    {
        auto p = wm_name.find("##");
        if (p != std::string::npos) wm_name.erase(p);
    }
    if (wm_name.empty()) wm_name = "Local";
    char playersb[32]; snprintf(playersb, sizeof(playersb), "Players:%2d", wm_players);
    const char* brand = "OAR";
    const char* sep = "  |  ";
    char userb[96]; snprintf(userb, sizeof(userb), "%s: Lifetime", wm_name.c_str());
    ImVec2 a = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, brand);
    ImVec2 b = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, playersb);
    ImVec2 c = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, fpsb);
    ImVec2 d = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, timeb);
    ImVec2 e = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, userb);
    ImVec2 s = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, sep);
    float w = quant_w(16.0f + a.x + s.x + b.x + s.x + c.x + s.x + d.x + s.x + e.x + 16.0f);
    float h = quant_w(fs + 14.0f);
    ImVec2 p0;
    float wx = g_cfg.wmX.load(), wy = g_cfg.wmY.load();
    if (wx >= 0.0f && wy >= 0.0f) p0 = ImVec2(wx, wy);
    else p0 = ImVec2((io.DisplaySize.x - w) * 0.5f, ImGui::GetStyle().DisplaySafeAreaPadding.y + 12.0f);
    if (g_cfg.menuOpen.load()) {
        static bool grabbed = false;
        static ImVec2 goff(0.0f, 0.0f);
        ImVec2 hmin = p0 - ImVec2(4.0f, 4.0f);
        ImVec2 hmax = p0 + ImVec2(w, h) + ImVec2(4.0f, 4.0f);
        bool hov = io.MousePos.x >= hmin.x && io.MousePos.x <= hmax.x &&
            io.MousePos.y >= hmin.y && io.MousePos.y <= hmax.y;
        if (!grabbed && io.MouseClicked[0] && hov) {
            grabbed = true;
            goff = ImVec2(io.MousePos.x - p0.x, io.MousePos.y - p0.y);
        }
        if (grabbed) {
            if (io.MouseDown[0]) {
                p0.x = io.MousePos.x - goff.x;
                p0.y = io.MousePos.y - goff.y;
                if (p0.x < 0.0f) p0.x = 0.0f;
                if (p0.y < 0.0f) p0.y = 0.0f;
                if (p0.x > io.DisplaySize.x - w) p0.x = io.DisplaySize.x - w;
                if (p0.y > io.DisplaySize.y - h) p0.y = io.DisplaySize.y - h;
                g_cfg.wmX.store(p0.x);
                g_cfg.wmY.store(p0.y);
            } else grabbed = false;
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        } else if (hov) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    ImVec2 p1 = p0 + ImVec2(w, h);
    ImU32 acc_col = takeui::accent();
    glass_box(p0, p1, 8.0f);
    float tx = p0.x + 16.0f, ty = p0.y + 7.0f;
    draw->AddText(font, fs, ImVec2(tx, ty), acc_col, brand); tx += a.x;
    draw->AddText(font, fs, ImVec2(tx, ty), IM_COL32(139, 151, 165, 255), sep); tx += s.x;
    draw->AddText(font, fs, ImVec2(tx, ty), IM_COL32(233, 239, 244, 255), playersb); tx += b.x;
    draw->AddText(font, fs, ImVec2(tx, ty), IM_COL32(139, 151, 165, 255), sep); tx += s.x;
    draw->AddText(font, fs, ImVec2(tx, ty), IM_COL32(233, 239, 244, 255), fpsb); tx += c.x;
    draw->AddText(font, fs, ImVec2(tx, ty), IM_COL32(139, 151, 165, 255), sep); tx += s.x;
    draw->AddText(font, fs, ImVec2(tx, ty), IM_COL32(139, 151, 165, 255), timeb); tx += d.x;
    draw->AddText(font, fs, ImVec2(tx, ty), IM_COL32(139, 151, 165, 255), sep); tx += s.x;
    draw->AddText(font, fs, ImVec2(tx, ty), acc_col, userb);
}

void visuals() {
    silchams::render();
    drawing::render_all(ImGui::GetBackgroundDrawList());
    combat::tick();
    lua::snapshot();
    watermark();
    player_count();
    keybind_list();
    killfeed();
    hitlog();
    overlays::render_all();
}

}

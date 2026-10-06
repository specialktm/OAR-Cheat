#include "pch/pch.h"
#include "ui/popouts.hpp"
#include "core/config.hpp"
#include "features/esp.hpp"
#include "features/shop.hpp"
#include "features/manager.hpp"
#include "features/modelview.hpp"
#include "features/manager.hpp"

#include <cmath>

#include "../../vendor/imgui/imgui.h"

namespace popouts {

static const char* kANames[8] = { "top", "top-right", "right", "bottom-right",
    "bottom", "bottom-left", "left", "top-left" };

// normalized mannequin overlay for the preview middle box
static void skel_overlay(ImDrawList* dl, float x0, float y0, float x1, float y1, ImU32 c) {
    float w = x1 - x0, h = y1 - y0;
    auto P = [&](float fx, float fy) { return ImVec2(x0 + w * fx, y0 + h * fy); };
    ImVec2 head = P(0.5f, 0.10f), neck = P(0.5f, 0.22f), pel = P(0.5f, 0.55f);
    ImVec2 hl = P(0.26f, 0.42f), hr = P(0.74f, 0.42f);
    ImVec2 el = P(0.30f, 0.30f), er = P(0.70f, 0.30f);
    ImVec2 fl = P(0.38f, 0.94f), fr = P(0.62f, 0.94f);
    ImVec2 kl = P(0.36f, 0.74f), kr = P(0.64f, 0.74f);
    dl->AddCircle(head, 7.0f, c, 14, 1.6f);
    dl->AddLine(neck, pel, c, 2.0f);
    dl->AddLine(neck, el, c, 1.6f); dl->AddLine(el, hl, c, 1.6f);
    dl->AddLine(neck, er, c, 1.6f); dl->AddLine(er, hr, c, 1.6f);
    dl->AddLine(pel, kl, c, 1.6f); dl->AddLine(kl, fl, c, 1.6f);
    dl->AddLine(pel, kr, c, 1.6f); dl->AddLine(kr, fr, c, 1.6f);
}

static int s_mode = 0; // 0 players, 1 NPC

static void draw_esp_preview() {
    if (!g_cfg.showEspPreview.load()) return;
    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(560, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("ESP Preview", &open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }
    if (!open) g_cfg.showEspPreview.store(false);
    bool npc = (s_mode == 1);
    if (ImGui::Button(npc ? "NPC" : "Players", ImVec2(90, 24))) s_mode = 1 - s_mode;

    float w = ImGui::GetContentRegionAvail().x;
    ImVec2 size(w, 300);
    ImVec2 cur = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // night scene
    const int bands = 18;
    for (int i = 0; i < bands; ++i) {
        float t0 = (float)i / bands, t1 = (float)(i + 1) / bands;
        dl->AddRectFilled(ImVec2(cur.x, cur.y + size.y * t0),
            ImVec2(cur.x + size.x, cur.y + size.y * t1),
            IM_COL32((int)(10 + 8 * t0), (int)(12 + 8 * t0), (int)(30 + 26 * t0), 255));
    }
    float horizon = cur.y + size.y * 0.62f;
    dl->AddLine(ImVec2(cur.x, horizon), ImVec2(cur.x + size.x, horizon),
        IM_COL32(90, 140, 220, 90), 1.5f);
    dl->AddRectFilled(ImVec2(cur.x, horizon), ImVec2(cur.x + size.x, cur.y + size.y),
        IM_COL32(8, 10, 18, 120));
    ImVec2 vanish(cur.x + size.x * 0.5f, horizon);
    for (int i = -4; i <= 4; ++i) {
        if (i == 0) continue;
        dl->AddLine(vanish, ImVec2(cur.x + size.x * (0.5f + i * 0.22f), cur.y + size.y),
            IM_COL32(50, 60, 100, 90));
    }
    dl->AddRectFilled(ImVec2(cur.x + size.x * 0.06f, horizon - 40),
        ImVec2(cur.x + size.x * 0.06f + 48, horizon), IM_COL32(24, 28, 46, 255), 3.0f);
    ImVec2 lamp(cur.x + size.x * 0.9f, horizon - 60);
    for (int i = 3; i >= 1; --i)
        dl->AddCircleFilled(lamp, (float)i * 9.0f, IM_COL32(255, 200, 110, 18));
    dl->AddCircleFilled(lamp, 4.0f, IM_COL32(255, 210, 130, 255));
    ImVec2 ch(cur.x + size.x * 0.5f, cur.y + size.y * 0.40f);
    dl->AddLine(ImVec2(ch.x - 8, ch.y), ImVec2(ch.x + 8, ch.y), IM_COL32(120, 200, 255, 160));
    dl->AddLine(ImVec2(ch.x, ch.y - 8), ImVec2(ch.x, ch.y + 8), IM_COL32(120, 200, 255, 160));

    // three samples, middle one interactive
    ImVec4 base = npc ? g_cfg.guardCol : g_cfg.playerCol;
    ImU32 col = IM_COL32((int)(base.x * 255), (int)(base.y * 255), (int)(base.z * 255), 255);
    int style = g_cfg.espBoxStyle.load();
    int anchor = g_cfg.espLabelAnchor.load();
    bool showBox = npc ? g_cfg.guardBox.load() : g_cfg.playerBox.load();
    bool showSkel = npc ? g_cfg.guardSkel.load() : g_cfg.playerSkel.load();
    float bw = 56.0f, bh = 108.0f;
    static bool labWas = false;
    static int hoverAnchor = 0;
    bool anyLab = false;

    for (int i = 0; i < 3; ++i) {
        float cx = cur.x + size.x * (0.20f + 0.30f * i);
        float cy = cur.y + size.y * (i == 1 ? g_cfg.espPrevY.load() : 0.52f);
        if (i == 1) cx = cur.x + g_cfg.espPrevX.load() * size.x;
        float x0 = cx - bw / 2, y0 = cy - bh / 2, x1 = cx + bw / 2, y1 = cy + bh / 2;
        bool alerted = npc && i == 1;
        ImU32 c = alerted ? IM_COL32(255, 70, 70, 255) : col;
        char nm[32], dm[16];
        if (npc) {
            snprintf(nm, sizeof(nm), i == 1 ? "ALERTED" : "Guard_%02d", i * 8 + 3);
            snprintf(dm, sizeof(dm), "%dm", 14 + i * 12);
        } else {
            snprintf(nm, sizeof(nm), "Robber_%02d", i * 5 + 7);
            snprintf(dm, sizeof(dm), "%dm", 12 + i * 12);
        }
        float lx, ly;
        esp::anchor_pos(anchor, x0, y0, x1, y1, 13.0f, 0, lx, ly);
        ImVec2 nsz = ImGui::CalcTextSize(nm);
        ImVec2 lp(lx - nsz.x / 2, ly - 4);
        dl->AddText(lp, IM_COL32(235, 238, 248, 255), nm);
        if (showBox)
            esp::draw_box_style(dl, style, x0, y0, x1, y1, c);
        if (showBox && g_cfg.boxFill.load())
            dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), (c & 0x00FFFFFFu) | (30u << 24));
        if (showSkel)
            skel_overlay(dl, x0, y0, x1, y1, IM_COL32(235, 245, 255, 255));
        float frac = 0.85f - i * 0.22f;
        dl->AddRectFilled(ImVec2(x0 - 7, y0), ImVec2(x0 - 3, y1), IM_COL32(0, 0, 0, 170));
        ImU32 hc = frac > 0.5f ? IM_COL32(90, 255, 120, 255)
            : frac > 0.25f ? IM_COL32(255, 200, 60, 255) : IM_COL32(255, 70, 70, 255);
        dl->AddRectFilled(ImVec2(x0 - 7, y1 - bh * frac), ImVec2(x0 - 3, y1), hc);
        if (npc && alerted)
            dl->AddCircle(ImVec2(x1 + 8, y0 + 8), 5.0f, IM_COL32(255, 70, 70, 255), 12, 1.5f);
        ImVec2 dsz = ImGui::CalcTextSize(dm);
        dl->AddText(ImVec2(cx - dsz.x / 2, y1 + 5), IM_COL32(200, 204, 214, 255), dm);

        if (i == 1) {
            // figure drag
            ImGui::SetCursorScreenPos(ImVec2((x0 + cx) / 2 - 40, y0));
            ImGui::InvisibleButton("##pvfig", ImVec2(80, bh));
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                ImVec2 d = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
                float nx = (cx + d.x - cur.x) / size.x, ny = (cy + d.y - cur.y) / size.y;
                nx = nx < 0.05f ? 0.05f : (nx > 0.95f ? 0.95f : nx);
                ny = ny > 0.95f ? 0.95f : (ny < 0.05f ? 0.05f : ny);
                g_cfg.espPrevX.store(nx); g_cfg.espPrevY.store(ny);
                ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
            }
            if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            // label drag with snap
            ImGui::SetCursorScreenPos(ImVec2(lp.x - 6, lp.y - 3));
            ImGui::InvisibleButton("##pvlab", ImVec2(nsz.x + 12, 20));
            bool active = ImGui::IsItemActive();
            anyLab = anyLab || active;
            if (active) {
                ImVec2 mp = ImGui::GetIO().MousePos;
                float best = 1e9f;
                hoverAnchor = anchor;
                for (int a = 0; a < 8; ++a) {
                    float ax, ay;
                    esp::anchor_pos(a, x0, y0, x1, y1, 13.0f, 0, ax, ay);
                    float dx = mp.x - ax, dy = mp.y - ay;
                    float dd = dx * dx + dy * dy;
                    if (dd < best) { best = dd; hoverAnchor = a; }
                }
                if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            }
            if (labWas && !active) {
                g_cfg.espLabelAnchor.store(hoverAnchor);
                anchor = hoverAnchor;
            }
            for (int a = 0; a < 8; ++a) {
                float ax, ay;
                esp::anchor_pos(a, x0, y0, x1, y1, 0.0f, 0, ax, ay);
                bool hot = active ? (a == hoverAnchor) : (a == anchor);
                dl->AddCircleFilled(ImVec2(ax, ay), hot ? 4.0f : 2.5f,
                    hot ? IM_COL32(140, 220, 255, 255) : IM_COL32(120, 130, 170, 140));
            }
        }
    }
    labWas = anyLab;
    ImGui::SetCursorScreenPos(ImVec2(cur.x, cur.y + size.y + 6));
    static const char* kAN[8] = { "top", "top-right", "right", "bottom-right",
        "bottom", "bottom-left", "left", "top-left" };
    ImGui::TextDisabled("drag figure to move - drag label, it snaps (%s)", kAN[anchor & 7]);
    ImGui::End();
}

// ---- item viewer popout (glyph + data + grant) ----
static void draw_item_glyph(ImDrawList* dl, ImVec2 c, float s, const std::string& name, ImU32 col) {
    std::string n = name;
    for (auto& ch : n) ch = (char)tolower(ch);
    bool gun = n.find("weapon") != std::string::npos || n.find("gun") != std::string::npos
        || n.find("rifle") != std::string::npos || n.find("pistol") != std::string::npos;
    bool mask = n.find("mask") != std::string::npos;
    bool map = n.find("map") != std::string::npos;
    if (gun) {
        dl->AddRectFilled(ImVec2(c.x - 26 * s, c.y - 6 * s), ImVec2(c.x + 14 * s, c.y + 6 * s), col, 3.0f);
        dl->AddLine(ImVec2(c.x + 14 * s, c.y), ImVec2(c.x + 30 * s, c.y), col, 3.0f);
        dl->AddLine(ImVec2(c.x - 8 * s, c.y + 6 * s), ImVec2(c.x - 8 * s, c.y + 20 * s), col, 4.0f);
    } else if (mask) {
        dl->AddCircle(ImVec2(c.x, c.y), 20 * s, col, 24, 2.5f);
        dl->AddCircleFilled(ImVec2(c.x - 7 * s, c.y - 2 * s), 3 * s, col);
        dl->AddCircleFilled(ImVec2(c.x + 7 * s, c.y - 2 * s), 3 * s, col);
        dl->AddLine(ImVec2(c.x - 8 * s, c.y + 9 * s), ImVec2(c.x + 8 * s, c.y + 9 * s), col, 2.0f);
    } else if (map) {
        dl->AddRectFilled(ImVec2(c.x - 22 * s, c.y - 16 * s), ImVec2(c.x + 22 * s, c.y + 16 * s), col, 2.0f);
        dl->AddLine(ImVec2(c.x - 7 * s, c.y - 16 * s), ImVec2(c.x - 7 * s, c.y + 16 * s),
            IM_COL32(10, 11, 20, 255), 2.0f);
        dl->AddLine(ImVec2(c.x + 7 * s, c.y - 16 * s), ImVec2(c.x + 7 * s, c.y + 16 * s),
            IM_COL32(10, 11, 20, 255), 2.0f);
    } else {
        dl->AddRect(ImVec2(c.x - 16 * s, c.y - 16 * s), ImVec2(c.x + 16 * s, c.y + 16 * s), col, 5.0f, 0, 2.5f);
        dl->AddLine(ImVec2(c.x - 16 * s, c.y), ImVec2(c.x + 16 * s, c.y), col, 1.5f);
    }
}

static void draw_item_popout() {
    if (!g_cfg.showItemPreview.load()) return;
    ImGui::SetNextWindowSize(ImVec2(380, 220), ImGuiCond_FirstUseEver);
    bool open = true;
    if (!ImGui::Begin("Item Viewer", &open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }
    if (!open) g_cfg.showItemPreview.store(false);
    const auto& cat = shop::catalog(false);
    int sel = shop::selected();
    if (sel < 0 || sel >= (int)cat.size()) {
        ImGui::TextDisabled("Pick an item in Players > inventory.");
    } else {
        const auto& it = cat[sel];
        ImVec2 cur = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 c(cur.x + 60, cur.y + 55);
        dl->AddRectFilled(ImVec2(cur.x, cur.y), ImVec2(cur.x + 120, cur.y + 110),
            IM_COL32(16, 18, 30, 255), 4.0f);
        dl->AddRect(ImVec2(cur.x, cur.y), ImVec2(cur.x + 120, cur.y + 110),
            IM_COL32(140, 110, 255, 120), 4.0f, 0, 1.0f);
        draw_item_glyph(dl, c, 1.0f, it.name, IM_COL32(150, 200, 255, 255));
        ImGui::Dummy(ImVec2(120, 110));
        ImGui::SameLine(0, 12);
        ImGui::BeginGroup();
        ImGui::TextWrapped("%s", it.name.c_str());
        ImGui::TextDisabled("cash $%d   coins %d   unlock L%d", it.cash, it.coin, it.level);
        ImGui::TextDisabled("steam def %d   category %d", it.defid, it.cat);
        if (ImGui::Button("GRANT TO INVENTORY", ImVec2(200, 26))) {
            void* cls = it.cls;
            feat::queue_fn([cls] { shop::grant_class(cls); });
        }
        ImGui::EndGroup();
    }
    ImGui::End();
}

void render_all() {
    // ESP + item previews are embedded (ESP tab / Players tab) - single
    // preview rendered while on the tab, no detached popouts.
    modelview::draw_model_popout();
}

} // namespace popouts

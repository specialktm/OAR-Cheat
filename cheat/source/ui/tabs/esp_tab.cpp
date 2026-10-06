#include "pch/pch.h"
#include "ui/tabs.hpp"
#include "ui/widgets.hpp"
#include "core/config.hpp"
#include "features/esp.hpp"

namespace {
static const char* kAnchors[8] = {
    "Top", "Top-right", "Right", "Bottom-right",
    "Bottom", "Bottom-left", "Left", "Top-left"
};
static const char* kBoxTypes[3] = { "2D full", "Corners", "3D box" };
static const char* kNameStyles[2] = { "Full name", "Short name" };
static const char* kHpStyles[3] = { "Bar + number", "Bar only", "Number only" };
static const char* kAN[8] = { "top", "top-right", "right", "bottom-right",
    "bottom", "bottom-left", "left", "top-left" };

static int visMode = 0;

// Single embedded preview (rendered while on this tab). Mirrors live math:
// box style + fill, health bar side, name @ espLabelAnchor line 0,
// sub-line (distance/state) @ espSubAnchor. No fake stickman - skeleton is
// live bones only (see "Skeleton" toggle). Every element is draggable:
// figure moves, labels snap to anchors, health bar flips side.
static void embedded_preview(bool npc) {
    float w = ImGui::GetContentRegionAvail().x;
    ImVec2 size(w, 300);
    ImVec2 cur = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // backdrop
    dl->AddRectFilled(cur, ImVec2(cur.x + size.x, cur.y + size.y),
        IM_COL32(10, 12, 22, 255), 6.0f);
    const int bands = 12;
    for (int i = 0; i < bands; ++i) {
        float t0 = (float)i / bands, t1 = (float)(i + 1) / bands;
        dl->AddRectFilled(ImVec2(cur.x, cur.y + size.y * t0),
            ImVec2(cur.x + size.x, cur.y + size.y * t1),
            IM_COL32((int)(10 + 6 * t0), (int)(12 + 6 * t0), (int)(26 + 18 * t0), 255));
    }
    dl->AddRect(cur, ImVec2(cur.x + size.x, cur.y + size.y),
        IM_COL32(60, 70, 100, 255), 6.0f, 0, 1.0f);

    ImVec4 base = npc ? g_cfg.guardCol : g_cfg.playerCol;
    ImU32 col = IM_COL32((int)(base.x * 255), (int)(base.y * 255), (int)(base.z * 255), 255);
    int style = g_cfg.espBoxStyle.load();
    int anchor = g_cfg.espLabelAnchor.load() & 7;
    int subAnchor = g_cfg.espSubAnchor.load() & 7;
    bool hpLeft = g_cfg.hpLeft.load();
    float s = g_cfg.espTextScale.load();
    float boxW = g_cfg.espBoxW.load();
    float nudge = g_cfg.espNameDy.load() * s;

    float bh = 150.0f, bwBox = bh * boxW;
    float cx = cur.x + g_cfg.espPrevX.load() * size.x;
    float cy = cur.y + g_cfg.espPrevY.load() * size.y;
    float x0 = cx - bwBox / 2, y0 = cy - bh / 2, x1 = cx + bwBox / 2, y1 = cy + bh / 2;

    bool showBox = npc ? g_cfg.guardBox.load() : g_cfg.playerBox.load();
    bool showName = npc ? g_cfg.guardState.load() : g_cfg.playerName.load();
    bool showSub = npc ? (g_cfg.guardState.load() && g_cfg.guardDistance.load())
                       : g_cfg.playerDistance.load();
    bool showHp = !npc && g_cfg.playerHealth.load();
    int hs = g_cfg.healthStyle.load();
    const char* nm = npc ? "ALERTED" : "Robber_07";
    const char* sub = npc ? "ALERTED [26m]" : "24m";

    // box + fill (same helpers as live)
    if (showBox) {
        if (g_cfg.boxFill.load())
            dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), (col & 0x00FFFFFFu) | (30u << 24));
        esp::draw_box_style(dl, style, x0, y0, x1, y1, col);
    } else {
        dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(90, 100, 130, 120), 0.0f, 0, 1.0f);
        ImVec2 tsz = ImGui::CalcTextSize("box off");
        dl->AddText(ImVec2(cx - tsz.x / 2, cy - tsz.y / 2), IM_COL32(140, 148, 170, 255), "box off");
    }

    // health bar (draggable: drops on left/right half to flip side)
    float hx0 = 0, hx1 = 0;
    if (showHp && hs != 2) {
        hx0 = hpLeft ? x0 - 7 : x1 + 3;
        hx1 = hpLeft ? x0 - 3 : x1 + 7;
        float frac = 0.72f;
        dl->AddRectFilled(ImVec2(hx0, y0), ImVec2(hx1, y1), IM_COL32(0, 0, 0, 170));
        ImU32 hc = IM_COL32(90, 255, 120, 255);
        dl->AddRectFilled(ImVec2(hx0, y1 - bh * frac), ImVec2(hx1, y1), hc);
    }
    if (showHp && hs != 1) {
        ImVec2 tsz = ImGui::CalcTextSize("72");
        dl->AddText(ImVec2(cx - tsz.x / 2, y1 + 4), IM_COL32(255, 255, 255, 220), "72");
    }

    // labels via live anchor math
    float lx = 0, ly = 0, sx = 0, sy = 0;
    ImVec2 nsz(0, 0), ssz(0, 0);
    if (showName) {
        esp::anchor_pos(anchor, x0, y0, x1, y1, 13.0f * s, 0, lx, ly);
        nsz = ImGui::CalcTextSize(nm);
        dl->AddText(ImVec2(lx - nsz.x / 2, ly + nudge), IM_COL32(235, 238, 248, 255), nm);
    }
    if (showSub) {
        int line = showName ? 1 : 0;
        esp::anchor_pos(subAnchor, x0, y0, x1, y1, 13.0f * s, line, sx, sy);
        ssz = ImGui::CalcTextSize(sub);
        dl->AddText(ImVec2(sx - ssz.x / 2, sy + nudge), IM_COL32(200, 204, 214, 255), sub);
    }

    // skeleton note (live bones only, no fake mannequin)
    bool sk = npc ? g_cfg.guardSkel.load() : g_cfg.playerSkel.load();
    if (sk) {
        ImVec2 tsz = ImGui::CalcTextSize("skeleton: live bones in-game");
        dl->AddText(ImVec2(cx - tsz.x / 2, y1 + 22), IM_COL32(140, 220, 255, 200),
            "skeleton: live bones in-game");
    }

    // ---- drag zones (all box elements) ----
    // figure
    ImGui::SetCursorScreenPos(ImVec2(x0, y0));
    ImGui::InvisibleButton("##pvfig", ImVec2(x1 - x0, y1 - y0));
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 d = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
        float nx = (cx + d.x - cur.x) / size.x, ny = (cy + d.y - cur.y) / size.y;
        nx = nx < 0.05f ? 0.05f : (nx > 0.95f ? 0.95f : nx);
        ny = ny > 0.95f ? 0.95f : (ny < 0.05f ? 0.05f : ny);
        g_cfg.espPrevX.store(nx); g_cfg.espPrevY.store(ny);
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
    }
    if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll); ImGui::SetTooltip("drag figure to move"); }

    // name label -> snap espLabelAnchor
    static int hoverA = 0;
    static bool nameWas = false;
    if (showName) {
        ImGui::SetCursorScreenPos(ImVec2(lx - nsz.x / 2 - 6, ly + nudge - 3));
        ImGui::InvisibleButton("##pvname", ImVec2(nsz.x + 12, 20));
        bool act = ImGui::IsItemActive();
        if (act) {
            ImVec2 mp = ImGui::GetIO().MousePos;
            float best = 1e9f;
            hoverA = anchor;
            for (int a = 0; a < 8; ++a) {
                float ax, ay;
                esp::anchor_pos(a, x0, y0, x1, y1, 13.0f * s, 0, ax, ay);
                float dx = mp.x - ax, dy = mp.y - (ay + nudge);
                float dd = dx * dx + dy * dy;
                if (dd < best) { best = dd; hoverA = a; }
            }
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        }
        if (nameWas && !act) g_cfg.espLabelAnchor.store(hoverA);
        nameWas = act;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("drag name, snaps to anchor");
    }

    // sub label -> snap espSubAnchor
    static int hoverS = 4;
    static bool subWas = false;
    if (showSub) {
        ImGui::SetCursorScreenPos(ImVec2(sx - ssz.x / 2 - 6, sy + nudge - 3));
        ImGui::InvisibleButton("##pvsub", ImVec2(ssz.x + 12, 20));
        bool act = ImGui::IsItemActive();
        if (act) {
            ImVec2 mp = ImGui::GetIO().MousePos;
            float best = 1e9f;
            hoverS = subAnchor;
            for (int a = 0; a < 8; ++a) {
                float ax, ay;
                esp::anchor_pos(a, x0, y0, x1, y1, 13.0f * s, 1, ax, ay);
                float dx = mp.x - ax, dy = mp.y - (ay + nudge);
                float dd = dx * dx + dy * dy;
                if (dd < best) { best = dd; hoverS = a; }
            }
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        }
        if (subWas && !act) g_cfg.espSubAnchor.store(hoverS);
        subWas = act;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("drag info line, snaps to anchor");
    }

    // health bar -> drag to flip side
    if (showHp && hs != 2) {
        ImGui::SetCursorScreenPos(ImVec2((hx0 < hx1 ? hx0 : hx1) - 3, y0));
        ImGui::InvisibleButton("##pvhp", ImVec2(10, y1 - y0));
        if (ImGui::IsItemActive()) {
            ImVec2 mp = ImGui::GetIO().MousePos;
            g_cfg.hpLeft.store(mp.x < cx);
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("drag health bar left/right");
    }

    // anchor dots (name = cyan, sub = amber)
    for (int a = 0; a < 8; ++a) {
        float ax, ay;
        esp::anchor_pos(a, x0, y0, x1, y1, 0.0f, 0, ax, ay);
        bool hot = (a == anchor);
        dl->AddCircleFilled(ImVec2(ax, ay), hot ? 4.0f : 2.5f,
            hot ? IM_COL32(140, 220, 255, 255) : IM_COL32(120, 130, 170, 140));
        esp::anchor_pos(a, x0, y0, x1, y1, 0.0f, 1, ax, ay);
        bool hot2 = (a == subAnchor);
        dl->AddCircleFilled(ImVec2(ax, ay), hot2 ? 3.0f : 1.8f,
            hot2 ? IM_COL32(255, 200, 110, 255) : IM_COL32(120, 130, 170, 80));
    }

    ImGui::SetCursorScreenPos(ImVec2(cur.x, cur.y + size.y + 6));
    ImGui::TextDisabled("drag figure / name / info / health - name:%s sub:%s",
        kAN[anchor & 7], kAN[subAnchor & 7]);
}

void draw_esp() {
    const char* modes[2] = { "ESP", "NPC ESP" };
    widgets::segmented(modes, 2, visMode);
    bool npc = (visMode == 1);
    ImGui::Spacing();

    float bw = ImGui::GetContentRegionAvail().x;
    float half = (bw - 8) / 2;
    ImGui::BeginChild("##espcard", ImVec2(half, 0), true);
    ImGui::TextDisabled(npc ? "NPC ESP" : "ESP");
    ImGui::Separator();
    if (!npc) {
        widgets::check_row("Enable players", g_cfg.playerEsp, &g_cfg.playerCol);
        widgets::check_row("Box", g_cfg.playerBox, nullptr);
        widgets::check_row("Box fill", g_cfg.boxFill, nullptr);
        widgets::check_row("Healthbar", g_cfg.playerHealth, &g_cfg.healthCol);
        widgets::check_row("Gradient", g_cfg.healthGradient, nullptr);
        widgets::check_row("Name", g_cfg.playerName, nullptr);
        widgets::check_row("Distance", g_cfg.playerDistance, nullptr);
        widgets::check_row("Skeleton", g_cfg.playerSkel, nullptr);
    } else {
        widgets::check_row("Enable guards", g_cfg.guardEsp, &g_cfg.guardCol);
        widgets::check_row("Box", g_cfg.guardBox, nullptr);
        widgets::check_row("Box fill", g_cfg.boxFill, nullptr);
        widgets::check_row("State", g_cfg.guardState, nullptr);
        widgets::check_row("Distance", g_cfg.guardDistance, nullptr);
        widgets::check_row("Skeleton", g_cfg.guardSkel, nullptr);
    }
    ImGui::EndChild();
    ImGui::SameLine(0, 8);
    ImGui::BeginChild("##espset", ImVec2(0, 0), true);
    ImGui::TextDisabled("ESP Settings");
    ImGui::Separator();
    widgets::dropdown("Box type", kBoxTypes, 3, g_cfg.espBoxStyle);
    widgets::dropdown("Name anchor", kAnchors, 8, g_cfg.espLabelAnchor);
    widgets::dropdown("Info anchor", kAnchors, 8, g_cfg.espSubAnchor);
    widgets::dropdown("Name type", kNameStyles, 2, g_cfg.nameMode);
    widgets::dropdown("Healthbar style", kHpStyles, 3, g_cfg.healthStyle);
    widgets::slider_f("Box width", g_cfg.espBoxW, 0.25f, 0.8f);
    widgets::slider_f("Text scale", g_cfg.espTextScale, 0.7f, 1.8f);
    widgets::slider_f("Label height", g_cfg.espNameDy, -40.0f, 20.0f);
    ImGui::EndChild();

    ImGui::BeginChild("##espprev", ImVec2(0, 0), true);
    ImGui::TextDisabled("PREVIEW (live on this tab - drag everything)");
    ImGui::Separator();
    embedded_preview(npc);
    ImGui::EndChild();
}
struct Reg { Reg() { tabs::add({ "esp", "Visuals", draw_esp }); } };
static Reg g_reg;
} // namespace

#include "pch/pch.h"
#include "ui/tabs.hpp"
#include "ui/widgets.hpp"
#include "ui/menu.hpp"
#include "ui/take_shell.hpp"
#include "config/config.hpp"
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

static void embedded_preview(bool npc) {
    float w = ImGui::GetContentRegionAvail().x;
    ImVec2 size(w, 300);
    ImVec2 cur = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(cur, ImVec2(cur.x + size.x, cur.y + size.y),
        (IM_COL32(10, 12, 22, 255)), 6.0f);
    dl->AddRect(cur, ImVec2(cur.x + size.x, cur.y + size.y),
        (IM_COL32(60, 70, 100, 255)), 6.0f, 0, 1.0f);

    ImVec4 base = npc ? g_cfg.guardCol : g_cfg.playerCol;
    ImU32 col = (IM_COL32((int)(base.x * 255), (int)(base.y * 255), (int)(base.z * 255), 255));
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

    if (showBox) {
        if (g_cfg.boxFill.load())
            dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), ((col & 0x00FFFFFFu) | (30u << 24)));
        esp::draw_box_style(dl, style, x0, y0, x1, y1, col);
    } else {
        dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), (IM_COL32(90, 100, 130, 120)), 0.0f, 0, 1.0f);
        ImVec2 tsz = ImGui::CalcTextSize("box off");
        dl->AddText(ImVec2(cx - tsz.x / 2, cy - tsz.y / 2), (IM_COL32(140, 148, 170, 255)), "box off");
    }

    float hx0 = 0, hx1 = 0;
    if (showHp && hs != 2) {
        hx0 = hpLeft ? x0 - 7 : x1 + 3;
        hx1 = hpLeft ? x0 - 3 : x1 + 7;
        float frac = 0.72f;
        dl->AddRectFilled(ImVec2(hx0, y0), ImVec2(hx1, y1), (IM_COL32(0, 0, 0, 170)));
        ImU32 hc = (IM_COL32(90, 255, 120, 255));
        dl->AddRectFilled(ImVec2(hx0, y1 - bh * frac), ImVec2(hx1, y1), hc);
    }
    if (showHp && hs != 1) {
        ImVec2 tsz = ImGui::CalcTextSize("72");
        dl->AddText(ImVec2(cx - tsz.x / 2, y1 + 4), (IM_COL32(255, 255, 255, 220)), "72");
    }

    float lx = 0, ly = 0, sx = 0, sy = 0;
    ImVec2 nsz(0, 0), ssz(0, 0);
    if (showName) {
        esp::anchor_pos(anchor, x0, y0, x1, y1, 13.0f * s, 0, lx, ly);
        nsz = ImGui::CalcTextSize(nm);
        dl->AddText(ImVec2(lx - nsz.x / 2, ly + nudge), (IM_COL32(235, 238, 248, 255)), nm);
    }
    if (showSub) {
        int line = showName ? 1 : 0;
        esp::anchor_pos(subAnchor, x0, y0, x1, y1, 13.0f * s, line, sx, sy);
        ssz = ImGui::CalcTextSize(sub);
        dl->AddText(ImVec2(sx - ssz.x / 2, sy + nudge), (IM_COL32(200, 204, 214, 255)), sub);
    }

    bool sk = npc ? g_cfg.guardSkel.load() : g_cfg.playerSkel.load();
    if (sk) {
        ImVec2 tsz = ImGui::CalcTextSize("skeleton: live bones in-game");
        dl->AddText(ImVec2(cx - tsz.x / 2, y1 + 22), (IM_COL32(140, 220, 255, 200)),
            "skeleton: live bones in-game");
    }

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

    ImGui::SetCursorScreenPos(ImVec2(cur.x, cur.y + size.y + 6));
    ImGui::TextDisabled("drag figure / name / info / health - name:%s sub:%s",
        kAN[anchor & 7], kAN[subAnchor & 7]);
}

static int visMode = 0;

void draw_esp() {
    ImGuiStyle& style = ImGui::GetStyle();
    float gw = ImTrunc((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) / 2.0f);
    if (widgets::os_subtab("PLAYERS", visMode == 0, gw)) visMode = 0;
    ImGui::SameLine(0, 8);
    if (widgets::os_subtab("GUARDS", visMode == 1, gw)) visMode = 1;

    if (visMode == 0) {
        if (widgets::os_section("Players", ImVec2(gw, 0))) {
            ImGui::SetWindowFontScale(dpi_scale);
            widgets::check_picker("Enable players", g_cfg.playerEsp, g_cfg.playerCol, "esp_p");
            static const char* elems[6] = { "Box", "Name", "Health", "Distance", "Skeleton", "Holding" };
            static std::atomic<bool>* pf[6] = { &g_cfg.playerBox, &g_cfg.playerName,
                &g_cfg.playerHealth, &g_cfg.playerDistance, &g_cfg.playerSkel,
                &g_cfg.playerHolding };
            widgets::multi_bridge("Elements", elems, pf, 6);
            widgets::check_bridge("Visibility check", g_cfg.visCheck);
            widgets::check_bridge("Hide dead", g_cfg.hideDead);
            widgets::check_bridge("Box fill", g_cfg.boxFill);
            widgets::check_bridge("Chams", g_cfg.playerChams);
            widgets::check_bridge("Glow", g_cfg.playerGlow);
            widgets::check_picker("Model chams", g_cfg.silChams, g_cfg.silHiddenP, "esp_sil");
            widgets::os_section_end();
        }
        ImGui::SameLine();
        if (widgets::os_section("Style", ImVec2(gw, 0))) {
            ImGui::SetWindowFontScale(dpi_scale);
            widgets::combo_bridge("Box type", kBoxTypes, 3, g_cfg.espBoxStyle);
            widgets::combo_bridge("Name anchor", kAnchors, 8, g_cfg.espLabelAnchor);
            widgets::combo_bridge("Info anchor", kAnchors, 8, g_cfg.espSubAnchor);
            widgets::combo_bridge("Name type", kNameStyles, 2, g_cfg.nameMode);
            widgets::combo_bridge("Healthbar style", kHpStyles, 3, g_cfg.healthStyle);
            widgets::sliderf_bridge("Box width", g_cfg.espBoxW, 0.25f, 0.8f, "%.2f");
            widgets::sliderf_bridge("Text scale", g_cfg.espTextScale, 0.7f, 1.8f, "%.2f");
            widgets::sliderf_bridge("Label height", g_cfg.espNameDy, -40.0f, 20.0f, "%.0f");
            widgets::check_bridge("Health left side", g_cfg.hpLeft);
            takeui::CenterSeparator("Preview");
            embedded_preview(false);
            widgets::os_section_end();
        }
    } else {
        if (widgets::os_section("Guards", ImVec2(gw, 0))) {
            ImGui::SetWindowFontScale(dpi_scale);
            widgets::check_picker("Enable guards", g_cfg.guardEsp, g_cfg.guardCol, "esp_g");
            static const char* gelems[5] = { "Box", "State", "Health", "Distance", "Skeleton" };
            static std::atomic<bool>* gf[5] = { &g_cfg.guardBox, &g_cfg.guardState,
                &g_cfg.guardHealth, &g_cfg.guardDistance, &g_cfg.guardSkel };
            widgets::multi_bridge("Elements", gelems, gf, 5);
            widgets::check_bridge("Chams", g_cfg.guardChams);
            widgets::check_bridge("Glow", g_cfg.guardGlow);
            widgets::check_picker("Model chams", g_cfg.silChamsG, g_cfg.silHiddenG, "esp_silg");
            widgets::os_section_end();
        }
        ImGui::SameLine();
        if (widgets::os_section("Colors", ImVec2(gw, 0))) {
            ImGui::SetWindowFontScale(dpi_scale);
            takeui::CenterSeparator("World colors");
            ImGui::TextDisabled("NPC");
            widgets::color_tail("Colour##esp_npc", g_cfg.npcCol);
            ImGui::TextDisabled("Dead");
            widgets::color_tail("Colour##esp_dead", g_cfg.deadCol);
            ImGui::TextDisabled("Health");
            widgets::color_tail("Colour##esp_hp", g_cfg.healthCol);
            ImGui::TextDisabled("Items");
            widgets::color_tail("Colour##esp_item", g_cfg.itemCol);
            ImGui::TextDisabled("Money");
            widgets::color_tail("Colour##esp_money", g_cfg.moneyCol);
            esp::SkelDbg d = esp::skel_debug();
            char b[96];
            snprintf(b, sizeof(b), "skeleton %d/%d drawn", d.drawn, d.checked);
            ImGui::TextDisabled("%s", b);
            takeui::CenterSeparator("Preview");
            embedded_preview(true);
            widgets::os_section_end();
        }
    }
}
struct Reg { Reg() { tabs::add({ "esp", "Visuals", draw_esp }); } };
static Reg g_reg;
}

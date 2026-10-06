#include "pch/pch.h"
#include "ui/tabs.hpp"
#include "ui/widgets.hpp"
#include "ui/theme.hpp"
#include "core/config.hpp"
#include "features/esp.hpp"
#include "features/manager.hpp"
#include "features/shop.hpp"

namespace {
void draw_home() {
    esp::Counts c = esp::counts();
    // status strip
    {
        float bw = ImGui::GetContentRegionAvail().x;
        ImVec2 cur = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(cur, ImVec2(cur.x + bw, cur.y + 52),
            IM_COL32(16, 18, 30, 255), 6.0f);
        char pbuf[32]; snprintf(pbuf, sizeof(pbuf), "%d", c.players + c.guards);
        char ibuf[32]; snprintf(ibuf, sizeof(ibuf), "%d", c.players);
        char gbuf[32]; snprintf(gbuf, sizeof(gbuf), "%d", c.guards);
        float x = cur.x + 14;
        auto stat = [&](const char* k, const char* v, ImU32 col) {
            ImVec2 vsz = ImGui::CalcTextSize(v);
            float cx = x + 40;
            dl->AddText(ImVec2(cx - vsz.x / 2, cur.y + 8), col, v);
            ImVec2 ksz = ImGui::CalcTextSize(k);
            dl->AddText(ImVec2(cx - ksz.x / 2, cur.y + 28), IM_COL32(150, 156, 175, 255), k);
            x += 90;
        };
        stat("PLAYERS", pbuf, IM_COL32(140, 220, 255, 255));
        stat("ROBBERS", ibuf, IM_COL32(90, 255, 140, 255));
        stat("GUARDS", gbuf, IM_COL32(255, 180, 90, 255));
        char fps[16]; snprintf(fps, sizeof(fps), "%d", (int)ImGui::GetIO().Framerate);
        stat("FPS", fps, IM_COL32(235, 238, 248, 255));
        ImGui::Dummy(ImVec2(bw, 52));
    }
    ImGui::Spacing();
    float bw = ImGui::GetContentRegionAvail().x;
    float half = (bw - 8) / 2;
    ImGui::BeginChild("##homeL", ImVec2(half, 0), true);
    ImGui::TextDisabled("QUICK TOGGLES");
    ImGui::Separator();
    widgets::check_row("Master ESP", g_cfg.espEnabled, nullptr);
    widgets::check_row("Players", g_cfg.playerEsp, &g_cfg.playerCol);
    widgets::check_row("Guards", g_cfg.guardEsp, &g_cfg.guardCol);
    widgets::check_row("Items", g_cfg.itemEsp, &g_cfg.itemCol);
    widgets::check_row("Godmode", g_cfg.godMode, nullptr);
    widgets::check_row("Watermark", g_cfg.watermark, nullptr);
    ImGui::EndChild();
    ImGui::SameLine(0, 8);
    ImGui::BeginChild("##homeR", ImVec2(0, 0), true);
    ImGui::TextDisabled("QUICK ACTIONS");
    ImGui::Separator();
    for (const feat::Entry* e : feat::by_tab("shop")) {
        if (widgets::action_button(e->name, e->desc, ImVec2(-1, 30)))
            feat::queue_run(e->id);
    }
    ImGui::Spacing();
    shop::Status st = shop::last_status();
    if (!st.note.empty())
        ImGui::TextWrapped("%s", st.note.c_str());
    else
        ImGui::TextDisabled("no action run yet");
    ImGui::EndChild();
}
struct Reg { Reg() { tabs::add({ "home", "Home", draw_home }); } };
static Reg g_reg;
} // namespace

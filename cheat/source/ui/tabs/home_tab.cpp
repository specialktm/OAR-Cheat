#include "pch/pch.h"
#include "ui/tabs.hpp"
#include "ui/widgets.hpp"
#include "ui/menu.hpp"
#include "config/config.hpp"
#include "steam/steam_user.hpp"
#include "features/esp.hpp"
#include "features/manager.hpp"
#include "features/shop.hpp"

namespace {
void draw_home() {
    widgets::header_card("Home", "Overview and quick toggles");
    esp::Counts c = esp::counts();

    widgets::os_section("Quick", ImVec2(400, 360));
    {
        ImGui::SetWindowFontScale(dpi_scale);
        widgets::check_bridge("Master ESP", g_cfg.espEnabled);
        widgets::check_bridge("Players", g_cfg.playerEsp);
        widgets::check_bridge("Guards", g_cfg.guardEsp);
        widgets::check_bridge("Items", g_cfg.itemEsp);
        widgets::check_bridge("Godmode", g_cfg.godMode);
        widgets::check_bridge("Watermark", g_cfg.watermark);
    }
    widgets::os_section_end();
    ImGui::SetCursorPos(ImVec2(420, 120));
    widgets::os_section("Status", ImVec2(400, 250));
    {
        ImGui::SetWindowFontScale(dpi_scale);
        ImVec2 cur = ImGui::GetCursorScreenPos();
        widgets::avatar_tile(cur, ImVec2(40, 40));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        std::string hnm = steamServiceHelper::name();
        {
            auto p = hnm.find("##");
            if (p != std::string::npos) hnm.erase(p);
        }
        const char* sid = steamServiceHelper::id_text();
        dl->AddText(ImVec2(cur.x + 48, cur.y + 2),
            IM_COL32(235, 238, 248, 255), hnm.c_str());
        dl->AddText(ImVec2(cur.x + 48, cur.y + 20),
            IM_COL32(140, 148, 170, 255), sid[0] ? sid : "steam id pending");
        ImGui::Dummy(ImVec2(0, 44));
        char b[64];
        snprintf(b, sizeof(b), "Players %d  Guards %d", c.players, c.guards);
        ImGui::TextDisabled("%s", b);
        snprintf(b, sizeof(b), "Items %d", c.items);
        ImGui::TextDisabled("%s", b);
        snprintf(b, sizeof(b), "%d FPS", (int)ImGui::GetIO().Framerate);
        ImGui::TextDisabled("%s", b);
    }
    widgets::os_section_end();
    ImGui::SetCursorPos(ImVec2(420, 390));
    widgets::os_section("Actions", ImVec2(400, 390));
    {
        ImGui::SetWindowFontScale(dpi_scale);
        for (const feat::Entry* e : feat::by_tab("shop")) {
            if (ImGui::Button(e->name, ImVec2(-1, 30)))
                feat::queue_run(e->id);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", e->desc);
        }
        shop::Status st = shop::last_status();
        if (!st.note.empty())
            ImGui::TextWrapped("%s", st.note.c_str());
        else
            ImGui::TextDisabled("no action run yet");
    }
    widgets::os_section_end();
}
struct Reg { Reg() { tabs::add({ "home", "Home", draw_home }); } };
static Reg g_reg;
}

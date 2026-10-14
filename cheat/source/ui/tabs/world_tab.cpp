#include "pch/pch.h"
#include "ui/tabs.hpp"
#include "ui/widgets.hpp"
#include "ui/menu.hpp"
#include "ui/take_shell.hpp"
#include "config/config.hpp"
#include "features/manager.hpp"
#include "features/takebind.hpp"
#include "features/exploits.hpp"

namespace {
void draw_world() {
    ImGuiStyle& style = ImGui::GetStyle();
    float gw = ImTrunc((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) / 2.0f);
    if (widgets::os_section("Actions", ImVec2(gw, 0))) {
        ImGui::SetWindowFontScale(dpi_scale);
        takeui::CenterSeparator("Level actions");
        for (const feat::Entry* e : feat::by_tab("world")) {
            widgets::action_button(e->id, e->name, e->desc, ImVec2(-1, 32));
        }
        ImGui::TextDisabled("last: %s", exploits::last_result());
        widgets::os_section_end();
    }
    ImGui::SameLine();
    if (widgets::os_section("Cheats", ImVec2(gw, 0))) {
        ImGui::SetWindowFontScale(dpi_scale);
        takeui::CenterSeparator("Engine tweaks");
        if (widgets::sliderf_bridge("Slomo", g_cfg.timeDilation, 0.1f, 3.0f, "%.2f"))
            exploits::set_slomo(g_cfg.timeDilation.load());
        if (widgets::sliderf_bridge("FOV", g_cfg.fov, 40.0f, 150.0f, "%.0f"))
            exploits::set_fov(g_cfg.fov.load());
        static char summonBuf[64] = { 0 };
        ImGui::TextDisabled("Summon class");
        ImGui::InputText("##summon", summonBuf, sizeof(summonBuf));
        if (ImAdd::Button("SUMMON", ImVec2(-1, 32)))
            exploits::summon_item(summonBuf);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("short class name, e.g. Money_base_C");
        ImGui::TextDisabled("cheats on first, ghost noclip needs rejoin to undo");
        widgets::os_section_end();
    }
}
struct Reg { Reg() { tabs::add({ "world", "World", draw_world }); } };
static Reg g_reg;
}

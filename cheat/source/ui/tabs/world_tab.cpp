#include "pch/pch.h"
#include "ui/tabs.hpp"
#include "ui/widgets.hpp"
#include "ui/menu.hpp"
#include "config/config.hpp"
#include "features/manager.hpp"
#include "features/exploits.hpp"

namespace {
void draw_world() {
    widgets::header_card("World", "Phones, cameras and doors");
    widgets::os_section("Actions", ImVec2(800, 440));
    {
        ImGui::SetWindowFontScale(dpi_scale);
        for (const feat::Entry* e : feat::by_tab("world")) {
            char btn[128];
            auto* k = feat::key_for(e->id);
            int vk = k ? k->load() : 0;
            if (vk)
                snprintf(btn, sizeof(btn), "%s  [%s]", e->name, widgets::key_name(vk));
            else
                snprintf(btn, sizeof(btn), "%s", e->name);
            ImGui::PushID(e->id);
            float bw = ImGui::GetContentRegionAvail().x;
            if (e->autoFlag) {
                if (ImGui::Button(btn, ImVec2(bw - 138.0f, 32)))
                    feat::queue_run(e->id);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", e->desc);
                ImGui::SameLine(0, 8);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);
                widgets::check_bridge("Auto", *e->autoFlag);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("auto-repeat this action");
            } else {
                if (ImGui::Button(btn, ImVec2(-1, 32)))
                    feat::queue_run(e->id);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", e->desc);
            }
            ImGui::PopID();
        }
        widgets::sliderf_bridge("Interval (sec)", g_cfg.autoIntervalSec, 1.0f, 30.0f);
        ImGui::TextDisabled("last: %s", exploits::last_result());
    }
    widgets::os_section_end();
}
struct Reg { Reg() { tabs::add({ "world", "World", draw_world }); } };
static Reg g_reg;
}

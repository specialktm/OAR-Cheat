#include "pch/pch.h"
#include "ui/tabs.hpp"
#include "ui/widgets.hpp"
#include "core/config.hpp"
#include "features/manager.hpp"
#include "features/exploits.hpp"

namespace {
void draw_world() {
    widgets::section("WORLD");
    float bw = ImGui::GetContentRegionAvail().x;
    for (const feat::Entry* e : feat::by_tab("world")) {
        char sub[160];
        auto* k = feat::key_for(e->id);
        int vk = k ? k->load() : 0;
        snprintf(sub, sizeof(sub), "%s%s%s", e->desc,
            vk ? "  -  " : "", vk ? widgets::key_name(vk) : "");
        if (widgets::action_button(e->name, sub, ImVec2(bw, 32)))
            feat::queue_run(e->id);
        if (e->autoFlag) {
            ImGui::PushID(e->id);
            bool b = e->autoFlag->load();
            char lbl[64];
            snprintf(lbl, sizeof(lbl), "Auto-repeat %s", e->name);
            if (ImGui::Checkbox(lbl, &b)) e->autoFlag->store(b);
            ImGui::PopID();
        }
        ImGui::Spacing();
    }
    widgets::section("AUTO REPEAT");
    widgets::slider_f("Interval (sec)", g_cfg.autoIntervalSec, 1.0f, 30.0f);
    ImGui::Spacing();
    ImGui::TextDisabled("last: %s", exploits::last_result());
}
struct Reg { Reg() { tabs::add({ "world", "World", draw_world }); } };
static Reg g_reg;
} // namespace

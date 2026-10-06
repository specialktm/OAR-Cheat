#include "pch/pch.h"
#include "ui/tabs.hpp"
#include "ui/widgets.hpp"
#include "features/manager.hpp"
#include "features/shop.hpp"
#include "features/crates.hpp"

namespace {
void draw_shop() {
    widgets::section("SHOP");
    float bw = ImGui::GetContentRegionAvail().x;
    for (const feat::Entry* e : feat::by_tab("shop")) {
        char sub[160];
        auto* k = feat::key_for(e->id);
        int vk = k ? k->load() : 0;
        snprintf(sub, sizeof(sub), "%s%s%s", e->desc,
            vk ? "  -  " : "", vk ? widgets::key_name(vk) : "");
        if (widgets::action_button(e->name, sub, ImVec2(bw, 34)))
            feat::queue_run(e->id);
        ImGui::Spacing();
    }
    widgets::section("LAST RESULT");
    shop::Status st = shop::last_status();
    if (!st.hasController && st.note.empty()) {
        ImGui::TextColored(ImVec4(0.58f, 0.62f, 0.72f, 1.0f), "No action run yet this session.");
    } else {
        widgets::stat_row("Controller", st.hasController ? 1 : 0,
            st.hasController ? IM_COL32(90, 255, 140, 255) : IM_COL32(255, 110, 110, 255));
        widgets::stat_row("Map classes", st.mapClasses, IM_COL32(140, 200, 255, 255));
        widgets::stat_row("Maps granted", st.mapsGranted, IM_COL32(90, 255, 140, 255));
        widgets::stat_row("Shop classes", st.itemClasses, IM_COL32(140, 200, 255, 255));
        widgets::stat_row("CDOs patched", st.itemsPatched, IM_COL32(255, 210, 120, 255));
        widgets::stat_row("Items granted", st.itemsGranted, IM_COL32(90, 255, 140, 255));
        widgets::stat_row("Items verified", st.itemsVerified, IM_COL32(90, 255, 140, 255));
        widgets::stat_row("Maps verified", st.mapsVerified, IM_COL32(90, 255, 140, 255));
        widgets::stat_row("Steam defs", st.steamDefs, IM_COL32(140, 200, 255, 255));
        widgets::stat_row("Steam granted", st.steamGranted,
            st.steamOk ? IM_COL32(90, 255, 140, 255) : IM_COL32(255, 210, 120, 255));
        if (st.coinsBefore >= 0)
            widgets::stat_row("Coins", st.coinsAfter, IM_COL32(255, 210, 120, 255));
        if (!st.note.empty())
            ImGui::TextColored(st.mapsGranted + st.itemsGranted > 0
                    ? ImVec4(0.35f, 0.95f, 0.55f, 1.0f)
                    : ImVec4(1.0f, 0.72f, 0.30f, 1.0f),
                "%s", st.note.c_str());
    }
    ImGui::Spacing();
    ImGui::TextDisabled("Crates: %s", crates::last_line());
}
struct Reg { Reg() { tabs::add({ "shop", "Shop", draw_shop }); } };
static Reg g_reg;
} // namespace

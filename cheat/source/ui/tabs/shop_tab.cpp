#include "pch/pch.h"
#include "ui/tabs.hpp"
#include "ui/widgets.hpp"
#include "ui/menu.hpp"
#include "ui/take_shell.hpp"
#include "features/manager.hpp"
#include "features/shop.hpp"
#include "features/crates.hpp"
#include "features/skins.hpp"

namespace {
void draw_shop() {
    ImGuiStyle& style = ImGui::GetStyle();
    float gw = ImTrunc((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) / 2.0f);
    if (widgets::os_section("Unlocks", ImVec2(gw, 0))) {
        ImGui::SetWindowFontScale(dpi_scale);
        takeui::CenterSeparator("One-click unlocks");
        for (const feat::Entry* e : feat::by_tab("shop")) {
            widgets::action_button(e->id, e->name, e->desc, ImVec2(-1, 32));
        }
        shop::Status st = shop::last_status();
        if (!st.hasController && st.note.empty()) {
            ImGui::TextDisabled("No action run yet this session.");
        } else {
            char b[96];
            snprintf(b, sizeof(b), "Maps %d/%d  Items %d/%d",
                st.mapsGranted, st.mapClasses, st.itemsGranted, st.itemClasses);
            ImGui::TextDisabled("%s", b);
            snprintf(b, sizeof(b), "Steam %d/%d", st.steamGranted, st.steamDefs);
            ImGui::TextDisabled("%s", b);
            if (st.coinsBefore >= 0) {
                snprintf(b, sizeof(b), "Coins %d", st.coinsAfter);
                ImGui::TextDisabled("%s", b);
            }
            if (!st.note.empty())
                ImGui::TextWrapped("%s", st.note.c_str());
        }
        widgets::os_section_end();
    }
    ImGui::SameLine();
    if (widgets::os_section("Skins", ImVec2(gw, 0))) {
        ImGui::SetWindowFontScale(dpi_scale);
        takeui::CenterSeparator("Steam skins + crates");
        if (ImAdd::Button("SCAN SKINS", ImVec2(-1, 32)))
            feat::queue_fn([] { skins::scan(); });
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("enumerate the skin table");
        if (ImAdd::Button("UNLOCK ALL SKINS", ImVec2(-1, 32)))
            feat::queue_keyed("unlock_all", [] { skins::unlock_all(); });
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("steam inventory + equip all guns");
        if (ImAdd::Button("ALL SKINS+CHARMS", ImVec2(-1, 32)))
            feat::queue_keyed("unlock_everything", [] { skins::unlock_everything(); });
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("every skin + charm def to steam");
        if (ImAdd::Button("GRANT ALL CRATES", ImVec2(-1, 32)))
            feat::queue_keyed("grant_crates", [] { crates::grant_all(); });
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("mission-style crate grant");
        if (ImAdd::Button("MISSION CRATE DROP", ImVec2(-1, 32)))
            feat::queue_keyed("mission_drop", [] { crates::drop_all(); });
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("win-screen drop call per crate");
        if (ImAdd::Button("READ LAST DROP", ImVec2(-1, 32)))
            feat::queue_keyed("sniff_drop", [] { crates::sniff_drop(); });
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("read win-screen drop def");
        if (ImAdd::Button("PROMO CRATES", ImVec2(-1, 32)))
            feat::queue_keyed("promo_crates", [] { crates::promo_all(); });
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("claim promo-eligible crates");
        if (ImAdd::Button("SCAN LIST NAMES", ImVec2(-1, 32)))
            feat::queue_keyed("scan_lists", [] { crates::scan_lists(true); });
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("dump all steam def names");
        if (ImAdd::Button("GRANT COIN PACKS", ImVec2(-1, 32)))
            feat::queue_keyed("grant_coins", [] { shop::coins_legit(); });
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("native coin pack grant");
        const auto& sk = skins::list();
        static std::vector<std::string> disp;
        static std::vector<const char*> ptrs;
        disp.clear();
        for (auto& e : sk) {
            std::string d = e.name;
            auto p = d.find("##");
            if (p != std::string::npos) d.erase(p);
            if (d.empty()) d = "Skin";
            disp.push_back(widgets::shorten(d, 44));
        }
        ptrs.clear();
        for (auto& d : disp) ptrs.push_back(d.c_str());
        if (!ptrs.empty()) {
            int cur = skins::selected();
            if (cur < 0 || cur >= (int)ptrs.size()) cur = 0;
            ImGui::TextDisabled("Skin");
            if (ImGui::Combo("##skinpick", &cur, ptrs.data(), (int)ptrs.size(), 9))
                skins::setSelected(cur);
            int row = skins::list()[cur].id;
            if (ImAdd::Button("APPLY SKIN", ImVec2(-1, 32)))
                feat::queue_keyed("apply_skin", [] { skins::apply_selected(); });
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("skin + charm onto all guns");
            if (ImAdd::Button("OPEN CRATE FOR SKIN", ImVec2(-1, 32)))
                feat::queue_keyed("open_crate_skin", [row] { crates::open_for_skin(row); });
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("real exchange open + equip");
        } else {
            ImGui::TextDisabled("no skins scanned yet");
        }
        ImGui::TextDisabled("%s", skins::status());
        ImGui::TextDisabled("Crates: %s", crates::last_line());
        widgets::os_section_end();
    }
}
struct Reg { Reg() { tabs::add({ "shop", "Shop", draw_shop }); } };
static Reg g_reg;
}

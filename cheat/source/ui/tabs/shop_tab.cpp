#include "pch/pch.h"
#include "ui/tabs.hpp"
#include "ui/widgets.hpp"
#include "ui/menu.hpp"
#include "features/manager.hpp"
#include "features/shop.hpp"
#include "features/crates.hpp"
#include "features/skins.hpp"

namespace {
void draw_shop() {
    widgets::header_card("Shop", "Unlocks, crates and skins");
    widgets::os_section("Unlocks", ImVec2(400, 480));
    {
        ImGui::SetWindowFontScale(dpi_scale);
        for (const feat::Entry* e : feat::by_tab("shop")) {
            if (ImGui::Button(e->name, ImVec2(-1, 32)))
                feat::queue_run(e->id);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", e->desc);
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
    }
    widgets::os_section_end();
    ImGui::SetCursorPos(ImVec2(420, 120));
    widgets::os_section("Skins", ImVec2(400, 620));
    {
        ImGui::SetWindowFontScale(dpi_scale);
        if (ImGui::Button("SCAN SKINS", ImVec2(-1, 32)))
            feat::queue_fn([] { skins::scan(); });
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("enumerate the skin table");
        if (ImGui::Button("UNLOCK ALL SKINS", ImVec2(-1, 32)))
            feat::queue_keyed("unlock_all", [] { skins::unlock_all(); });
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("steam inventory + equip all guns");
        if (ImGui::Button("ALL SKINS+CHARMS", ImVec2(-1, 32)))
            feat::queue_keyed("unlock_everything", [] { skins::unlock_everything(); });
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("every skin + charm def to steam");
        if (ImGui::Button("GRANT ALL CRATES", ImVec2(-1, 32)))
            feat::queue_keyed("grant_crates", [] { crates::grant_all(); });
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("mission-style crate grant");
        if (ImGui::Button("MISSION CRATE DROP", ImVec2(-1, 32)))
            feat::queue_keyed("mission_drop", [] { crates::drop_all(); });
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("win-screen drop call per crate");
        if (ImGui::Button("READ LAST DROP", ImVec2(-1, 32)))
            feat::queue_keyed("sniff_drop", [] { crates::sniff_drop(); });
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("read win-screen drop def");
        if (ImGui::Button("PROMO CRATES", ImVec2(-1, 32)))
            feat::queue_keyed("promo_crates", [] { crates::promo_all(); });
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("claim promo-eligible crates");
        if (ImGui::Button("SCAN LIST NAMES", ImVec2(-1, 32)))
            feat::queue_keyed("scan_lists", [] { crates::scan_lists(true); });
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("dump all steam def names");
        if (ImGui::Button("GRANT COIN PACKS", ImVec2(-1, 32)))
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
            disp.push_back(d);
        }
        ptrs.clear();
        for (auto& d : disp) ptrs.push_back(d.c_str());
        if (!ptrs.empty()) {
            int cur = skins::selected();
            if (cur < 0 || cur >= (int)ptrs.size()) cur = 0;
            if (ImGui::Combo("Skin", &cur, ptrs.data(), (int)ptrs.size(), 9))
                skins::setSelected(cur);
            int row = skins::list()[cur].id;
            if (ImGui::Button("APPLY SKIN", ImVec2(-1, 32)))
                feat::queue_keyed("apply_skin", [] { skins::apply_selected(); });
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("skin + charm onto all guns");
            if (ImGui::Button("OPEN CRATE FOR SKIN", ImVec2(-1, 32)))
                feat::queue_keyed("open_crate_skin", [row] { crates::open_for_skin(row); });
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("real exchange open + equip");
        } else {
            ImGui::TextDisabled("no skins scanned yet");
        }
        ImGui::TextDisabled("%s", skins::status());
        ImGui::TextDisabled("Crates: %s", crates::last_line());
    }
    widgets::os_section_end();
}
struct Reg { Reg() { tabs::add({ "shop", "Shop", draw_shop }); } };
static Reg g_reg;
}

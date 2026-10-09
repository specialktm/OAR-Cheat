#include "pch/pch.h"
#include "ui/tabs.hpp"
#include "ui/widgets.hpp"
#include "ui/menu.hpp"
#include "config/config.hpp"
#include "steam/steam_user.hpp"
#include "features/entities.hpp"
#include "features/manager.hpp"
#include "features/exploits.hpp"

namespace {

void draw_lobby() {
    widgets::header_card("Lobby", "Players, actions and loot");

    widgets::os_section("Robbers", ImVec2(400, 480));
    {
        ImGui::SetWindowFontScale(dpi_scale);
        const auto& ps = esp::playerManager::players();
        int sel = esp::playerManager::selected();
        if (sel >= (int)ps.size()) { sel = -1; esp::playerManager::setSelected(-1); }
        if (ps.empty()) {
            ImGui::TextDisabled("no robbers in level");
        } else {
            int n = (int)ps.size() > 10 ? 10 : (int)ps.size();
            for (int i = 0; i < n; ++i) {
                const esp::PlayerInfo& p = ps[i];
                char row[128];
                const char* st = (p.downed || p.hp <= 0) ? "DOWN" : "OK";
                if (p.distM >= 0.0f)
                    snprintf(row, sizeof(row), "%s  %d hp  %.0fm  %s",
                        p.name.c_str(), p.hp, p.distM, st);
                else
                    snprintf(row, sizeof(row), "%s  %d hp  %s",
                        p.name.c_str(), p.hp, st);
                if (ImGui::Selectable(row, sel == i))
                    esp::playerManager::setSelected(i);
                if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }
            if ((int)ps.size() > 10) {
                char more[32];
                snprintf(more, sizeof(more), "+%d more", (int)ps.size() - 10);
                ImGui::TextDisabled("%s", more);
            }
        }
        const esp::PlayerInfo* sp = esp::playerManager::selectedPlayer();
        bool canAct = sp && !sp->self && IS_VALID_PTR(sp->actor);
        if (!canAct) ImGui::BeginDisabled();
        if (ImGui::Button("BRING", ImVec2(-1, 30)) && canAct) {
            void* a = sp->actor;
            feat::queue_fn([a] { exploits::bring_player(a); });
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("teleport them to you");
        if (ImGui::Button("GO TO", ImVec2(-1, 30)) && canAct) {
            void* a = sp->actor;
            feat::queue_fn([a] { exploits::goto_player(a); });
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("teleport to them");
        if (!canAct) ImGui::EndDisabled();
        if (!sp) ImGui::TextDisabled("select a robber first");
        else if (sp->self) ImGui::TextDisabled("that's you - pick someone else");
    }
    widgets::os_section_end();
    ImGui::SetCursorPos(ImVec2(420, 120));
    widgets::os_section("Steam", ImVec2(400, 140));
    {
        ImGui::SetWindowFontScale(dpi_scale);
        ImVec2 cur = ImGui::GetCursorScreenPos();
        widgets::avatar_tile(cur, ImVec2(40, 40));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        std::string lnm = steamServiceHelper::name();
        {
            auto p = lnm.find("##");
            if (p != std::string::npos) lnm.erase(p);
        }
        dl->AddText(ImVec2(cur.x + 48, cur.y + 4),
            IM_COL32(255, 255, 255, 255), lnm.c_str());
        const char* sid = steamServiceHelper::id_text();
        dl->AddText(ImVec2(cur.x + 48, cur.y + 22),
            IM_COL32(140, 148, 170, 255), sid[0] ? sid : "steam id pending");
        ImGui::Dummy(ImVec2(0, 44));
    }
    widgets::os_section_end();
    ImGui::SetCursorPos(ImVec2(420, 280));
    widgets::os_section("Loot", ImVec2(400, 340));
    {
        ImGui::SetWindowFontScale(dpi_scale);
        const auto& items = esp::itemManager::items();
        int shown = 0;
        for (size_t i = 0; i < items.size() && shown < 10; ++i) {
            const esp::LootInfo& it = items[i];
            char row[160];
            if (it.distM >= 0.0f)
                snprintf(row, sizeof(row), "%s  %.0fm", it.label.c_str(), it.distM);
            else
                snprintf(row, sizeof(row), "%s", it.label.c_str());
            ImGui::BulletText("%s", row);
            ++shown;
        }
        if (items.size() > 10) {
            char more[32];
            snprintf(more, sizeof(more), "+%d more", (int)items.size() - 10);
            ImGui::TextDisabled("%s", more);
        }
        if (items.empty()) ImGui::TextDisabled("no loot in level");
    }
    widgets::os_section_end();
}
struct Reg { Reg() { tabs::add({ "lobby", "Lobby", draw_lobby }); } };
static Reg g_reg;
}

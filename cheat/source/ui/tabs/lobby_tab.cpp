#include "pch/pch.h"
#include "ui/tabs.hpp"
#include "ui/widgets.hpp"
#include "ui/menu.hpp"
#include "ui/take_shell.hpp"
#include "config/config.hpp"
#include "steam/steam_user.hpp"
#include "features/entities.hpp"
#include "features/manager.hpp"
#include "features/exploits.hpp"

namespace {

static int listMode = 0;

static const char* hp_text(const esp::PlayerInfo& p, char* b, size_t n) {
    if (p.maxHp > 0 && p.hp > p.maxHp * 10)
        snprintf(b, n, "GOD");
    else if (p.maxHp > 0)
        snprintf(b, n, "%d/%d hp", p.hp, p.maxHp);
    else
        snprintf(b, n, "%d hp", p.hp);
    return b;
}

void draw_lobby() {
    ImGuiStyle& style = ImGui::GetStyle();
    float gw = ImTrunc((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) / 2.0f);
    if (widgets::os_section("Lobby", ImVec2(gw, 0))) {
        ImGui::SetWindowFontScale(dpi_scale);
        ImVec2 cur = ImGui::GetCursorScreenPos();
        widgets::avatar_tile(cur, ImVec2(44, 44));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        std::string lnm = steamServiceHelper::name();
        {
            auto p = lnm.find("##");
            if (p != std::string::npos) lnm.erase(p);
        }
        std::string disp = widgets::shorten(lnm.empty() ? "player" : lnm, 22);
        dl->AddText(ImVec2(cur.x + 52, cur.y + 4),
            IM_COL32(255, 255, 255, 255), disp.c_str());
        const char* sid = steamServiceHelper::id_text();
        dl->AddText(ImVec2(cur.x + 52, cur.y + 22),
            IM_COL32(140, 148, 170, 255), sid[0] ? sid : "steam id pending");
        ImGui::Dummy(ImVec2(0, 48));
        takeui::CenterSeparator("Account");
        ImGui::TextDisabled("Steam ID");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(sid[0] ? sid : "-").x);
        ImGui::TextUnformatted(sid[0] ? sid : "-");
        {
            const auto& ps = esp::playerManager::players();
            const auto& gs = esp::playerManager::npcs();
            char cb[48];
            snprintf(cb, sizeof(cb), "%d robbers  %d npcs", (int)ps.size(), (int)gs.size());
            ImGui::TextDisabled("In level");
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(cb).x);
            ImGui::TextUnformatted(cb);
        }
        takeui::CenterSeparator("Players");
        if (widgets::os_subtab("ROBBERS", listMode == 0, gw)) listMode = 0;
        ImGui::SameLine(0, 8);
        if (widgets::os_subtab("GUARDS", listMode == 1, gw)) listMode = 1;
        if (listMode == 0) {
            takeui::CenterSeparator("Robbers");
            const auto& ps = esp::playerManager::players();
            int sel = esp::playerManager::selected();
            if (sel >= (int)ps.size()) { sel = -1; esp::playerManager::setSelected(-1); }
            if (ps.empty()) {
                ImGui::TextDisabled("no robbers in level");
            } else {
                for (int i = 0; i < (int)ps.size(); ++i) {
                    const esp::PlayerInfo& p = ps[i];
                    char hb[32];
                    char row[192];
                    std::string nm = widgets::shorten(p.name, 20);
                    const char* st = (p.downed || p.hp <= 0) ? "DOWN" : "OK";
                    if (p.self) st = "YOU";
                    if (p.distM >= 0.0f)
                        snprintf(row, sizeof(row), "%s  %s  %.0fm  %s",
                            nm.c_str(), hp_text(p, hb, sizeof(hb)), p.distM, st);
                    else
                        snprintf(row, sizeof(row), "%s  %s  %s",
                            nm.c_str(), hp_text(p, hb, sizeof(hb)), st);
                    if (ImGui::Selectable(row, sel == i))
                        esp::playerManager::setSelected(i);
                    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }
            }
            const esp::PlayerInfo* sp = esp::playerManager::selectedPlayer();
            if (!sp) ImGui::TextDisabled("select a robber for remote actions");
            else if (sp->self) ImGui::TextDisabled("that's you - pick someone else");
            else ImGui::TextDisabled("target: %s", widgets::shorten(sp->name, 26).c_str());
        } else {
            takeui::CenterSeparator("Guards + NPCs");
            const auto& gs = esp::playerManager::npcs();
            bool hideDead = g_cfg.hideDead.load();
            int shown = 0;
            for (size_t i = 0; i < gs.size(); ++i) {
                const esp::GuardInfo& g = gs[i];
                if (!IS_VALID_PTR(g.actor)) continue;
                if (g.dead && hideDead) continue;
                char row[192];
                const char* st = g.dead ? "DEAD" : (g.alerted ? "ALERTED" : "CALM");
                const char* arm = g.armed ? "ARMED" : "unarmed";
                const char* lb = g.label.empty() ? "NPC" : g.label.c_str();
                if (g.distM >= 0.0f)
                    snprintf(row, sizeof(row), "%s  %s  %s  %.0fm", lb, st, arm, g.distM);
                else
                    snprintf(row, sizeof(row), "%s  %s  %s", lb, st, arm);
                ImGui::BulletText("%s", row);
                ++shown;
            }
            if (!shown) ImGui::TextDisabled("no guards in level");
            char cnt[64];
            snprintf(cnt, sizeof(cnt), "robbers %d  npcs %d", (int)esp::playerManager::players().size(), (int)gs.size());
            ImGui::TextDisabled("%s", cnt);
        }
        widgets::os_section_end();
    }
    ImGui::SameLine();
    if (widgets::os_section("Remote", ImVec2(gw, 0))) {
        ImGui::SetWindowFontScale(dpi_scale);
        takeui::CenterSeparator("Teleport");
        const esp::PlayerInfo* sp = esp::playerManager::selectedPlayer();
        bool canAct = sp && !sp->self && IS_VALID_PTR(sp->actor);
        if (!canAct) ImGui::BeginDisabled();
        if (ImAdd::Button("BRING TO ME", ImVec2(-1, 30)) && canAct) {
            void* a = sp->actor;
            feat::queue_fn([a] { exploits::bring_player(a); });
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("teleport them to you");
        if (ImAdd::Button("GO TO THEM", ImVec2(-1, 30)) && canAct) {
            void* a = sp->actor;
            feat::queue_fn([a] { exploits::goto_player(a); });
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("teleport to them");
        if (!canAct) ImGui::EndDisabled();
        if (!sp) ImGui::TextDisabled("select a robber first");
        else if (sp->self) ImGui::TextDisabled("that's you - pick someone else");
        else ImGui::TextDisabled("target: %s", widgets::shorten(sp->name, 26).c_str());
        takeui::CenterSeparator("Session");
        if (!canAct) ImGui::BeginDisabled();
        if (ImAdd::Button("KICK PLAYER", ImVec2(-1, 30)) && canAct) {
            feat::queue_fn([] { exploits::kick_selected(); });
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("kick selected player (host only)");
        if (!canAct) ImGui::EndDisabled();
        takeui::CenterSeparator("Identity");
        static char spoofBuf[33] = { 0 };
        ImGui::TextDisabled("Spoof name");
        ImGui::InputText("##spoof", spoofBuf, sizeof(spoofBuf));
        float bw = ImGui::GetContentRegionAvail().x;
        float hb = (bw - 4) / 2;
        if (ImAdd::Button("APPLY", ImVec2(hb, 28)))
            exploits::set_spoof(spoofBuf);
        ImGui::SameLine(0, 4);
        if (ImAdd::Button("RESTORE", ImVec2(hb, 28)))
            exploits::clear_spoof();
        ImGui::TextDisabled("%s", exploits::spoof_status());
        ImGui::TextDisabled("local effect only");
        takeui::CenterSeparator("Protections");
        widgets::check_bridge("Godmode", g_cfg.godMode);
        if (ImAdd::Button("IMMUNITY", ImVec2(-1, 30)))
            feat::queue_fn([] { exploits::immunity(); });
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("9999s immunity level");
        takeui::CenterSeparator("Nearby loot");
        const auto& items = esp::itemManager::items();
        int shown = 0;
        for (size_t i = 0; i < items.size() && shown < 10; ++i) {
            const esp::LootInfo& it = items[i];
            char row[160];
            std::string lb = widgets::shorten(it.label, 30);
            if (it.distM >= 0.0f)
                snprintf(row, sizeof(row), "%s  %.0fm", lb.c_str(), it.distM);
            else
                snprintf(row, sizeof(row), "%s", lb.c_str());
            ImGui::BulletText("%s", row);
            ++shown;
        }
        if (items.size() > 10) {
            char more[32];
            snprintf(more, sizeof(more), "+%d more", (int)items.size() - 10);
            ImGui::TextDisabled("%s", more);
        }
        if (items.empty()) ImGui::TextDisabled("no loot in level");
        widgets::os_section_end();
    }
}
struct Reg { Reg() { tabs::add({ "lobby", "Lobby", draw_lobby }); } };
static Reg g_reg;
}

#include "pch/pch.h"
#include "ui/tabs.hpp"
#include "ui/widgets.hpp"
#include "ui/menu.hpp"
#include "config/config.hpp"
#include "config/config_io.hpp"
#include "world/diagnostics.hpp"
#include "core/memory.hpp"
#include "render/blur.hpp"
#include "steam/steam_user.hpp"
#include "features/takebind.hpp"
#include "ui/take_shell.hpp"

namespace {
static void diag_line(const char* k, const char* v, bool ok) {
    ImGui::TextDisabled("%s", k);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(v).x);
    if (ok)
        ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.55f, 1.0f), "%s", v);
    else
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", v);
}

void draw_settings() {
    ImGuiStyle& style = ImGui::GetStyle();
    float gw = ImTrunc((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) / 2.0f);
    if (widgets::os_section("Config", ImVec2(gw, 0))) {
        ImGui::SetWindowFontScale(dpi_scale);
        takeui::CenterSeparator("Profiles");
        static char nameBuf[64] = { 0 };
        static bool nameInit = false;
        if (!nameInit) {
            nameInit = true;
            snprintf(nameBuf, sizeof(nameBuf), "%s", g_cfg.configName.c_str());
        }
        static std::vector<std::string> names;
        static std::vector<const char*> ptrs;
        names = cfgio::list();
        ptrs.clear();
        for (auto& n : names) ptrs.push_back(n.c_str());
        if (!ptrs.empty()) {
            int cur = 0;
            for (int i = 0; i < (int)names.size(); ++i)
                if (names[i] == nameBuf) { cur = i; break; }
            ImGui::TextDisabled("Saved configs");
            if (ImGui::Combo("##savedcfgs", &cur, ptrs.data(), (int)ptrs.size(), 9)) {
                if (cur >= 0 && cur < (int)names.size()) {
                    snprintf(nameBuf, sizeof(nameBuf), "%s", names[cur].c_str());
                    g_cfg.configName = nameBuf;
                    std::string err;
                    if (cfgio::load(g_cfg.configName, err)) {
                        g_cfg.configStatus = "loaded " + g_cfg.configName;
                    } else g_cfg.configStatus = err;
                }
            }
        } else {
            ImGui::TextDisabled("no saved configs yet");
        }
        ImGui::TextDisabled("Name");
        ImGui::InputText("##cfgname", nameBuf, sizeof(nameBuf));
        float bw = ImGui::GetContentRegionAvail().x;
        float hb = (bw - 12) / 3;
        if (ImAdd::Button("SAVE", ImVec2(hb, 30))) {
            g_cfg.configName = nameBuf;
            std::string err;
            if (cfgio::save(g_cfg.configName, err))
                g_cfg.configStatus = "saved " + g_cfg.configName;
            else
                g_cfg.configStatus = err;
        }
        ImGui::SameLine(0, -1);
        if (ImAdd::Button("LOAD", ImVec2(hb, 30))) {
            g_cfg.configName = nameBuf;
            std::string err;
            if (cfgio::load(g_cfg.configName, err)) {
                snprintf(nameBuf, sizeof(nameBuf), "%s", g_cfg.configName.c_str());
                g_cfg.configStatus = "loaded " + g_cfg.configName;
            } else g_cfg.configStatus = err;
        }
        ImGui::SameLine(0, -1);
        if (ImAdd::Button("UNLOAD", ImVec2(hb, 30)))
            programRunning.store(false);
        if (!g_cfg.configStatus.empty())
            ImGui::TextDisabled("%s", g_cfg.configStatus.c_str());
        ImGui::TextWrapped("file: %s", cfgio::path_for(g_cfg.configName).c_str());
        takeui::CenterSeparator("Hotkeys");
        ImGui::TextDisabled("click key to bind, right-click mode");
        widgets::key_row("Menu", "kb_menu", g_cfg.keyMenu, g_cfg.keyModeMenu);
        static std::atomic<int> unloadMode{ 1 };
        widgets::key_row("Unload", "kb_unload", g_cfg.keyUnload, unloadMode);
        widgets::key_row("Unlock maps", "kb_maps", g_cfg.keyMaps, g_cfg.keyModeMaps);
        widgets::key_row("Cheap shop", "kb_shop", g_cfg.keyShop, g_cfg.keyModeShop);
        widgets::key_row("Cheap skills", "kb_skills", g_cfg.keySkills, g_cfg.keyModeSkills);
        widgets::key_row("Coin packs", "kb_coins", g_cfg.keyCoins, g_cfg.keyModeCoins);
        widgets::key_row("Crates", "kb_crates", g_cfg.keyCrates, g_cfg.keyModeCrates);
        widgets::key_row("Vacuum", "kb_vacuum", g_cfg.keyVacuum, g_cfg.keyModeVacuum);
        widgets::key_row("Phones", "kb_phones", g_cfg.keyPhones, g_cfg.keyModePhones);
        widgets::key_row("Cameras", "kb_cameras", g_cfg.keyCameras, g_cfg.keyModeCameras);
        widgets::key_row("Doors", "kb_doors", g_cfg.keyDoors, g_cfg.keyModeDoors);
        widgets::key_row("Guns", "kb_guns", g_cfg.keyGuns, g_cfg.keyModeGuns);
        takeui::CenterSeparator("World extras");
        ImGui::TextDisabled("bind extras, unbound hides");
        widgets::key_row("Complete heist", "kb_complete", g_cfg.keyComplete, g_cfg.keyModeComplete);
        widgets::key_row("Instant win", "kb_wingame", g_cfg.keyWingame, g_cfg.keyModeWingame);
        widgets::key_row("Infinite ammo", "kb_ammo", g_cfg.keyAmmo, g_cfg.keyModeAmmo);
        widgets::key_row("+100K XP", "kb_xp", g_cfg.keyXp, g_cfg.keyModeXp);
        widgets::key_row("Immunity", "kb_immunity", g_cfg.keyImmunity, g_cfg.keyModeImmunity);
        widgets::key_row("Dance", "kb_dance", g_cfg.keyDance, g_cfg.keyModeDance);
        widgets::key_row("Kick self", "kb_kickself", g_cfg.keyKickself, g_cfg.keyModeKickself);
        widgets::key_row("Floppy arm", "kb_floppy", g_cfg.keyFloppy, g_cfg.keyModeFloppy);
        widgets::key_row("Toggle cops", "kb_cops", g_cfg.keyCops, g_cfg.keyModeCops);
        widgets::key_row("Random mask", "kb_randmask", g_cfg.keyRandmask, g_cfg.keyModeRandmask);
        widgets::key_row("End timer", "kb_endtimer", g_cfg.keyEndtimer, g_cfg.keyModeEndtimer);
        widgets::key_row("Enable cheats", "kb_cheats", g_cfg.keyCheats, g_cfg.keyModeCheats);
        widgets::key_row("Ghost", "kb_ghost", g_cfg.keyGhost, g_cfg.keyModeGhost);
        widgets::key_row("Freeze world", "kb_playersonly", g_cfg.keyPlayersonly, g_cfg.keyModePlayersonly);
        widgets::key_row("Debug camera", "kb_debugcam", g_cfg.keyDebugcam, g_cfg.keyModeDebugcam);
        widgets::key_row("Depth chams", "kb_depthchams", g_cfg.keyDepthchams, g_cfg.keyModeDepthchams);
        widgets::os_section_end();
    }
    ImGui::SameLine();
    if (widgets::os_section("Style", ImVec2(gw, 0))) {
        ImGui::SetWindowFontScale(dpi_scale);
        takeui::CenterSeparator("ESP colors");
        ImGui::TextDisabled("Players");
        widgets::color_tail("Players##c", g_cfg.playerCol);
        ImGui::TextDisabled("Guards");
        widgets::color_tail("Guards##c", g_cfg.guardCol);
        ImGui::TextDisabled("Items");
        widgets::color_tail("Items##c", g_cfg.itemCol);
        ImGui::TextDisabled("Money");
        widgets::color_tail("Money##c", g_cfg.moneyCol);
        ImGui::TextDisabled("Health");
        widgets::color_tail("Health##c", g_cfg.healthCol);
        takeui::CenterSeparator("Overlays");
        widgets::check_bridge("Show watermark", g_cfg.watermark);
        widgets::check_bridge("Glass overlays", g_cfg.glassBlur);
        ImGui::TextDisabled("Glass tint");
        widgets::color_tail("Colour##glasstint", g_cfg.glassTint);
        widgets::check_bridge("24-hour clock", g_cfg.clock24h);
        widgets::check_bridge("Real blur backdrop", g_cfg.blurBackdrop);
        widgets::check_bridge("Menu particles", g_cfg.particles);
        widgets::check_bridge("Border FX comets", g_cfg.borderFx);
        widgets::check_bridge("Border rainbow", g_cfg.borderRainbow);
        widgets::check_bridge("Hotkey list overlay", g_cfg.keybindList);
        widgets::check_bridge("Player count box", g_cfg.playerCountBox);
        widgets::check_bridge("Center crosshair", g_cfg.crosshair);
        widgets::check_bridge("Kill feed", g_cfg.killfeed);
        widgets::check_bridge("Hit log", g_cfg.hitlog);
        widgets::check_bridge("Radar", g_cfg.radar);
        widgets::sliderf_bridge("Radar range", g_cfg.radarRange, 10.0f, 200.0f, "%.0f");
        widgets::sliderf_bridge("Blur strength", g_cfg.blurAmount, 0.1f, 1.5f, "%.2f");
        takeui::CenterSeparator("Diagnostics");
        if (ImAdd::Button("RESCAN SIGNATURES", ImVec2(-1, 30))) {
            mem::rescan();
            diag::report();
            g_cfg.configStatus = "signatures rescanned";
        }
        const diag::Report r = diag::last();
        char b[32];
        snprintf(b, sizeof(b), "0x%llX", (unsigned long long)r.imageBase);
        diag_line("Image base", b, true);
        snprintf(b, sizeof(b), "0x%llX", (unsigned long long)r.gwAbs);
        diag_line(r.gwSigHit ? "GWorld (sig)" : "GWorld (fallback)", b, r.gwValid);
        snprintf(b, sizeof(b), "0x%llX", (unsigned long long)r.goAbs);
        diag_line(r.goSigHit ? "GObjects (sig)" : "GObjects (fallback)", b, r.goNum > 0);
        snprintf(b, sizeof(b), "%d", r.goNum);
        diag_line("GObjects count", b, r.goNum > 0);
        snprintf(b, sizeof(b), "0x%llX", (unsigned long long)r.peAbs);
        diag_line("ProcessEvent", b, r.peSigHit);
        diag_line("Local controller", r.pcValid ? "valid" : "NULL", r.pcValid);
        snprintf(b, sizeof(b), "0x%llX", (uintptr_t)r.presentTarget);
        diag_line("Present target", b, r.presentValid);
        snprintf(b, sizeof(b), "0x%llX", (uintptr_t)r.resizeTarget);
        diag_line("Resize target", b, r.resizeValid);
        diag_line("Steam Uid", steamServiceHelper::id_text(), true);
        diag_line("Blur toggle", g_cfg.blurBackdrop.load() ? "on" : "OFF", g_cfg.blurBackdrop.load());
        snprintf(b, sizeof(b), "%s r%.2f", blur::ready() ? "ready" : "FAILED", blur::radius());
        diag_line("Blur backend", b, blur::ready());
        takeui::CenterSeparator("Help");
        ImGui::BulletText("Menu %s, Unload %s", widgets::key_name(g_cfg.keyMenu.load()),
            widgets::key_name(g_cfg.keyUnload.load()));
        ImGui::BulletText("List UI: NUM8/2 move, NUM5 select, NUM0 back");
        widgets::os_section_end();
    }
}
struct Reg { Reg() { tabs::add({ "settings", "Settings", draw_settings }); } };
static Reg g_reg;
}

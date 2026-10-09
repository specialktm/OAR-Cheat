#include "pch/pch.h"
#include "ui/tabs.hpp"
#include "ui/widgets.hpp"
#include "ui/menu.hpp"
#include "ui/theme.hpp"
#include "config/config.hpp"
#include "config/config_io.hpp"
#include "world/diagnostics.hpp"
#include "render/blur.hpp"

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
    widgets::header_card("Settings", "Configs, theme and keys");

    widgets::os_section("Config", ImVec2(800, 300));
    {
        ImGui::SetWindowFontScale(dpi_scale);
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
            if (ImGui::Combo("Saved configs", &cur, ptrs.data(), (int)ptrs.size(), 9)) {
                if (cur >= 0 && cur < (int)names.size()) {
                    snprintf(nameBuf, sizeof(nameBuf), "%s", names[cur].c_str());
                    g_cfg.configName = nameBuf;
                    std::string err;
                    if (cfgio::load(g_cfg.configName, err)) {
                        theme::apply_current();
                        g_cfg.configStatus = "loaded " + g_cfg.configName;
                    } else g_cfg.configStatus = err;
                }
            }
        } else {
            ImGui::TextDisabled("no saved configs yet");
        }
        ImGui::InputText("Name", nameBuf, sizeof(nameBuf));
        float bw = ImGui::GetContentRegionAvail().x;
        float hb = (bw - 12) / 3;
        if (ImGui::Button("SAVE", ImVec2(hb, 30))) {
            g_cfg.configName = nameBuf;
            std::string err;
            if (cfgio::save(g_cfg.configName, err))
                g_cfg.configStatus = "saved " + g_cfg.configName;
            else
                g_cfg.configStatus = err;
        }
        ImGui::SameLine(0, -1);
        if (ImGui::Button("LOAD", ImVec2(hb, 30))) {
            g_cfg.configName = nameBuf;
            std::string err;
            if (cfgio::load(g_cfg.configName, err)) {
                theme::apply_current();
                snprintf(nameBuf, sizeof(nameBuf), "%s", g_cfg.configName.c_str());
                g_cfg.configStatus = "loaded " + g_cfg.configName;
            } else g_cfg.configStatus = err;
        }
        ImGui::SameLine(0, -1);
        if (ImGui::Button("UNLOAD", ImVec2(hb, 30)))
            programRunning.store(false);
        if (!g_cfg.configStatus.empty())
            ImGui::TextDisabled("%s", g_cfg.configStatus.c_str());
        ImGui::TextDisabled("file: %s", cfgio::path_for(g_cfg.configName).c_str());
    }
    widgets::os_section_end();

    ImGui::SetCursorPos(ImVec2(0, 440));
    widgets::os_section("Keys", ImVec2(800, 730));
    {
        ImGui::SetWindowFontScale(dpi_scale);
        ImGui::TextDisabled("click to bind, right-click mode");
        widgets::keybind_row("Menu", g_cfg.keyMenu, &g_cfg.keyModeMenu);
        widgets::keybind_row("Unload", g_cfg.keyUnload, nullptr);
        widgets::keybind_row("Unlock maps", g_cfg.keyMaps, &g_cfg.keyModeMaps);
        widgets::keybind_row("Cheap shop", g_cfg.keyShop, &g_cfg.keyModeShop);
        widgets::keybind_row("Cheap skills", g_cfg.keySkills, &g_cfg.keyModeSkills);
        widgets::keybind_row("Coin packs", g_cfg.keyCoins, &g_cfg.keyModeCoins);
        widgets::keybind_row("Crates", g_cfg.keyCrates, &g_cfg.keyModeCrates);
        widgets::keybind_row("Vacuum", g_cfg.keyVacuum, &g_cfg.keyModeVacuum);
        widgets::keybind_row("Phones", g_cfg.keyPhones, &g_cfg.keyModePhones);
        widgets::keybind_row("Cameras", g_cfg.keyCameras, &g_cfg.keyModeCameras);
        widgets::keybind_row("Doors", g_cfg.keyDoors, &g_cfg.keyModeDoors);
        widgets::keybind_row("Guns", g_cfg.keyGuns, &g_cfg.keyModeGuns);
    }
    widgets::os_section_end();

    ImGui::SetCursorPos(ImVec2(0, 1190));
    widgets::os_section("Theme", ImVec2(800, 900));
    {
        ImGui::SetWindowFontScale(dpi_scale);
        int n = theme::count();
        static std::vector<const char*> tnames;
        tnames.clear();
        for (int i = 0; i < n; ++i) tnames.push_back(theme::get(i).name);
        int ti = theme::current();
        if (ImGui::Combo("Theme", &ti, tnames.data(), n, 9)) {
            if (ti >= 0 && ti < n) {
                theme::set_current(ti);
                g_cfg.themeIdx.store(ti);
            }
        }
        ImGui::TextDisabled("Custom theme editor");
        bool customActive = (theme::current() == theme::count() - 1);
        if (ImGui::ColorEdit4("Accent A", (float*)&g_cfg.customA)) if (customActive) theme::apply_current();
        if (ImGui::ColorEdit4("Accent B", (float*)&g_cfg.customB)) if (customActive) theme::apply_current();
        if (ImGui::ColorEdit4("Glow", (float*)&g_cfg.customGlow)) if (customActive) theme::apply_current();
        ImGui::TextDisabled("ESP colors");
        ImGui::ColorEdit4("Players", (float*)&g_cfg.playerCol);
        ImGui::ColorEdit4("Guards", (float*)&g_cfg.guardCol);
        ImGui::ColorEdit4("Items", (float*)&g_cfg.itemCol);
        ImGui::ColorEdit4("Money", (float*)&g_cfg.moneyCol);
        ImGui::ColorEdit4("Health", (float*)&g_cfg.healthCol);
        static const char* styles[2] = { "Mouse", "List (numpad)" };
        int ui = g_cfg.uiStyle.load();
        if (ImGui::Combo("UI style", &ui, styles, 2, 2)) g_cfg.uiStyle.store(ui);
        widgets::check_bridge("Show watermark", g_cfg.watermark);
        widgets::check_bridge("24-hour clock", g_cfg.clock24h);
        widgets::check_bridge("Real blur backdrop", g_cfg.blurBackdrop);
        widgets::check_bridge("Menu particles", g_cfg.particles);
        widgets::sliderf_bridge("Blur strength", g_cfg.blurAmount, 0.1f, 1.5f, "%.2f");
    }
    widgets::os_section_end();

    ImGui::SetCursorPos(ImVec2(0, 2110));
    widgets::os_section("Diagnostics", ImVec2(800, 340));
    {
        ImGui::SetWindowFontScale(dpi_scale);
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
        diag_line("Blur toggle", g_cfg.blurBackdrop.load() ? "on" : "OFF", g_cfg.blurBackdrop.load());
        snprintf(b, sizeof(b), "%s r%.2f", blur::ready() ? "ready" : "FAILED", blur::radius());
        diag_line("Blur backend", b, blur::ready());
    }
    widgets::os_section_end();

    ImGui::SetCursorPos(ImVec2(0, 2470));
    widgets::os_section("Help", ImVec2(800, 140));
    {
        ImGui::SetWindowFontScale(dpi_scale);
        ImGui::BulletText("Menu %s, Unload %s", widgets::key_name(g_cfg.keyMenu.load()),
            widgets::key_name(g_cfg.keyUnload.load()));
        ImGui::BulletText("List UI: NUM8/2 move, NUM5 select, NUM0 back");
    }
    widgets::os_section_end();
}
struct Reg { Reg() { tabs::add({ "settings", "Settings", draw_settings }); } };
static Reg g_reg;
}

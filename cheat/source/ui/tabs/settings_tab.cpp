#include "pch/pch.h"
#include "ui/tabs.hpp"
#include "ui/widgets.hpp"
#include "ui/theme.hpp"
#include "core/config.hpp"
#include "core/config_io.hpp"
#include "core/diagnostics.hpp"

namespace {
static void hex_row(const char* k, uintptr_t v, bool ok) {
    char buf[32];
    snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)v);
    widgets::stat_text(k, buf,
        ok ? IM_COL32(90, 255, 140, 255) : IM_COL32(255, 110, 110, 255));
}

void draw_settings() {
    widgets::section("CONFIG");
    {
        static char nameBuf[64] = { 0 };
        static bool nameInit = false;
        if (!nameInit) {
            nameInit = true;
            snprintf(nameBuf, sizeof(nameBuf), "%s", g_cfg.configName.c_str());
        }
        ImGui::InputText("Name", nameBuf, sizeof(nameBuf));
        float bw = ImGui::GetContentRegionAvail().x;
        float hb = (bw - 12) / 3;
        if (widgets::action_button("SAVE", "write ini", ImVec2(hb, 30))) {
            g_cfg.configName = nameBuf;
            std::string err;
            if (cfgio::save(g_cfg.configName, err))
                g_cfg.configStatus = "saved " + g_cfg.configName;
            else
                g_cfg.configStatus = err;
        }
        ImGui::SameLine(0, 6);
        if (widgets::action_button("LOAD", "read ini", ImVec2(hb, 30))) {
            g_cfg.configName = nameBuf;
            std::string err;
            if (cfgio::load(g_cfg.configName, err)) {
                theme::apply_current();
                snprintf(nameBuf, sizeof(nameBuf), "%s", g_cfg.configName.c_str());
                g_cfg.configStatus = "loaded " + g_cfg.configName;
            } else g_cfg.configStatus = err;
        }
        ImGui::SameLine(0, 6);
        if (widgets::action_button("UNLOAD", "eject dll", ImVec2(hb, 30)))
            programRunning.store(false);
        if (!g_cfg.configStatus.empty())
            ImGui::TextDisabled("%s", g_cfg.configStatus.c_str());
        ImGui::TextDisabled("file: %s", cfgio::path_for(g_cfg.configName).c_str());
    }
    widgets::section("KEYBINDS (click to bind, right-click clears)");
    {
        widgets::keybind("Menu", g_cfg.keyMenu);
        widgets::keybind("Unload", g_cfg.keyUnload);
        widgets::keybind("Unlock maps", g_cfg.keyMaps);
        widgets::keybind("Cheap shop", g_cfg.keyShop);
        widgets::keybind("Cheap skills", g_cfg.keySkills);
        widgets::keybind("Coin packs", g_cfg.keyCoins);
        widgets::keybind("Crates", g_cfg.keyCrates);
        widgets::keybind("Vacuum", g_cfg.keyVacuum);
        widgets::keybind("Phones", g_cfg.keyPhones);
        widgets::keybind("Cameras", g_cfg.keyCameras);
        widgets::keybind("Doors", g_cfg.keyDoors);
        widgets::keybind("Guns", g_cfg.keyGuns);
    }
    widgets::section("THEME (included in config)");
    for (int i = 0; i < theme::count(); ++i) {
        bool sel = (theme::current() == i);
        if (ImGui::Selectable(theme::get(i).name, sel))
            theme::set_current(i);
        if (sel) ImGui::SetItemDefaultFocus();
    }
    ImGui::Spacing();
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
    widgets::section("INTERFACE");
    {
        const char* styles[2] = { "Mouse", "List (numpad)" };
        widgets::dropdown("UI style", styles, 2, g_cfg.uiStyle);
    }
    widgets::section("WATERMARK");
    widgets::toggle_ex("Show watermark", g_cfg.watermark);
    widgets::toggle_ex("24-hour clock", g_cfg.clock24h);
    widgets::section("BACKDROP");
    widgets::toggle_ex("Real blur backdrop", g_cfg.blurBackdrop);
    widgets::section("DIAGNOSTICS");
    const diag::Report r = diag::last();
    char b[32];
    snprintf(b, sizeof(b), "0x%llX", (unsigned long long)r.imageBase);
    widgets::stat_text("Image base", b, IM_COL32(140, 200, 255, 255));
    hex_row(r.gwSigHit ? "GWorld (sig hit)" : "GWorld (fallback)", r.gwAbs, r.gwValid);
    hex_row(r.goSigHit ? "GObjects (sig hit)" : "GObjects (fallback)", r.goAbs, r.goNum > 0);
    widgets::stat_row("GObjects count", r.goNum, r.goNum > 0
        ? IM_COL32(90, 255, 140, 255) : IM_COL32(255, 110, 110, 255));
    hex_row("ProcessEvent", r.peAbs, r.peSigHit);
    widgets::stat_text("Local controller", r.pcValid ? "valid" : "NULL",
        r.pcValid ? IM_COL32(90, 255, 140, 255) : IM_COL32(255, 210, 120, 255));
    hex_row("Present target", (uintptr_t)r.presentTarget, r.presentValid);
    hex_row("Resize target", (uintptr_t)r.resizeTarget, r.resizeValid);
    widgets::section("HELP");
    ImGui::BulletText("Menu %s, Unload %s", widgets::key_name(g_cfg.keyMenu.load()),
        widgets::key_name(g_cfg.keyUnload.load()));
    ImGui::BulletText("List UI: NUM8/2 move, NUM5 select, NUM0 back");
}
struct Reg { Reg() { tabs::add({ "settings", "Settings", draw_settings }); } };
static Reg g_reg;
} // namespace

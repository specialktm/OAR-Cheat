#include "pch/pch.h"
#include "ui/tabs.hpp"
#include "ui/widgets.hpp"
#include "ui/menu.hpp"
#include "ui/take_shell.hpp"
#include "config/config.hpp"
#include "features/oar_lua.hpp"

namespace {
void draw_lua() {
    ImGuiStyle& style = ImGui::GetStyle();
    float gw = ImTrunc((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) / 2.0f);
    if (widgets::os_section("Scripts", ImVec2(gw, 0))) {
        ImGui::SetWindowFontScale(dpi_scale);
        takeui::CenterSeparator("Files");
        static std::vector<std::string> names;
        static bool wantRefresh = true;
        if (wantRefresh) {
            wantRefresh = false;
            names = lua::list_scripts();
        }
        if (ImAdd::Button("REFRESH", ImVec2(-1, 30)))
            wantRefresh = true;
        if (names.empty()) {
            ImGui::TextDisabled("no .lua files - drop scripts in the Scripts folder");
        }
        for (auto& n : names) {
            ImGui::PushID(n.c_str());
            bool running = lua::is_running(n);
            ImGui::TextDisabled("%s", widgets::shorten(n, 40).c_str());
            ImGui::SameLine();
            float w = 120.0f;
            ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - w);
            if (running) {
                if (ImAdd::Button("STOP", ImVec2(w, 26)))
                    lua::stop(n);
            } else {
                if (ImAdd::Button("RUN", ImVec2(w, 26))) {
                    lua::start(n);
                    wantRefresh = true;
                }
            }
            ImGui::PopID();
        }
        auto runs = lua::list_runs();
        if (!runs.empty()) {
            char b[64];
            snprintf(b, sizeof(b), "active: %d", (int)runs.size());
            ImGui::TextDisabled("%s", b);
            if (ImAdd::Button("STOP ALL", ImVec2(-1, 30)))
                lua::stop_all();
        }
        takeui::CenterSeparator("Run inline");
        static char buf[8192] = { 0 };
        ImGui::InputTextMultiline("##luaexec", buf, sizeof(buf), ImVec2(-1, 150));
        if (ImAdd::Button("EXECUTE", ImVec2(-1, 30))) {
            lua::execute_string(buf);
            memset(buf, 0, sizeof(buf));
        }
        widgets::os_section_end();
    }
    ImGui::SameLine();
    if (widgets::os_section("Output", ImVec2(gw, 0))) {
        ImGui::SetWindowFontScale(dpi_scale);
        takeui::CenterSeparator("Console");
        if (ImAdd::Button("CLEAR", ImVec2(-1, 28)))
            lua::clear_output();
        auto lines = lua::get_output();
        ImGui::BeginChild("##luaout", ImVec2(-1, 250), true);
        for (auto& ln : lines)
            ImGui::TextUnformatted(ln.c_str());
        if (!lines.empty())
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
        widgets::os_section_end();
    }
}
struct Reg { Reg() { tabs::add({ "lua", "Lua", draw_lua }); } };
static Reg g_reg;
}

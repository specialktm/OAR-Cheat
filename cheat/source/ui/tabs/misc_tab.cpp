#include "pch/pch.h"
#include "ui/tabs.hpp"
#include "ui/widgets.hpp"
#include "ui/menu.hpp"
#include "ui/take_shell.hpp"
#include "config/config.hpp"
#include "util/veh.hpp"
#include "features/music/music.h"
#include "features/objbrowser.hpp"
#include "features/propeditor.hpp"
#include "SDK/Basic.hpp"
#include "SDK/CoreUObject_classes.hpp"

namespace {
static int miscMode = 0;

void draw_misc() {
    ImGuiStyle& style = ImGui::GetStyle();
    float gw = ImTrunc((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) / 2.0f);
    static int objSel = -1;
    if (widgets::os_subtab("MUSIC", miscMode == 0, gw)) miscMode = 0;
    ImGui::SameLine(0, 8);
    if (widgets::os_subtab("EXPLORER", miscMode == 1, gw)) miscMode = 1;
    if (miscMode == 0) {
        if (widgets::os_section("Tracks", ImVec2(gw, 0))) {
            ImGui::SetWindowFontScale(dpi_scale);
            static std::vector<music::track_t> tracks;
            static bool wantScan = true;
            if (wantScan) {
                wantScan = false;
                music::ensure_song_folder();
                tracks = music::scan_songs();
            }
            takeui::CenterSeparator("Library");
            if (ImAdd::Button("RESCAN", ImVec2(-1, 30)))
                wantScan = true;
            if (tracks.empty()) {
                ImGui::TextDisabled("drop .mp3/.wav/.m4a/.aac/.flac/.wma in the Music folder");
            }
            ImGui::BeginChild("##tracks", ImVec2(-1, 220), true);
            for (auto& t : tracks) {
                ImGui::PushID(t.path.c_str());
                bool cur = (music::current_name() == t.name) &&
                    (music::state() == music::state_t::playing || music::state() == music::state_t::paused);
                if (ImGui::Selectable(widgets::shorten(t.name, 42).c_str(), cur))
                    music::play(t.path, t.name);
                ImGui::PopID();
            }
            ImGui::EndChild();
            widgets::os_section_end();
        }
        ImGui::SameLine();
        if (widgets::os_section("Player", ImVec2(gw, 0))) {
            ImGui::SetWindowFontScale(dpi_scale);
            takeui::CenterSeparator("Now playing");
            music::state_t st = music::state();
            const char* sname = music::current_name().c_str();
            char line[160];
            const char* sst = (st == music::state_t::playing) ? "playing" :
                (st == music::state_t::paused) ? "paused" :
                (st == music::state_t::decoding) ? "decoding..." :
                (st == music::state_t::stopped) ? "stopped" : "idle";
            snprintf(line, sizeof(line), "%s [%s]", sname[0] ? sname : "-", sst);
            ImGui::TextDisabled("%s", line);
            std::string err = music::error_message();
            if (!err.empty())
                ImGui::TextDisabled("error: %s", err.c_str());
            char pos[96];
            snprintf(pos, sizeof(pos), "%s / %s", music::position_text().c_str(), music::duration_text().c_str());
            ImGui::TextDisabled("%s", pos);
            float frac = music::position_fraction();
            float f = frac;
            if (widgets::os_slider("Seek", &f, 0.0f, 1.0f, "%.2f") && (f < frac - 0.01f || f > frac + 0.01f))
                music::seek_fraction(f);
            float bw = ImGui::GetContentRegionAvail().x;
            float hb = (bw - 8) / 3;
            if (ImAdd::Button("PLAY/PAUSE", ImVec2(hb, 30)))
                music::toggle_play_pause();
            ImGui::SameLine(0, 4);
            if (ImAdd::Button("STOP", ImVec2(hb, 30)))
                music::stop();
            ImGui::SameLine(0, 4);
            bool loop = music::loop_enabled();
            if (ImAdd::Button(loop ? "LOOP ON" : "LOOP OFF", ImVec2(hb, 30)))
                music::set_loop(!loop);
            float vol = music::volume();
            if (widgets::os_slider("Volume", &vol, 0.0f, 1.0f, "%.2f"))
                music::set_volume(vol);
            widgets::os_section_end();
        }
    } else {
        if (widgets::os_section("Objects", ImVec2(gw, 0))) {
            ImGui::SetWindowFontScale(dpi_scale);
            takeui::CenterSeparator("Live classes");
            static char filter[128] = { 0 };
            ImGui::TextDisabled("Filter");
            ImGui::InputText("##objfilter", filter, sizeof(filter));
            widgets::check_bridge("Classes only", g_cfg.objClassesOnly);
            auto rows = objbrowser::rows(filter, 1500);
            ImGui::BeginChild("##objrows", ImVec2(-1, 330), true);
            for (auto& r : rows) {
                if (g_cfg.objClassesOnly.load() && r.cls != "Class") continue;
                char b[160];
                snprintf(b, sizeof(b), "%6d | %s", r.index, r.name.empty() ? "?" : widgets::shorten(r.name, 40).c_str());
                ImGui::PushID(r.index);
                if (ImGui::Selectable(b, objSel == r.index))
                    objSel = r.index;
                ImGui::PopID();
            }
            ImGui::EndChild();
            if (objSel >= 0) {
                objbrowser::Row d;
                if (objbrowser::resolve(objSel, d)) {
                    char b[256];
                    snprintf(b, sizeof(b), "#%d %p", d.index, d.obj);
                    ImGui::TextDisabled("%s", b);
                    ImGui::TextWrapped("name: %s", d.name.empty() ? "?" : d.name.c_str());
                    ImGui::TextWrapped("class: %s", d.cls.empty() ? "?" : d.cls.c_str());
                } else {
                    ImGui::TextDisabled("stale entry, rescan");
                }
            }
            widgets::os_section_end();
        }
        ImGui::SameLine();
        if (widgets::os_section("Inspector", ImVec2(gw, 0))) {
            ImGui::SetWindowFontScale(dpi_scale);
            takeui::CenterSeparator("Live edit");
            static void* inspObj = nullptr;
            static void* inspCls = nullptr;
            static int selProp = -1;
            static char valBuf[128] = { 0 };
            static char propStatus[128] = { 0 };
            static std::vector<std::string> valCache;
            objbrowser::Row d;
            bool hasSel = false;
            if (objSel >= 0 && objbrowser::resolve(objSel, d) && IS_VALID_PTR(d.obj)) {
                hasSel = true;
                if (d.obj != inspObj) {
                    inspObj = d.obj;
                    inspCls = nullptr;
                    selProp = -1;
                    valBuf[0] = 0;
                    valCache.clear();
                    snprintf(propStatus, sizeof(propStatus), "pick a property");
                    veh::try_call([&] {
                        auto* uo = reinterpret_cast<SDK::UObject*>(d.obj);
                        if (IS_VALID_PTR(uo)) inspCls = uo->Class;
                    });
                }
            } else {
                inspObj = nullptr;
                inspCls = nullptr;
            }
            if (!hasSel) {
                ImGui::TextDisabled("select an object left");
            } else if (!IS_VALID_PTR(inspCls)) {
                ImGui::TextDisabled("class unavailable");
            } else {
                const std::vector<proped::Prop>& props = proped::props_for(inspCls);
                if (!proped::layout_verified())
                    ImGui::TextDisabled("layout unverified - read only");
                if (props.empty()) {
                    ImGui::TextDisabled("no editable scalars");
                } else {
                    if (valCache.size() != props.size()) {
                        valCache.assign(props.size(), std::string());
                        for (size_t i = 0; i < props.size(); ++i)
                            proped::read_value(inspObj, props[i], valCache[i]);
                    }
                    ImGui::BeginChild("##props", ImVec2(-1, 260), true);
                    for (size_t i = 0; i < props.size(); ++i) {
                        char row[192];
                        snprintf(row, sizeof(row), "%s [%s] = %s", props[i].name.c_str(),
                            props[i].type.c_str(), valCache[i].c_str());
                        ImGui::PushID((int)i);
                        if (ImGui::Selectable(row, selProp == (int)i)) {
                            selProp = (int)i;
                            snprintf(valBuf, sizeof(valBuf), "%s", valCache[i].c_str());
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndChild();
                    if (selProp >= 0 && selProp < (int)props.size()) {
                        const proped::Prop& pr = props[selProp];
                        ImGui::TextDisabled("Value (%s)", pr.name.c_str());
                        ImGui::InputText("##propval", valBuf, sizeof(valBuf));
                        bool canWrite = proped::layout_verified() && pr.kind >= 1 && pr.kind <= 6;
                        if (!canWrite) ImGui::BeginDisabled();
                        if (ImAdd::Button("APPLY", ImVec2(-1, 28)) && canWrite) {
                            std::string err;
                            if (proped::write_value(inspObj, pr, valBuf, err)) {
                                snprintf(propStatus, sizeof(propStatus), "wrote %s", pr.name.c_str());
                                proped::read_value(inspObj, pr, valCache[selProp]);
                                snprintf(valBuf, sizeof(valBuf), "%s", valCache[selProp].c_str());
                            } else {
                                snprintf(propStatus, sizeof(propStatus), "%.90s", err.c_str());
                            }
                        }
                        if (!canWrite) ImGui::EndDisabled();
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("writes this instance only");
                    }
                    if (ImAdd::Button("REFRESH", ImVec2(-1, 26))) {
                        for (size_t i = 0; i < props.size(); ++i)
                            proped::read_value(inspObj, props[i], valCache[i]);
                        snprintf(propStatus, sizeof(propStatus), "reread %d", (int)props.size());
                    }
                }
                ImGui::TextWrapped("%s", propStatus);
            }
            widgets::os_section_end();
        }
    }
}
struct Reg { Reg() { tabs::add({ "misc", "Misc", draw_misc }); } };
static Reg g_reg;
}

#include "pch/pch.h"
#include "ui/menu.hpp"
#include "ui/listmenu.hpp"
#include "ui/popouts.hpp"
#include "ui/tabs.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"
#include "core/config.hpp"
#include "core/config_io.hpp"
#include "features/esp.hpp"
#include "render/blur.hpp"
#include "render/dx11_hook.hpp"

#include <cmath>
#include <cctype>
#include <cstring>

#include "../../vendor/imgui/imgui.h"

namespace ui {

void apply_modern_style() { theme::apply_current(); }

// last window rect: the blur image is drawn behind the window, so the frosted
// glass IS the window background (no fullscreen dim layer).
static ImVec2 g_lastPos(0, 0), g_lastSize(880, 600);
static bool g_hasRect = false;

static void fallback_panel(ImVec2 pos, ImVec2 size) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
        IM_COL32(13, 16, 26, 244), 10.0f);
    ImVec4 a = theme::accentA(0.5f);
    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
        IM_COL32((int)(a.x * 255), (int)(a.y * 255), (int)(a.z * 255), 120),
        10.0f, 0, 1.0f);
}

void render_backdrop() {
    ImVec2 pos = g_hasRect ? g_lastPos
        : ImVec2((ImGui::GetIO().DisplaySize.x - g_lastSize.x) * 0.5f,
                 (ImGui::GetIO().DisplaySize.y - g_lastSize.y) * 0.5f);
    ImVec2 size = g_lastSize;
    bool ok = false;
    if (g_cfg.blurBackdrop.load())
        ok = blur::backdrop_region(dx11::swapchain(), pos.x, pos.y, size.x, size.y);
    if (!ok) {
        fallback_panel(pos, size);
    } else {
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            IM_COL32(10, 13, 24, 205), 10.0f);
        ImVec4 a = theme::accentA(0.8f);
        dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            IM_COL32((int)(a.x * 255), (int)(a.y * 255), (int)(a.z * 255), 160),
            10.0f, 0, 1.0f);
    }
}

static void draw_shadow(ImVec2 pos, ImVec2 size, float rounding) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    for (int i = 5; i >= 1; --i) {
        float o = (float)i * 7.0f;
        int a = 26 - i * 4;
        dl->AddRectFilled(ImVec2(pos.x - o, pos.y - o),
            ImVec2(pos.x + size.x + o, pos.y + size.y + o),
            IM_COL32(0, 0, 0, a), rounding + o, ImDrawFlags_RoundCornersAll);
    }
}

static const char* cached_user() {
    static char name[64] = { 0 };
    static bool init = false;
    if (!init) {
        init = true;
        DWORD n = (DWORD)sizeof(name);
        if (!GetUserNameA(name, &n) || n == 0)
            strcpy_s(name, "player");
        else if (n < sizeof(name))
            name[n] = 0;
    }
    return name;
}

static void watermark() {
    if (!g_cfg.watermark.load()) return;
    SYSTEMTIME st{};
    GetLocalTime(&st);
    char clock[32];
    if (g_cfg.clock24h.load())
        snprintf(clock, sizeof(clock), "%02d:%02d", st.wHour, st.wMinute);
    else {
        int h = st.wHour % 12;
        if (h == 0) h = 12;
        snprintf(clock, sizeof(clock), "%d:%02d %s", h, st.wMinute,
            st.wHour >= 12 ? "PM" : "AM");
    }
    char text[160];
    snprintf(text, sizeof(text), "OAR  |  %s  |  %s", cached_user(), clock);
    ImVec2 tsz = ImGui::CalcTextSize(text);
    ImVec2 pad(12, 7);
    ImVec2 winsz(tsz.x + pad.x * 2, tsz.y + pad.y * 2);
    float wx = g_cfg.wmX.load(), wy = g_cfg.wmY.load();
    ImVec2 vp = ImGui::GetIO().DisplaySize;
    if (wx < 0) wx = 0; if (wy < 0) wy = 0;
    if (wx + winsz.x > vp.x) wx = vp.x - winsz.x;
    if (wy + winsz.y > vp.y) wy = vp.y - winsz.y;

    ImGui::SetNextWindowPos(ImVec2(wx, wy), ImGuiCond_Always);
    ImGui::SetNextWindowSize(winsz);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##oar_wm", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    ImGui::SetCursorScreenPos(ImGui::GetWindowPos());
    ImGui::InvisibleButton("##wmdrag", winsz);
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 d = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
        g_cfg.wmX.store(wx + d.x);
        g_cfg.wmY.store(wy + d.y);
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
    }
    bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wpos = ImGui::GetWindowPos();
    dl->AddRectFilled(wpos, ImVec2(wpos.x + winsz.x, wpos.y + winsz.y),
        IM_COL32(10, 11, 20, 200), 9.0f);
    ImVec4 a = theme::accentA(0.7f);
    dl->AddRect(wpos, ImVec2(wpos.x + winsz.x, wpos.y + winsz.y),
        IM_COL32((int)(a.x * 255), (int)(a.y * 255), (int)(a.z * 255),
            hovered ? 255 : 140),
        9.0f, 0, 1.0f);
    dl->AddText(ImVec2(wpos.x + pad.x, wpos.y + pad.y),
        IM_COL32(235, 238, 248, 255), text);
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    ImGui::End();
    ImGui::PopStyleVar();
}

// Fixed display order (registry order is link-order dependent).
static int ordered_index(const std::vector<tabs::Tab>& all, int slot) {
    static const char* want[6] = { "home", "esp", "shop", "world", "player", "settings" };
    if (slot < 0 || slot >= 6) return -1;
    for (int i = 0; i < (int)all.size(); ++i)
        if (all[i].id && !strcmp(all[i].id, want[slot])) return i;
    return -1;
}

static void top_bar(const std::vector<tabs::Tab>& all, int& cur) {
    ImVec2 wpos = ImGui::GetWindowPos();
    float ww = ImGui::GetWindowSize().x;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // logo left
    dl->AddText(ImVec2(wpos.x + 16, wpos.y + 12), IM_COL32(235, 238, 248, 255), "OAR");
    ImVec2 vsz = ImGui::CalcTextSize("v2.0");
    dl->AddText(ImVec2(wpos.x + 16 + ImGui::CalcTextSize("OAR ").x, wpos.y + 12),
        IM_COL32(120, 130, 160, 255), "v2.0");
    (void)vsz;
    // user right
    const char* un = cached_user();
    ImVec2 usz = ImGui::CalcTextSize(un);
    ImVec2 ac(wpos.x + ww - usz.x - 36, wpos.y + 22);
    dl->AddCircle(ac, 11.0f, IM_COL32(150, 110, 255, 255), 20, 2.0f);
    dl->AddCircleFilled(ac, 9.0f, IM_COL32(30, 26, 54, 255));
    char initial[2] = { (char)toupper(un[0] ? un[0] : '?'), 0 };
    ImVec2 isz = ImGui::CalcTextSize(initial);
    dl->AddText(ImVec2(ac.x - isz.x / 2, ac.y - isz.y / 2),
        IM_COL32(190, 160, 255, 255), initial);
    dl->AddText(ImVec2(wpos.x + ww - usz.x - 16, wpos.y + 14),
        IM_COL32(200, 205, 220, 255), un);

    // tabs row (OSGames-style pills)
    ImGui::SetCursorPosY(44);
    ImGui::BeginChild("##topnav", ImVec2(0, 40), false);
    float avail = ImGui::GetContentRegionAvail().x;
    const int nslots = 6;
    float bw = (avail - (nslots - 1) * 6.0f) / nslots;
    for (int s = 0; s < nslots; ++s) {
        int ri = ordered_index(all, s);
        if (s > 0) ImGui::SameLine(0, 6);
        if (ri < 0) { ImGui::Dummy(ImVec2(bw, 30)); continue; }
        ImVec2 cur0 = ImGui::GetCursorScreenPos();
        ImVec2 sz(bw, 30);
        char id[32]; snprintf(id, sizeof(id), "##tab%d", s);
        ImGui::InvisibleButton(id, sz);
        bool hovered = ImGui::IsItemHovered();
        bool active = (cur == ri);
        if (ImGui::IsItemClicked()) cur = ri;
        ImVec4 a = theme::accentA(), b = theme::accentB();
        auto toU = [](ImVec4 c) {
            return IM_COL32((int)(c.x * 255), (int)(c.y * 255), (int)(c.z * 255), 255);
        };
        if (active) {
            dl->AddRectFilledMultiColor(cur0, ImVec2(cur0.x + sz.x, cur0.y + sz.y),
                toU(ImVec4(a.x * 0.75f, a.y * 0.75f, a.z, 1.0f)),
                toU(ImVec4(b.x * 0.6f, b.y * 0.6f, b.z, 1.0f)),
                toU(ImVec4(b.x * 0.6f, b.y * 0.6f, b.z, 1.0f)),
                toU(ImVec4(a.x * 0.75f, a.y * 0.75f, a.z, 1.0f)));
        } else if (hovered) {
            dl->AddRectFilled(cur0, ImVec2(cur0.x + sz.x, cur0.y + sz.y),
                IM_COL32(34, 40, 58, 255), 4.0f);
        } else {
            dl->AddRectFilled(cur0, ImVec2(cur0.x + sz.x, cur0.y + sz.y),
                IM_COL32(20, 24, 36, 255), 4.0f);
        }
        const char* t = all[ri].title ? all[ri].title : "?";
        char upper[32]; snprintf(upper, sizeof(upper), "%.24s", t);
        for (char* p = upper; *p; ++p) *p = (char)toupper(*p);
        ImVec2 tsz = ImGui::CalcTextSize(upper);
        dl->AddText(ImVec2(cur0.x + (sz.x - tsz.x) / 2, cur0.y + (sz.y - tsz.y) / 2),
            active ? IM_COL32(255, 255, 255, 255)
            : hovered ? IM_COL32(215, 222, 242, 255) : IM_COL32(140, 148, 170, 255),
            upper);
        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    ImGui::EndChild();
    ImGui::Separator();
}

static void bottom_bar() {
    esp::Counts c = esp::counts();
    char left[96];
    snprintf(left, sizeof(left), "%d players in game  |  %d FPS",
        c.players + c.guards, (int)ImGui::GetIO().Framerate);
    ImGui::BeginChild("##botbar", ImVec2(0, 44), false);
    ImGui::TextDisabled("%s", left);
    if (!g_cfg.configStatus.empty()) {
        ImGui::SameLine(0, 12);
        ImGui::TextDisabled("|  %s", g_cfg.configStatus.c_str());
    }
    // right-aligned SAVE / LOAD / UNLOAD (OSGames layout)
    float bw = 78.0f;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - bw * 3 - 12);
    if (ImGui::Button("SAVE", ImVec2(bw, 26))) {
        std::string err;
        if (cfgio::save(g_cfg.configName, err))
            g_cfg.configStatus = "saved " + g_cfg.configName;
        else
            g_cfg.configStatus = err;
    }
    ImGui::SameLine(0, 6);
    if (ImGui::Button("LOAD", ImVec2(bw, 26))) {
        std::string err;
        if (cfgio::load(g_cfg.configName, err)) {
            theme::apply_current();
            g_cfg.configStatus = "loaded " + g_cfg.configName;
        } else g_cfg.configStatus = err;
    }
    ImGui::SameLine(0, 6);
    if (ImGui::Button("UNLOAD", ImVec2(bw, 26))) {
        programRunning.store(false);
    }
    ImGui::EndChild();
}

void render() {
    watermark();
    popouts::render_all();
    if (!g_cfg.menuOpen.load()) return;

    if (g_cfg.uiStyle.load() == 1) {
        listui::render();
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 winsz(880, 600);
    ImGui::SetNextWindowSize(winsz, ImGuiCond_FirstUseEver);
    ImVec2 pos((io.DisplaySize.x - winsz.x) * 0.5f, (io.DisplaySize.y - winsz.y) * 0.5f);
    ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 10));
    ImGui::Begin("##oar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize);
    ImGui::PopStyleVar();

    ImVec2 wpos = ImGui::GetWindowPos(), wsz = ImGui::GetWindowSize();
    g_lastPos = wpos; g_lastSize = wsz; g_hasRect = true;
    draw_shadow(wpos, wsz, 6.0f);

    const std::vector<tabs::Tab>& all = tabs::list();
    int& cur = tabs::current();
    if (cur < 0 || cur >= (int)all.size()) cur = 0;

    top_bar(all, cur);
    ImGui::Spacing();
    ImGui::BeginChild("##body", ImVec2(0, -58), false);
    if (!all.empty() && all[cur].draw) all[cur].draw();
    ImGui::EndChild();
    ImGui::Separator();
    bottom_bar();

    ImGui::End();
}

} // namespace ui

#include "pch/pch.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "ui/menu.hpp"
#include "ui/listmenu.hpp"
#include "ui/popouts.hpp"
#include "ui/tabs.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"
#include "config/config.hpp"
#include "config/config_io.hpp"
#include "core/loading.hpp"
#include "steam/steam_user.hpp"
#include "features/esp.hpp"
#include "render/blur.hpp"
#include "render/dx11_hook.hpp"

#include <cmath>
#include <cctype>
#include <cstring>

#include "../../vendor/imgui/imgui.h"
#include "../../vendor/imgui/imgui_internal.h"

namespace ui {

void apply_modern_style() { theme::apply_current(); }

static ImVec2 g_lastPos(0, 0), g_lastSize(880, 580);
static bool g_hasRect = false;

static float g_vis = 0.0f;
float visibility() { return g_vis; }

static void fallback_panel(ImVec2 pos, ImVec2 size, float vis) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
        IM_COL32(16, 16, 16, (int)(244 * vis)), 0.0f);
}

void render_backdrop() {
    ImVec2 pos = g_hasRect ? g_lastPos
        : ImVec2((ImGui::GetIO().DisplaySize.x - g_lastSize.x) * 0.5f,
                 (ImGui::GetIO().DisplaySize.y - g_lastSize.y) * 0.5f);
    ImVec2 size = g_lastSize;
    float vis = g_vis;
    blur::set_radius(g_cfg.blurAmount.load());
    blur::set_alpha(vis);
    bool ok = false;
    if (g_cfg.blurBackdrop.load())
        ok = blur::backdrop_region(dx11::swapchain(), pos.x, pos.y, size.x, size.y);
    if (!ok) {
        fallback_panel(pos, size, vis);
    } else {
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            IM_COL32(8, 10, 18, (int)(120 * vis)), 0.0f);
    }
}

static void draw_shadow(ImVec2 pos, ImVec2 size, float rounding) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    for (int i = 5; i >= 1; --i) {
        float o = (float)i * 7.0f;
        int a = 26 - i * 4;
        dl->AddRectFilled(ImVec2(pos.x - o, pos.y - o),
            ImVec2(pos.x + size.x + o, pos.y + size.y + o),
            fade(IM_COL32(0, 0, 0, a)), rounding + o, ImDrawFlags_RoundCornersAll);
    }
}

static const char* cached_user() {
    return steamServiceHelper::name();
}

static void watermark() {
    if (!g_cfg.watermark.load()) return;
    if (g_cfg.uiStyle.load() == 1) return;
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
    std::string wun = cached_user();
    {
        auto p = wun.find("##");
        if (p != std::string::npos) wun.erase(p);
    }
    snprintf(text, sizeof(text), "OAR  |  %s  |  %s", wun.c_str(), clock);
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
        fade(IM_COL32(16, 16, 16, 210)), 5.0f);
    dl->AddText(ImVec2(wpos.x + pad.x, wpos.y + pad.y),
        fade(IM_COL32(235, 238, 248, 255)), text);
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    ImGui::End();
    ImGui::PopStyleVar();
}

static int ordered_index(const std::vector<tabs::Tab>& all, int slot) {
    static const char* want[8] = { "home", "esp", "shop", "world", "player", "lobby", "settings", "loadout" };
    if (slot < 0 || slot >= 8) return -1;
    for (int i = 0; i < (int)all.size(); ++i)
        if (all[i].id && !strcmp(all[i].id, want[slot])) return i;
    return -1;
}

static const char* tab_glyph(const char* id) {
    if (!id) return "?";
    if (!strcmp(id, "home")) return "\xf0\x9f\x91\x93";
    if (!strcmp(id, "esp")) return "\xf0\x9f\x8e\xaf";
    if (!strcmp(id, "shop")) return "\xf0\x9f\x9a\xb6";
    if (!strcmp(id, "world")) return "\xf0\x9f\xaa\x84";
    if (!strcmp(id, "player")) return "\xf0\x9f\x93\x81";
    if (!strcmp(id, "lobby")) return "\xf0\x9f\x91\xa5";
    if (!strcmp(id, "settings")) return "\xf0\x9f\x94\x8d";
    if (!strcmp(id, "loadout")) return "\xe2\x9a\x99\xef\xb8\x8f";
    return "?";
}

static const char* tab_hint(const char* id) {
    if (!id) return "";
    if (!strcmp(id, "home")) return "Overview + quick";
    if (!strcmp(id, "esp")) return "ESP + overlay";
    if (!strcmp(id, "shop")) return "Unlocks + skins";
    if (!strcmp(id, "world")) return "Phones + doors";
    if (!strcmp(id, "player")) return "Stats + items";
    if (!strcmp(id, "lobby")) return "Players + loot";
    if (!strcmp(id, "settings")) return "Configs + keys";
    if (!strcmp(id, "loadout")) return "guns + looks";
    return "";
}

static const char* tab_title(const char* id) {
    if (!id) return "?";
    if (!strcmp(id, "home")) return "Home";
    if (!strcmp(id, "esp")) return "Visuals";
    if (!strcmp(id, "shop")) return "Shop";
    if (!strcmp(id, "world")) return "World";
    if (!strcmp(id, "player")) return "Players";
    if (!strcmp(id, "lobby")) return "Lobby";
    if (!strcmp(id, "settings")) return "Settings";
    if (!strcmp(id, "loadout")) return "Loadout";
    return "?";
}

static float g_tabSize = 160.0f;
static float g_arrowRoll = 1.0f;
static bool g_tabOpening = true;
static float g_tabAlpha = 1.0f;
static int g_activeTab = 0;
static ImGuiWindow* s_mainWin = nullptr;
static ImGuiWindow* s_railWin = nullptr;
static ImGuiWindow* s_genWin = nullptr;

static void particles(ImVec2 wpos, ImVec2 wsz) {
    static ImVec2 ppos[100];
    static ImVec2 ptgt[100];
    static float pspd[100];
    static float prad[100];
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(wpos, ImVec2(wpos.x + wsz.x, wpos.y + wsz.y), true);
    for (int i = 1; i < 50; i++) {
        if (ppos[i].x == 0 || ppos[i].y == 0) {
            ppos[i].x = wpos.x + (float)(rand() % (int)wsz.x);
            ppos[i].y = wpos.y + 15.f;
            pspd[i] = 1 + rand() % 25;
            prad[i] = (float)(rand() % 4);
            ptgt[i].x = wpos.x + (float)(rand() % (int)wsz.x);
            ptgt[i].y = wpos.y + wsz.y * 2;
        }
        float k = ImGui::GetIO().DeltaTime * (pspd[i] / 60);
        ppos[i].x += (ptgt[i].x - ppos[i].x) * k;
        ppos[i].y += (ptgt[i].y - ppos[i].y) * k;
        if (ppos[i].y > wpos.y + wsz.y) {
            ppos[i].x = 0;
            ppos[i].y = 0;
        }
        dl->AddCircleFilled(ppos[i], prad[i], fade(IM_COL32(255, 255, 255, 255)));
    }
    dl->PopClipRect();
}

void render() {
    loading::render();
    if (!loading::done()) return;
    watermark();
    popouts::render_all();

    float open = g_cfg.menuOpen.load() ? 1.0f : 0.0f;
    float dt = ImGui::GetIO().DeltaTime;
    float rate = open > g_vis ? 16.0f : 12.0f;
    g_vis += (open - g_vis) * (1.0f - expf(-dt * rate));
    if (g_vis <= 0.004f) return;

    if (g_cfg.uiStyle.load() == 1) {
        listui::render(g_vis);
        return;
    }

    const std::vector<tabs::Tab>& all = tabs::list();
    int& cur = tabs::current();
    if (cur < 0 || cur >= (int)all.size()) cur = 0;
    if (g_activeTab < 0 || g_activeTab >= (int)all.size()) g_activeTab = cur;

    g_tabSize = ImLerp(g_tabSize, g_tabOpening ? 160.0f : 0.0f, dt * 12.0f);
    g_arrowRoll = ImLerp(g_arrowRoll, (g_tabOpening && g_tabSize >= 159.0f) ? 1.0f : -1.0f, dt * 12.0f);
    float openT = ImClamp(g_tabSize / 160.0f, 0.0f, 1.0f);

    const float totalW = 900.0f + g_tabSize, totalH = 580.0f;
    const float sidebarW = 60.0f + g_tabSize;
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize(ImVec2(totalW, totalH), ImGuiCond_Always);
    ImVec2 pos((io.DisplaySize.x - totalW) * 0.5f, (io.DisplaySize.y - totalH) * 0.5f);
    ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_vis);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::Begin("##oar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);
    s_mainWin = ImGui::GetCurrentWindow();

    ImVec2 wpos = ImGui::GetWindowPos(), wsz = ImGui::GetWindowSize();
    g_lastPos = wpos; g_lastSize = wsz; g_hasRect = true;
    draw_shadow(wpos, wsz, 6.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(wpos, ImVec2(wpos.x + totalW, wpos.y + totalH),
        fade(IM_COL32(13, 13, 13, 242)), 6.0f);
    dl->AddRect(wpos, ImVec2(wpos.x + totalW, wpos.y + totalH),
        fade(IM_COL32(77, 77, 77, 255)), 6.0f);
    dl->AddRectFilled(wpos, ImVec2(wpos.x + sidebarW, wpos.y + totalH),
        fade(IM_COL32(20, 20, 20, 255)), 6.0f, ImDrawFlags_RoundCornersLeft);
    dl->AddRectFilled(wpos, ImVec2(wpos.x + totalW, wpos.y + 60.0f),
        fade(IM_COL32(26, 26, 26, 255)), 6.0f, ImDrawFlags_RoundCornersTop);
    {
        ImVec4 acc = theme::accentA(1.0f);
        dl->AddLine(ImVec2(wpos.x, wpos.y + 60.0f), ImVec2(wpos.x + totalW, wpos.y + 60.0f),
            fade(IM_COL32((int)(acc.x * 255.0f), (int)(acc.y * 255.0f), (int)(acc.z * 255.0f), 255)));
        dl->AddLine(ImVec2(wpos.x + sidebarW, wpos.y), ImVec2(wpos.x + sidebarW, wpos.y + totalH),
            fade(IM_COL32((int)(acc.x * 255.0f), (int)(acc.y * 255.0f), (int)(acc.z * 255.0f), 255)));
    }
    {
        const char* title = g_tabSize > 100.0f ? "OAR" : "OAR Cheat";
        ImFont* tf = tab_text2 ? tab_text2 : NULL;
        ImVec2 tsz = tf ? tf->CalcTextSizeA(24.0f, 10000.0f, 0.0f, title) : ImGui::CalcTextSize(title);
        ImVec2 tp;
        if (g_tabSize > 100.0f)
            tp = ImVec2(wpos.x + (sidebarW - tsz.x) * 0.5f, wpos.y + (60.0f - tsz.y) * 0.5f - 2.0f);
        else
            tp = ImVec2(wpos.x + (totalW - tsz.x) * 0.5f, wpos.y + (60.0f - tsz.y) * 0.5f - 2.0f);
        if (tf)
            dl->AddText(tf, 24.0f, tp, fade(IM_COL32(255, 255, 255, 255)), title);
        else
            dl->AddText(tp, fade(IM_COL32(255, 255, 255, 255)), title);
    }
    dl->AddRectFilled(ImVec2(wpos.x, wpos.y + 550.0f), ImVec2(wpos.x + totalW, wpos.y + totalH),
        fade(IM_COL32(20, 20, 20, 255)), 0.0f);
    {
        char uid[64];
        snprintf(uid, sizeof(uid), "Uid:%s", steamServiceHelper::id_text());
        if (tab_text3)
            dl->AddText(tab_text3, 16, ImVec2(10 + wpos.x, 553 + wpos.y), fade(IM_COL32(255, 255, 255, 255)), uid);
        else
            dl->AddText(ImVec2(10 + wpos.x, 553 + wpos.y), fade(IM_COL32(255, 255, 255, 255)), uid);
#ifdef NDEBUG
        const char* rel = "Release";
#else
        const char* rel = "Developer";
#endif
        ImVec2 rsz = ImGui::CalcTextSize(rel);
        if (tab_text3)
            dl->AddText(tab_text3, 16, ImVec2(wpos.x + totalW - rsz.x - 12.0f, 553 + wpos.y), fade(IM_COL32(255, 255, 255, 255)), rel);
        else
            dl->AddText(ImVec2(wpos.x + totalW - rsz.x - 12.0f, 553 + wpos.y), fade(IM_COL32(255, 255, 255, 255)), rel);
    }

    ImGui::PushClipRect(ImVec2(wpos.x, wpos.y), ImVec2(wpos.x + sidebarW, wpos.y + totalH), true);
    ImGui::SetCursorPos(ImVec2(10, 10));
    {
        ImVec2 bpos = ImGui::GetCursorScreenPos();
        ImVec2 bsz(40, 40);
        if (widgets::os_circle_toggle("##rail_toggle", bsz))
            g_tabOpening = !g_tabOpening;
        ImVec2 center(bpos.x + 20.0f, bpos.y + 20.0f);
        void* av = steamServiceHelper::avatar();
        if (av)
            dl->AddImageRounded(av, bpos, ImVec2(bpos.x + 40.0f, bpos.y + 40.0f), ImVec2(0, 0), ImVec2(1, 1), fade(IM_COL32_WHITE), 20.0f, ImDrawFlags_RoundCornersAll);
        else
            widgets::avatar_tile(bpos, ImVec2(40, 40));
        float dir = g_tabOpening ? -1.0f : 1.0f;
        ImVec2 cc(center.x + 26.0f, center.y);
        dl->AddLine(ImVec2(cc.x - 4.0f * dir, cc.y - 6.0f), ImVec2(cc.x + 2.0f * dir, cc.y), fade(IM_COL32(255, 255, 255, 220)), 2.0f);
        dl->AddLine(ImVec2(cc.x + 2.0f * dir, cc.y), ImVec2(cc.x - 4.0f * dir, cc.y + 6.0f), fade(IM_COL32(255, 255, 255, 220)), 2.0f);
    }
    ImGui::SetCursorPos(ImVec2(5, 70));
    ImGui::BeginGroup();
    {
        for (int s = 0; s < 8; ++s) {
            int ri = ordered_index(all, s);
            if (ri < 0) continue;
            const char* hid = all[ri].id ? all[ri].id : "?";
            if (widgets::os_tab(cur == ri, tab_glyph(hid), tab_title(hid), tab_hint(hid), ImVec2(40, 40), openT))
                cur = ri;
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
        }
        ImGui::Dummy(ImVec2(0.0f, 100.0f));
    }
    ImGui::EndGroup();
    ImGui::PopClipRect();
    if (g_tabSize > 100.0f) {
        float footer_y = wpos.y + 500.0f;
        float left_x = wpos.x + 20.0f;
        char fpsb[32];
        snprintf(fpsb, sizeof(fpsb), "%d FPS", (int)io.Framerate);
        dl->AddText(ImVec2(left_x, footer_y), fade(IM_COL32(204, 204, 204, 255)), "Menu Stats");
        dl->AddText(ImVec2(left_x, footer_y + 15.0f), fade(IM_COL32(255, 255, 255, 255)), fpsb);
        char tabb[32];
        snprintf(tabb, sizeof(tabb), "%d Tabs", (int)all.size());
        dl->AddText(ImVec2(left_x, footer_y + 30.0f), fade(IM_COL32(255, 255, 255, 255)), tabb);
        ImVec4 acc = theme::accentA(1.0f);
        float sepx = left_x + 80.0f;
        dl->AddLine(ImVec2(sepx, footer_y), ImVec2(sepx, footer_y + 45.0f),
            fade(IM_COL32((int)(acc.x * 255.0f), (int)(acc.y * 255.0f), (int)(acc.z * 255.0f), 140)), 1.0f);
        float gx = sepx + 10.0f;
        dl->AddText(ImVec2(gx, footer_y), fade(IM_COL32(204, 204, 204, 255)), "OAR Stats");
        std::string wun = cached_user();
        {
            auto p = wun.find("##");
            if (p != std::string::npos) wun.erase(p);
        }
        char ubuf[20];
        snprintf(ubuf, sizeof(ubuf), "%.12s", wun.c_str());
        dl->AddText(ImVec2(gx, footer_y + 15.0f), fade(IM_COL32(255, 255, 255, 255)), ubuf);
        bool espOn = g_cfg.espEnabled.load();
        dl->AddText(ImVec2(gx, footer_y + 30.0f), fade(espOn ? IM_COL32(0, 255, 0, 255) : IM_COL32(110, 110, 110, 255)), espOn ? "ESP On" : "ESP Off");
    } else {
        std::string wun = cached_user();
        {
            auto p = wun.find("##");
            if (p != std::string::npos) wun.erase(p);
        }
        char ubuf[24];
        snprintf(ubuf, sizeof(ubuf), "%.12s", wun.c_str());
        dl->AddText(ImVec2(wpos.x + 66.0f, wpos.y + 14.0f), fade(IM_COL32(255, 255, 255, 255)), ubuf);
        dl->AddText(ImVec2(wpos.x + 66.0f, wpos.y + 30.0f), fade(IM_COL32(24, 255, 160, 255)), "Lifetime");
    }

    if (cur != g_activeTab) {
        g_activeTab = cur;
        g_tabAlpha = 0.0f;
    }
    g_tabAlpha = ImClamp(g_tabAlpha + (4.0f * dt), 0.0f, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_tabAlpha * ImGui::GetStyle().Alpha);

    {
        ImGuiContext& gg = *GImGui;
        if (gg.OpenPopupStack.Size > 0) {
            bool inPopup = false;
            for (ImGuiWindow* ww = gg.HoveredWindow; ww; ww = ww->ParentWindow)
                if (ww->Flags & ImGuiWindowFlags_Popup) { inPopup = true; break; }
            if (!inPopup) { io.MouseWheel = 0.0f; io.MouseWheelH = 0.0f; }
        }
    }
    float contentX = sidebarW + 10.0f;
    float contentW = totalW - sidebarW - 20.0f;
    ImGui::SetCursorPos(ImVec2(contentX, 70));
    s_railWin = s_mainWin;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("ContentArea", ImVec2(contentW, 470), false);
    s_genWin = ImGui::GetCurrentWindow();
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
        if (!all.empty() && g_activeTab >= 0 && g_activeTab < (int)all.size() && all[g_activeTab].draw) all[g_activeTab].draw();
        ImGui::PopStyleColor(1);
        ImGui::Spacing();
        ImGui::EndChild();
        ImGui::PopStyleColor(1);
    }
    if (io.MouseWheel != 0.0f && s_genWin) {
        ImGuiContext& gg = *GImGui;
        ImGuiWindow* hov = gg.HoveredWindow;
        if (hov == s_mainWin || hov == s_railWin) {
            float step = 3.0f * ImGui::GetFontSize();
            float ny = s_genWin->Scroll.y - io.MouseWheel * step;
            if (ny < 0.0f) ny = 0.0f;
            if (ny > s_genWin->ScrollMax.y) ny = s_genWin->ScrollMax.y;
            s_genWin->Scroll.y = ny;
        }
    }
    if (g_cfg.particles.load())
        particles(wpos, wsz);
    ImGui::PopStyleVar(1);

    ImGui::End();
    ImGui::PopStyleVar(3);
}

}

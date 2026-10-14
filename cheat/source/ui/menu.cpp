#include "pch/pch.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "ui/menu.hpp"
#include "ui/takerender.hpp"
#include "ui/tabs.hpp"
#include "ui/widgets.hpp"
#include "ui/take_shell.hpp"
#include "features/esp.hpp"
#include "features/entities.hpp"
#include "imgui/addons/imgui_notification.h"
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
#include <ctime>

#include "../../vendor/imgui/imgui.h"
#include "../../vendor/imgui/imgui_internal.h"

namespace ui {

void apply_modern_style() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    style.WindowRounding = 10;
    style.ChildRounding = 8;
    style.FrameRounding = 6;
    style.GrabRounding = 6;
    style.PopupRounding = 8;
    style.ScrollbarSize = 8.0f;
    style.GrabMinSize = 8.0f;
    style.WindowBorderSize = 1;
    style.FrameBorderSize = 0;
    style.PopupBorderSize = 1;
    style.WindowPadding = ImVec2(14, 14);
    style.ItemSpacing = ImVec2(8, 8);
    style.ItemInnerSpacing = ImVec2(6, 6);
    style.FramePadding = ImVec2(8, 6);
    style.CellPadding = ImVec2(0.0f, 0.0f);
    style.Colors[ImGuiCol_WindowBg] = ImAdd::HexToColorVec4(PAL_BG_MAIN, 0.94f);
    style.Colors[ImGuiCol_ChildBg] = ImAdd::HexToColorVec4(PAL_BG_MAIN, 0.88f);
    style.Colors[ImGuiCol_Header] = ImAdd::HexToColorVec4(PAL_FRAME_HOV);
    style.Colors[ImGuiCol_HeaderHovered] = ImAdd::HexToColorVec4(PAL_FRAME_HOV);
    style.Colors[ImGuiCol_HeaderActive] = ImAdd::HexToColorVec4(PAL_FRAME_ACT);
    style.Colors[ImGuiCol_PopupBg] = ImAdd::HexToColorVec4(PAL_BG_CHILD);
    style.Colors[ImGuiCol_CheckMark] = ImAdd::HexToColorVec4(PAL_TEXT);
    style.Colors[ImGuiCol_Text] = ImAdd::HexToColorVec4(PAL_TEXT);
    style.Colors[ImGuiCol_TextDisabled] = ImAdd::HexToColorVec4(PAL_TEXT_DIM);
    style.Colors[ImGuiCol_SliderGrab] = ImAdd::HexToColorVec4(PAL_ACCENT);
    style.Colors[ImGuiCol_SliderGrabActive] = ImAdd::HexToColorVec4(PAL_ACCENT_ACT);
    style.Colors[ImGuiCol_SeparatorHovered] = style.Colors[ImGuiCol_SliderGrab];
    style.Colors[ImGuiCol_SeparatorActive] = style.Colors[ImGuiCol_SliderGrabActive];
    style.Colors[ImGuiCol_BorderShadow] = ImAdd::HexToColorVec4(0x000000, 0.00f);
    style.Colors[ImGuiCol_Border] = ImAdd::HexToColorVec4(PAL_BORDER);
    style.Colors[ImGuiCol_Separator] = style.Colors[ImGuiCol_Border];
    style.Colors[ImGuiCol_Button] = ImAdd::HexToColorVec4(PAL_FRAME);
    style.Colors[ImGuiCol_ButtonHovered] = ImAdd::HexToColorVec4(PAL_FRAME_HOV);
    style.Colors[ImGuiCol_ButtonActive] = ImAdd::HexToColorVec4(PAL_FRAME_ACT);
    style.Colors[ImGuiCol_FrameBg] = style.Colors[ImGuiCol_Button];
    style.Colors[ImGuiCol_FrameBgHovered] = style.Colors[ImGuiCol_ButtonHovered];
    style.Colors[ImGuiCol_FrameBgActive] = style.Colors[ImGuiCol_ButtonActive];
    style.Colors[ImGuiCol_ScrollbarBg] = ImAdd::HexToColorVec4(0x000000, 0.0f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImAdd::HexToColorVec4(PAL_BORDER_HI);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImAdd::HexToColorVec4(PAL_FRAME_ACT);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = style.Colors[ImGuiCol_SliderGrab];
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4();
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4();
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4();
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4();
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4();
}

static ImVec2 g_lastPos(0, 0), g_lastSize(880, 580);
static bool g_hasRect = false;

static void fallback_panel(ImVec2 pos, ImVec2 size) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
        IM_COL32(16, 16, 16, 244), 0.0f);
}

void render_backdrop() {
    ImVec2 pos = g_hasRect ? g_lastPos
        : ImVec2((ImGui::GetIO().DisplaySize.x - g_lastSize.x) * 0.5f,
                 (ImGui::GetIO().DisplaySize.y - g_lastSize.y) * 0.5f);
    ImVec2 size = g_lastSize;
    blur::set_radius(g_cfg.blurAmount.load());
    blur::set_alpha(1.0f);
    bool ok = false;
    if (g_cfg.blurBackdrop.load())
        ok = blur::backdrop_region(dx11::swapchain(), pos.x, pos.y, size.x, size.y);
    if (!ok) {
        fallback_panel(pos, size);
    } else {
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            IM_COL32(8, 10, 18, 120), 0.0f);
    }
}

static int ordered_index(const std::vector<tabs::Tab>& all, int slot) {
    static const char* want[10] = { "home", "esp", "shop", "world", "player", "lobby", "lua", "misc", "settings", "loadout" };
    if (slot < 0 || slot >= 10) return -1;
    for (int i = 0; i < (int)all.size(); ++i)
        if (all[i].id && !strcmp(all[i].id, want[slot])) return i;
    return -1;
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
    if (!strcmp(id, "lua")) return "Scripts + executor";
    if (!strcmp(id, "misc")) return "Music + objects";
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
    if (!strcmp(id, "lua")) return "Lua";
    if (!strcmp(id, "misc")) return "Misc";
    return "?";
}

static float g_tabSize = 160.0f;
static float g_arrowRoll = 1.0f;
static bool g_tabOpening = true;
static int g_activeTab = 0;
static ImGuiWindow* s_mainWin = nullptr;
static ImGuiWindow* s_railWin = nullptr;
static ImGuiWindow* s_genWin = nullptr;

static float g_menuAnim = 0.0f;
static ImVec2 g_menuCenter;
static bool g_menuCenterInit = false;

static std::string header_name() {
    std::string wun = steamServiceHelper::name();
    auto p = wun.find("##");
    if (p != std::string::npos) wun.erase(p);
    std::string clean;
    clean.reserve(wun.size());
    for (unsigned char c : wun)
        if (c >= 32 && c < 127) clean.push_back((char)c);
    if (!clean.empty()) wun.swap(clean);
    else wun = "player";
    if (wun.empty()) wun = "Local";
    if (wun.size() > 24) wun.resize(24);
    return wun;
}

void render() {
    loading::render();
    if (!loading::done()) return;
    takerender::visuals();
    ImNotify::Update();
    ImNotify::Render();

    ImGuiIO& io = ImGui::GetIO();
    float dt = io.DeltaTime;
    {
        float d = (dt > 0.0f && dt < 0.25f) ? dt : 0.016f;
        float target = g_cfg.menuOpen.load() ? 1.0f : 0.0f;
        float step = d * 5.0f;
        if (g_menuAnim < target) g_menuAnim = (std::min)(target, g_menuAnim + step);
        else if (g_menuAnim > target) g_menuAnim = (std::max)(target, g_menuAnim - step);
        if (!g_cfg.menuOpen.load() && g_menuAnim <= 0.0f) return;
    }

    const std::vector<tabs::Tab>& all = tabs::list();
    int& cur = tabs::current();
    if (cur < 0 || cur >= (int)all.size()) cur = 0;
    if (g_activeTab < 0 || g_activeTab >= (int)all.size()) g_activeTab = cur;

    g_tabSize = ImLerp(g_tabSize, g_tabOpening ? 162.0f : 0.0f, dt * 12.0f);
    g_arrowRoll = ImLerp(g_arrowRoll, (g_tabOpening && g_tabSize >= 159.0f) ? 1.0f : -1.0f, dt * 12.0f);

    const float windowW = 900.0f;
    const float windowH = 580.0f;
    const float headerH = 60.0f;
    const float sidebarW = 62.0f + g_tabSize;
    float animScale = 0.92f + 0.08f * g_menuAnim;
    if (!g_menuCenterInit) { g_menuCenter = io.DisplaySize * 0.5f; g_menuCenterInit = true; }

    ImGui::SetNextWindowSize(ImVec2(windowW * animScale, windowH * animScale), ImGuiCond_Always);
    ImGui::SetNextWindowPos(g_menuCenter, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("##oar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(2);
    s_mainWin = ImGui::GetCurrentWindow();

    ImVec2 wpos = ImGui::GetWindowPos(), wsz = ImGui::GetWindowSize();
    g_menuCenter = ImVec2(wpos.x + wsz.x * 0.5f, wpos.y + wsz.y * 0.5f);
    g_lastPos = wpos; g_lastSize = wsz; g_hasRect = true;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float rounding = 10.0f;
    dl->AddRectFilled(wpos + ImVec2(0, 4), wpos + wsz + ImVec2(0, 4), (IM_COL32(0, 0, 0, 110)), rounding);
    dl->AddRectFilled(wpos, wpos + wsz, (ImGui::GetColorU32(ImGuiCol_WindowBg)), rounding);
    dl->AddRect(wpos, wpos + wsz, (ImGui::GetColorU32(ImGuiCol_Border)), rounding, 0, 1.0f);
    {
        ImVec4 acc = takeui::accentA(1.0f);
        ImU32 ac = (ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 1.0f)));
        dl->AddRectFilled(ImVec2(wpos.x + 10.0f, wpos.y), ImVec2(wpos.x + wsz.x - 10.0f, wpos.y + 2.0f), ac, 1.0f);
        dl->AddRectFilled(ImVec2(wpos.x + 22.0f, wpos.y + 2.0f), ImVec2(wpos.x + wsz.x - 22.0f, wpos.y + 3.0f),
            (ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 55.0f / 255.0f))), 1.0f);
    }
    ImRect headerBb(wpos, wpos + ImVec2(wsz.x, headerH));
    dl->AddRectFilled(headerBb.Min, headerBb.Max, (IM_COL32(24, 28, 34, 255)), rounding, ImDrawFlags_RoundCornersTop);
    dl->AddLine(ImVec2(headerBb.Min.x, headerBb.Max.y - 0.5f), ImVec2(headerBb.Max.x, headerBb.Max.y - 0.5f),
        (ImGui::GetColorU32(ImGuiCol_Border)), 1.0f);
    {
        ImVec4 acc = takeui::accentA(1.0f);
        ImU32 ac = (ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 1.0f)));
        dl->AddRectFilled(ImVec2(headerBb.Min.x + 10.0f, headerBb.Max.y - 0.5f),
            ImVec2(headerBb.Max.x - 10.0f, headerBb.Max.y + 1.0f), ac, 1.0f);
    }

    ImRect sidebarBb(wpos + ImVec2(0, headerH), wpos + ImVec2(sidebarW, wsz.y));
    dl->AddRectFilled(sidebarBb.Min, sidebarBb.Max, (ImGui::GetColorU32(ImVec4(0.063f, 0.071f, 0.086f, 1.0f))), rounding, ImDrawFlags_RoundCornersBottomLeft);
    dl->AddLine(ImVec2(sidebarBb.Max.x - 0.5f, sidebarBb.Min.y), ImVec2(sidebarBb.Max.x - 0.5f, sidebarBb.Max.y),
        (ImGui::GetColorU32(ImGuiCol_Border)), 1.0f);
    {
        ImVec4 acc = takeui::accentA(1.0f);
        ImU32 ac = (ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 1.0f)));
        ImU32 ach = (ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.55f)));
        float logoSz = 30.0f;
        ImVec2 logoMin(headerBb.Min.x + 16.0f, headerBb.Min.y + (headerH - logoSz) * 0.5f);
        ImVec2 logoMax(logoMin + ImVec2(logoSz, logoSz));
        ImVec2 ctr = (logoMin + logoMax) * 0.5f;
        dl->AddCircleFilled(ctr, 17.0f, (ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 40.0f / 255.0f))), 32);
        dl->AddCircleFilled(ctr, 15.0f, (IM_COL32(16, 20, 26, 255)), 32);
        void* av = steamServiceHelper::avatar();
        if (av)
            dl->AddImageRounded(av, logoMin, logoMax, ImVec2(0, 0), ImVec2(1, 1), (IM_COL32_WHITE), 15.0f, ImDrawFlags_RoundCornersAll);
        else {
            std::string wun = header_name();
            char ch[2] = { "?" };
            if (!wun.empty()) ch[0] = (char)toupper((unsigned char)wun[0]);
            ImVec2 msz = ImGui::CalcTextSize(ch);
            dl->AddText(ctr - msz * 0.5f - ImVec2(0, 1), (IM_COL32(233, 239, 244, 255)), ch);
        }
        dl->AddCircle(ctr, 15.0f, ach, 32, 1.5f);
        {
            ImFont* tf = ImGui::GetFont();
            const float tsz = ImGui::GetFontSize() * 1.6f;
            const char* t1 = "O";
            const char* t2 = "AR";
            ImVec2 s1 = tf->CalcTextSizeA(tsz, FLT_MAX, 0.0f, t1);
            ImVec2 s2 = tf->CalcTextSizeA(tsz, FLT_MAX, 0.0f, t2);
            float tw = s1.x + s2.x;
            float tx = headerBb.Min.x + (headerBb.GetWidth() - tw) * 0.5f;
            float ty = headerBb.Min.y + (headerH - tsz) * 0.5f;
            dl->AddText(tf, tsz, ImVec2(tx, ty), ac, t1);
            dl->AddText(tf, tsz, ImVec2(tx + s1.x, ty), (IM_COL32(255, 255, 255, 255)), t2);
        }
        ImGui::SetCursorScreenPos(logoMin - ImVec2(5, 5));
        if (ImGui::InvisibleButton("##sidebar_toggle", ImVec2(40, 40))) g_tabOpening = !g_tabOpening;
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        std::string wun = header_name();
        const char* nm = wun.c_str();
        ImVec2 np = ImVec2(logoMax.x + 10.0f, headerBb.Min.y + 10.0f);
        ImVec2 ns = ImGui::CalcTextSize(nm);
        float nx = ImMin(np.x, headerBb.Max.x - ns.x - 14.0f);
        dl->AddText(ImVec2(nx, np.y), (IM_COL32(233, 239, 244, 255)), nm);
        dl->AddText(ImVec2(nx, np.y + ImGui::GetFontSize() + 1.0f), ac, "Lifetime");
    }
    if (g_cfg.menuOpen.load() && g_menuAnim > 0.9f) {
        static bool hdrag = false;
        static ImVec2 doff(0.0f, 0.0f);
        bool inHead = io.MousePos.x >= headerBb.Min.x && io.MousePos.x <= headerBb.Max.x &&
            io.MousePos.y >= headerBb.Min.y && io.MousePos.y <= headerBb.Max.y;
        if (!hdrag && io.MouseClicked[0] && inHead && !ImGui::IsAnyItemHovered()) {
            hdrag = true;
            doff = ImVec2(io.MousePos.x - wpos.x, io.MousePos.y - wpos.y);
        }
        if (hdrag) {
            if (io.MouseDown[0]) {
                float dnx = io.MousePos.x - doff.x;
                float dny = io.MousePos.y - doff.y;
                dnx = (std::max)(0.0f, (std::min)(dnx, io.DisplaySize.x - wsz.x));
                dny = (std::max)(0.0f, (std::min)(dny, io.DisplaySize.y - wsz.y));
                g_menuCenter = ImVec2(dnx + wsz.x * 0.5f, dny + wsz.y * 0.5f);
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            } else {
                hdrag = false;
            }
        } else if (inHead && !ImGui::IsAnyItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
    }
    ImGui::SetCursorScreenPos(sidebarBb.Min + ImVec2(10.0f, 10.0f));
    float sideReserve = (g_tabSize > 100.0f) ? 100.0f : 10.0f;
    ImVec2 sideSz = sidebarBb.GetSize() - ImVec2(10.0f, 10.0f + sideReserve);
    if (sideSz.y < 80.0f) sideSz.y = 80.0f;
    if (ImGui::BeginChild("sidebar", sideSz, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground)) {
        for (int s = 0; s < 10; ++s) {
            int ri = ordered_index(all, s);
            if (ri < 0) continue;
            const char* hid = all[ri].id ? all[ri].id : "?";
            if (takeui::TabBtn(cur == ri, s, tab_title(hid), tab_hint(hid), g_tabSize))
                cur = ri;
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
        }
        ImGui::EndChild();
    }
    if (g_tabSize > 100.0f) {
        ImVec4 acc = takeui::accentA(1.0f);
        ImU32 ac = (ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 1.0f)));
        float footerY = sidebarBb.Max.y - 90.0f;
        dl->AddRectFilled(ImVec2(sidebarBb.Min.x, footerY - 10.0f), sidebarBb.Max,
            (ImGui::GetColorU32(ImVec4(0.063f, 0.071f, 0.086f, 1.0f))), rounding, ImDrawFlags_RoundCornersBottomLeft);
        dl->AddLine(ImVec2(sidebarBb.Min.x + 10.0f, footerY - 10.0f), ImVec2(sidebarBb.Max.x - 10.0f, footerY - 10.0f),
            (ImGui::GetColorU32(ImGuiCol_Border)), 1.0f);
        float midx = sidebarBb.Min.x + sidebarBb.GetWidth() * 0.44f;
        int pcn = esp::counts().players;
        char plb[32]; snprintf(plb, sizeof(plb), "Players: %d", pcn);
        dl->AddText(ImVec2(sidebarBb.Min.x + 14, footerY), (IM_COL32(139, 151, 165, 255)), plb);
        char fpsb[32]; snprintf(fpsb, sizeof(fpsb), "%d FPS", (int)io.Framerate);
        dl->AddText(ImVec2(sidebarBb.Min.x + 14, footerY + 19), (IM_COL32(233, 239, 244, 255)), fpsb);
        dl->AddText(ImVec2(sidebarBb.Min.x + 14, footerY + 38), ac, "Lifetime");
        dl->AddLine(ImVec2(midx, footerY - 2), ImVec2(midx, footerY + 62), (IM_COL32(46, 55, 66, 255)), 1.0f);
        char dateb[16]; char timeb[24];
        takeui::date_str(dateb, sizeof(dateb));
        takeui::clock_str(timeb, sizeof(timeb), g_cfg.clock24h.load());
        dl->AddText(ImVec2(midx + 20, footerY), (IM_COL32(233, 239, 244, 255)), dateb);
        dl->AddText(ImVec2(midx + 20, footerY + 19), (IM_COL32(233, 239, 244, 255)), timeb);
#ifdef NDEBUG
        dl->AddText(ImVec2(midx + 20, footerY + 38), ac, "Release");
#else
        dl->AddText(ImVec2(midx + 20, footerY + 38), ac, "Developer");
#endif
    }

    if (cur != g_activeTab)
        g_activeTab = cur;

    {
        ImGuiContext& gg = *GImGui;
        if (gg.OpenPopupStack.Size > 0) {
            bool inPopup = false;
            for (ImGuiWindow* ww = gg.HoveredWindow; ww; ww = ww->ParentWindow)
                if (ww->Flags & ImGuiWindowFlags_Popup) { inPopup = true; break; }
            if (!inPopup) { io.MouseWheel = 0.0f; io.MouseWheelH = 0.0f; }
        }
    }
    ImRect contentBb(ImVec2(sidebarBb.Max.x + 1.0f, headerBb.Max.y), wpos + wsz - ImVec2(10.0f, 10.0f));
    dl->AddRect(contentBb.Min, contentBb.Max, (ImGui::GetColorU32(ImGuiCol_Border)), ImGui::GetStyle().ChildRounding, 0, 1.0f);
    ImGui::SetCursorScreenPos(contentBb.Min);
    s_railWin = s_mainWin;
    static float s_tabFade = 1.0f;
    static int s_lastTab = -1;
    if (s_lastTab != cur) { s_lastTab = cur; s_tabFade = 0.0f; }
    s_tabFade = ImMin(1.0f, s_tabFade + io.DeltaTime * 5.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    s_genWin = nullptr;
    if (ImGui::BeginChild("content", contentBb.GetSize(), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground)) {
        s_genWin = ImGui::GetCurrentWindow();
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImMax(s_tabFade, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
        if (!all.empty() && g_activeTab >= 0 && g_activeTab < (int)all.size() && all[g_activeTab].draw) all[g_activeTab].draw();
        ImGui::PopStyleColor(1);
        ImGui::PopStyleVar(1);
        ImGui::Spacing();
        ImGui::EndChild();
    }
    ImGui::PopStyleColor(1);
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
        takeui::particles();
    if (g_cfg.borderFx.load()) {
        float now = (float)ImGui::GetTime();
        ImVec4 acc = takeui::accentA(1.0f);
        ImVec4 fx1 = acc, fx2 = acc;
        if (g_cfg.borderRainbow.load()) {
            float h = fmodf(now * 0.18f, 1.0f);
            float r, g, b;
            ImGui::ColorConvertHSVtoRGB(h, 0.75f, 1.0f, r, g, b);
            fx1 = ImVec4(r, g, b, 1.0f);
            float h2 = h + 0.5f;
            if (h2 >= 1.0f) h2 -= 1.0f;
            ImGui::ColorConvertHSVtoRGB(h2, 0.75f, 1.0f, r, g, b);
            fx2 = ImVec4(r, g, b, 1.0f);
        }
        float R = rounding, W = wsz.x, H = wsz.y;
        float bh = W - 2.0f * R, bv = H - 2.0f * R;
        float arc = 3.14159265f * R * 0.5f;
        float total = 2.0f * bh + 2.0f * bv + 4.0f * arc;
        if (total > 1.0f && bh > 0.0f && bv > 0.0f) {
            auto bpoint = [&](float dist) -> ImVec2 {
                float d = fmodf(dist, total);
                if (d < 0.0f) d += total;
                if (d < bh) return ImVec2(wpos.x + R + d, wpos.y);
                d -= bh;
                if (d < arc) { float t = d / R - 3.14159265f * 0.5f; return ImVec2(wpos.x + W - R + cosf(t) * R, wpos.y + R + sinf(t) * R); }
                d -= arc;
                if (d < bv) return ImVec2(wpos.x + W, wpos.y + R + d);
                d -= bv;
                if (d < arc) { float t = d / R; return ImVec2(wpos.x + W - R + cosf(t) * R, wpos.y + H - R + sinf(t) * R); }
                d -= arc;
                if (d < bh) return ImVec2(wpos.x + W - R - d, wpos.y + H);
                d -= bh;
                if (d < arc) { float t = d / R + 3.14159265f * 0.5f; return ImVec2(wpos.x + R + cosf(t) * R, wpos.y + H - R + sinf(t) * R); }
                d -= arc;
                if (d < bv) return ImVec2(wpos.x, wpos.y + H - R - d);
                d -= bv;
                float t = d / R + 3.14159265f;
                return ImVec2(wpos.x + R + cosf(t) * R, wpos.y + R + sinf(t) * R);
            };
            float head1 = now * 220.0f;
            float head2 = head1 + total * 0.5f;
            float alen = 220.0f;
            int steps = 18;
            for (int k = 0; k < 2; k++) {
                float head = (k == 0) ? head1 : head2;
                ImVec4 base = (k == 0) ? fx1 : fx2;
                ImVec2 px = bpoint(head);
                for (int j = 1; j <= steps; j++) {
                    ImVec2 q = bpoint(head - (float)j * (alen / (float)steps));
                    float f = 1.0f - (float)j / (float)(steps + 1);
                    dl->AddLine(px, q, (ImGui::GetColorU32(ImVec4(base.x, base.y, base.z, base.w * f * 0.35f))), 12.0f);
                    dl->AddLine(px, q, (ImGui::GetColorU32(ImVec4(base.x, base.y, base.z, base.w * f * 0.9f))), 5.5f);
                    dl->AddLine(px, q, (ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, base.w * f * 0.6f))), 2.0f);
                    px = q;
                }
            }
        }
    }
    ImGui::End();
}

}

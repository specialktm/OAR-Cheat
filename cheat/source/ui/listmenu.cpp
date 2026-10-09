#include "pch/pch.h"
#include "ui/listmenu.hpp"
#include "config/config.hpp"
#include "features/manager.hpp"
#include "features/shop.hpp"
#include "features/exploits.hpp"
#include "features/esp.hpp"
#include "world/actor_iter.hpp"
#include "ui/theme.hpp"
#include "SDK/Basic.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/PlayerCharacter_classes.hpp"
#include "SDK/RobberController_classes.hpp"

#include <cmath>
#include "../../vendor/imgui/imgui.h"

namespace listui {

enum Kind { K_TOGGLE, K_ACTION, K_SUB, K_CYCLE, K_SLIDER, K_INFO };
struct Row {
    const char* label = "";
    int kind = K_INFO;
    std::atomic<bool>* flag = nullptr;
    const char* action = "";
    int sub = -1;
    const char** opts = nullptr;
    int nopts = 0;
    std::atomic<int>* opt = nullptr;
    std::atomic<float>* fval = nullptr;
    float fmin = 0, fmax = 0, fstep = 1;
    std::string info;
};
struct Menu { const char* title = ""; std::vector<Row> rows; };

static const char* kBoxT[3] = { "2D full", "Corners", "3D box" };
static const char* kAnch[8] = { "Top", "Top-right", "Right", "Bot-right", "Bottom", "Bot-left", "Left", "Top-left" };
static const char* kNameT[2] = { "Full name", "Short name" };
static const char* kHpT[3] = { "Bar + number", "Bar only", "Number only" };
static const char* kUiT[2] = { "Mouse", "List" };

static std::vector<const char*> g_themeNames;
static std::vector<Menu>& menus() {
    static std::vector<Menu> ms;
    static bool built = false;
    if (!built) {
        built = true;
        for (int i = 0; i < theme::count(); ++i) g_themeNames.push_back(theme::get(i).name);
        Menu m;
        m.title = "Main";
        m.rows = {
            { "Visuals", K_SUB, nullptr, "", 1 },
            { "Shop", K_SUB, nullptr, "", 2 },
            { "World", K_SUB, nullptr, "", 3 },
            { "Players", K_SUB, nullptr, "", 4 },
            { "Config", K_SUB, nullptr, "", 5 },
        };
        ms.push_back(std::move(m));
        Menu v; v.title = "Visuals";
        auto T = [](const char* l, std::atomic<bool>& f) {
            Row r; r.label = l; r.kind = K_TOGGLE; r.flag = &f; return r; };
        auto C = [](const char* l, const char** o, int n, std::atomic<int>& s) {
            Row r; r.label = l; r.kind = K_CYCLE; r.opts = o; r.nopts = n; r.opt = &s; return r; };
        v.rows = {
            T("Master ESP", g_cfg.espEnabled), T("Item ESP", g_cfg.itemEsp),
            T("Money bags", g_cfg.moneyEsp), T("Pickups", g_cfg.pickupEsp),
            T("Item boxes", g_cfg.itemBox), T("Player ESP", g_cfg.playerEsp),
            T("Boxes", g_cfg.playerBox), T("Names", g_cfg.playerName),
            T("Health", g_cfg.playerHealth), T("Distance", g_cfg.playerDistance),
            T("Skeleton", g_cfg.playerSkel), T("Guards", g_cfg.guardEsp),
            T("G boxes", g_cfg.guardBox), T("G state", g_cfg.guardState),
            T("G distance", g_cfg.guardDistance), T("G skeleton", g_cfg.guardSkel),
            T("Box fill", g_cfg.boxFill), C("HP bar", kHpT, 3, g_cfg.healthStyle),
        };
        v.rows.push_back(C("Box type", kBoxT, 3, g_cfg.espBoxStyle));
        v.rows.push_back(C("Anchor", kAnch, 8, g_cfg.espLabelAnchor));
        v.rows.push_back(C("Name", kNameT, 2, g_cfg.nameMode));
        v.rows.push_back(C("HP bar", kHpT, 3, g_cfg.healthStyle));
        ms.push_back(std::move(v));
        Menu s; s.title = "Shop";
        auto A = [](const char* l, const char* id) {
            Row r; r.label = l; r.kind = K_ACTION; r.action = id; return r; };
        s.rows = { A("Unlock maps", "maps"), A("Cheap shop", "cheap_shop"), A("Cheap skills", "cheap_skills") };
        ms.push_back(std::move(s));
        Menu w; w.title = "World";
        w.rows = { A("Vacuum + god", "vacuum"), A("Phones off", "phones"), A("Cameras off", "cameras"),
            A("Unlock doors", "doors"), A("Buff guns", "guns") };
        w.rows.push_back(T("Auto phones", g_cfg.autoPhones));
        w.rows.push_back(T("Auto cameras", g_cfg.autoCameras));
        w.rows.push_back(T("Auto doors", g_cfg.autoDoors));
        w.rows.push_back(T("Godmode", g_cfg.godMode));
        {
            Row r; r.label = "Auto interval"; r.kind = K_SLIDER;
            r.fval = &g_cfg.autoIntervalSec; r.fmin = 1.0f; r.fmax = 30.0f; r.fstep = 1.0f;
            w.rows.push_back(r);
        }
        ms.push_back(std::move(w));
        Menu p; p.title = "Players";
        p.rows = { T("Godmode", g_cfg.godMode) };
        ms.push_back(std::move(p));
        Menu c; c.title = "Config";
        c.rows.push_back(C("UI style", kUiT, 2, g_cfg.uiStyle));
        c.rows.push_back(C("Theme", g_themeNames.data(), (int)g_themeNames.size(), g_cfg.themeIdx));
        c.rows.push_back(T("Watermark", g_cfg.watermark));
        c.rows.push_back(T("24h clock", g_cfg.clock24h));
        c.rows.push_back(T("Blur", g_cfg.blurBackdrop));
        ms.push_back(std::move(c));
    }

    return ms;
}

static std::vector<int> g_stack{ 0 };
static std::vector<int> g_sel{ 0 };

static void queue_action(const char* id) {
    feat::queue_run(id);
}

static bool down_now(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }
static bool nav_key(int vk, uint64_t& last, uint64_t now) {
    if (GetAsyncKeyState(vk) & 1) { last = now; return true; }
    if (down_now(vk) && now - last > 140) { last = now; return true; }
    if (!down_now(vk)) last = 0;
    return false;
}

static int cur_menu() { return g_stack.empty() ? 0 : g_stack.back(); }
static int& cur_sel() {
    while ((int)g_sel.size() < (int)g_stack.size()) g_sel.push_back(0);
    return g_sel[g_stack.size() - 1];
}

static void activate(Row& r) {
    switch (r.kind) {
    case K_TOGGLE:
        if (r.flag) r.flag->store(!r.flag->load());
        break;
    case K_ACTION:
        queue_action(r.action);
        break;
    case K_SUB:
        if (r.sub >= 0) { g_stack.push_back(r.sub); }
        break;
    case K_CYCLE:
        if (r.opt && r.nopts > 0) r.opt->store((r.opt->load() + 1) % r.nopts);
        break;
    default: break;
    }
}

static void adjust(Row& r, int dir) {
    if (r.kind == K_CYCLE && r.opt && r.nopts > 0) {
        int v = r.opt->load() + dir;
        v %= r.nopts; if (v < 0) v += r.nopts;
        r.opt->store(v);
    } else if (r.kind == K_SLIDER && r.fval) {
        float v = r.fval->load() + dir * r.fstep;
        if (v < r.fmin) v = r.fmin;
        if (v > r.fmax) v = r.fmax;
        r.fval->store(v);
    }
}

void handle_keys() {
    uint64_t now = GetTickCount64();
    static uint64_t t8 = 0, t2 = 0, t5 = 0, t0 = 0, t4 = 0, t6 = 0;
    static uint64_t tu = 0, td = 0, te = 0, tl = 0, tbk = 0;
    Menu& m = menus()[cur_menu()];
    int& sel = cur_sel();
    auto clamp_sel = [&] {
        int n = (int)m.rows.size();
        if (n == 0) { sel = 0; return; }
        if (sel < 0) sel = n - 1;
        if (sel >= n) sel = 0;
    };
    clamp_sel();
    if (nav_key(VK_NUMPAD8, t8, now) || nav_key(VK_UP, tu, now)) { sel--; clamp_sel(); }
    if (nav_key(VK_NUMPAD2, t2, now) || nav_key(VK_DOWN, td, now)) { sel++; clamp_sel(); }
    if (!m.rows.empty()) {
        Row& r = m.rows[sel];
        if (nav_key(VK_NUMPAD5, t5, now) || nav_key(VK_RETURN, te, now)) activate(r);
        if (nav_key(VK_NUMPAD4, t4, now) || nav_key(VK_LEFT, tl, now)) adjust(r, -1);
        if (nav_key(VK_NUMPAD6, t6, now)) adjust(r, +1);
    }
    if ((nav_key(VK_NUMPAD0, t0, now) || nav_key(VK_BACK, tbk, now)) && g_stack.size() > 1) {
        g_stack.pop_back();
        if (g_sel.size() > g_stack.size()) g_sel.pop_back();
    }
}

void render(float vis) {
    if (vis <= 0.004f) return;
    Menu& m = menus()[cur_menu()];
    int& sel = cur_sel();
    if (sel < 0 || sel >= (int)m.rows.size()) sel = 0;

    ImVec2 vp = ImGui::GetIO().DisplaySize;
    float dt = ImGui::GetIO().DeltaTime;
    float w = 300, rh = 26;

    int extras = (cur_menu() == 4 ? 1 : 0) + (cur_menu() == 2 ? 1 : 0);
    int n = (int)m.rows.size() + extras;
    float headH = 38, footH = 46;
    float contentH = n * rh;
    float panelH = headH + contentH + footH;
    float maxH = vp.y - 24;
    bool scroll = panelH > maxH;
    if (scroll) panelH = maxH;
    float viewH = panelH - headH - footH;

    static float g_scroll = 0.0f;
    float target = 0.0f;
    if (scroll && contentH > viewH) {
        float top = g_scroll, bottom = g_scroll + viewH;
        float sy = (float)sel * rh;
        if (sy < top + rh) target = sy - rh;
        else if (sy + 2.0f * rh > bottom) target = sy + 2.0f * rh - viewH;
        float lo = 0.0f, hi = contentH - viewH;
        if (target < lo) target = lo;
        if (target > hi) target = hi;
    }
    g_scroll += (target - g_scroll) * (1.0f - expf(-dt * 14.0f));
    if (fabsf(target - g_scroll) < 0.25f) g_scroll = target;

    static float g_hiY = 0.0f;
    float hiTarget = (float)sel * rh - g_scroll;
    g_hiY += (hiTarget - g_hiY) * (1.0f - expf(-dt * 18.0f));
    if (fabsf(hiTarget - g_hiY) < 0.25f) g_hiY = hiTarget;

    float slide = 1.0f - powf(1.0f - vis, 3.0f);
    float x = 24.0f - (1.0f - slide) * (w + 48.0f);
    float y = (vp.y - panelH) * 0.5f;
    if (y < 12) y = 12;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    auto A = [&](int r, int g, int b, int a) {
        return IM_COL32(r, g, b, (int)(a * vis));
    };
    ImVec4 acc = theme::accentA();
    ImU32 accHi = A((int)(acc.x * 255), (int)(acc.y * 255), (int)(acc.z * 255), 255);

    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + panelH),
        A(16, 16, 16, 238), 5.0f);
    dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + panelH),
        A(80, 40, 130, 255), 5.0f, 0, 1.0f);

    char crumb[64];
    if (g_stack.size() > 1)
        snprintf(crumb, sizeof(crumb), "Main > %s", m.title);
    else
        snprintf(crumb, sizeof(crumb), "Main");
    dl->AddText(ImVec2(x + 12, y + 10), A(235, 238, 248, 255), crumb);
    char cnt[16];
    snprintf(cnt, sizeof(cnt), "%d / %d", m.rows.empty() ? 0 : sel + 1,
        (int)m.rows.size());
    ImVec2 csz = ImGui::CalcTextSize(cnt);
    dl->AddText(ImVec2(x + w - csz.x - 12, y + 10), A(235, 238, 248, 255), cnt);
    dl->AddLine(ImVec2(x + 8, y + headH - 6), ImVec2(x + w - 8, y + headH - 6),
        A(80, 40, 130, 140), 1.0f);

    float clipTop = y + headH, clipBot = y + headH + viewH;
    {
        ImVec2 mp = ImGui::GetIO().MousePos;
        bool mIn = (mp.x >= x && mp.x <= x + w && mp.y >= clipTop && mp.y <= clipBot);
        static ImVec2 lastMp(-1, -1);
        if (mIn) {
            int hr = (int)((mp.y - clipTop + g_scroll) / rh);
            if (hr >= 0 && hr < (int)m.rows.size()) {
                if (mp.x != lastMp.x || mp.y != lastMp.y) sel = hr;
                if (ImGui::IsMouseClicked(0)) {
                    sel = hr;
                    if (m.rows[hr].kind == K_SLIDER)
                        adjust(m.rows[hr], +1);
                    else
                        activate(m.rows[hr]);
                }
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }
        }
        lastMp = mp;
    }

    dl->PushClipRect(ImVec2(x, clipTop), ImVec2(x + w, clipBot), true);
    auto rowY = [&](int i) { return clipTop + i * rh - g_scroll; };

    int cover = -1;
    for (int i = 0; i < (int)m.rows.size(); ++i) {
        float ry = i * rh - g_scroll;
        if (g_hiY + rh * 0.5f >= ry && g_hiY + rh * 0.5f < ry + rh) {
            cover = i; break;
        }
    }

    {
        float hy = clipTop + g_hiY;
        dl->AddRectFilled(ImVec2(x + 4, hy),
            ImVec2(x + w - 4, hy + rh - 2),
            A((int)(acc.x * 110), (int)(acc.y * 110), (int)(acc.z * 160), 235), 5.0f);
        dl->AddRect(ImVec2(x + 4, hy),
            ImVec2(x + w - 4, hy + rh - 2),
            accHi, 5.0f, 0, 1.0f);
    }

    for (int i = 0; i < (int)m.rows.size(); ++i) {
        Row& r = m.rows[i];
        float ry = rowY(i);
        if (ry + rh < clipTop || ry > clipBot) continue;
        ImU32 tx = (i == cover) ? A(255, 255, 255, 255) : A(200, 205, 220, 255);
        char right[64] = { 0 };
        if (r.kind == K_TOGGLE && r.flag)
            snprintf(right, sizeof(right), "[%c]", r.flag->load() ? 'x' : ' ');
        else if (r.kind == K_SUB)
            snprintf(right, sizeof(right), ">");
        else if (r.kind == K_CYCLE && r.opt && r.opts && r.nopts > 0) {
            int v = r.opt->load();
            if (v < 0 || v >= r.nopts) v = 0;
            snprintf(right, sizeof(right), "< %s >", r.opts[v]);
        } else if (r.kind == K_SLIDER && r.fval) {
            snprintf(right, sizeof(right), "%.0f", r.fval->load());
        }
        dl->AddText(ImVec2(x + 14, ry + 5), tx, r.label);
        if (right[0]) {
            ImVec2 rsz = ImGui::CalcTextSize(right);
            dl->AddText(ImVec2(x + w - rsz.x - 12, ry + 5), tx, right);
        }
    }

    int idx = (int)m.rows.size();
    if (cur_menu() == 5) {

        static int lastTheme = -1;
        int ti = g_cfg.themeIdx.load();
        if (ti != lastTheme) {
            lastTheme = ti;
            int c = theme::count();
            if (ti >= 0 && ti < c) theme::set_current(ti);
        }
    }
    if (cur_menu() == 4) {
        esp::Counts c = esp::counts();
        char b[64]; snprintf(b, sizeof(b), "seen P%d G%d I%d",
            c.players, c.guards, c.items);
        float ry = rowY(idx);
        if (ry + rh >= clipTop && ry <= clipBot)
            dl->AddText(ImVec2(x + 14, ry + 5), A(140, 200, 150, 255), b);
        ++idx;
    }
    if (cur_menu() == 2) {
        shop::Status st = shop::last_status();
        const char* s = st.note.empty() ? "no run yet" : st.note.c_str();
        char b[96]; snprintf(b, sizeof(b), "%.60s", s);
        float ry = rowY(idx);
        if (ry + rh >= clipTop && ry <= clipBot)
            dl->AddText(ImVec2(x + 14, ry + 5), A(150, 160, 185, 255), b);
        ++idx;
    }

    if (scroll && contentH > viewH) {
        float sbX = x + w - 4;
        dl->AddRectFilled(ImVec2(sbX, clipTop), ImVec2(sbX + 2, clipBot),
            A(30, 34, 50, 200), 0.0f);
        float th = viewH * (viewH / contentH);
        if (th < 18.0f) th = 18.0f;
        float range = viewH - th;
        float frac = g_scroll / (contentH - viewH);
        float ty = clipTop + frac * range;
        dl->AddRectFilled(ImVec2(sbX, ty), ImVec2(sbX + 2, ty + th),
            accHi, 0.0f);
    }
    dl->PopClipRect();

    float fy = y + panelH - footH;
    dl->AddLine(ImVec2(x + 8, fy), ImVec2(x + w - 8, fy),
        A(80, 40, 130, 140), 1.0f);
    dl->AddText(ImVec2(x + 12, fy + 8), A(130, 138, 160, 255),
        "NUM8/2 move  NUM5 select");
    dl->AddText(ImVec2(x + 12, fy + 26), A(130, 138, 160, 255),
        "NUM4/6 adjust  NUM0 back (NUMLOCK ON)");
}

}

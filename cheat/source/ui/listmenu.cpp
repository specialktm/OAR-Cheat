#include "pch/pch.h"
#include "ui/listmenu.hpp"
#include "core/config.hpp"
#include "features/manager.hpp"
#include "features/shop.hpp"
#include "features/exploits.hpp"
#include "features/esp.hpp"
#include "core/actor_iter.hpp"
#include "ui/theme.hpp"
#include "SDK/Basic.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/PlayerCharacter_classes.hpp"
#include "SDK/RobberController_classes.hpp"

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
        v.rows = {
            T("Master ESP", g_cfg.espEnabled), T("Item ESP", g_cfg.itemEsp),
            T("Money bags", g_cfg.moneyEsp), T("Pickups", g_cfg.pickupEsp),
            T("Item boxes", g_cfg.itemBox), T("Player ESP", g_cfg.playerEsp),
            T("Boxes", g_cfg.playerBox), T("Names", g_cfg.playerName),
            T("Health", g_cfg.playerHealth), T("Distance", g_cfg.playerDistance),
            T("Skeleton", g_cfg.playerSkel), T("Guards", g_cfg.guardEsp),
            T("G boxes", g_cfg.guardBox), T("G state", g_cfg.guardState),
            T("G distance", g_cfg.guardDistance), T("G skeleton", g_cfg.guardSkel),
            T("Box fill", g_cfg.boxFill), T("Gradient", g_cfg.healthGradient),
        };
        auto C = [](const char* l, const char** o, int n, std::atomic<int>& s) {
            Row r; r.label = l; r.kind = K_CYCLE; r.opts = o; r.nopts = n; r.opt = &s; return r; };
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
    // dynamic rows (theme list may vary; stats refresh each frame is cheap)
    return ms;
}

// nav state (render thread only: handle_keys + render both run on Present)
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

void render() {
    Menu& m = menus()[cur_menu()];
    int& sel = cur_sel();
    if (sel < 0 || sel >= (int)m.rows.size()) sel = 0;

    ImVec2 vp = ImGui::GetIO().DisplaySize;
    float w = 300, rh = 26;
    // config theme row is dynamic (theme count known at runtime)
    int extraStats = (cur_menu() == 4) ? 1 : 0;
    int shopNote = (cur_menu() == 2) ? 1 : 0;
    int n = (int)m.rows.size() + extraStats + shopNote;
    float h = 64 + n * rh + 64;
    ImVec2 pos(24, (vp.y - h) * 0.5f);
    if (pos.y < 12) pos.y = 12;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), IM_COL32(8, 10, 16, 235), 2.0f);
    dl->AddRect(pos, ImVec2(pos.x + w, pos.y + h), IM_COL32(60, 70, 100, 255), 2.0f, 0, 1.0f);

    char crumb[64];
    if (g_stack.size() > 1)
        snprintf(crumb, sizeof(crumb), "Main > %s", m.title);
    else
        snprintf(crumb, sizeof(crumb), "Main");
    dl->AddText(ImVec2(pos.x + 12, pos.y + 10), IM_COL32(235, 238, 248, 255), crumb);
    char cnt[16];
    snprintf(cnt, sizeof(cnt), "%d / %d", m.rows.empty() ? 0 : sel + 1, (int)m.rows.size());
    ImVec2 csz = ImGui::CalcTextSize(cnt);
    dl->AddText(ImVec2(pos.x + w - csz.x - 12, pos.y + 10), IM_COL32(235, 238, 248, 255), cnt);
    float y = pos.y + 38;

    for (int i = 0; i < (int)m.rows.size(); ++i) {
        Row& r = m.rows[i];
        bool hov = (i == sel);
        if (hov)
            dl->AddRectFilled(ImVec2(pos.x + 4, y), ImVec2(pos.x + w - 4, y + rh - 2),
                IM_COL32(235, 238, 248, 255));
        ImU32 tx = hov ? IM_COL32(10, 12, 18, 255) : IM_COL32(200, 205, 220, 255);
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
        dl->AddText(ImVec2(pos.x + 14, y + 5), tx, r.label);
        if (right[0]) {
            ImVec2 rsz = ImGui::CalcTextSize(right);
            dl->AddText(ImVec2(pos.x + w - rsz.x - 12, y + 5), tx, right);
        }
        y += rh;
    }
    // dynamic extras
    if (cur_menu() == 5) {
        // theme row above stores the index; apply it here on the render thread
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
        char b[64]; snprintf(b, sizeof(b), "seen P%d G%d I%d", c.players, c.guards, c.items);
        dl->AddText(ImVec2(pos.x + 14, y + 5), IM_COL32(140, 200, 150, 255), b);
        y += rh;
    }
    if (cur_menu() == 2) {
        shop::Status st = shop::last_status();
        const char* s = st.note.empty() ? "no run yet" : st.note.c_str();
        char b[96]; snprintf(b, sizeof(b), "%.60s", s);
        dl->AddText(ImVec2(pos.x + 14, y + 5), IM_COL32(150, 160, 185, 255), b);
        y += rh;
    }
    dl->AddText(ImVec2(pos.x + 12, pos.y + h - 40), IM_COL32(130, 138, 160, 255),
        "NUM8/2 move  NUM5 select");
    dl->AddText(ImVec2(pos.x + 12, pos.y + h - 22), IM_COL32(130, 138, 160, 255),
        "NUM4/6 adjust  NUM0 back (NUMLOCK ON)");
}

} // namespace listui

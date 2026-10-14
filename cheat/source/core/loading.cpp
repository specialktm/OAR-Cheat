#include "pch/pch.h"
#include "core/loading.hpp"
#include "config/config.hpp"
#include "render/blur.hpp"
#include "render/dx11_hook.hpp"
#include "ui/widgets.hpp"
#include "ui/take_shell.hpp"

#include <atomic>
#include <cstring>

#include "../../vendor/imgui/imgui.h"

namespace loading {

static std::atomic<bool> g_on{ false };
static std::atomic<int> g_stage{ 0 };
static std::atomic<unsigned long long> g_stageAt{ 0 };
static std::atomic<bool> g_finishing{ false };
static std::atomic<unsigned long long> g_doneAt{ 0 };
static int g_shown = 0;
static unsigned long long g_shownAt = 0;
static unsigned long long g_beginAt = 0;
static char g_labels[6][64] = {
    "Starting", "Signature scan", "Steam profile", "Overlay", "Ready", ""
};
static int g_total = 4;

static unsigned long long now_ms() { return GetTickCount64(); }

void begin() {
    g_stage.store(0);
    g_stageAt.store(now_ms());
    g_shown = 0;
    g_shownAt = now_ms();
    g_beginAt = now_ms();
    g_finishing.store(false);
    g_doneAt.store(0);
    g_on.store(true);
}

void stage(int idx, const char* label) {
    if (idx < 0 || idx >= 6) return;
    if (label && label[0]) {

        strncpy_s(g_labels[idx], label, _TRUNCATE);
    }
    int cur = g_stage.load();
    if (idx > cur) g_stage.store(idx);
    g_stageAt.store(now_ms());
}

void finish() {
    g_stage.store(g_total);
    g_stageAt.store(now_ms());
    g_finishing.store(true);
}

bool visible() {
    if (!g_on.load()) return false;
    unsigned long long done = g_doneAt.load();
    if (!done) return true;

    return now_ms() - done < 900;
}

bool done() { return g_doneAt.load() != 0; }

static float alpha() {
    unsigned long long done = g_doneAt.load();
    if (!done) return 1.0f;
    unsigned long long dt = now_ms() - done;
    if (dt < 500) return 1.0f;
    return dt < 900 ? 1.0f - (float)(dt - 500) / 400.0f : 0.0f;
}

static void tick_shown() {
    int target = g_stage.load();
    if (target > g_total) target = g_total;
    unsigned long long now = now_ms();
    while (g_shown < target && now - g_shownAt >= 500) {
        g_shown++;
        g_shownAt = now;
    }
    if (g_finishing.load() && g_shown >= g_total && g_doneAt.load() == 0)
        g_doneAt.store(now);
}

static bool dismiss_available() {
    if (!g_on.load() || g_doneAt.load() != 0) return false;
    return now_ms() - g_beginAt >= 6000;
}

void render() {
    if (!visible()) return;
    tick_shown();
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 vp = io.DisplaySize;
    float a = alpha();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(vp);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("##oar_load", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleVar(2);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (g_cfg.blurBackdrop.load()) {
        blur::set_alpha(a);
        blur::backdrop(dx11::swapchain());
    }
    dl->AddRectFilled(ImVec2(0, 0), vp, IM_COL32(6, 8, 14, (int)(210 * a)));

    ImVec4 ac = takeui::accentA();
    ImU32 accent = IM_COL32((int)(ac.x * 255), (int)(ac.y * 255), (int)(ac.z * 255),
        (int)(255 * a));
    ImU32 txt = IM_COL32(235, 238, 248, (int)(255 * a));
    ImU32 dim = IM_COL32(140, 148, 170, (int)(220 * a));

    float cx = vp.x * 0.5f, cy = vp.y * 0.5f;

    const char* title = "OAR";
    ImVec2 tsz = ImGui::CalcTextSize(title);
    dl->AddText(ImVec2(cx - tsz.x * 0.5f, cy - 118), txt, title);
    ImVec2 ssz = ImGui::CalcTextSize("injecting");
    dl->AddText(ImVec2(cx - ssz.x * 0.5f, cy - 118 + tsz.y + 6), dim, "injecting");

    int shown = g_shown < g_total ? g_shown : g_total;
    bool finishing = g_doneAt.load() != 0;

    float lx = cx - 130.0f, ly = cy - 44.0f;
    for (int i = 0; i < g_total; ++i) {
        bool done = i < shown;
        bool cur = i == shown && !finishing;
        ImU32 c = done ? accent : cur ? txt : dim;
        char box[48];
        snprintf(box, sizeof(box), "%s  %s", done ? "[x]" : cur ? "[~]" : "[ ]",
            g_labels[i]);
        dl->AddText(ImVec2(lx, ly + i * 22), c, box);
    }

    float bw = 260.0f, bh = 4.0f;
    float bx = cx - bw * 0.5f, by = cy + 58.0f;
    dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + bh),
        IM_COL32(30, 34, 50, (int)(200 * a)), 0.0f);
    float base = (float)shown / (float)g_total;
    float pulse = 0.0f;
    if (!finishing) {
        unsigned long long dt = now_ms() - g_stageAt.load();
        pulse = 0.08f + 0.06f * (float)((dt / 220) % 2);
        if (base + pulse > 1.0f) pulse = 1.0f - base;
    }
    float fill = (base + pulse) * bw;
    if (fill > 1.0f)
        dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + fill, by + bh), accent, 0.0f);

    const char* live = g_labels[shown < 6 ? shown : 5];
    ImVec2 lsz = ImGui::CalcTextSize(live);
    dl->AddText(ImVec2(cx - lsz.x * 0.5f, by + bh + 10), dim, live);

    if (dismiss_available()) {
        ImGuiIO& lio = ImGui::GetIO();
        const float bs = 30.0f;
        ImVec2 bmin(vp.x - bs - 14.0f, 14.0f);
        ImVec2 bmax(vp.x - 14.0f, 14.0f + bs);
        bool hov = lio.MousePos.x >= bmin.x && lio.MousePos.x <= bmax.x &&
            lio.MousePos.y >= bmin.y && lio.MousePos.y <= bmax.y;
        dl->AddRectFilled(bmin, bmax, hov ? IM_COL32(60, 30, 34, (int)(230 * a)) : IM_COL32(24, 28, 34, (int)(230 * a)), 6.0f);
        dl->AddRect(bmin, bmax, IM_COL32(100, 60, 66, (int)(255 * a)), 6.0f);
        ImVec2 xsz = ImGui::CalcTextSize("X");
        dl->AddText(ImVec2((bmin.x + bmax.x - xsz.x) * 0.5f, (bmin.y + bmax.y - xsz.y) * 0.5f),
            IM_COL32(255, 120, 120, (int)(255 * a)), "X");
        if (hov && lio.MouseClicked[0])
            finish();
    }

    ImGui::End();
}

}

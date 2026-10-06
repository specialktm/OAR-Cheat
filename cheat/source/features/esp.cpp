#include "pch/pch.h"
#include "features/esp.hpp"
#include "core/actor_iter.hpp"
#include "core/config.hpp"

#include "SDK/Basic.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/PlayerCharacter_classes.hpp"
#include "SDK/PickupItem_base_classes.hpp"
#include "SDK/Money_base_classes.hpp"
#include "SDK/Item_Keycard_classes.hpp"
#include "SDK/GuardPhone_classes.hpp"
#include "SDK/NPC_Guard_classes.hpp"
#include "SDK/NPCBase_classes.hpp"
#include "SDK/NPCBase_classes.hpp"
#include "core/veh.hpp"

#include <cstring>

#include "../../vendor/imgui/imgui.h"

namespace esp {

static Counts g_counts{};
Counts counts() { return g_counts; }

static ImU32 u32c(ImVec4 c) {
    return IM_COL32((int)(c.x * 255.0f), (int)(c.y * 255.0f), (int)(c.z * 255.0f), 255);
}
static std::string short_name(const std::string& n) {
    auto q = n.find('_');
    return (q == std::string::npos) ? n : n.substr(0, q);
}
static void box_fill(ImDrawList* d, float x0, float y0, float x1, float y1, ImU32 c) {
    d->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), (c & 0x00FFFFFFu) | (38u << 24));
}
static ImU32 lerp_c(ImU32 a, ImU32 b, float k) {
    auto ch = [](ImU32 c, int s) { return (int)((c >> s) & 0xFF); };
    return IM_COL32(ch(a, 0) + (int)((ch(b, 0) - ch(a, 0)) * k),
        ch(a, 8) + (int)((ch(b, 8) - ch(a, 8)) * k),
        ch(a, 16) + (int)((ch(b, 16) - ch(a, 16)) * k), 255);
}
static void health_bar_grad(ImDrawList* d, float x0, float y0, float x1, float y1, float frac) {
    d->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(0, 0, 0, 160));
    const int N = 14;
    ImU32 lo = IM_COL32(255, 70, 70, 255), mid = IM_COL32(255, 200, 60, 255), hi = IM_COL32(90, 255, 120, 255);
    for (int i = 0; i < N; ++i) {
        float f0 = (float)i / N, f1 = (float)(i + 1) / N;
        if (f0 >= frac) break;
        float tp = f1 > frac ? frac : f1;
        float k = (f0 + tp) * 0.5f;
        ImU32 c = k < 0.5f ? lerp_c(lo, mid, k * 2.0f) : lerp_c(mid, hi, (k - 0.5f) * 2.0f);
        d->AddRectFilled(ImVec2(x0, y1 - (y1 - y0) * tp), ImVec2(x1, y1 - (y1 - y0) * f0), c);
    }
}

// ---- SEH leaves (no vector/string locals) ----
static bool seh_viewport(SDK::APlayerController* pc, int& w, int& h) {
    __try { pc->GetViewportSize(&w, &h); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_project(SDK::APlayerController* pc, const SDK::FVector& loc, SDK::FVector2D& scr) {
    __try { return pc->ProjectWorldLocationToScreen(loc, &scr, false); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_loc(SDK::AActor* a, SDK::FVector& out) {
    __try { out = a->K2_GetActorLocation(); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
template<typename T>
static bool seh_isa(SDK::AActor* a, bool& out) {
    __try { out = a->IsA(T::StaticClass()); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_money_vals(SDK::AMoney_base_C* m, int& v, float& e) {
    __try { v = m->Value; e = m->Exp; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_hp(SDK::APlayerCharacter_C* p, int& hp, int& maxHp, bool& downed) {
    __try { hp = p->Health; maxHp = p->MaxHealth; downed = p->Downed_; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_guard_state(SDK::ANPC_Guard_C* g, bool& alerted, bool& armed) {
    __try { alerted = g->Alert_; armed = g->Armed_; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_npc(SDK::ANPCBase_C* n, int& hp, bool& dead) {
    __try { hp = n->Health; dead = n->Dead_; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// Per-Present shared state: one UWorld walk, one PC/viewport resolve, one
// counts reset per frame instead of per render pass / per projection.
struct Frame {
    void* pc = nullptr;
    void* pawn = nullptr;
    int w = 0, h = 0;
    bool valid = false;
    std::vector<void*> actors;
};
static Frame g_frame{};

void begin_present() {
    FrameCtx ctx = begin_frame();
    g_frame.pc = ctx.pc;
    g_frame.pawn = ctx.localPawn;
    g_frame.w = ctx.vpW;
    g_frame.h = ctx.vpH;
    g_frame.valid = ctx.valid;
    g_counts = Counts{};
    g_frame.actors.clear();
    if (ctx.valid) {
        auto snap = actors::copy_level_actors();
        g_frame.actors = std::move(snap.list);
    }
}

// Same math as world_to_screen but reuses the cached PC/viewport: ~3
// ProcessEvent calls saved per projection (matters x18 per skeleton).
static bool project_cached(const SDK::FVector& loc, ScreenPos& out) {
    out = {};
    if (!g_frame.valid) return world_to_screen(&loc, out);
    auto* pc = reinterpret_cast<SDK::APlayerController*>(g_frame.pc);
    if (!IS_VALID_PTR(pc)) return false;
    SDK::FVector2D scr{};
    if (!seh_project(pc, loc, scr)) return false;
    int w = g_frame.w, h = g_frame.h;
    if (w <= 0 || h <= 0) return false;
    if (scr.X < -100 || scr.Y < -100 || scr.X > w + 100 || scr.Y > h + 100) {
        out.x = scr.X; out.y = scr.Y;
        out.visible = (scr.X >= 0 && scr.Y >= 0 && scr.X <= (float)w && scr.Y <= (float)h);
        return out.visible;
    }
    out.x = scr.X; out.y = scr.Y; out.visible = true;
    return true;
}

static bool seh_name(SDK::AActor* a, std::string& out);

static bool seh_name(SDK::AActor* a, std::string& out) {
    // VEH-guarded for the same reason (level snapshot can race teardown).
    if (!IS_VALID_PTR(a)) return false;
    out = veh::get_name(a);
    return !out.empty();
}

FrameCtx begin_frame() {
    FrameCtx ctx{};
    ctx.pc = actors::local_player_controller();
    ctx.localPawn = actors::local_pawn();
    if (!IS_VALID_PTR(ctx.pc)) return ctx;
    auto* pc = reinterpret_cast<SDK::APlayerController*>(ctx.pc);
    int w = 0, h = 0;
    if (!seh_viewport(pc, w, h)) return ctx;
    ctx.vpW = w; ctx.vpH = h;
    ctx.valid = (w > 0 && h > 0);
    return ctx;
}

bool world_to_screen(const void* worldLocation, ScreenPos& out) {
    out = {};
    auto* pc = reinterpret_cast<SDK::APlayerController*>(actors::local_player_controller());
    if (!IS_VALID_PTR(pc)) return false;
    const auto& loc = *reinterpret_cast<const SDK::FVector*>(worldLocation);
    SDK::FVector2D scr{};
    if (!seh_project(pc, loc, scr)) return false;
    int w = 0, h = 0;
    if (!seh_viewport(pc, w, h)) return false;
    if (w <= 0 || h <= 0) return false;
    if (scr.X < -100 || scr.Y < -100 || scr.X > w + 100 || scr.Y > h + 100) {
        out.x = scr.X; out.y = scr.Y;
        out.visible = (scr.X >= 0 && scr.Y >= 0 && scr.X <= (float)w && scr.Y <= (float)h);
        return out.visible;
    }
    out.x = scr.X; out.y = scr.Y; out.visible = true;
    return true;
}

static float dist_to(const SDK::FVector& a, const SDK::FVector& b) {
    float dx = a.X - b.X, dy = a.Y - b.Y, dz = a.Z - b.Z;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

static void text_centered(const char* txt, float x, float y, ImU32 col, float scale = 1.0f) {
    ImVec2 sz = ImGui::CalcTextSize(txt);
    ImGui::GetBackgroundDrawList()->AddText(nullptr, 13.0f * scale,
        ImVec2(x - sz.x * 0.5f, y), col, txt);
}

void anchor_pos(int anchor, float x0, float y0, float x1, float y1,
        float lh, int line, float& ox, float& oy) {
    float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
    float ax = cx, ay = y0, nx = 0.0f, ny = -1.0f;
    switch (anchor & 7) {
    case 0: ax = cx; ay = y0; nx = 0.0f; ny = -1.0f; break;
    case 1: ax = x1; ay = y0; nx = 0.7f; ny = -0.7f; break;
    case 2: ax = x1; ay = cy; nx = 1.0f; ny = 0.0f; break;
    case 3: ax = x1; ay = y1; nx = 0.7f; ny = 0.7f; break;
    case 4: ax = cx; ay = y1; nx = 0.0f; ny = 1.0f; break;
    case 5: ax = x0; ay = y1; nx = -0.7f; ny = 0.7f; break;
    case 6: ax = x0; ay = cy; nx = -1.0f; ny = 0.0f; break;
    default: ax = x0; ay = y0; nx = -0.7f; ny = -0.7f; break;
    }
    float d = 6.0f + (float)line * (lh + 2.0f);
    ox = ax + nx * d; oy = ay + ny * d;
}

static void seg(ImDrawList* draw, float xa, float ya, float xb, float yb, ImU32 c) {
    draw->AddLine(ImVec2(xa, ya), ImVec2(xb, yb), IM_COL32(0, 0, 0, 200), 3.0f);
    draw->AddLine(ImVec2(xa, ya), ImVec2(xb, yb), c, 1.6f);
}

void draw_box_style(void* drawList, int style, float x0, float y0, float x1, float y1, unsigned col) {
    auto* draw = reinterpret_cast<ImDrawList*>(drawList);
    if (!draw) return;
    ImU32 c = (ImU32)col;
    if (style == 2) {
        // 3D: back rect + connectors + front rect
        float dx = (x1 - x0) * 0.22f, dy = -(y1 - y0) * 0.13f;
        if (dx < 4.0f) dx = 4.0f;
        if (dy > -4.0f) dy = -4.0f;
        draw->AddRect(ImVec2(x0 + dx, y0 + dy), ImVec2(x1 + dx, y1 + dy),
            IM_COL32(0, 0, 0, 150), 0.0f, 0, 1.0f);
        draw->AddRect(ImVec2(x0 + dx, y0 + dy), ImVec2(x1 + dx, y1 + dy), c, 0.0f, 0, 1.2f);
        draw->AddLine(ImVec2(x0, y0), ImVec2(x0 + dx, y0 + dy), c, 1.2f);
        draw->AddLine(ImVec2(x1, y0), ImVec2(x1 + dx, y0 + dy), c, 1.2f);
        draw->AddLine(ImVec2(x0, y1), ImVec2(x0 + dx, y1 + dy), c, 1.2f);
        draw->AddLine(ImVec2(x1, y1), ImVec2(x1 + dx, y1 + dy), c, 1.2f);
        draw->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), c, 0.0f, 0, 1.5f);
        draw->AddRect(ImVec2(x0 - 1, y0 - 1), ImVec2(x1 + 1, y1 + 1),
            IM_COL32(0, 0, 0, 180), 0.0f, 0, 1.0f);
    } else if (style == 1) {
        // corners
        float L = (x1 - x0) < (y1 - y0) ? (x1 - x0) : (y1 - y0);
        L *= 0.28f;
        if (L < 4.0f) L = 4.0f;
        if (L > 42.0f) L = 42.0f;
        seg(draw, x0, y0, x0 + L, y0, c); seg(draw, x0, y0, x0, y0 + L, c);
        seg(draw, x1, y0, x1 - L, y0, c); seg(draw, x1, y0, x1, y0 + L, c);
        seg(draw, x0, y1, x0 + L, y1, c); seg(draw, x0, y1, x0, y1 - L, c);
        seg(draw, x1, y1, x1 - L, y1, c); seg(draw, x1, y1, x1, y1 - L, c);
    } else {
        draw->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), c, 2.0f, 0, 1.5f);
        draw->AddRect(ImVec2(x0 - 1, y0 - 1), ImVec2(x1 + 1, y1 + 1),
            IM_COL32(0, 0, 0, 180), 2.0f, 0, 1.0f);
    }
}

void render_items() {
    if (!g_cfg.espEnabled.load() || !g_cfg.itemEsp.load()) return;
    if (!g_frame.valid) return;

    SDK::FVector localLoc{};
    bool haveLocal = false;
    if (IS_VALID_PTR(g_frame.pawn)) {
        if (seh_loc(reinterpret_cast<SDK::AActor*>(g_frame.pawn), localLoc))
            haveLocal = true;
    }

    auto* draw = ImGui::GetBackgroundDrawList();
    const float maxDist = g_cfg.itemMaxDist.load();

    for (void* raw : g_frame.actors) {
        auto* actor = reinterpret_cast<SDK::AActor*>(raw);
        if (!IS_VALID_PTR(actor)) continue;

        bool isMoney = false, isPickup = false;
        if (!seh_isa<SDK::AMoney_base_C>(actor, isMoney)) continue;
        if (!seh_isa<SDK::APickupItem_base_C>(actor, isPickup)) continue;
        if (!isMoney && !isPickup) continue;
        if (isMoney && !g_cfg.moneyEsp.load()) continue;
        if (isPickup && !isMoney && !g_cfg.pickupEsp.load()) continue;

        SDK::FVector loc{};
        if (!seh_loc(actor, loc)) continue;
        if (haveLocal && maxDist > 0.0f) {
            if (dist_to(localLoc, loc) > maxDist) continue;
        }

        ScreenPos sp{};
        if (!project_cached(loc, sp) || !sp.visible) continue;

        std::string label;
        if (!seh_name(actor, label)) continue;
        ImU32 col = u32c(g_cfg.itemCol);
        if (isMoney) {
            auto* m = reinterpret_cast<SDK::AMoney_base_C*>(actor);
            int v = 0; float exp = 0;
            seh_money_vals(m, v, exp);
            col = u32c(g_cfg.moneyCol);
            if (g_cfg.itemShowValue.load())
                label = std::format("Cash ${} ({}xp)", v, (int)exp);
            else
                label = "Cash";
        } else {
            col = u32c(g_cfg.itemCol);
            label = short_name(label);
        }
        if (g_cfg.itemShowDistance.load() && haveLocal) {
            float d = dist_to(localLoc, loc) / 100.0f;
            label += std::format(" [{:.0f}m]", d);
        }

        float s = g_cfg.espTextScale.load();
        float ix0 = sp.x - 7.0f, iy0 = sp.y - 7.0f, ix1 = sp.x + 7.0f, iy1 = sp.y + 7.0f;
        if (g_cfg.itemBox.load())
            draw_box_style(draw, g_cfg.espBoxStyle.load(), ix0, iy0, ix1, iy1, col);
        else {
            draw->AddCircleFilled(ImVec2(sp.x, sp.y), 3.0f, col);
            draw->AddCircle(ImVec2(sp.x, sp.y), 6.0f, col, 16, 1.0f);
        }
        {
            float lx, ly;
            anchor_pos(g_cfg.espLabelAnchor.load(), ix0, iy0, ix1, iy1,
                13.0f * s, 0, lx, ly);
            text_centered(label.c_str(), lx, ly + g_cfg.espNameDy.load() * s * 0.25f, col, s);
        }
    }
}

void render_players() {
    if (!g_cfg.espEnabled.load() || !g_cfg.playerEsp.load()) return;
    if (!g_frame.valid) return;

    SDK::FVector localLoc{};
    if (IS_VALID_PTR(g_frame.pawn))
        seh_loc(reinterpret_cast<SDK::AActor*>(g_frame.pawn), localLoc);

    auto* draw = ImGui::GetBackgroundDrawList();
    const float maxDist = g_cfg.playerMaxDist.load();
    void* selfPawn = g_frame.pawn;

    for (void* raw : g_frame.actors) {
        auto* actor = reinterpret_cast<SDK::AActor*>(raw);
        if (!IS_VALID_PTR(actor) || actor == selfPawn) continue;
        bool isp = false;
        if (!seh_isa<SDK::APlayerCharacter_C>(actor, isp) || !isp) continue;

        auto* pc = reinterpret_cast<SDK::APlayerCharacter_C*>(actor);
        SDK::FVector loc{};
        if (!seh_loc(pc, loc)) continue;
        if (maxDist > 0.0f) {
            float dx = loc.X - localLoc.X, dy = loc.Y - localLoc.Y, dz = loc.Z - localLoc.Z;
            if (sqrtf(dx*dx+dy*dy+dz*dz) > maxDist) continue;
        }

        SDK::FVector head = loc; head.Z += 180.0f;
        SDK::FVector feet = loc; feet.Z -= 20.0f;
        ScreenPos sHead{}, sFeet{};
        if (!project_cached(head, sHead)) continue;
        if (!project_cached(feet, sFeet)) continue;
        if (!sFeet.visible) continue;

        float h = sFeet.y - sHead.y;
        if (h < 4.0f || h > 1200.0f) continue;
        float w = h * g_cfg.espBoxW.load();
        float cx = sFeet.x;
        float x0 = cx - w * 0.5f, x1 = cx + w * 0.5f;
        float y0 = sHead.y, y1 = sFeet.y;

        int hp = 0, maxHp = 100; bool downed = false;
        seh_hp(pc, hp, maxHp, downed);
        ImU32 col = downed ? IM_COL32(255, 180, 60, 255) : u32c(g_cfg.playerCol);

        ++g_counts.players;
        if (g_cfg.playerBox.load()) {
            if (g_cfg.boxFill.load()) box_fill(draw, x0, y0, x1, y1, col);
            draw_box_style(draw, g_cfg.espBoxStyle.load(), x0, y0, x1, y1, col);
        }
        if (g_cfg.playerSkel.load())
            draw_skeleton(actor, draw, IM_COL32(235, 245, 255, 255));
        float s = g_cfg.espTextScale.load();
        int anchor = g_cfg.espLabelAnchor.load();
        float lh = 13.0f * s;
        float nudge = g_cfg.espNameDy.load() * s;
        if (g_cfg.playerName.load()) {
            std::string nm;
            if (seh_name(actor, nm)) {
                if (g_cfg.nameMode.load() == 1) nm = short_name(nm);
                float lx, ly;
                anchor_pos(anchor, x0, y0, x1, y1, lh, 0, lx, ly);
                text_centered(nm.c_str(), lx, ly + nudge, IM_COL32(255,255,255,255), s);
            }
        }
        if (g_cfg.playerHealth.load()) {
            if (maxHp <= 0) maxHp = 100;
            float frac = std::clamp((float)hp / (float)maxHp, 0.0f, 1.0f);
            int hs = g_cfg.healthStyle.load();
            bool left = g_cfg.hpLeft.load();
            float bx0 = left ? x0 - 6 : x1 + 2;
            float bx1 = left ? x0 - 2 : x1 + 6;
            if (hs != 2) {
                if (g_cfg.healthGradient.load())
                    health_bar_grad(draw, bx0, y0, bx1, y1, frac);
                else {
                    draw->AddRectFilled(ImVec2(bx0, y0), ImVec2(bx1, y1), IM_COL32(0,0,0,160));
                    ImVec4 hcv = g_cfg.healthCol;
                    ImU32 hc = IM_COL32((int)(hcv.x * 255), (int)(hcv.y * 255), (int)(hcv.z * 255), 255);
                    draw->AddRectFilled(ImVec2(bx0, y1 - h * frac), ImVec2(bx1, y1), hc);
                }
            }
            if (hs != 1) {
                char buf[32]; snprintf(buf, sizeof(buf), "%d", hp);
                text_centered(buf, cx, y1 + 4, IM_COL32(255,255,255,220), s * 0.9f);
            }
        }
        if (g_cfg.playerDistance.load()) {
            float dx = loc.X - localLoc.X, dy = loc.Y - localLoc.Y, dz = loc.Z - localLoc.Z;
            float dm = sqrtf(dx*dx+dy*dy+dz*dz) / 100.0f;
            char buf[32]; snprintf(buf, sizeof(buf), "%.0fm", dm);
            float lx, ly;
            int subA = g_cfg.espSubAnchor.load();
            int line = (subA == anchor) ? (g_cfg.playerName.load() ? 1 : 0) : 0;
            anchor_pos(subA, x0, y0, x1, y1, lh, line, lx, ly);
            text_centered(buf, lx, ly + nudge, IM_COL32(200,200,200,220), s * 0.9f);
        }
    }
}

void render_guards() {
    if (!g_cfg.espEnabled.load() || !g_cfg.guardEsp.load()) return;
    if (!g_frame.valid) return;

    SDK::FVector localLoc{};
    if (IS_VALID_PTR(g_frame.pawn))
        seh_loc(reinterpret_cast<SDK::AActor*>(g_frame.pawn), localLoc);

    auto* draw = ImGui::GetBackgroundDrawList();
    const float maxDist = g_cfg.guardMaxDist.load();
    float s = g_cfg.espTextScale.load();

    for (void* raw : g_frame.actors) {
        auto* actor = reinterpret_cast<SDK::AActor*>(raw);
        if (!IS_VALID_PTR(actor)) continue;
        // ANPCBase_C covers guards AND police variants (single IsA).
        bool isn = false;
        if (!seh_isa<SDK::ANPCBase_C>(actor, isn) || !isn) continue;

        auto* n = reinterpret_cast<SDK::ANPCBase_C*>(actor);
        SDK::FVector loc{};
        if (!seh_loc(n, loc)) continue;
        float dx = loc.X - localLoc.X, dy = loc.Y - localLoc.Y, dz = loc.Z - localLoc.Z;
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);
        if (maxDist > 0.0f && dist > maxDist) continue;

        SDK::FVector head = loc; head.Z += 180.0f;
        SDK::FVector feet = loc; feet.Z -= 20.0f;
        ScreenPos sHead{}, sFeet{};
        if (!project_cached(head, sHead)) continue;
        if (!project_cached(feet, sFeet)) continue;
        if (!sFeet.visible) continue;

        float h = sFeet.y - sHead.y;
        if (h < 4.0f || h > 1200.0f) continue;
        float w = h * g_cfg.espBoxW.load();
        float cx = sFeet.x;
        float x0 = cx - w * 0.5f, x1 = cx + w * 0.5f;
        float y0 = sHead.y, y1 = sFeet.y;

        int nhp = 0; bool dead = false;
        seh_npc(n, nhp, dead);
        bool alerted = false, armed = false;
        bool isg = false;
        if (seh_isa<SDK::ANPC_Guard_C>(actor, isg) && isg)
            seh_guard_state(reinterpret_cast<SDK::ANPC_Guard_C*>(actor), alerted, armed);
        ++g_counts.guards;
        if (dead) alerted = false;
        ImU32 col = alerted ? IM_COL32(255, 60, 60, 255)
            : armed         ? IM_COL32(255, 170, 60, 255)
                            : u32c(g_cfg.guardCol);

        if (g_cfg.guardBox.load()) {
            if (g_cfg.boxFill.load()) box_fill(draw, x0, y0, x1, y1, col);
            draw_box_style(draw, g_cfg.espBoxStyle.load(), x0, y0, x1, y1, col);
        }
        if (g_cfg.guardSkel.load())
            draw_skeleton(actor, draw, col);
        if (g_cfg.guardState.load()) {
            const char* st = dead ? "DEAD" : alerted ? "ALERTED" : (armed ? "ARMED" : "patrol");
            char buf[48];
            if (g_cfg.guardDistance.load())
                snprintf(buf, sizeof(buf), "%s [%.0fm]", st, dist / 100.0f);
            else
                snprintf(buf, sizeof(buf), "%s", st);
            float lx, ly;
            anchor_pos(g_cfg.espLabelAnchor.load(), x0, y0, x1, y1,
                13.0f * s, 0, lx, ly);
            ImVec2 tsz = ImGui::CalcTextSize(buf);
            draw->AddText(ImVec2(lx - tsz.x * 0.5f, ly + g_cfg.espNameDy.load() * s),
                col, buf);
        }
    }
}


// ---- real bone skeleton (no bone names needed) --------------------------
// Pipeline: skeletal mesh -> GetNumBones -> CachedBoneSpaceTransforms
// (component space) -> K2_GetComponentToWorld -> manual quat transform.
// Bone slots follow the UE4 mannequin layout; a rig self-validates on
// anatomical constraints before anything is drawn, so foreign rigs simply
// get no skeleton instead of garbage lines.
static SDK::USkeletalMeshComponent* seh_skmesh(SDK::AActor* a) {
    __try {
        SDK::UClass* sk = SDK::USkeletalMeshComponent::StaticClass();
        if (!IS_VALID_PTR(sk)) return nullptr;
        return static_cast<SDK::USkeletalMeshComponent*>(a->GetComponentByClass(sk));
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static int seh_numbones(SDK::USkeletalMeshComponent* m) {
    __try { return m->GetNumBones(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
// Mirrors FTransform memory layout {Q@0, T@0x10, S@0x20} (0x30 bytes).
struct RawT { float q[4]; float p[4]; float s[4]; };
struct BoneBuf { RawT b[22]; int n; bool ok; };
static BoneBuf seh_bones(SDK::USkeletalMeshComponent* m) {
    BoneBuf r{}; r.ok = false; r.n = 0;
    __try {
        auto& arr = m->CachedBoneSpaceTransforms;
        if (arr.Num() < 22) return r;
        const SDK::FTransform* d = arr.GetDataPtr();
        if (!d) return r;
        for (int i = 0; i < 22; ++i)
            memcpy(&r.b[i], &d[i], sizeof(RawT));
        r.n = 22; r.ok = true; return r;
    } __except (EXCEPTION_EXECUTE_HANDLER) { r.ok = false; return r; }
}
static bool seh_compworld(SDK::USceneComponent* c, RawT& out) {
    __try {
        SDK::FTransform t = c->K2_GetComponentToWorld();
        memcpy(&out, &t, sizeof(out));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool fin(float v) { return v > -20000.0f && v < 20000.0f && v == v; }
static bool valid_rig(const BoneBuf& r) {
    for (int i = 0; i < 22; ++i)
        for (int k = 0; k < 3; ++k)
            if (!fin(r.b[i].p[k])) return false;
    const RawT& pel = r.b[0];
    const RawT& s3 = r.b[3];
    const RawT& neck = r.b[12];
    const RawT& head = r.b[13];
    const RawT& hl = r.b[7];
    const RawT& hr = r.b[11];
    const RawT& fl = r.b[16];
    const RawT& fr = r.b[20];
    if (!(head.p[2] > neck.p[2] && neck.p[2] > s3.p[2] && s3.p[2] > pel.p[2])) return false;
    if (!(pel.p[2] > fl.p[2] && pel.p[2] > fr.p[2])) return false;
    float dl = hl.p[1] - pel.p[1], dr = hr.p[1] - pel.p[1];
    if (!(dl * dr < 0.0f)) return false;
    if (!(dl > 10.0f || dl < -10.0f)) return false;
    if (!(dr > 10.0f || dr < -10.0f)) return false;
    float hgt = head.p[2] - (fl.p[2] < fr.p[2] ? fl.p[2] : fr.p[2]);
    if (!(hgt > 50.0f && hgt < 400.0f)) return false;
    return true;
}
static SDK::FVector xform(const RawT& c, const RawT& b) {
    float vx = b.p[0] * c.s[0], vy = b.p[1] * c.s[1], vz = b.p[2] * c.s[2];
    float tx = 2.0f * (c.q[1] * vz - c.q[2] * vy);
    float ty = 2.0f * (c.q[2] * vx - c.q[0] * vz);
    float tz = 2.0f * (c.q[0] * vy - c.q[1] * vx);
    SDK::FVector o;
    o.X = vx + c.q[3] * tx + (c.q[1] * tz - c.q[2] * ty) + c.p[0];
    o.Y = vy + c.q[3] * ty + (c.q[2] * tx - c.q[0] * tz) + c.p[1];
    o.Z = vz + c.q[3] * tz + (c.q[0] * ty - c.q[1] * tx) + c.p[2];
    return o;
}

void draw_skeleton(void* actorRaw, void* drawList, unsigned col) {
    auto* draw = reinterpret_cast<ImDrawList*>(drawList);
    if (!draw) return;
    SDK::FVector j[18];
    if (skeleton_points(actorRaw, j, 18) != 18) return;
    static const int chains[5][6] = {
        { 0, 1, 2, 3, 4, 5 },
        { 3, 6, 7, 8, -1, -1 },
        { 3, 9, 10, 11, -1, -1 },
        { 0, 12, 13, 14, -1, -1 },
        { 0, 15, 16, 17, -1, -1 },
    };
    static const int lens[5] = { 6, 4, 4, 4, 4 };
    ImU32 c = (ImU32)col;
    for (int ci = 0; ci < 5; ++ci) {
        for (int k = 0; k + 1 < lens[ci]; ++k) {
            ScreenPos s0{}, s1{};
            if (!project_cached(j[chains[ci][k]], s0) || !s0.visible) continue;
            if (!project_cached(j[chains[ci][k + 1]], s1) || !s1.visible) continue;
            draw->AddLine(ImVec2(s0.x, s0.y), ImVec2(s1.x, s1.y), c, 1.6f);
        }
    }
}

// Mannequin-order subset shared by draw_skeleton and the rig popout.
int skeleton_points(void* actorRaw, void* outLocations, int maxOut) {
    auto* out = static_cast<SDK::FVector*>(outLocations);
    if (!out || maxOut < 18) return 0;
    auto* actor = reinterpret_cast<SDK::AActor*>(actorRaw);
    if (!IS_VALID_PTR(actor)) return 0;
    auto* mesh = seh_skmesh(actor);
    if (!IS_VALID_PTR(mesh)) return 0;
    if (seh_numbones(mesh) < 22) return 0;
    BoneBuf bb = seh_bones(mesh);
    if (!bb.ok || !valid_rig(bb)) return 0;
    RawT cw{};
    if (!seh_compworld(reinterpret_cast<SDK::USceneComponent*>(mesh), cw)) return 0;
    float ql = cw.q[0] * cw.q[0] + cw.q[1] * cw.q[1] + cw.q[2] * cw.q[2] + cw.q[3] * cw.q[3];
    if (ql < 0.5f || ql > 2.0f) return 0;
    static const int bones[18] = { 0, 1, 2, 3, 12, 13, 5, 6, 7, 9, 10, 11, 14, 15, 16, 18, 19, 20 };
    for (int i = 0; i < 18; ++i) out[i] = xform(cw, bb.b[bones[i]]);
    return 18;
}

// Stylized guard silhouette: head + torso + arms + legs + visor tick.
// Used by the ESP style-lab preview (and as an in-box marker above).
void draw_guard_model(void* drawList, float cx, float cy, float scale, unsigned col, bool alerted) {
    auto* dl = reinterpret_cast<ImDrawList*>(drawList);
    if (!dl || scale <= 0.0f) return;
    ImU32 c = (ImU32)col;
    float hx = cx, hy = cy - 52.0f * scale;               // head center
    dl->AddCircle(ImVec2(hx, hy), 11.0f * scale, c, 20, 2.0f);
    dl->AddLine(ImVec2(hx - 6.0f * scale, hy), ImVec2(hx + 6.0f * scale, hy), c, 2.0f); // visor
    ImVec2 neck(hx, hy + 11.0f * scale), hip(cx, cy + 22.0f * scale);
    dl->AddLine(neck, hip, c, 3.0f);                       // torso
    dl->AddLine(ImVec2(neck.x - 20.0f * scale, neck.y + 16.0f * scale),
        ImVec2(neck.x + 20.0f * scale, neck.y + 16.0f * scale), c, 2.5f); // arms
    dl->AddLine(hip, ImVec2(hip.x - 13.0f * scale, hip.y + 30.0f * scale), c, 2.5f);
    dl->AddLine(hip, ImVec2(hip.x + 13.0f * scale, hip.y + 30.0f * scale), c, 2.5f);
    if (alerted) { // "!" ping above head
        dl->AddCircleFilled(ImVec2(hx + 16.0f * scale, hy - 16.0f * scale),
            7.0f * scale, IM_COL32(255, 60, 60, 255));
        dl->AddText(ImVec2(hx + 16.0f * scale - 2.5f * scale, hy - 16.0f * scale - 7.0f * scale),
            IM_COL32(255, 255, 255, 255), "!");
    }
}

void draw_player_model(void* drawList, float cx, float cy, float scale, unsigned col) {
    auto* dl = reinterpret_cast<ImDrawList*>(drawList);
    if (!dl || scale <= 0.0f) return;
    ImU32 c = (ImU32)col;
    float hx = cx, hy = cy - 52.0f * scale;
    dl->AddCircle(ImVec2(hx, hy), 11.0f * scale, c, 20, 2.0f);
    dl->AddLine(ImVec2(hx - 8.0f * scale, hy - 11.0f * scale),
        ImVec2(hx + 8.0f * scale, hy - 11.0f * scale), c, 2.5f); // headset band
    ImVec2 neck(hx, hy + 11.0f * scale), hip(cx, cy + 22.0f * scale);
    dl->AddLine(neck, hip, c, 3.0f);
    dl->AddLine(ImVec2(neck.x - 20.0f * scale, neck.y + 16.0f * scale),
        ImVec2(neck.x + 20.0f * scale, neck.y + 16.0f * scale), c, 2.5f);
    dl->AddLine(hip, ImVec2(hip.x - 13.0f * scale, hip.y + 30.0f * scale), c, 2.5f);
    dl->AddLine(hip, ImVec2(hip.x + 13.0f * scale, hip.y + 30.0f * scale), c, 2.5f);
}

} // namespace esp

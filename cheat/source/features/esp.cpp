#include "pch/pch.h"
#include "features/esp.hpp"
#include "world/actor_iter.hpp"
#include "config/config.hpp"

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
#include "util/files.hpp"
#include "util/veh.hpp"
#include "features/entities.hpp"

#include <cstring>
#include <unordered_map>
#include <cctype>
#include <cstdio>

#include "../../vendor/imgui/imgui.h"

namespace esp {

static Counts g_counts{};
Counts counts() { return g_counts; }
static SkelDbg g_skel{};
SkelDbg skel_debug() { return g_skel; }

static ImU32 u32c(ImVec4 c) {
    return IM_COL32((int)(c.x * 255.0f), (int)(c.y * 255.0f), (int)(c.z * 255.0f), 255);
}
static std::string short_name(const std::string& n) {
    auto q = n.find('_');
    return (q == std::string::npos) ? n : n.substr(0, q);
}

static std::string clean_label(std::string n) {
    if (n.rfind("BP_", 0) == 0) n.erase(0, 3);
    auto cpos = n.find("_C_");
    if (cpos != std::string::npos) n.erase(cpos);
    else if (n.size() > 2 && n.compare(n.size() - 2, 2, "_C") == 0) n.erase(n.size() - 2);
    while (!n.empty() && n.back() >= '0' && n.back() <= '9') n.pop_back();
    while (!n.empty() && n.back() == '_') n.pop_back();
    for (auto& ch : n) ch = (ch == '_' ? ' ' : (char)toupper((unsigned char)ch));
    static const char* drop[] = { "GEN", "VARIABLE", "DEFAULT", "BASE",
        "ACTOR", "CLASS", "STATIC", "MESH", nullptr };
    std::string out;
    std::string seen[24];
    int nseen = 0;
    size_t i = 0;
    while (i < n.size()) {
        while (i < n.size() && n[i] == ' ') ++i;
        size_t j = i;
        while (j < n.size() && n[j] != ' ') ++j;
        if (j > i) {
            std::string w = n.substr(i, j - i);
            bool generic = false;
            for (int k = 0; drop[k]; ++k) if (w == drop[k]) { generic = true; break; }
            bool dup = false;
            for (int k = 0; k < nseen; ++k) if (seen[k] == w) { dup = true; break; }
            if (!generic && !dup) {
                if (!out.empty()) out += ' ';
                out += w;
                if (nseen < 24) seen[nseen++] = w;
            }
        }
        i = j;
    }
    if (!out.empty()) out[0] = (char)toupper((unsigned char)out[0]);
    for (size_t k = 1; k < out.size(); ++k)
        if (out[k - 1] == ' ') out[k] = (char)toupper((unsigned char)out[k]);
        else out[k] = (char)tolower((unsigned char)out[k]);
    return out;
}
static void box_fill(ImDrawList* d, float x0, float y0, float x1, float y1, ImU32 c) {
    d->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), (c & 0x00FFFFFFu) | (38u << 24));
}

static bool seh_visible(SDK::AActor* a) {
    __try { return a->WasRecentlyRendered(0.15f); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return true; }
}

static ImU32 dim_c(ImU32 c, float k = 0.43f) {
    unsigned a = (unsigned)(((c >> 24) & 0xFFu) * k);
    return (c & 0x00FFFFFFu) | (a << 24);
}

static std::string item_label(SDK::AActor* a, bool isMoney, int moneyVal);
static ImU32 health_color(float frac) {
    frac = std::clamp(frac, 0.0f, 1.0f);
    int r, g;
    if (frac < 0.5f) {
        float k = frac * 2.0f;
        r = 255;
        g = 70 + (int)((200 - 70) * k);
    } else {
        float k = (frac - 0.5f) * 2.0f;
        r = 255 - (int)((255 - 90) * k);
        g = 200 + (int)((255 - 200) * k);
    }
    return IM_COL32(r, g, 70, 255);
}

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

static bool seh_capsule_half(SDK::AActor* a, float& half, float* radius = nullptr) {
    __try {
        auto* ch = static_cast<SDK::ACharacter*>(a);
        SDK::UCapsuleComponent* cap = ch->CapsuleComponent;
        if (!IS_VALID_PTR(cap)) return false;
        float h = cap->GetScaledCapsuleHalfHeight();
        if (!(h > 5.0f && h < 500.0f)) return false;
        half = h;
        if (radius) *radius = cap->GetScaledCapsuleRadius();
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
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

static void* seh_held_class(SDK::APlayerCharacter_C* p) {
    __try {
        if (!IS_VALID_PTR(p)) return nullptr;
        int slot = p->EquippedSlot;
        int n = p->EquippedGuns.Num();
        if (slot < 0 || slot >= n) return nullptr;
        auto d = p->EquippedGuns.GetDataPtr();
        if (!d) return nullptr;
        void* c = d[slot];
        return IS_VALID_PTR(c) ? c : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static bool seh_holding(SDK::APlayerCharacter_C* p, std::string& out) {
    void* cls = seh_held_class(p);
    if (!IS_VALID_PTR(cls)) return false;
    std::string nm = veh::get_name(cls);
    if (nm.empty()) return false;
    nm = clean_label(nm);
    if (nm.empty()) return false;
    out = nm;
    return true;
}

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
    g_skel = SkelDbg{};
    g_frame.actors.clear();
    if (ctx.valid) {
        auto snap = actors::copy_level_actors();
        g_frame.actors = std::move(snap.list);
    }
    playerManager::rebuild(g_frame.actors, g_frame.pawn);
    itemManager::rebuild(g_frame.actors);
}

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

    if (!IS_VALID_PTR(a)) return false;
    out = veh::get_name(a);
    return !out.empty();
}

static std::string item_label(SDK::AActor* a, bool isMoney, int moneyVal) {
    if (isMoney) {
        char b[48];
        if (moneyVal > 0) snprintf(b, sizeof(b), "Cash $%d", moneyVal);
        else snprintf(b, sizeof(b), "Cash");
        return b;
    }
    std::string nm;
    if (seh_name(a, nm) && !nm.empty()) {
        nm = clean_label(nm);
        if (!nm.empty()) return nm;
    }

    bool isk = false, isp = false;
    seh_isa<SDK::AItem_Keycard_C>(a, isk);
    seh_isa<SDK::AGuardPhone_C>(a, isp);
    if (isk) return "Keycard";
    if (isp) return "Phone";
    return "Pickup";
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

static std::vector<PlayerInfo>& players_mut() {
    static std::vector<PlayerInfo> v; return v;
}
static std::vector<GuardInfo>& npcs_mut() {
    static std::vector<GuardInfo> v; return v;
}
static std::vector<LootInfo>& items_mut() {
    static std::vector<LootInfo> v; return v;
}

void playerManager::rebuild(const std::vector<void*>& actors, void* selfPawn) {
    players_mut().clear();
    npcs_mut().clear();
    SDK::FVector localLoc{};
    bool haveLocal = IS_VALID_PTR(selfPawn) &&
        seh_loc(reinterpret_cast<SDK::AActor*>(selfPawn), localLoc);
    int robberNo = 0;
    for (void* raw : actors) {
        auto* actor = reinterpret_cast<SDK::AActor*>(raw);
        if (!IS_VALID_PTR(actor)) continue;
        bool isp = false;
        if (!seh_isa<SDK::APlayerCharacter_C>(actor, isp)) continue;
        if (isp) {
            auto* pc = reinterpret_cast<SDK::APlayerCharacter_C*>(actor);
            SDK::FVector loc{};
            if (!seh_loc(pc, loc)) continue;
            PlayerInfo e;
            e.actor = actor;
            e.x = loc.X; e.y = loc.Y; e.z = loc.Z;
            e.self = (actor == selfPawn);
            e.distM = haveLocal ? dist_to(localLoc, loc) / 100.0f : -1.0f;
            int hp = 0, mx = 100; bool down = false;
            seh_hp(pc, hp, mx, down);
            e.hp = hp; e.maxHp = mx; e.downed = down;
            std::string nm;
            if (seh_name(actor, nm) && !nm.empty()) {
                auto p = nm.find("##");
                if (p != std::string::npos) nm.erase(p);
                while (!nm.empty() && nm.back() == ' ') nm.pop_back();
                e.name = nm;
            }
            if (e.name.empty()) {
                char b[32]; snprintf(b, sizeof(b), "Robber #%d", ++robberNo);
                e.name = b;
            } else if (!e.self) {
                ++robberNo;
            }
            e.visible = seh_visible(actor);
            players_mut().push_back(std::move(e));
            continue;
        }
        bool isn = false;
        if (!seh_isa<SDK::ANPCBase_C>(actor, isn) || !isn) continue;
        auto* n = reinterpret_cast<SDK::ANPCBase_C*>(actor);
        SDK::FVector loc{};
        if (!seh_loc(n, loc)) continue;
        GuardInfo g;
        g.actor = actor;
        g.x = loc.X; g.y = loc.Y; g.z = loc.Z;
        g.distM = haveLocal ? dist_to(localLoc, loc) / 100.0f : -1.0f;
        int hp = 0; bool dead = false;
        seh_npc(n, hp, dead);
        g.hp = hp; g.dead = dead;
        bool isg = false;
        if (seh_isa<SDK::ANPC_Guard_C>(actor, isg) && isg) {
            g.guard = true;
            bool alerted = false, armed = false;
            seh_guard_state(reinterpret_cast<SDK::ANPC_Guard_C*>(actor), alerted, armed);
            g.alerted = alerted; g.armed = armed;
        }
        g.visible = seh_visible(actor);
        npcs_mut().push_back(std::move(g));
    }
}

const std::vector<PlayerInfo>& playerManager::players() { return players_mut(); }
const std::vector<GuardInfo>& playerManager::npcs() { return npcs_mut(); }
static int& sel_mut() { static int s = -1; return s; }
int playerManager::selected() { return sel_mut(); }
void playerManager::setSelected(int i) { sel_mut() = i; }
const PlayerInfo* playerManager::selectedPlayer() {
    int i = sel_mut();
    auto& v = players_mut();
    if (i < 0 || i >= (int)v.size()) return nullptr;
    return &v[i];
}

void itemManager::rebuild(const std::vector<void*>& actors) {
    items_mut().clear();
    SDK::FVector localLoc{};
    bool haveLocal = IS_VALID_PTR(g_frame.pawn) &&
        seh_loc(reinterpret_cast<SDK::AActor*>(g_frame.pawn), localLoc);
    for (void* raw : actors) {
        auto* actor = reinterpret_cast<SDK::AActor*>(raw);
        if (!IS_VALID_PTR(actor)) continue;
        bool isMoney = false, isPickup = false;
        if (!seh_isa<SDK::AMoney_base_C>(actor, isMoney)) continue;
        if (!seh_isa<SDK::APickupItem_base_C>(actor, isPickup)) continue;
        if (!isMoney && !isPickup) continue;
        SDK::FVector loc{};
        if (!seh_loc(actor, loc)) continue;
        LootInfo e;
        e.actor = actor;
        e.x = loc.X; e.y = loc.Y; e.z = loc.Z;
        e.distM = haveLocal ? dist_to(localLoc, loc) / 100.0f : -1.0f;
        e.money = isMoney;
        int v = 0;
        if (isMoney) {
            float exp = 0;
            seh_money_vals(reinterpret_cast<SDK::AMoney_base_C*>(actor), v, exp);
        }
        e.value = v;
        e.label = item_label(actor, isMoney, v);
        items_mut().push_back(std::move(e));
    }
}

const std::vector<LootInfo>& itemManager::items() { return items_mut(); }

void render_items() {
    if (!g_cfg.espEnabled.load() || !g_cfg.itemEsp.load()) return;
    if (!g_frame.valid) return;

    auto* draw = ImGui::GetBackgroundDrawList();
    const float maxDist = g_cfg.itemMaxDist.load();

    for (const LootInfo& e : itemManager::items()) {
        auto* actor = reinterpret_cast<SDK::AActor*>(e.actor);
        if (!IS_VALID_PTR(actor)) continue;
        if (e.money && !g_cfg.moneyEsp.load()) continue;
        if (!e.money && !g_cfg.pickupEsp.load()) continue;
        if (maxDist > 0.0f && e.distM >= 0.0f && e.distM * 100.0f > maxDist) continue;

        SDK::FVector loc{};
        loc.X = e.x; loc.Y = e.y; loc.Z = e.z;
        ScreenPos sp{};
        if (!project_cached(loc, sp) || !sp.visible) continue;

        std::string label = e.label;
        if (g_cfg.itemShowDistance.load() && e.distM >= 0.0f) {
            char db[24]; snprintf(db, sizeof(db), " [%.0fm]", e.distM);
            label += db;
        }
        ImU32 col = e.money ? u32c(g_cfg.moneyCol) : u32c(g_cfg.itemCol);
        if (g_cfg.visCheck.load() && !seh_visible(actor)) col = dim_c(col);

        float half = 7.0f;
        if (e.distM > 0.0f)
            half = std::clamp(140.0f / e.distM, 5.0f, 14.0f);
        float s = g_cfg.espTextScale.load();
        float ix0 = sp.x - half, iy0 = sp.y - half, ix1 = sp.x + half, iy1 = sp.y + half;
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

    auto* draw = ImGui::GetBackgroundDrawList();
    const float maxDist = g_cfg.playerMaxDist.load();

    for (const PlayerInfo& e : playerManager::players()) {
        auto* actor = reinterpret_cast<SDK::AActor*>(e.actor);
        auto* pc = reinterpret_cast<SDK::APlayerCharacter_C*>(e.actor);
        if (!IS_VALID_PTR(actor) || e.self) continue;
        if (maxDist > 0.0f && e.distM >= 0.0f && e.distM * 100.0f > maxDist) continue;

        SDK::FVector loc{};
        loc.X = e.x; loc.Y = e.y; loc.Z = e.z;

        float halfH = 0.0f, capR = 0.0f;
        SDK::FVector head = loc, feet = loc;
        if (seh_capsule_half(actor, halfH, &capR)) { head.Z += halfH; feet.Z -= halfH; }
        else { head.Z += 180.0f; feet.Z -= 20.0f; }
        ScreenPos sHead{}, sFeet{};
        if (!project_cached(head, sHead) || !sHead.visible) continue;
        if (!project_cached(feet, sFeet) || !sFeet.visible) continue;

        float h = sFeet.y - sHead.y;
        if (h < 4.0f || h > 1200.0f) continue;

        float w;
        if (capR > 0.0f) {
            SDK::FVector left = loc, right = loc;
            left.X -= capR; right.X += capR;
            ScreenPos sL{}, sR{};
            if (project_cached(left, sL) && project_cached(right, sR)) {
                w = fabsf(sR.x - sL.x);
            } else {
                w = h * g_cfg.espBoxW.load();
            }
        } else {
            w = h * g_cfg.espBoxW.load();
        }
        float cx = (sHead.x + sFeet.x) * 0.5f;
        float x0 = cx - w * 0.5f, x1 = cx + w * 0.5f;
        float y0 = sHead.y, y1 = sFeet.y;

        int hp = e.hp, maxHp = e.maxHp;
        bool downed = e.downed;
        bool deadP = downed || hp <= 0;
        if (deadP && g_cfg.hideDead.load()) continue;
        ImU32 col = downed ? IM_COL32(255, 180, 60, 255) : u32c(g_cfg.playerCol);
        if (g_cfg.visCheck.load() && !e.visible) col = dim_c(col);

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
        if (g_cfg.playerName.load() && !e.name.empty()) {
            std::string nm = e.name;
            if (g_cfg.nameMode.load() == 1) nm = short_name(nm);
            float lx, ly;
            anchor_pos(anchor, x0, y0, x1, y1, lh, 0, lx, ly);
            text_centered(nm.c_str(), lx, ly + nudge, IM_COL32(255,255,255,255), s);
        }
        if (g_cfg.playerHealth.load()) {
            if (maxHp <= 0) maxHp = 100;
            float frac = std::clamp((float)hp / (float)maxHp, 0.0f, 1.0f);
            int hs = g_cfg.healthStyle.load();
            bool left = g_cfg.hpLeft.load();
            float bx0 = left ? x0 - 6 : x1 + 2;
            float bx1 = left ? x0 - 2 : x1 + 6;
            if (hs != 2) {
                draw->AddRectFilled(ImVec2(bx0, y0), ImVec2(bx1, y1), IM_COL32(0, 0, 0, 160));
                ImU32 hc = health_color(frac);
                draw->AddRectFilled(ImVec2(bx0, y1 - h * frac), ImVec2(bx1, y1), hc);
            }
            if (hs != 1) {
                char buf[32]; snprintf(buf, sizeof(buf), "%d", hp);
                text_centered(buf, cx, y1 + 4, IM_COL32(255,255,255,220), s * 0.9f);
            }
        }
        if (g_cfg.playerDistance.load() && e.distM >= 0.0f) {
            char buf[32]; snprintf(buf, sizeof(buf), "%.0fm", e.distM);
            float lx, ly;
            int subA = g_cfg.espSubAnchor.load();
            int line = (subA == anchor) ? (g_cfg.playerName.load() ? 1 : 0) : 0;
            anchor_pos(subA, x0, y0, x1, y1, lh, line, lx, ly);
            text_centered(buf, lx, ly + nudge, IM_COL32(200,200,200,220), s * 0.9f);
        }
        if (g_cfg.playerHolding.load() && !deadP) {
            std::string hold;
            if (seh_holding(pc, hold))
                text_centered(hold.c_str(), cx, y1 + 18, IM_COL32(200,200,200,220), s * 0.85f);
        }
    }
}

void render_guards() {
    if (!g_cfg.espEnabled.load() || !g_cfg.guardEsp.load()) return;
    if (!g_frame.valid) return;

    auto* draw = ImGui::GetBackgroundDrawList();
    const float maxDist = g_cfg.guardMaxDist.load();
    float s = g_cfg.espTextScale.load();

    for (const GuardInfo& e : playerManager::npcs()) {
        auto* actor = reinterpret_cast<SDK::AActor*>(e.actor);
        if (!IS_VALID_PTR(actor) || !e.guard) continue;
        if (maxDist > 0.0f && e.distM >= 0.0f && e.distM * 100.0f > maxDist) continue;

        SDK::FVector loc{};
        loc.X = e.x; loc.Y = e.y; loc.Z = e.z;

        float halfH = 0.0f, capR = 0.0f;
        SDK::FVector head = loc, feet = loc;
        if (seh_capsule_half(actor, halfH, &capR)) { head.Z += halfH; feet.Z -= halfH; }
        else { head.Z += 180.0f; feet.Z -= 20.0f; }
        ScreenPos sHead{}, sFeet{};
        if (!project_cached(head, sHead) || !sHead.visible) continue;
        if (!project_cached(feet, sFeet) || !sFeet.visible) continue;

        float h = sFeet.y - sHead.y;
        if (h < 4.0f || h > 1200.0f) continue;
        float w;
        if (capR > 0.0f) {
            SDK::FVector left = loc, right = loc;
            left.X -= capR; right.X += capR;
            ScreenPos sL{}, sR{};
            if (project_cached(left, sL) && project_cached(right, sR)) {
                w = fabsf(sR.x - sL.x);
            } else {
                w = h * g_cfg.espBoxW.load();
            }
        } else {
            w = h * g_cfg.espBoxW.load();
        }
        float cx = (sHead.x + sFeet.x) * 0.5f;
        float x0 = cx - w * 0.5f, x1 = cx + w * 0.5f;
        float y0 = sHead.y, y1 = sFeet.y;

        int nhp = e.hp;
        bool dead = e.dead;
        if (dead && g_cfg.hideDead.load()) continue;
        bool alerted = e.alerted, armed = e.armed;
        ++g_counts.guards;
        ImU32 col = dead ? u32c(g_cfg.deadCol)
            : alerted ? IM_COL32(255, 60, 60, 255)
            : armed ? IM_COL32(255, 170, 60, 255)
            : IM_COL32(255, 200, 60, 255);
        if (!dead && g_cfg.visCheck.load() && !e.visible)
            col = dim_c(col);

        if (g_cfg.guardBox.load()) {
            if (g_cfg.boxFill.load()) box_fill(draw, x0, y0, x1, y1, col);
            draw_box_style(draw, g_cfg.espBoxStyle.load(), x0, y0, x1, y1, col);
        }
        if (g_cfg.guardSkel.load())
            draw_skeleton(actor, draw, col);
        if (g_cfg.guardState.load()) {
            const char* st = dead ? "DEAD" : alerted ? "ALERTED" : (armed ? "ARMED" : "PATROL");
            char buf[48];
            if (g_cfg.guardDistance.load() && e.distM >= 0.0f)
                snprintf(buf, sizeof(buf), "%s [%.0fm]", st, e.distM);
            else
                snprintf(buf, sizeof(buf), "%s", st);
            float lx, ly;
            anchor_pos(g_cfg.espLabelAnchor.load(), x0, y0, x1, y1,
                13.0f * s, 0, lx, ly);
            ImVec2 tsz = ImGui::CalcTextSize(buf);
            draw->AddText(ImVec2(lx - tsz.x * 0.5f, ly + g_cfg.espNameDy.load() * s),
                col, buf);
        }

        if (g_cfg.guardHealth.load()) {
            int maxHp = 100;
            float frac = std::clamp((float)nhp / (float)maxHp, 0.0f, 1.0f);
            int hs = g_cfg.healthStyle.load();
            bool left = g_cfg.hpLeft.load();
            float bx0 = left ? x0 - 6 : x1 + 2;
            float bx1 = left ? x0 - 2 : x1 + 6;
            if (hs != 2) {
                draw->AddRectFilled(ImVec2(bx0, y0), ImVec2(bx1, y1), IM_COL32(0, 0, 0, 160));
                ImU32 hc = health_color(frac);
                draw->AddRectFilled(ImVec2(bx0, y1 - h * frac), ImVec2(bx1, y1), hc);
            }
            if (hs != 1) {
                char buf[32]; snprintf(buf, sizeof(buf), "%d", nhp);
                text_centered(buf, cx, y1 + 4, IM_COL32(255,255,255,220), s * 0.9f);
            }
        }
    }
}

void render_npcs() {
    if (!g_cfg.espEnabled.load() || !g_cfg.guardEsp.load()) return;
    if (!g_frame.valid) return;

    auto* draw = ImGui::GetBackgroundDrawList();
    const float maxDist = g_cfg.guardMaxDist.load();
    float s = g_cfg.espTextScale.load();

    for (const GuardInfo& e : playerManager::npcs()) {
        auto* actor = reinterpret_cast<SDK::AActor*>(e.actor);
        if (!IS_VALID_PTR(actor) || e.guard) continue;
        if (maxDist > 0.0f && e.distM >= 0.0f && e.distM * 100.0f > maxDist) continue;

        SDK::FVector loc{};
        loc.X = e.x; loc.Y = e.y; loc.Z = e.z;

        float halfH = 0.0f, capR = 0.0f;
        SDK::FVector head = loc, feet = loc;
        if (seh_capsule_half(actor, halfH, &capR)) { head.Z += halfH; feet.Z -= halfH; }
        else { head.Z += 180.0f; feet.Z -= 20.0f; }
        ScreenPos sHead{}, sFeet{};
        if (!project_cached(head, sHead) || !sHead.visible) continue;
        if (!project_cached(feet, sFeet) || !sFeet.visible) continue;

        float h = sFeet.y - sHead.y;
        if (h < 4.0f || h > 1200.0f) continue;
        float w;
        if (capR > 0.0f) {
            SDK::FVector left = loc, right = loc;
            left.X -= capR; right.X += capR;
            ScreenPos sL{}, sR{};
            if (project_cached(left, sL) && project_cached(right, sR)) {
                w = fabsf(sR.x - sL.x);
            } else {
                w = h * g_cfg.espBoxW.load();
            }
        } else {
            w = h * g_cfg.espBoxW.load();
        }
        float cx = (sHead.x + sFeet.x) * 0.5f;
        float x0 = cx - w * 0.5f, x1 = cx + w * 0.5f;
        float y0 = sHead.y, y1 = sFeet.y;

        int nhp = e.hp;
        bool dead = e.dead;
        if (dead && g_cfg.hideDead.load()) continue;
        ++g_counts.guards;
        ImU32 col = dead ? u32c(g_cfg.deadCol) : u32c(g_cfg.npcCol);
        if (!dead && g_cfg.visCheck.load() && !e.visible)
            col = dim_c(col);

        if (g_cfg.guardBox.load()) {
            if (g_cfg.boxFill.load()) box_fill(draw, x0, y0, x1, y1, col);
            draw_box_style(draw, g_cfg.espBoxStyle.load(), x0, y0, x1, y1, col);
        }
        if (g_cfg.guardSkel.load())
            draw_skeleton(actor, draw, col);
        if (g_cfg.guardState.load()) {
            char buf[48];
            if (g_cfg.guardDistance.load() && e.distM >= 0.0f)
                snprintf(buf, sizeof(buf), "NPC %s [%.0fm]", dead ? "DEAD" : "ALIVE", e.distM);
            else
                snprintf(buf, sizeof(buf), "NPC %s", dead ? "DEAD" : "ALIVE");
            float lx, ly;
            anchor_pos(g_cfg.espLabelAnchor.load(), x0, y0, x1, y1,
                13.0f * s, 0, lx, ly);
            ImVec2 tsz = ImGui::CalcTextSize(buf);
            draw->AddText(ImVec2(lx - tsz.x * 0.5f, ly + g_cfg.espNameDy.load() * s),
                col, buf);
        }

        if (g_cfg.guardHealth.load()) {
            int maxHp = 100;
            float frac = std::clamp((float)nhp / (float)maxHp, 0.0f, 1.0f);
            int hs = g_cfg.healthStyle.load();
            bool left = g_cfg.hpLeft.load();
            float bx0 = left ? x0 - 6 : x1 + 2;
            float bx1 = left ? x0 - 2 : x1 + 6;
            if (hs != 2) {
                draw->AddRectFilled(ImVec2(bx0, y0), ImVec2(bx1, y1), IM_COL32(0, 0, 0, 160));
                ImU32 hc = health_color(frac);
                draw->AddRectFilled(ImVec2(bx0, y1 - h * frac), ImVec2(bx1, y1), hc);
            }
            if (hs != 1) {
                char buf[32]; snprintf(buf, sizeof(buf), "%d", nhp);
                text_centered(buf, cx, y1 + 4, IM_COL32(255,255,255,220), s * 0.9f);
            }
        }
    }
}

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
static void* seh_meshkey(SDK::USkeletalMeshComponent* m) {
    __try {
        void* asset = m->SkeletalMesh;
        return IS_VALID_PTR(asset) ? asset : m;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static SDK::FName seh_bonename(SDK::USkeletalMeshComponent* m, int i) {
    __try { return m->GetBoneName(i); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return SDK::FName{}; }
}

struct RawT { float q[4]; float p[4]; float s[4]; };
struct BoneBuf { RawT b[128]; int n; bool ok; };
static BoneBuf seh_bones(SDK::USkeletalMeshComponent* m) {
    BoneBuf r{}; r.ok = false; r.n = 0;
    __try {
        auto& arr = m->CachedComponentSpaceTransforms;
        int c = arr.Num();
        if (c <= 0) return r;
        if (c > 128) c = 128;
        const SDK::FTransform* d = arr.GetDataPtr();
        if (!d) return r;
        for (int i = 0; i < c; ++i)
            memcpy(&r.b[i], &d[i], sizeof(RawT));
        r.n = c; r.ok = true; return r;
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

enum Joint {
    J_PELVIS, J_SPINE1, J_SPINE2, J_SPINE3, J_NECK, J_HEAD,
    J_SH_L, J_EL_L, J_HAND_L, J_SH_R, J_EL_R, J_HAND_R,
    J_THIGH_L, J_CALF_L, J_FOOT_L, J_THIGH_R, J_CALF_R, J_FOOT_R,
    J_COUNT
};
static int joint_parent(int j) {
    switch (j) {
    case J_SPINE1: return J_PELVIS;
    case J_SPINE2: return J_SPINE1;
    case J_SPINE3: return J_SPINE2;
    case J_NECK: return J_SPINE3;
    case J_HEAD: return J_NECK;
    case J_SH_L: case J_SH_R: return J_SPINE3;
    case J_EL_L: return J_SH_L; case J_HAND_L: return J_EL_L;
    case J_EL_R: return J_SH_R; case J_HAND_R: return J_EL_R;
    case J_THIGH_L: case J_THIGH_R: return J_PELVIS;
    case J_CALF_L: return J_THIGH_L; case J_FOOT_L: return J_CALF_L;
    case J_CALF_R: return J_THIGH_R; case J_FOOT_R: return J_CALF_R;
    default: return J_PELVIS;
    }
}
struct JointMap { int idx[J_COUNT]; int n; bool ok; };
static bool is_center_slot(int j) { return j <= J_HEAD; }
static bool has_side_token(const std::string& n) {
    return n.find("_l") != std::string::npos || n.find("_r") != std::string::npos ||
        n.find("left") != std::string::npos || n.find("right") != std::string::npos;
}
static std::string norm_bone(std::string s) {
    for (auto& ch : s) {
        ch = (char)tolower((unsigned char)ch);
        if (ch == '-' || ch == '.' || ch == ' ') ch = '_';
    }
    return s;
}
static bool bone_string(const SDK::FName& n, std::string& out) {

    if (n.ComparisonIndex <= 0) return false;
    out = norm_bone(n.ToString());
    return !out.empty();
}

static const char* kPat[J_COUNT][12] = {
     { "pelvis", "hips", "hip", nullptr },
     { "spine_01", "spine_1", "spine1", nullptr },
     { "spine_02", "spine_2", "spine2", nullptr },
     { "spine_03", "spine_3", "spine3", "upper_chest", "upperchest", "chest", nullptr },
       { "neck_01", "neck_1", "neck", nullptr },
       { "head", nullptr },
       { "clavicle_l", "shoulder_l", "upperarm_l", "upper_arm_l", "arm_l", "clavicle_left", "shoulder_left", "leftshoulder", "leftclavicle", "leftarm", nullptr },
       { "forearm_l", "lowerarm_l", "lower_arm_l", "elbow_l", "forearm_left", "elbow_left", "leftforearm", "leftlowerarm", "leftelbow", nullptr },
     { "hand_l", "wrist_l", "palm_l", "hand_left", "wrist_left", "lefthand", "leftwrist", nullptr },
       { "clavicle_r", "shoulder_r", "upperarm_r", "upper_arm_r", "arm_r", "clavicle_right", "shoulder_right", "rightshoulder", "rightclavicle", "rightarm", nullptr },
       { "forearm_r", "lowerarm_r", "lower_arm_r", "elbow_r", "forearm_right", "elbow_right", "rightforearm", "rightlowerarm", "rightelbow", nullptr },
     { "hand_r", "wrist_r", "palm_r", "hand_right", "wrist_right", "righthand", "rightwrist", nullptr },
    { "thigh_l", "upperleg_l", "upper_leg_l", "leg_l", "hip_l", "thigh_left", "leg_left", "leftupleg", "leftthigh", nullptr },
     { "calf_l", "lowerleg_l", "lower_leg_l", "shin_l", "knee_l", "calf_left", "shin_left", "leftleg", "leftcalf", "leftknee", nullptr },
     { "foot_l", "ankle_l", "ball_l", "toe_l", "foot_left", "ankle_left", "leftfoot", "leftankle", "lefttoe", "lefttoebase", nullptr },
    { "thigh_r", "upperleg_r", "upper_leg_r", "leg_r", "hip_r", "thigh_right", "leg_right", "rightupleg", "rightthigh", nullptr },
     { "calf_r", "lowerleg_r", "lower_leg_r", "shin_r", "knee_r", "calf_right", "shin_right", "rightleg", "rightcalf", "rightknee", nullptr },
     { "foot_r", "ankle_r", "ball_r", "toe_r", "foot_right", "ankle_right", "rightfoot", "rightankle", "righttoe", "righttoebase", nullptr },
};

static const char* kBase[J_COUNT] = {
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    "clavicle", "forearm", "hand", "clavicle", "forearm", "hand",
    "thigh", "calf", "foot", "thigh", "calf", "foot",
};

static std::unordered_map<void*, JointMap>& rig_cache() {
    static std::unordered_map<void*, JointMap> c;
    return c;
}
static bool slot_match(int slot, const std::string& nm, bool centerOnly) {
    if (centerOnly && !is_center_slot(slot)) return false;
    if (centerOnly && has_side_token(nm)) return false;
    for (int p = 0; kPat[slot][p]; ++p)
        if (nm.find(kPat[slot][p]) != std::string::npos) return true;
    if (!centerOnly && kBase[slot] && !has_side_token(nm) &&
        nm.find(kBase[slot]) != std::string::npos) return true;
    return false;
}
static JointMap discover_rig(SDK::USkeletalMeshComponent* mesh, int numBones) {
    JointMap m{};
    for (int j = 0; j < J_COUNT; ++j) m.idx[j] = -1;
    m.n = 0; m.ok = false;
    int cap = numBones < 256 ? numBones : 256;

    for (int phase = 0; phase < 2; ++phase) {
        for (int i = 0; i < cap; ++i) {
            SDK::FName fn = seh_bonename(mesh, i);
            std::string nm;
            if (!bone_string(fn, nm)) continue;
            for (int s = 0; s < J_COUNT; ++s) {
                if (m.idx[s] >= 0) continue;
                if (slot_match(s, nm, phase == 0)) { m.idx[s] = i; break; }
            }
        }
    }

    for (int s = J_SPINE1; s <= J_SPINE3; ++s) {
        if (m.idx[s] >= 0) continue;
        for (int i = 0; i < cap; ++i) {
            SDK::FName fn = seh_bonename(mesh, i);
            std::string nm;
            if (!bone_string(fn, nm)) continue;
            if (!has_side_token(nm) && nm.find("spine") != std::string::npos) {
                bool taken = false;
                for (int t = J_SPINE1; t <= J_SPINE3; ++t)
                    if (m.idx[t] == i) { taken = true; break; }
                if (!taken) { m.idx[s] = i; break; }
            }
        }
    }
    m.n = numBones;
    m.ok = (m.idx[J_PELVIS] >= 0 && m.idx[J_NECK] >= 0 && m.idx[J_HEAD] >= 0);
    return m;
}

static void dump_skel_report(const char* why, SDK::USkeletalMeshComponent* mesh, int numBones, const JointMap& m) {
    FILE* f = nullptr;
    std::string p = fileManager::dumpPath("oar_bone_dump.txt");
    if (p.empty()) return;
    fopen_s(&f, p.c_str(), "w");
    if (!f) return;
    fprintf(f, "reason: %s\nnumBones: %d, ok: %d\nslots:", why, numBones, m.ok ? 1 : 0);
    for (int i = 0; i < J_COUNT; ++i) fprintf(f, " %d", m.idx[i]);
    fprintf(f, "\n\n");
    int cap = numBones < 128 ? numBones : 128;
    for (int i = 0; i < cap; ++i) {
        SDK::FName fn = seh_bonename(mesh, i);
        std::string nm;
        if (!bone_string(fn, nm)) nm = "<none>";
        fprintf(f, "%3d %s\n", i, nm.c_str());
    }
    fclose(f);
}

static bool valid_world(const SDK::FVector* w, const JointMap& m,
    const BoneBuf& b) {
    auto fin3 = [](const SDK::FVector& v) {
        return fin(v.X) && fin(v.Y) && fin(v.Z);
    };
    if (!fin3(w[J_PELVIS]) || !fin3(w[J_NECK]) || !fin3(w[J_HEAD]))
        return false;

    if (!(w[J_HEAD].Z + 5.0f >= w[J_NECK].Z && w[J_NECK].Z + 5.0f >= w[J_PELVIS].Z))
        return false;
    float hgt = w[J_HEAD].Z - w[J_PELVIS].Z;
    if (!(hgt > 15.0f && hgt < 500.0f)) return false;
    int fl = m.idx[J_FOOT_L], fr = m.idx[J_FOOT_R];
    if (fl >= 0 && fl < b.n && fr >= 0 && fr < b.n) {

        if (!(w[J_PELVIS].Z + 5.0f > w[J_FOOT_L].Z && w[J_PELVIS].Z + 5.0f > w[J_FOOT_R].Z))
            return false;
    }
    int hl = m.idx[J_HAND_L], hr = m.idx[J_HAND_R];
    if (hl >= 0 && hl < b.n && hr >= 0 && hr < b.n) {
        float dx = w[J_HAND_L].X - w[J_HAND_R].X;
        float dy = w[J_HAND_L].Y - w[J_HAND_R].Y;
        if (dx * dx + dy * dy < 25.0f) return false;
    }
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
    ++g_skel.drawn;
    static const int chains[5][6] = {
        { 0, 1, 2, 3, 4, 5 },
        { 3, 6, 7, 8, -1, -1 },
        { 3, 9, 10, 11, -1, -1 },
        { 0, 12, 13, 14, -1, -1 },
        { 0, 15, 16, 17, -1, -1 },
    };
    static const int lens[5] = { 6, 4, 4, 4, 4 };
    ImU32 c = (ImU32)col;
    ImVec2 seg[17][2];
    int nseg = 0;
    bool jd[18] = { false };
    for (int ci = 0; ci < 5; ++ci) {
        for (int k = 0; k + 1 < lens[ci]; ++k) {
            const SDK::FVector& a = j[chains[ci][k]];
            const SDK::FVector& b = j[chains[ci][k + 1]];
            if (a.X == b.X && a.Y == b.Y && a.Z == b.Z) continue;
            ScreenPos s0{}, s1{};
            if (!project_cached(a, s0) || !s0.visible) continue;
            if (!project_cached(b, s1) || !s1.visible) continue;
            float dx = s1.x - s0.x, dy = s1.y - s0.y;
            if (dx * dx + dy * dy < 4.0f) continue;
            if (nseg < 17) {
                seg[nseg][0] = ImVec2(s0.x, s0.y);
                seg[nseg][1] = ImVec2(s1.x, s1.y);
                ++nseg;
            }
            jd[chains[ci][k]] = true;
            jd[chains[ci][k + 1]] = true;
        }
    }
    ImU32 outline = IM_COL32(0, 0, 0, 210);
    for (int i = 0; i < nseg; ++i)
        draw->AddLine(seg[i][0], seg[i][1], outline, 3.6f);
    for (int i = 0; i < nseg; ++i)
        draw->AddLine(seg[i][0], seg[i][1], c, 1.8f);
    for (int ci = 0; ci < 5; ++ci) {
        for (int k = 0; k < lens[ci]; ++k) {
            int ji = chains[ci][k];
            if (ji < 0 || !jd[ji]) continue;
            jd[ji] = false;
            ScreenPos s{};
            if (!project_cached(j[ji], s) || !s.visible) continue;
            ImVec2 p(s.x, s.y);
            draw->AddCircleFilled(p, 2.4f, outline, 12);
            draw->AddCircleFilled(p, 1.4f, c, 12);
        }
    }
}

int skeleton_points(void* actorRaw, void* outLocations, int maxOut) {
    auto* out = static_cast<SDK::FVector*>(outLocations);
    if (!out || maxOut < 18) return 0;
    auto* actor = reinterpret_cast<SDK::AActor*>(actorRaw);
    if (!IS_VALID_PTR(actor)) return 0;
    ++g_skel.checked;
    auto* mesh = seh_skmesh(actor);
    if (!IS_VALID_PTR(mesh)) { ++g_skel.noMesh; return 0; }
    int numBones = seh_numbones(mesh);
    if (numBones <= 0 || numBones > 10000) { ++g_skel.fewBones; return 0; }
    void* key = seh_meshkey(mesh);
    if (!IS_VALID_PTR(key)) { ++g_skel.noMesh; return 0; }
    auto& cache = rig_cache();
    auto it = cache.find(key);
    if (it == cache.end() || it->second.n != numBones) {
        JointMap m = discover_rig(mesh, numBones);
        cache[key] = m;
        it = cache.find(key);
        dump_skel_report("discovery", mesh, numBones, m);
    }
    const JointMap& m = it->second;
    if (!m.ok) { ++g_skel.noJoints; return 0; }
    BoneBuf bb = seh_bones(mesh);
    if (!bb.ok) { ++g_skel.noXf; return 0; }
    for (int s = 0; s < J_COUNT; ++s) {
        if (m.idx[s] >= bb.n) { ++g_skel.noXf; return 0; }
    }
    RawT cw{};
    if (!seh_compworld(reinterpret_cast<SDK::USceneComponent*>(mesh), cw)) {
        ++g_skel.noXf;
        return 0;
    }
    float ql = cw.q[0] * cw.q[0] + cw.q[1] * cw.q[1] + cw.q[2] * cw.q[2] + cw.q[3] * cw.q[3];
    if (ql < 0.5f || ql > 2.0f) { ++g_skel.noXf; return 0; }
    SDK::FVector w[J_COUNT];
    for (int s = 0; s < J_COUNT; ++s) {
        int i = m.idx[s];
        w[s] = (i >= 0 && i < bb.n) ? xform(cw, bb.b[i]) : SDK::FVector{};
    }
    bool ok = valid_world(w, m, bb);
    if (!ok) {
        ++g_skel.rejected;
        dump_skel_report("valid_world rejected", mesh, numBones, m);

    }

    for (int s = 0; s < J_COUNT; ++s) {
        if (m.idx[s] < 0) w[s] = w[joint_parent(s)];
    }
    for (int i = 0; i < 18; ++i) out[i] = w[i];
    return 18;
}

}

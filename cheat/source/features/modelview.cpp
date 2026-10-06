#include "pch/pch.h"
#include "features/modelview.hpp"
#include "features/shop.hpp"
#include "features/esp.hpp"
#include "features/capture.hpp"
#include "core/config.hpp"
#include "core/actor_iter.hpp"
#include "core/veh.hpp"
#include "SDK/Basic.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/Engine_structs.hpp"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/PlayerCharacter_classes.hpp"
#include "SDK/ShopItem_Base_classes.hpp"
#include "SDK/NPCBase_classes.hpp"
#include "SDK/ShopItem_Base_classes.hpp"

#include <cmath>

#include "../../vendor/imgui/imgui.h"

namespace modelview {

// ---------- SEH leaves (no string/vector locals) ----------
static SDK::UObject* seh_cdo(SDK::UClass* cls) {
    __try { return cls->DefaultObject; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static SDK::UClass* seh_itembp(SDK::AShopItem_Base_C* it) {
    __try { return it->ItemBP; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static SDK::UActorComponent* seh_comp(SDK::AActor* a, SDK::UClass* cls) {
    __try { return a->GetComponentByClass(cls); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static SDK::UClass* seh_smcomp_static() {
    __try { return SDK::UStaticMeshComponent::StaticClass(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static SDK::UStaticMesh* seh_mesh_of(SDK::UStaticMeshComponent* c) {
    __try { return c->StaticMesh; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static SDK::UBodySetup* seh_body(SDK::UStaticMesh* m) {
    __try { return m->BodySetup; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
struct ArrSnap { const void* d; int n; bool ok; };
template<typename T>
static ArrSnap seh_arr(const SDK::TArray<T>& arr, int cap) {
    ArrSnap r{ nullptr, 0, false };
    __try {
        int n = arr.Num();
        if (n <= 0) return r;
        if (n > cap) n = cap;
        const T* d = arr.GetDataPtr();
        if (!d) return r;
        r.d = d; r.n = n; r.ok = true;
        return r;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return r; }
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

// ---------- item geometry cache (render thread only) ----------
struct BoxE { float c[3]; float rpy[3]; float e[3]; };
struct SphE { float c[3]; float r; };
struct SphylE { float c[3]; float rpy[3]; float r; float len; };
struct ItemGeo {
    void* cls = nullptr;
    std::vector<BoxE> boxes;
    std::vector<SphE> spheres;
    std::vector<SphylE> sphyls;
    float center[3] = { 0, 0, 0 };
    float radius = 1.0f;
    bool ok = false, tried = false;
};
static ItemGeo g_geo{};

static void resolve_item_geo(void* uclass) {
    g_geo = ItemGeo{};
    g_geo.cls = uclass;
    g_geo.tried = true;
    auto* cls = static_cast<SDK::UClass*>(uclass);
    if (!IS_VALID_PTR(cls)) return;
    SDK::UObject* cdo = seh_cdo(cls);
    if (!IS_VALID_PTR(cdo)) return;
    SDK::UClass* itemBp = seh_itembp(static_cast<SDK::AShopItem_Base_C*>(cdo));
    if (!IS_VALID_PTR(itemBp)) return;
    SDK::UObject* itemCdo = seh_cdo(itemBp);
    if (!IS_VALID_PTR(itemCdo)) return;
    SDK::UClass* smc = seh_smcomp_static();
    if (!IS_VALID_PTR(smc)) return;
    SDK::UActorComponent* comp = seh_comp(static_cast<SDK::AActor*>(itemCdo), smc);
    if (!IS_VALID_PTR(comp)) return;
    SDK::UStaticMesh* sm = seh_mesh_of(static_cast<SDK::UStaticMeshComponent*>(comp));
    if (!IS_VALID_PTR(sm)) return;
    SDK::UBodySetup* bs = seh_body(sm);
    if (!IS_VALID_PTR(bs)) return;
    // copy element arrays (capped)
    ArrSnap sb = seh_arr(bs->AggGeom.BoxElems, 64);
    if (sb.ok) {
        const SDK::FKBoxElem* d = static_cast<const SDK::FKBoxElem*>(sb.d);
        for (int i = 0; i < sb.n; ++i) {
            BoxE b{};
            b.c[0] = d[i].Center.X; b.c[1] = d[i].Center.Y; b.c[2] = d[i].Center.Z;
            b.rpy[0] = d[i].Rotation.Pitch; b.rpy[1] = d[i].Rotation.Yaw; b.rpy[2] = d[i].Rotation.Roll;
            b.e[0] = d[i].X; b.e[1] = d[i].Y; b.e[2] = d[i].Z;
            bool finite = true;
            for (int k = 0; k < 3; ++k)
                if (!(b.c[k] > -1e6f && b.c[k] < 1e6f && b.e[k] >= 0.f && b.e[k] < 1e6f)) finite = false;
            if (finite) g_geo.boxes.push_back(b);
        }
    }
    ArrSnap ss = seh_arr(bs->AggGeom.SphereElems, 32);
    if (ss.ok) {
        const SDK::FKSphereElem* d = static_cast<const SDK::FKSphereElem*>(ss.d);
        for (int i = 0; i < ss.n; ++i) {
            if (!(d[i].Radius > 0.f && d[i].Radius < 1e6f)) continue;
            SphE s{};
            s.c[0] = d[i].Center.X; s.c[1] = d[i].Center.Y; s.c[2] = d[i].Center.Z;
            s.r = d[i].Radius;
            g_geo.spheres.push_back(s);
        }
    }
    ArrSnap sp = seh_arr(bs->AggGeom.SphylElems, 32);
    if (sp.ok) {
        const SDK::FKSphylElem* d = static_cast<const SDK::FKSphylElem*>(sp.d);
        for (int i = 0; i < sp.n; ++i) {
            if (!(d[i].Radius > 0.f && d[i].Radius < 1e6f && d[i].Length >= 0.f && d[i].Length < 1e6f)) continue;
            SphylE s{};
            s.c[0] = d[i].Center.X; s.c[1] = d[i].Center.Y; s.c[2] = d[i].Center.Z;
            s.rpy[0] = d[i].Rotation.Pitch; s.rpy[1] = d[i].Rotation.Yaw; s.rpy[2] = d[i].Rotation.Roll;
            s.r = d[i].Radius; s.len = d[i].Length;
            g_geo.sphyls.push_back(s);
        }
    }
    if (g_geo.boxes.empty() && g_geo.spheres.empty() && g_geo.sphyls.empty()) return;
    // bbox center/extent over everything
    float mn[3] = { 1e9f, 1e9f, 1e9f }, mx[3] = { -1e9f, -1e9f, -1e9f };
    auto eat = [&](float x, float y, float z) {
        if (x < mn[0]) mn[0] = x; if (y < mn[1]) mn[1] = y; if (z < mn[2]) mn[2] = z;
        if (x > mx[0]) mx[0] = x; if (y > mx[1]) mx[1] = y; if (z > mx[2]) mx[2] = z;
    };
    for (auto& b : g_geo.boxes)
        for (int a = -1; a <= 1; a += 2)
            for (int c = -1; c <= 1; c += 2)
                for (int d = -1; d <= 1; d += 2)
                    eat(b.c[0] + a * b.e[0], b.c[1] + c * b.e[1], b.c[2] + d * b.e[2]);
    for (auto& s : g_geo.spheres)
        for (int a = -1; a <= 1; a += 2)
            eat(s.c[0] + a * s.r, s.c[1] + a * s.r, s.c[2] + a * s.r);
    for (auto& s : g_geo.sphyls) {
        eat(s.c[0], s.c[1], s.c[2] - s.len / 2 - s.r);
        eat(s.c[0], s.c[1], s.c[2] + s.len / 2 + s.r);
    }
    g_geo.center[0] = (mn[0] + mx[0]) / 2; g_geo.center[1] = (mn[1] + mx[1]) / 2; g_geo.center[2] = (mn[2] + mx[2]) / 2;
    float dx = mx[0] - mn[0], dy = mx[1] - mn[1], dz = mx[2] - mn[2];
    g_geo.radius = sqrtf(dx * dx + dy * dy + dz * dz) / 2;
    if (!(g_geo.radius > 0.5f && g_geo.radius < 1e6f)) return;
    g_geo.ok = true;
}

// FRotator(deg) -> 3x3, then orbit yaw/pitch + perspective
struct V3 { float x, y, z; };
static void rot_matrix(const float rpy[3], float m[3][3]) {
    const float D = 0.01745329252f;
    float sp = sinf(rpy[0] * D), cp = cosf(rpy[0] * D);
    float sy = sinf(rpy[1] * D), cy = cosf(rpy[1] * D);
    float sr = sinf(rpy[2] * D), cr = cosf(rpy[2] * D);
    m[0][0] = cp * cy; m[0][1] = cp * sy; m[0][2] = sp;
    m[1][0] = sr * sp * cy - cr * sy; m[1][1] = sr * sp * sy + cr * cy; m[1][2] = -sr * cp;
    m[2][0] = -(cr * sp * cy + sr * sy); m[2][1] = cy * sr - cr * sp * sy; m[2][2] = cr * cp;
}
static V3 xpt(const float m[3][3], float x, float y, float z, const float* c) {
    return { m[0][0] * x + m[0][1] * y + m[0][2] * z + c[0],
             m[1][0] * x + m[1][1] * y + m[1][2] * z + c[1],
             m[2][0] * x + m[2][1] * y + m[2][2] * z + c[2] };
}

static float g_yaw = 35.0f, g_pitch = -18.0f;
static bool g_dragging = false;

static void draw_item_model(ImVec2 origin, ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y),
        IM_COL32(10, 12, 22, 255), 6.0f);
    if (!g_geo.ok) {
        ImVec2 tsz = ImGui::CalcTextSize("no collision mesh");
        dl->AddText(ImVec2(origin.x + (size.x - tsz.x) / 2, origin.y + size.y / 2),
            IM_COL32(150, 156, 175, 255), "no collision mesh");
        return;
    }
    float yaw = g_yaw * 0.01745329252f, pit = g_pitch * 0.01745329252f;
    float cy = cosf(yaw), sy = sinf(yaw), cp = cosf(pit), sp = sinf(pit);
    auto proj = [&](V3 v, ImVec2& o) -> bool {
        // center, orbit, perspective
        float x = v.x - g_geo.center[0], y = v.y - g_geo.center[1], z = v.z - g_geo.center[2];
        float rx = x * cy - y * sy, ry = x * sy + y * cy;
        float ry2 = ry * cp - z * sp, rz = ry * sp + z * cp;
        float dist = g_geo.radius * 3.4f + 60.0f;
        float depth = dist + ry2;
        if (depth < 1.0f) return false;
        float f = 320.0f / depth;
        o = ImVec2(origin.x + size.x / 2 + rx * f, origin.y + size.y / 2 - rz * f);
        return true;
    };
    ImU32 col = IM_COL32(120, 200, 255, 255);
    for (auto& b : g_geo.boxes) {
        float m[3][3];
        rot_matrix(b.rpy, m);
        ImVec2 p[8];
        bool ok = true;
        int k = 0;
        for (int a = -1; a <= 1 && ok; a += 2)
            for (int c = -1; c <= 1 && ok; c += 2)
                for (int d = -1; d <= 1 && ok; d += 2) {
                    V3 w = xpt(m, (float)a * b.e[0], (float)c * b.e[1], (float)d * b.e[2], b.c);
                    // to viewer space relative to geo center handled in proj
                    if (!proj({ w.x, w.y, w.z }, p[k++])) ok = false;
                }
        if (!ok) continue;
        // recentre: proj already subtracts center; corners were absolute so fine
        static const int E[12][2] = { {0,1},{1,3},{3,2},{2,0},{4,5},{5,7},{7,6},{6,4},{0,4},{1,5},{2,6},{3,7} };
        for (auto& e : E) dl->AddLine(p[e[0]], p[e[1]], col, 1.4f);
    }
    for (auto& s : g_geo.spheres) {
        ImVec2 o;
        if (!proj({ s.c[0], s.c[1], s.c[2] }, o)) continue;
        float dist = g_geo.radius * 3.4f + 60.0f;
        dl->AddCircle(o, s.r * 320.0f / dist, col, 20, 1.4f);
    }
    for (auto& s : g_geo.sphyls) {
        float m[3][3];
        rot_matrix(s.rpy, m);
        // local Z axis in viewer space
        V3 a = xpt(m, 0, 0, -s.len / 2, s.c), e = xpt(m, 0, 0, s.len / 2, s.c);
        ImVec2 pa, pe;
        if (!proj(a, pa) || !proj(e, pe)) continue;
        dl->AddLine(pa, pe, col, 1.4f);
        float dist = g_geo.radius * 3.4f + 60.0f;
        dl->AddCircle(pa, s.r * 320.0f / dist, col, 16, 1.2f);
        dl->AddCircle(pe, s.r * 320.0f / dist, col, 16, 1.2f);
    }
}

// ---------- rig viewer: nearest actor's live joints, rotating ----------
static void* g_rigActor = nullptr;
static uint64_t g_rigTs = 0;
static char g_rigName[64] = { 0 };

static void* pick_rig() {
    uint64_t now = GetTickCount64();
    if (g_rigActor && now - g_rigTs < 1000 && IS_VALID_PTR(g_rigActor)) return g_rigActor;
    g_rigActor = nullptr;
    g_rigTs = now;
    auto snap = actors::copy_level_actors();
    void* self = actors::local_pawn();
    SDK::FVector me{};
    bool haveMe = IS_VALID_PTR(self) && seh_loc(static_cast<SDK::AActor*>(self), me);
    float best = 1e30f;
    for (void* r : snap.list) {
        auto* a = static_cast<SDK::AActor*>(r);
        if (!IS_VALID_PTR(a) || a == self) continue;
        bool p = false, n = false;
        if (!seh_isa<SDK::APlayerCharacter_C>(a, p)) continue;
        if (!p && !seh_isa<SDK::ANPCBase_C>(a, n)) continue;
        if (!p && !n) continue;
        SDK::FVector loc{};
        if (!seh_loc(a, loc)) continue;
        float d = haveMe ? (loc.X - me.X) * (loc.X - me.X) + (loc.Y - me.Y) * (loc.Y - me.Y) : 0.0f;
        if (d < best) { best = d; g_rigActor = a; }
    }
    if (IS_VALID_PTR(g_rigActor)) {
        std::string nm = veh::get_name(static_cast<SDK::UObject*>(g_rigActor));
        snprintf(g_rigName, sizeof(g_rigName), "%s", nm.empty() ? "?" : nm.c_str());
    } else g_rigName[0] = 0;
    return g_rigActor;
}

static void draw_rig(ImVec2 origin, ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y),
        IM_COL32(10, 12, 22, 255), 6.0f);
    void* a = pick_rig();
    if (!IS_VALID_PTR(a)) {
        ImVec2 tsz = ImGui::CalcTextSize("no rig in level");
        dl->AddText(ImVec2(origin.x + (size.x - tsz.x) / 2, origin.y + size.y / 2),
            IM_COL32(150, 156, 175, 255), "no rig in level");
        return;
    }
    // live textured capture when the pipeline has produced a frame
    SDK::FVector rloc{};
    if (seh_loc(static_cast<SDK::AActor*>(a), rloc))
        capture::set_target(rloc.X, rloc.Y, rloc.Z);
    if (capture::has_image()) {
        dl->AddImage((ImTextureID)capture::srv(), origin,
            ImVec2(origin.x + size.x, origin.y + size.y));
        ImVec2 nsz = ImGui::CalcTextSize(g_rigName);
        dl->AddText(ImVec2(origin.x + (size.x - nsz.x) / 2, origin.y + 8),
            IM_COL32(235, 238, 248, 255), g_rigName);
        ImVec2 lsz = ImGui::CalcTextSize("LIVE");
        dl->AddText(ImVec2(origin.x + size.x - lsz.x - 8, origin.y + 8),
            IM_COL32(140, 230, 170, 255), "LIVE");
        return;
    }
    SDK::FVector j[18];
    if (esp::skeleton_points(a, j, 18) != 18) {
        ImVec2 tsz = ImGui::CalcTextSize("rig rejected");
        dl->AddText(ImVec2(origin.x + (size.x - tsz.x) / 2, origin.y + size.y / 2),
            IM_COL32(255, 180, 120, 255), "rig rejected");
        return;
    }
    // centroid + radius
    float cx = 0, cy = 0, cz = 0;
    for (int i = 0; i < 18; ++i) { cx += j[i].X; cy += j[i].Y; cz += j[i].Z; }
    cx /= 18; cy /= 18; cz /= 18;
    float rad = 1.0f;
    for (int i = 0; i < 18; ++i) {
        float dx = j[i].X - cx, dy = j[i].Y - cy, dz = j[i].Z - cz;
        float d = sqrtf(dx * dx + dy * dy + dz * dz);
        if (d > rad) rad = d;
    }
    float yaw = g_yaw * 0.01745329252f;
    float cyy = cosf(yaw), syy = sinf(yaw);
    ImVec2 P[18];
    bool ok[18];
    for (int i = 0; i < 18; ++i) {
        float x = (j[i].X - cx) * cyy - (j[i].Y - cy) * syy;
        float y = (j[i].X - cx) * syy + (j[i].Y - cy) * cyy;
        float z = j[i].Z - cz;
        float dist = rad * 3.2f + 40.0f;
        float depth = dist + y;
        ok[i] = depth > 1.0f;
        float f = 300.0f / (depth > 1.0f ? depth : 1.0f);
        P[i] = ImVec2(origin.x + size.x / 2 + x * f, origin.y + size.y / 2 - z * f);
    }
    static const int CH[5][6] = { {0,1,2,3,4,5},{3,6,7,8,-1,-1},{3,9,10,11,-1,-1},{0,12,13,14,-1,-1},{0,15,16,17,-1,-1} };
    static const int LN[5] = { 6, 4, 4, 4, 4 };
    ImU32 col = IM_COL32(140, 220, 255, 255);
    for (int ci = 0; ci < 5; ++ci)
        for (int k = 0; k + 1 < LN[ci]; ++k) {
            int a0 = CH[ci][k], b0 = CH[ci][k + 1];
            if (ok[a0] && ok[b0]) dl->AddLine(P[a0], P[b0], col, 2.0f);
        }
    for (int i = 0; i < 18; ++i)
        if (ok[i]) dl->AddCircleFilled(P[i], 2.5f, col);
    ImVec2 nsz = ImGui::CalcTextSize(g_rigName);
    dl->AddText(ImVec2(origin.x + (size.x - nsz.x) / 2, origin.y + 8),
        IM_COL32(235, 238, 248, 255), g_rigName);
}

void draw_item_turntable(ImVec2 origin, ImVec2 size) {
    const auto& cat = shop::catalog(false);
    int sel = shop::selected();
    if (sel >= 0 && sel < (int)cat.size()) {
        if (g_geo.cls != cat[sel].cls) resolve_item_geo(cat[sel].cls);
    } else if (!g_geo.tried || g_geo.cls != nullptr) {
        g_geo = ItemGeo{};
        g_geo.tried = true;
    }
    ImGui::SetCursorScreenPos(origin);
    ImGui::InvisibleButton("##mvturn", size);
    g_dragging = ImGui::IsItemActive();
    if (g_dragging && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 d = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
        g_yaw += d.x * 0.4f;
        g_pitch -= d.y * 0.4f;
        if (g_pitch > 60) g_pitch = 60;
        if (g_pitch < -60) g_pitch = -60;
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
    } else if (!g_dragging) {
        g_yaw += ImGui::GetIO().DeltaTime * 24.0f;
        if (g_yaw >= 360.0f) g_yaw -= 360.0f;
    }
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    draw_item_model(origin, size);
}

void draw_model_popout() {
    if (!g_cfg.showModelViewer.load()) return;
    ImGui::SetNextWindowSize(ImVec2(360, 480), ImGuiCond_FirstUseEver);
    bool open = true;
    if (!ImGui::Begin("Model Viewer", &open,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::End();
        return;
    }
    if (!open) g_cfg.showModelViewer.store(false);
    static int mode = 0;
    const char* modes[2] = { "Item", "Rig" };
    ImVec2 cur0 = ImGui::GetCursorScreenPos();
    float bw = ImGui::GetContentRegionAvail().x;
    for (int i = 0; i < 2; ++i) {
        if (i > 0) { /* laid out below via cursor */ }
        ImVec2 bc(cur0.x + i * (bw / 2), cur0.y);
        ImGui::SetCursorScreenPos(bc);
        char id[16]; snprintf(id, sizeof(id), "##mv%d", i);
        if (ImGui::InvisibleButton(id, ImVec2(bw / 2 - 2, 26))) mode = i;
        bool act = (mode == i);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(bc, ImVec2(bc.x + bw / 2 - 2, bc.y + 26),
            act ? IM_COL32(70, 130, 230, 255) : IM_COL32(26, 31, 46, 255), 4.0f);
        ImVec2 tsz = ImGui::CalcTextSize(modes[i]);
        dl->AddText(ImVec2(bc.x + (bw / 2 - 2 - tsz.x) / 2, bc.y + 6),
            IM_COL32(255, 255, 255, 255), modes[i]);
    }
    ImGui::SetCursorScreenPos(ImVec2(cur0.x, cur0.y + 32));
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 size(ImGui::GetContentRegionAvail().x, 300);
    if (mode == 0) {
        draw_item_turntable(origin, size);
        ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + size.y + 6));
        ImGui::TextDisabled("%d boxes %d spheres (collision geometry)",
            (int)g_geo.boxes.size(), (int)g_geo.spheres.size());
    } else {
        // drag to rotate (shared yaw/pitch)
        ImGui::SetCursorScreenPos(origin);
        ImGui::InvisibleButton("##mvrot", size);
        g_dragging = ImGui::IsItemActive();
        if (g_dragging && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 d = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
            g_yaw += d.x * 0.4f;
            g_pitch -= d.y * 0.4f;
            if (g_pitch > 60) g_pitch = 60;
            if (g_pitch < -60) g_pitch = -60;
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        } else if (!g_dragging) {
            g_yaw += ImGui::GetIO().DeltaTime * 24.0f;
            if (g_yaw >= 360.0f) g_yaw -= 360.0f;
        }
        draw_rig(origin, size);
        ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + size.y + 6));
        ImGui::TextDisabled("live pose - nearest rig");
    }
    ImGui::End();
}

} // namespace modelview

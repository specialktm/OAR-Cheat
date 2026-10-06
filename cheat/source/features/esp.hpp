#pragma once
#include "pch/pch.h"

// Correct World->Screen. Primary path uses the engine's own
// APlayerController::ProjectWorldLocationToScreen (handles view/projection,
// viewport, aspect, stereo - no hand-rolled matrix math to get wrong).
// Implemented in esp.cpp where SDK is available.
namespace esp {

struct ScreenPos { float x = 0, y = 0; bool visible = false; };

bool world_to_screen(const void* worldLocation /*const SDK::FVector* */,
                     ScreenPos& out);

// Cached per-frame context to avoid re-resolving PC 2000x/frame.
struct FrameCtx {
    void* pc = nullptr;      // APlayerController*
    void* localPawn = nullptr;
    int vpW = 0, vpH = 0;
    bool valid = false;
};
FrameCtx begin_frame();

// Call once per Present before the render passes: single UWorld walk,
// single PC/viewport resolve, single counts reset shared by all passes.
void begin_present();

// Item + player + guard ESP entry points (called from Present hook).
void render_items();
void render_players();
void render_guards();

// Live per-frame entity counts (render thread only, for sidebar footer).
struct Counts { int players = 0, guards = 0, items = 0; };
Counts counts();

// Stylized guard silhouette for the style-lab preview (drawList = ImDrawList*).
void draw_guard_model(void* drawList, float cx, float cy, float scale, unsigned col, bool alerted);
// Real bone skeleton from the actor's skeletal mesh (self-validating rig).
void draw_skeleton(void* actor, void* drawList, unsigned col);
// Mannequin-order world joints (spine6, arms, legs); 18 or 0 if rig rejected.
int skeleton_points(void* actor, void* outLocations, int maxOut);
// Same rig without visor/ping, used for the player figure in previews.
void draw_player_model(void* drawList, float cx, float cy, float scale, unsigned col);
// Box styles shared by live ESP and the preview: 0 = 2D full, 1 = corners, 2 = 3D.
void draw_box_style(void* drawList, int style, float x0, float y0, float x1, float y1, unsigned col);
// Label anchor 0..7 (TC TR R BR BC BL L TL); line 0 = name, 1 = sub-line.
void anchor_pos(int anchor, float x0, float y0, float x1, float y1,
    float lh, int line, float& ox, float& oy);

} // namespace esp

#pragma once
#include "pch/pch.h"

namespace esp {

struct ScreenPos { float x = 0, y = 0; bool visible = false; };

bool world_to_screen(const void* worldLocation ,
                     ScreenPos& out);

struct FrameCtx {
    void* pc = nullptr;
    void* localPawn = nullptr;
    int vpW = 0, vpH = 0;
    bool valid = false;
};
FrameCtx begin_frame();

void begin_present();

void render_items();
void render_players();
void render_guards();
void render_npcs();

struct Counts { int players = 0, guards = 0, items = 0; };
Counts counts();

struct SkelDbg { int checked = 0, noMesh = 0, fewBones = 0, noXf = 0, noJoints = 0, rejected = 0, drawn = 0; };
SkelDbg skel_debug();

void draw_skeleton(void* actor, void* drawList, unsigned col);

int skeleton_points(void* actor, void* outLocations, int maxOut);

void draw_box_style(void* drawList, int style, float x0, float y0, float x1, float y1, unsigned col);

void anchor_pos(int anchor, float x0, float y0, float x1, float y1,
    float lh, int line, float& ox, float& oy);

}

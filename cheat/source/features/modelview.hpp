#pragma once
#include "pch/pch.h"
#include "../../vendor/imgui/imgui.h"
// 3D model popout: collision-true wireframes (AggGeom) for shop items and a
// rotating live-pose rig for players/NPCs. UE textures can't be sampled from
// RHI in-overlay, so geometry is exact and shading is flat + wireframe.
namespace modelview {
void draw_model_popout(); // "Model" window with Item | Rig pills
void draw_item_turntable(ImVec2 origin, ImVec2 size); // embeddable 3D view
} // namespace modelview

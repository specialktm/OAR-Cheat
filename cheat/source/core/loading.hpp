#pragma once
#include "pch/pch.h"

namespace loading {

void begin();
void stage(int idx, const char* label);
void finish();
bool done();
bool visible();
void render();

}

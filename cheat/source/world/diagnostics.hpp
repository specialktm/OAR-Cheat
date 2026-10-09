#pragma once
#include "pch/pch.h"

namespace diag {

struct Report {
    uintptr_t imageBase = 0;
    bool gwSigHit = false;
    uintptr_t gwRva = 0, gwAbs = 0, gwPtr = 0;
    bool gwValid = false;
    bool goSigHit = false;
    uintptr_t goRva = 0, goAbs = 0;
    int goNum = -1;
    bool peSigHit = false;
    uintptr_t peAbs = 0;
    bool pcValid = false;
    void* presentTarget = nullptr;
    bool presentValid = false;
    void* resizeTarget = nullptr;
    bool resizeValid = false;
};

void report();
Report last();

}

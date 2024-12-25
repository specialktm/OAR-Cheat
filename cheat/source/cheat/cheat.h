#pragma once
#include "utils/utility.h"

inline bool programRunning = true;

class cheat : public Baseclass {
public:
    void Initialize() override;
    void Uninitialize() override;

    void tick();
    void tick2();

    std::string ClassName() override
    {
        return "cheat";
    }
};

inline cheat* g_cheat; 
#pragma once
#include "pch/pch.h"
#include "cheat/base/Baseclass.hpp"
#include "cheat/utils/utility.h"

class cheat : public Baseclass {
public:
    void Initialize() override;
    void Uninitialize() override;
    void tick();
    std::string ClassName() override { return "cheat"; }
private:
    std::atomic<bool> didInitDx = false;
};
inline cheat* g_cheat = nullptr;


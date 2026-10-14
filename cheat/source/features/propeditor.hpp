#pragma once
#include "pch/pch.h"
#include <string>
#include <vector>

namespace proped {

struct Prop {
    std::string name;
    std::string type;
    int kind = 0;
    int32_t offset = -1;
    int32_t size = 0;
    uint8_t mask = 0;
};

bool layout_verified();
const std::vector<Prop>& props_for(void* cls);
bool read_value(void* obj, const Prop& p, std::string& out);
bool write_value(void* obj, const Prop& p, const std::string& text, std::string& err);

}

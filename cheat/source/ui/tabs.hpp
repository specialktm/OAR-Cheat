#pragma once
#include "pch/pch.h"

namespace tabs {

struct Tab {
    const char* id;
    const char* title;
    void (*draw)();
};

void add(Tab t);
const std::vector<Tab>& list();
int& current();

}

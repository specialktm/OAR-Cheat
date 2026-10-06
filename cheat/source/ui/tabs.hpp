#pragma once
#include "pch/pch.h"
// Tab registry: each tab renders itself; the shell just iterates.
// No if-chains in menu code: to add a tab, add one file under ui/tabs/.
namespace tabs {

struct Tab {
    const char* id;
    const char* title;
    void (*draw)();
};

void add(Tab t);
const std::vector<Tab>& list();
int& current();

} // namespace tabs

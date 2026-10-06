#include "pch/pch.h"
#include "ui/tabs.hpp"

namespace tabs {

static std::vector<Tab>& reg() {
    static std::vector<Tab> v;
    return v;
}

void add(Tab t) { reg().push_back(t); }
const std::vector<Tab>& list() { return reg(); }
int& current() {
    static int idx = 0;
    return idx;
}

} // namespace tabs

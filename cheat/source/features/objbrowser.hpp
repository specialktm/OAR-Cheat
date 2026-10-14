#pragma once
#include "pch/pch.h"
#include <string>
#include <vector>

namespace objbrowser {
struct Row {
    int index = -1;
    void* obj = nullptr;
    std::string name;
    std::string cls;
};
void scan();
void cancel();
bool running();
float progress();
int total();
std::vector<Row> rows(const char* filter, int cap);
bool resolve(int index, Row& out);
}

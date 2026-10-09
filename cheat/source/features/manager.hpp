#pragma once
#include "pch/pch.h"

namespace feat {

struct Entry {
    const char* id;
    const char* name;
    const char* desc;
    const char* hotkey;
    const char* tab;
    void (*run)();
    std::atomic<bool>* autoFlag;
};

void register_builtin();
void add(Entry e);
const std::vector<const Entry*>& by_tab(const char* tab);
const std::vector<const Entry*>& all();

std::atomic<int>* key_for(const char* id);
bool run(const char* id);
void tick_autos();

void queue_run(const char* id);
void queue_fn(std::function<void()> fn);
void queue_keyed(const char* key, std::function<void()> fn);
void run_queued();

void queue_game(std::function<void()> fn);
bool ensure_gamehook(void* obj);
void shutdown_gamehook();

}

#include "pch/pch.h"
#include "features/combatlog.hpp"
#include "features/entities.hpp"
#include "config/config.hpp"
#include "ui/tabs.hpp"
#include "ui/take_shell.hpp"
#include "../../vendor/imgui/imgui.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <chrono>
#include <cstring>

namespace combat {
namespace {
std::mutex mtx;
std::vector<LEntry> log;
std::unordered_map<void*, int> prevHp;
std::unordered_map<void*, bool> prevDead;
constexpr std::size_t CAP = 32;

void push(bool kill, const std::string& name, const std::string& part, int dmg) {
    std::lock_guard<std::mutex> lk(mtx);
    LEntry e;
    e.kill = kill;
    e.name = name;
    e.part = part;
    e.dmg = dmg;
    e.at = std::chrono::steady_clock::now();
    log.push_back(e);
    while (log.size() > CAP) log.erase(log.begin());
}
}

void tick() {
    struct HitEv { bool kill = false; char name[48] = {}; int dmg = 0; };
    HitEv ev[16];
    int n = 0;
    {
        const auto& ps = esp::playerManager::players();
        size_t nn = ps.size();
        for (size_t i = 0; i < nn && n < 16; ++i) {
            const auto& e = ps[i];
            if (e.self || !IS_VALID_PTR(e.actor)) continue;
            int prev = 0; bool wasDead = false;
            auto it = prevHp.find(e.actor);
            bool known = (it != prevHp.end());
            if (known) {
                prev = it->second;
                auto jt = prevDead.find(e.actor);
                wasDead = (jt != prevDead.end()) ? jt->second : false;
            }
            prevHp[e.actor] = e.hp;
            prevDead[e.actor] = false;
            if (!known) continue;
            if (e.hp <= 0 && prev > 0) {
                ev[n].kill = true;
                strncpy_s(ev[n].name, e.name.c_str(), 47);
                ev[n].dmg = prev;
                ++n;
            } else if (e.hp < prev) {
                ev[n].kill = false;
                strncpy_s(ev[n].name, e.name.c_str(), 47);
                ev[n].dmg = prev - e.hp;
                ++n;
            }
        }
        const auto& gs = esp::playerManager::npcs();
        size_t mm = gs.size();
        for (size_t i = 0; i < mm && n < 16; ++i) {
            const auto& e = gs[i];
            if (!IS_VALID_PTR(e.actor)) continue;
            int prev = 0; bool wasDead = false;
            auto it = prevHp.find(e.actor);
            bool known = (it != prevHp.end());
            if (known) {
                prev = it->second;
                auto jt = prevDead.find(e.actor);
                wasDead = (jt != prevDead.end()) ? jt->second : e.dead;
            }
            prevHp[e.actor] = e.hp;
            prevDead[e.actor] = e.dead;
            if (!known) continue;
            if (e.dead && !wasDead) {
                ev[n].kill = true;
                strncpy_s(ev[n].name, e.label.empty() ? "NPC" : e.label.c_str(), 47);
                ev[n].dmg = prev;
                ++n;
            } else if (!e.dead && e.hp < prev) {
                ev[n].kill = false;
                strncpy_s(ev[n].name, e.label.empty() ? "NPC" : e.label.c_str(), 47);
                ev[n].dmg = prev - e.hp;
                ++n;
            }
        }
    }
    for (int i = 0; i < n; ++i) push(ev[i].kill, ev[i].name, "", ev[i].dmg);
    if (prevHp.size() > 512) {
        prevHp.clear();
        prevDead.clear();
    }
}


std::vector<LEntry> entries() {
    std::lock_guard<std::mutex> lk(mtx);
    return log;
}

void clear() {
    std::lock_guard<std::mutex> lk(mtx);
    log.clear();
    prevHp.clear();
    prevDead.clear();
}

}

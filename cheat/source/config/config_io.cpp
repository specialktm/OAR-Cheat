#include "pch/pch.h"
#include "config/config_io.hpp"
#include "config/config.hpp"
#include "util/files.hpp"

#include <fstream>
#include <sstream>
#include <filesystem>

namespace cfgio {

std::string path_for(const std::string& name) {
    return fileManager::configPath(name);
}

static void put(std::ostringstream& o, const char* k, bool v) {
    o << k << "=" << (v ? 1 : 0) << "\n";
}
static void put(std::ostringstream& o, const char* k, int v) { o << k << "=" << v << "\n"; }
static void put(std::ostringstream& o, const char* k, float v) { o << k << "=" << v << "\n"; }
static void put4(std::ostringstream& o, const char* k, const ImVec4& c) {
    o << k << "=" << c.x << " " << c.y << " " << c.z << " " << c.w << "\n";
}

bool save(const std::string& name, std::string& err) {
    std::ostringstream o;
    o << "# OAR config v2\n";
    put(o, "itemEsp", g_cfg.itemEsp.load());
    put(o, "moneyEsp", g_cfg.moneyEsp.load());
    put(o, "pickupEsp", g_cfg.pickupEsp.load());
    put(o, "keycardEsp", g_cfg.keycardEsp.load());
    put(o, "itemShowDistance", g_cfg.itemShowDistance.load());
    put(o, "itemShowValue", g_cfg.itemShowValue.load());
    put(o, "playerEsp", g_cfg.playerEsp.load());
    put(o, "playerBox", g_cfg.playerBox.load());
    put(o, "playerName", g_cfg.playerName.load());
    put(o, "playerHealth", g_cfg.playerHealth.load());
    put(o, "playerDistance", g_cfg.playerDistance.load());
    put(o, "playerSkel", g_cfg.playerSkel.load());
    put(o, "guardEsp", g_cfg.guardEsp.load());
    put(o, "guardBox", g_cfg.guardBox.load());
    put(o, "guardState", g_cfg.guardState.load());
    put(o, "guardDistance", g_cfg.guardDistance.load());
    put(o, "guardSkel", g_cfg.guardSkel.load());
    put(o, "playerHolding", g_cfg.playerHolding.load());
    put(o, "guardHealth", g_cfg.guardHealth.load());
    put(o, "visCheck", g_cfg.visCheck.load());
    put(o, "hideDead", g_cfg.hideDead.load());
    put(o, "itemBox", g_cfg.itemBox.load());
    put(o, "boxFill", g_cfg.boxFill.load());
    put(o, "hpLeft", g_cfg.hpLeft.load());
    put(o, "espEnabled", g_cfg.espEnabled.load());
    put(o, "autoFreeShop", g_cfg.autoFreeShop.load());
    put(o, "autoPhones", g_cfg.autoPhones.load());
    put(o, "autoCameras", g_cfg.autoCameras.load());
    put(o, "autoDoors", g_cfg.autoDoors.load());
    put(o, "watermark", g_cfg.watermark.load());
    put(o, "clock24h", g_cfg.clock24h.load());
    put(o, "particles", g_cfg.particles.load());
    put(o, "blurBackdrop", g_cfg.blurBackdrop.load());
    put(o, "blurAmount", g_cfg.blurAmount.load());
    put(o, "godMode", g_cfg.godMode.load());
    put(o, "itemMaxDist", g_cfg.itemMaxDist.load());
    put(o, "playerMaxDist", g_cfg.playerMaxDist.load());
    put(o, "guardMaxDist", g_cfg.guardMaxDist.load());
    put(o, "espBoxW", g_cfg.espBoxW.load());
    put(o, "espNameDy", g_cfg.espNameDy.load());
    put(o, "espPrevX", g_cfg.espPrevX.load());
    put(o, "espPrevY", g_cfg.espPrevY.load());
    put(o, "espTextScale", g_cfg.espTextScale.load());
    put(o, "autoIntervalSec", g_cfg.autoIntervalSec.load());
    put(o, "espBoxStyle", g_cfg.espBoxStyle.load());
    put(o, "espLabelAnchor", g_cfg.espLabelAnchor.load());
    put(o, "espSubAnchor", g_cfg.espSubAnchor.load());
    put(o, "healthStyle", g_cfg.healthStyle.load());
    put(o, "nameMode", g_cfg.nameMode.load());
    put(o, "uiStyle", g_cfg.uiStyle.load());
    put(o, "themeIdx", g_cfg.themeIdx.load());
    put(o, "keyMenu", g_cfg.keyMenu.load());
    put(o, "keyUnload", g_cfg.keyUnload.load());
    put(o, "keyMaps", g_cfg.keyMaps.load());
    put(o, "keyShop", g_cfg.keyShop.load());
    put(o, "keySkills", g_cfg.keySkills.load());
    put(o, "keyCoins", g_cfg.keyCoins.load());
    put(o, "keyCrates", g_cfg.keyCrates.load());
    put(o, "keyVacuum", g_cfg.keyVacuum.load());
    put(o, "keyPhones", g_cfg.keyPhones.load());
    put(o, "keyCameras", g_cfg.keyCameras.load());
    put(o, "keyDoors", g_cfg.keyDoors.load());
    put(o, "keyGuns", g_cfg.keyGuns.load());
    put(o, "keyModeMenu", g_cfg.keyModeMenu.load());
    put(o, "keyModeMaps", g_cfg.keyModeMaps.load());
    put(o, "keyModeShop", g_cfg.keyModeShop.load());
    put(o, "keyModeSkills", g_cfg.keyModeSkills.load());
    put(o, "keyModeCoins", g_cfg.keyModeCoins.load());
    put(o, "keyModeCrates", g_cfg.keyModeCrates.load());
    put(o, "keyModeVacuum", g_cfg.keyModeVacuum.load());
    put(o, "keyModePhones", g_cfg.keyModePhones.load());
    put(o, "keyModeCameras", g_cfg.keyModeCameras.load());
    put(o, "keyModeDoors", g_cfg.keyModeDoors.load());
    put(o, "keyModeGuns", g_cfg.keyModeGuns.load());
    put4(o, "playerCol", g_cfg.playerCol);
    put4(o, "guardCol", g_cfg.guardCol);
    put4(o, "itemCol", g_cfg.itemCol);
    put4(o, "moneyCol", g_cfg.moneyCol);
    put4(o, "npcCol", g_cfg.npcCol);
    put4(o, "deadCol", g_cfg.deadCol);
    put4(o, "healthCol", g_cfg.healthCol);
    put4(o, "customA", g_cfg.customA);
    put4(o, "customB", g_cfg.customB);
    put4(o, "customGlow", g_cfg.customGlow);

    std::string path = path_for(name);
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    std::ofstream f(path, std::ios::trunc);
    if (!f) { err = "cannot write " + path; return false; }
    f << o.str();
    f.close();
    return true;
}

static bool get_bool(const std::string& v, bool& out) {
    if (v == "1" || v == "true") { out = true; return true; }
    if (v == "0" || v == "false") { out = false; return true; }
    return false;
}

bool load(const std::string& name, std::string& err) {
    std::string path = path_for(name);
    std::ifstream f(path);
    if (!f) { err = "not found: " + path; return false; }
    std::string line;
    int applied = 0;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        auto sb = [&](std::atomic<bool>& a) { bool b; if (get_bool(v, b)) { a.store(b); ++applied; } };
        auto si = [&](std::atomic<int>& a) {
            try { a.store(std::stoi(v)); ++applied; } catch (...) {}
        };
        auto sf = [&](std::atomic<float>& a) {
            try { a.store(std::stof(v)); ++applied; } catch (...) {}
        };
        auto s4 = [&](ImVec4& c) {
            std::istringstream s(v);
            float x, y, z, w;
            if (s >> x >> y >> z >> w) { c = ImVec4(x, y, z, w); ++applied; }
        };
        if (k == "itemEsp") sb(g_cfg.itemEsp);
        else if (k == "moneyEsp") sb(g_cfg.moneyEsp);
        else if (k == "pickupEsp") sb(g_cfg.pickupEsp);
        else if (k == "keycardEsp") sb(g_cfg.keycardEsp);
        else if (k == "itemShowDistance") sb(g_cfg.itemShowDistance);
        else if (k == "itemShowValue") sb(g_cfg.itemShowValue);
        else if (k == "playerEsp") sb(g_cfg.playerEsp);
        else if (k == "playerBox") sb(g_cfg.playerBox);
        else if (k == "playerName") sb(g_cfg.playerName);
        else if (k == "playerHealth") sb(g_cfg.playerHealth);
        else if (k == "playerDistance") sb(g_cfg.playerDistance);
        else if (k == "playerSkel") sb(g_cfg.playerSkel);
        else if (k == "guardEsp") sb(g_cfg.guardEsp);
        else if (k == "guardBox") sb(g_cfg.guardBox);
        else if (k == "guardState") sb(g_cfg.guardState);
        else if (k == "guardDistance") sb(g_cfg.guardDistance);
        else if (k == "guardSkel") sb(g_cfg.guardSkel);
        else if (k == "playerHolding") sb(g_cfg.playerHolding);
        else if (k == "guardHealth") sb(g_cfg.guardHealth);
        else if (k == "visCheck") sb(g_cfg.visCheck);
        else if (k == "hideDead") sb(g_cfg.hideDead);
        else if (k == "itemBox") sb(g_cfg.itemBox);
        else if (k == "boxFill") sb(g_cfg.boxFill);
        else if (k == "hpLeft") sb(g_cfg.hpLeft);
        else if (k == "espEnabled") sb(g_cfg.espEnabled);
        else if (k == "autoFreeShop") sb(g_cfg.autoFreeShop);
        else if (k == "autoPhones") sb(g_cfg.autoPhones);
        else if (k == "autoCameras") sb(g_cfg.autoCameras);
        else if (k == "autoDoors") sb(g_cfg.autoDoors);
        else if (k == "watermark") sb(g_cfg.watermark);
        else if (k == "clock24h") sb(g_cfg.clock24h);
        else if (k == "particles") sb(g_cfg.particles);
        else if (k == "blurBackdrop") sb(g_cfg.blurBackdrop);
        else if (k == "blurAmount") sf(g_cfg.blurAmount);
        else if (k == "godMode") sb(g_cfg.godMode);
        else if (k == "itemMaxDist") sf(g_cfg.itemMaxDist);
        else if (k == "playerMaxDist") sf(g_cfg.playerMaxDist);
        else if (k == "guardMaxDist") sf(g_cfg.guardMaxDist);
        else if (k == "espBoxW") sf(g_cfg.espBoxW);
        else if (k == "espNameDy") sf(g_cfg.espNameDy);
        else if (k == "espPrevX") sf(g_cfg.espPrevX);
        else if (k == "espPrevY") sf(g_cfg.espPrevY);
        else if (k == "espTextScale") sf(g_cfg.espTextScale);
        else if (k == "autoIntervalSec") sf(g_cfg.autoIntervalSec);
        else if (k == "espBoxStyle") si(g_cfg.espBoxStyle);
        else if (k == "espLabelAnchor") si(g_cfg.espLabelAnchor);
        else if (k == "espSubAnchor") si(g_cfg.espSubAnchor);
        else if (k == "healthStyle") si(g_cfg.healthStyle);
        else if (k == "nameMode") si(g_cfg.nameMode);
        else if (k == "uiStyle") si(g_cfg.uiStyle);
        else if (k == "themeIdx") si(g_cfg.themeIdx);
        else if (k == "keyMenu") si(g_cfg.keyMenu);
        else if (k == "keyUnload") si(g_cfg.keyUnload);
        else if (k == "keyMaps") si(g_cfg.keyMaps);
        else if (k == "keySkills") si(g_cfg.keySkills);
        else if (k == "keyShop") si(g_cfg.keyShop);
        else if (k == "keyCoins") si(g_cfg.keyCoins);
        else if (k == "keyCrates") si(g_cfg.keyCrates);
        else if (k == "keyVacuum") si(g_cfg.keyVacuum);
        else if (k == "keyPhones") si(g_cfg.keyPhones);
        else if (k == "keyCameras") si(g_cfg.keyCameras);
        else if (k == "keyDoors") si(g_cfg.keyDoors);
        else if (k == "keyGuns") si(g_cfg.keyGuns);
        else if (k == "keyModeMenu") si(g_cfg.keyModeMenu);
        else if (k == "keyModeMaps") si(g_cfg.keyModeMaps);
        else if (k == "keyModeShop") si(g_cfg.keyModeShop);
        else if (k == "keyModeSkills") si(g_cfg.keyModeSkills);
        else if (k == "keyModeCoins") si(g_cfg.keyModeCoins);
        else if (k == "keyModeCrates") si(g_cfg.keyModeCrates);
        else if (k == "keyModeVacuum") si(g_cfg.keyModeVacuum);
        else if (k == "keyModePhones") si(g_cfg.keyModePhones);
        else if (k == "keyModeCameras") si(g_cfg.keyModeCameras);
        else if (k == "keyModeDoors") si(g_cfg.keyModeDoors);
        else if (k == "keyModeGuns") si(g_cfg.keyModeGuns);
        else if (k == "playerCol") s4(g_cfg.playerCol);
        else if (k == "guardCol") s4(g_cfg.guardCol);
        else if (k == "itemCol") s4(g_cfg.itemCol);
        else if (k == "moneyCol") s4(g_cfg.moneyCol);
        else if (k == "npcCol") s4(g_cfg.npcCol);
        else if (k == "deadCol") s4(g_cfg.deadCol);
        else if (k == "healthCol") s4(g_cfg.healthCol);
        else if (k == "customA") s4(g_cfg.customA);
        else if (k == "customB") s4(g_cfg.customB);
        else if (k == "customGlow") s4(g_cfg.customGlow);
    }
    if (applied == 0) { err = "empty config"; return false; }
    return true;
}

std::vector<std::string> list() {
    return fileManager::listConfigs();
}

}

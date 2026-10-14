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
    put(o, "playerChams", g_cfg.playerChams.load());
    put(o, "playerGlow", g_cfg.playerGlow.load());
    put(o, "silChams", g_cfg.silChams.load());
    put(o, "silChamsG", g_cfg.silChamsG.load());
    put(o, "guardChams", g_cfg.guardChams.load());
    put(o, "guardGlow", g_cfg.guardGlow.load());
    put(o, "hpLeft", g_cfg.hpLeft.load());
    put(o, "espEnabled", g_cfg.espEnabled.load());
    put(o, "autoFreeShop", g_cfg.autoFreeShop.load());
    put(o, "watermark", g_cfg.watermark.load());
    put(o, "clock24h", g_cfg.clock24h.load());
    put(o, "particles", g_cfg.particles.load());
    put(o, "blurBackdrop", g_cfg.blurBackdrop.load());
    put(o, "blurAmount", g_cfg.blurAmount.load());
    put(o, "borderFx", g_cfg.borderFx.load());
    put(o, "borderRainbow", g_cfg.borderRainbow.load());
    put(o, "keybindList", g_cfg.keybindList.load());
    put(o, "objClassesOnly", g_cfg.objClassesOnly.load());
    put(o, "playerCountBox", g_cfg.playerCountBox.load());
    put(o, "crosshair", g_cfg.crosshair.load());
    put(o, "pcX", g_cfg.pcX.load());
    put(o, "pcY", g_cfg.pcY.load());
    put(o, "killfeed", g_cfg.killfeed.load());
    put(o, "hitlog", g_cfg.hitlog.load());
    put(o, "glassBlur", g_cfg.glassBlur.load());
    put4(o, "glassTint", g_cfg.glassTint);
    put(o, "radar", g_cfg.radar.load());
    put(o, "radarRange", g_cfg.radarRange.load());
    put(o, "kbX", g_cfg.kbX.load());
    put(o, "kbY", g_cfg.kbY.load());
    put(o, "logX", g_cfg.logX.load());
    put(o, "logY", g_cfg.logY.load());
    put(o, "killX", g_cfg.killX.load());
    put(o, "killY", g_cfg.killY.load());
    put(o, "godMode", g_cfg.godMode.load());
    put(o, "itemMaxDist", g_cfg.itemMaxDist.load());
    put(o, "playerMaxDist", g_cfg.playerMaxDist.load());
    put(o, "guardMaxDist", g_cfg.guardMaxDist.load());
    put(o, "timeDilation", g_cfg.timeDilation.load());
    put(o, "fov", g_cfg.fov.load());
    put(o, "espBoxW", g_cfg.espBoxW.load());
    put(o, "espNameDy", g_cfg.espNameDy.load());
    put(o, "espPrevX", g_cfg.espPrevX.load());
    put(o, "espPrevY", g_cfg.espPrevY.load());
    put(o, "espTextScale", g_cfg.espTextScale.load());
    put(o, "espBoxStyle", g_cfg.espBoxStyle.load());
    put(o, "espLabelAnchor", g_cfg.espLabelAnchor.load());
    put(o, "espSubAnchor", g_cfg.espSubAnchor.load());
    put(o, "healthStyle", g_cfg.healthStyle.load());
    put(o, "nameMode", g_cfg.nameMode.load());
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
    put(o, "keyComplete", g_cfg.keyComplete.load());
    put(o, "keyWingame", g_cfg.keyWingame.load());
    put(o, "keyAmmo", g_cfg.keyAmmo.load());
    put(o, "keyXp", g_cfg.keyXp.load());
    put(o, "keyImmunity", g_cfg.keyImmunity.load());
    put(o, "keyDance", g_cfg.keyDance.load());
    put(o, "keyKickself", g_cfg.keyKickself.load());
    put(o, "keyFloppy", g_cfg.keyFloppy.load());
    put(o, "keyCops", g_cfg.keyCops.load());
    put(o, "keyRandmask", g_cfg.keyRandmask.load());
    put(o, "keyEndtimer", g_cfg.keyEndtimer.load());
    put(o, "keyCheats", g_cfg.keyCheats.load());
    put(o, "keyGhost", g_cfg.keyGhost.load());
    put(o, "keyPlayersonly", g_cfg.keyPlayersonly.load());
    put(o, "keyDebugcam", g_cfg.keyDebugcam.load());
    put(o, "keyDepthchams", g_cfg.keyDepthchams.load());
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
    put(o, "keyModeComplete", g_cfg.keyModeComplete.load());
    put(o, "keyModeWingame", g_cfg.keyModeWingame.load());
    put(o, "keyModeAmmo", g_cfg.keyModeAmmo.load());
    put(o, "keyModeXp", g_cfg.keyModeXp.load());
    put(o, "keyModeImmunity", g_cfg.keyModeImmunity.load());
    put(o, "keyModeDance", g_cfg.keyModeDance.load());
    put(o, "keyModeKickself", g_cfg.keyModeKickself.load());
    put(o, "keyModeFloppy", g_cfg.keyModeFloppy.load());
    put(o, "keyModeCops", g_cfg.keyModeCops.load());
    put(o, "keyModeRandmask", g_cfg.keyModeRandmask.load());
    put(o, "keyModeEndtimer", g_cfg.keyModeEndtimer.load());
    put(o, "keyModeCheats", g_cfg.keyModeCheats.load());
    put(o, "keyModeGhost", g_cfg.keyModeGhost.load());
    put(o, "keyModePlayersonly", g_cfg.keyModePlayersonly.load());
    put(o, "keyModeDebugcam", g_cfg.keyModeDebugcam.load());
    put(o, "keyModeDepthchams", g_cfg.keyModeDepthchams.load());
    put4(o, "playerCol", g_cfg.playerCol);
    put4(o, "guardCol", g_cfg.guardCol);
    put4(o, "itemCol", g_cfg.itemCol);
    put4(o, "moneyCol", g_cfg.moneyCol);
    put4(o, "npcCol", g_cfg.npcCol);
    put4(o, "deadCol", g_cfg.deadCol);
    put4(o, "silHiddenP", g_cfg.silHiddenP);
    put4(o, "silHiddenG", g_cfg.silHiddenG);
    put4(o, "healthCol", g_cfg.healthCol);

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

static bool apply_key(const std::string& k, int n) {
    if (k == "keyMenu") { g_cfg.keyMenu.store(n); return true; }
    if (k == "keyUnload") { g_cfg.keyUnload.store(n); return true; }
    if (k == "keyMaps") { g_cfg.keyMaps.store(n); return true; }
    if (k == "keySkills") { g_cfg.keySkills.store(n); return true; }
    if (k == "keyShop") { g_cfg.keyShop.store(n); return true; }
    if (k == "keyCoins") { g_cfg.keyCoins.store(n); return true; }
    if (k == "keyCrates") { g_cfg.keyCrates.store(n); return true; }
    if (k == "keyVacuum") { g_cfg.keyVacuum.store(n); return true; }
    if (k == "keyPhones") { g_cfg.keyPhones.store(n); return true; }
    if (k == "keyCameras") { g_cfg.keyCameras.store(n); return true; }
    if (k == "keyDoors") { g_cfg.keyDoors.store(n); return true; }
    if (k == "keyGuns") { g_cfg.keyGuns.store(n); return true; }
    if (k == "keyComplete") { g_cfg.keyComplete.store(n); return true; }
    if (k == "keyWingame") { g_cfg.keyWingame.store(n); return true; }
    if (k == "keyAmmo") { g_cfg.keyAmmo.store(n); return true; }
    if (k == "keyXp") { g_cfg.keyXp.store(n); return true; }
    if (k == "keyImmunity") { g_cfg.keyImmunity.store(n); return true; }
    if (k == "keyDance") { g_cfg.keyDance.store(n); return true; }
    if (k == "keyKickself") { g_cfg.keyKickself.store(n); return true; }
    if (k == "keyFloppy") { g_cfg.keyFloppy.store(n); return true; }
    if (k == "keyCops") { g_cfg.keyCops.store(n); return true; }
    if (k == "keyRandmask") { g_cfg.keyRandmask.store(n); return true; }
    if (k == "keyEndtimer") { g_cfg.keyEndtimer.store(n); return true; }
    if (k == "keyCheats") { g_cfg.keyCheats.store(n); return true; }
    if (k == "keyGhost") { g_cfg.keyGhost.store(n); return true; }
    if (k == "keyPlayersonly") { g_cfg.keyPlayersonly.store(n); return true; }
    if (k == "keyDebugcam") { g_cfg.keyDebugcam.store(n); return true; }
    if (k == "keyDepthchams") { g_cfg.keyDepthchams.store(n); return true; }
    if (k == "keyModeMenu") { g_cfg.keyModeMenu.store(n); return true; }
    if (k == "keyModeMaps") { g_cfg.keyModeMaps.store(n); return true; }
    if (k == "keyModeShop") { g_cfg.keyModeShop.store(n); return true; }
    if (k == "keyModeSkills") { g_cfg.keyModeSkills.store(n); return true; }
    if (k == "keyModeCoins") { g_cfg.keyModeCoins.store(n); return true; }
    if (k == "keyModeCrates") { g_cfg.keyModeCrates.store(n); return true; }
    if (k == "keyModeVacuum") { g_cfg.keyModeVacuum.store(n); return true; }
    if (k == "keyModePhones") { g_cfg.keyModePhones.store(n); return true; }
    if (k == "keyModeCameras") { g_cfg.keyModeCameras.store(n); return true; }
    if (k == "keyModeDoors") { g_cfg.keyModeDoors.store(n); return true; }
    if (k == "keyModeGuns") { g_cfg.keyModeGuns.store(n); return true; }
    if (k == "keyModeComplete") { g_cfg.keyModeComplete.store(n); return true; }
    if (k == "keyModeWingame") { g_cfg.keyModeWingame.store(n); return true; }
    if (k == "keyModeAmmo") { g_cfg.keyModeAmmo.store(n); return true; }
    if (k == "keyModeXp") { g_cfg.keyModeXp.store(n); return true; }
    if (k == "keyModeImmunity") { g_cfg.keyModeImmunity.store(n); return true; }
    if (k == "keyModeDance") { g_cfg.keyModeDance.store(n); return true; }
    if (k == "keyModeKickself") { g_cfg.keyModeKickself.store(n); return true; }
    if (k == "keyModeFloppy") { g_cfg.keyModeFloppy.store(n); return true; }
    if (k == "keyModeCops") { g_cfg.keyModeCops.store(n); return true; }
    if (k == "keyModeRandmask") { g_cfg.keyModeRandmask.store(n); return true; }
    if (k == "keyModeEndtimer") { g_cfg.keyModeEndtimer.store(n); return true; }
    if (k == "keyModeCheats") { g_cfg.keyModeCheats.store(n); return true; }
    if (k == "keyModeGhost") { g_cfg.keyModeGhost.store(n); return true; }
    if (k == "keyModePlayersonly") { g_cfg.keyModePlayersonly.store(n); return true; }
    if (k == "keyModeDebugcam") { g_cfg.keyModeDebugcam.store(n); return true; }
    if (k == "keyModeDepthchams") { g_cfg.keyModeDepthchams.store(n); return true; }
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
        if (k == "keybindList") {
            bool b;
            if (get_bool(v, b)) { g_cfg.keybindList.store(b); ++applied; }
            continue;
        }
        if (k.size() > 3 && k[0] == 'k' && k[1] == 'e' && k[2] == 'y') {
            try {
                int n = std::stoi(v);
                if (apply_key(k, n)) ++applied;
            } catch (...) {}
            continue;
        }
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
        else if (k == "playerChams") sb(g_cfg.playerChams);
        else if (k == "playerGlow") sb(g_cfg.playerGlow);
        else if (k == "silChams") sb(g_cfg.silChams);
        else if (k == "silChamsG") sb(g_cfg.silChamsG);
        else if (k == "guardChams") sb(g_cfg.guardChams);
        else if (k == "guardGlow") sb(g_cfg.guardGlow);
        else if (k == "hpLeft") sb(g_cfg.hpLeft);
        else if (k == "espEnabled") sb(g_cfg.espEnabled);
        else if (k == "autoFreeShop") sb(g_cfg.autoFreeShop);
        else if (k == "watermark") sb(g_cfg.watermark);
        else if (k == "clock24h") sb(g_cfg.clock24h);
        else if (k == "particles") sb(g_cfg.particles);
        else if (k == "blurBackdrop") sb(g_cfg.blurBackdrop);
        else if (k == "borderFx") sb(g_cfg.borderFx);
        else if (k == "borderRainbow") sb(g_cfg.borderRainbow);
        else if (k == "playerCountBox") sb(g_cfg.playerCountBox);
        else if (k == "objClassesOnly") sb(g_cfg.objClassesOnly);
        else if (k == "crosshair") sb(g_cfg.crosshair);
        else if (k == "pcX") sf(g_cfg.pcX);
        else if (k == "pcY") sf(g_cfg.pcY);
        else if (k == "killfeed") sb(g_cfg.killfeed);
        else if (k == "hitlog") sb(g_cfg.hitlog);
        else if (k == "glassBlur") sb(g_cfg.glassBlur);
        else if (k == "glassTint") s4(g_cfg.glassTint);
        else if (k == "radar") sb(g_cfg.radar);
        else if (k == "radarRange") sf(g_cfg.radarRange);
        else if (k == "kbX") sf(g_cfg.kbX);
        else if (k == "kbY") sf(g_cfg.kbY);
        else if (k == "logX") sf(g_cfg.logX);
        else if (k == "logY") sf(g_cfg.logY);
        else if (k == "killX") sf(g_cfg.killX);
        else if (k == "killY") sf(g_cfg.killY);
        else if (k == "blurAmount") sf(g_cfg.blurAmount);
        else if (k == "godMode") sb(g_cfg.godMode);
        else if (k == "itemMaxDist") sf(g_cfg.itemMaxDist);
        else if (k == "playerMaxDist") sf(g_cfg.playerMaxDist);
        else if (k == "guardMaxDist") sf(g_cfg.guardMaxDist);
        else if (k == "timeDilation") sf(g_cfg.timeDilation);
        else if (k == "fov") sf(g_cfg.fov);
        else if (k == "espBoxW") sf(g_cfg.espBoxW);
        else if (k == "espNameDy") sf(g_cfg.espNameDy);
        else if (k == "espPrevX") sf(g_cfg.espPrevX);
        else if (k == "espPrevY") sf(g_cfg.espPrevY);
        else if (k == "espTextScale") sf(g_cfg.espTextScale);
        else if (k == "espBoxStyle") si(g_cfg.espBoxStyle);
        else if (k == "espLabelAnchor") si(g_cfg.espLabelAnchor);
        else if (k == "espSubAnchor") si(g_cfg.espSubAnchor);
        else if (k == "healthStyle") si(g_cfg.healthStyle);
        else if (k == "nameMode") si(g_cfg.nameMode);
        else if (k == "playerCol") s4(g_cfg.playerCol);
        else if (k == "guardCol") s4(g_cfg.guardCol);
        else if (k == "itemCol") s4(g_cfg.itemCol);
        else if (k == "moneyCol") s4(g_cfg.moneyCol);
        else if (k == "npcCol") s4(g_cfg.npcCol);
        else if (k == "deadCol") s4(g_cfg.deadCol);
        else if (k == "silHiddenP") s4(g_cfg.silHiddenP);
        else if (k == "silHiddenG") s4(g_cfg.silHiddenG);
        else if (k == "healthCol") s4(g_cfg.healthCol);
    }
    if (applied == 0) { err = "empty config"; return false; }
    return true;
}

std::vector<std::string> list() {
    return fileManager::listConfigs();
}

}

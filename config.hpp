// config.hpp — defaults recalibrés
// comboDelayMs : 0 (était 16 — c'était le bottleneck principal)
// timingVarianceMs : 4 (était 8 — moins de bruit sur les timings bas)
#pragma once
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>

struct Config {
    bool  espEnabled      = true;
    bool  espBoxes        = true;
    bool  espHP           = true;
    bool  espNames        = true;
    bool  espWeapons      = true;
    bool  espAtkState     = true;   // nouveau — affiche Idle/Startup/Active/Recovery
    bool  autoDodge       = true;
    int   dodgeDelayMs    = 0;
    bool  dodgeGroundOnly = false;
    bool  dexBot          = true;
    int   comboDelayMs    = 0;      // FIX : était 16, plancher de latence supprimé
    bool  dexBotOnlyHit   = false;  // FIX : était true — mode agressif par défaut
    bool  weaponSnap      = true;
    float snapRangeUnits  = 400.f;
    bool  randomizeTimings= true;
    int   timingVarianceMs= 4;      // FIX : était 8

    void Load(const std::string& path = "bhcheat_cfg.json") {
        std::ifstream f(path);
        if (!f.is_open()) { Save(path); return; }
        try {
            auto j = nlohmann::json::parse(f);
            espEnabled       = j.value("espEnabled",       espEnabled);
            espBoxes         = j.value("espBoxes",         espBoxes);
            espHP            = j.value("espHP",            espHP);
            espNames         = j.value("espNames",         espNames);
            espWeapons       = j.value("espWeapons",       espWeapons);
            espAtkState      = j.value("espAtkState",      espAtkState);
            autoDodge        = j.value("autoDodge",        autoDodge);
            dodgeDelayMs     = j.value("dodgeDelayMs",     dodgeDelayMs);
            dodgeGroundOnly  = j.value("dodgeGroundOnly",  dodgeGroundOnly);
            dexBot           = j.value("dexBot",           dexBot);
            comboDelayMs     = j.value("comboDelayMs",     comboDelayMs);
            dexBotOnlyHit    = j.value("dexBotOnlyHit",   dexBotOnlyHit);
            weaponSnap       = j.value("weaponSnap",       weaponSnap);
            snapRangeUnits   = j.value("snapRangeUnits",   snapRangeUnits);
            randomizeTimings = j.value("randomizeTimings", randomizeTimings);
            timingVarianceMs = j.value("timingVarianceMs", timingVarianceMs);
        } catch (...) { Save(path); }
    }

    void Save(const std::string& path = "bhcheat_cfg.json") {
        nlohmann::json j;
        j["espEnabled"]      = espEnabled;
        j["espBoxes"]        = espBoxes;
        j["espHP"]           = espHP;
        j["espNames"]        = espNames;
        j["espWeapons"]      = espWeapons;
        j["espAtkState"]     = espAtkState;
        j["autoDodge"]       = autoDodge;
        j["dodgeDelayMs"]    = dodgeDelayMs;
        j["dodgeGroundOnly"] = dodgeGroundOnly;
        j["dexBot"]          = dexBot;
        j["comboDelayMs"]    = comboDelayMs;
        j["dexBotOnlyHit"]   = dexBotOnlyHit;
        j["weaponSnap"]      = weaponSnap;
        j["snapRangeUnits"]  = snapRangeUnits;
        j["randomizeTimings"]= randomizeTimings;
        j["timingVarianceMs"]= timingVarianceMs;
        std::ofstream f(path); f << j.dump(4);
    }
};

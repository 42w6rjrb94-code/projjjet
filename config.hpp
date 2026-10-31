// config.hpp — ajout threatDodge + chaseEnabled
#pragma once
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>

struct Config {
    // ESP
    bool  espEnabled      = true;
    bool  espBoxes        = true;
    bool  espHP           = true;
    bool  espNames        = true;
    bool  espWeapons      = true;
    bool  espAtkState     = true;
    // W2S calibration
    float w2sOffsetX      = 0.f;
    float w2sOffsetY      = 0.f;
    bool  w2sFlipY        = false;
    // AutoDodge (attaques joueur) — géré par ChaseAI
    bool  autoDodge       = true;
    int   dodgeDelayMs    = 0;
    bool  dodgeGroundOnly = false;
    // Threat Dodge — projectiles + hazards de map
    bool  threatDodge     = true;
    // Chase AI — mouvement vers l'ennemi
    bool  chaseEnabled    = true;
    // DexBot — attaque
    bool  dexBot          = true;
    int   comboDelayMs    = 0;
    bool  dexBotOnlyHit   = false;
    bool  dexBotNoActive  = true;
    float attackRange     = 270.f;
    float chaseRange      = 2000.f;
    // WeaponSnap
    bool  weaponSnap      = true;
    float snapRangeUnits  = 400.f;
    // Timings
    bool  randomizeTimings = true;
    int   timingVarianceMs = 4;

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
            w2sOffsetX       = j.value("w2sOffsetX",       w2sOffsetX);
            w2sOffsetY       = j.value("w2sOffsetY",       w2sOffsetY);
            w2sFlipY         = j.value("w2sFlipY",         w2sFlipY);
            autoDodge        = j.value("autoDodge",        autoDodge);
            dodgeDelayMs     = j.value("dodgeDelayMs",     dodgeDelayMs);
            dodgeGroundOnly  = j.value("dodgeGroundOnly",  dodgeGroundOnly);
            threatDodge      = j.value("threatDodge",      threatDodge);
            chaseEnabled     = j.value("chaseEnabled",     chaseEnabled);
            dexBot           = j.value("dexBot",           dexBot);
            comboDelayMs     = j.value("comboDelayMs",     comboDelayMs);
            dexBotOnlyHit    = j.value("dexBotOnlyHit",   dexBotOnlyHit);
            dexBotNoActive   = j.value("dexBotNoActive",   dexBotNoActive);
            attackRange      = j.value("attackRange",      attackRange);
            chaseRange       = j.value("chaseRange",       chaseRange);
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
        j["w2sOffsetX"]      = w2sOffsetX;
        j["w2sOffsetY"]      = w2sOffsetY;
        j["w2sFlipY"]        = w2sFlipY;
        j["autoDodge"]       = autoDodge;
        j["dodgeDelayMs"]    = dodgeDelayMs;
        j["dodgeGroundOnly"] = dodgeGroundOnly;
        j["threatDodge"]     = threatDodge;
        j["chaseEnabled"]    = chaseEnabled;
        j["dexBot"]          = dexBot;
        j["comboDelayMs"]    = comboDelayMs;
        j["dexBotOnlyHit"]   = dexBotOnlyHit;
        j["dexBotNoActive"]  = dexBotNoActive;
        j["attackRange"]     = attackRange;
        j["chaseRange"]      = chaseRange;
        j["weaponSnap"]      = weaponSnap;
        j["snapRangeUnits"]  = snapRangeUnits;
        j["randomizeTimings"]= randomizeTimings;
        j["timingVarianceMs"]= timingVarianceMs;
        std::ofstream f(path); f << j.dump(4);
    }
};

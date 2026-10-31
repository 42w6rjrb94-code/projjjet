// config.hpp — v2 : ajout dodgeRange, dexBotSwapLocal, autoGravCancel, useSig, sigRange, lowHpThreshold
#pragma once
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>

struct Config {
    // ESP
    bool  espEnabled       = true;
    bool  espBoxes         = true;
    bool  espHP            = true;
    bool  espNames         = true;
    bool  espWeapons       = true;
    bool  espAtkState      = true;
    // W2S calibration
    float w2sOffsetX       = 0.f;
    float w2sOffsetY       = 0.f;
    bool  w2sFlipY         = false;
    // AutoDodge — géré par ChaseAI
    bool  autoDodge        = true;
    int   dodgeDelayMs     = 0;
    bool  dodgeGroundOnly  = false;
    float dodgeRange       = 500.f;     // ← NEW : portée détection attaque joueur
    // Threat Dodge — projectiles + hazards
    bool  threatDodge      = true;
    // Chase AI
    bool  chaseEnabled     = true;
    float lowHpThreshold   = 0.f;       // ← NEW : hp% local → mode défensif (0=off)
    // DexBot
    bool  dexBot           = true;
    int   comboDelayMs     = 0;
    bool  dexBotOnlyHit    = false;
    bool  dexBotNoActive   = true;
    bool  dexBotSwapLocal  = false;     // ← NEW : inverser détection local/ennemi
    float attackRange      = 270.f;
    float chaseRange       = 2000.f;
    // Grav cancel + sig
    bool  autoGravCancel   = false;     // ← NEW : gravity cancel après combo aérien
    bool  useSig           = false;     // ← NEW : utiliser la signature
    float sigRange         = 200.f;     // ← NEW : portée déclenchement sig
    // WeaponSnap
    bool  weaponSnap       = true;
    float snapRangeUnits   = 400.f;
    // Timings
    bool  randomizeTimings = true;
    int   timingVarianceMs = 4;

    void Load(const std::string& path = "bhcheat_cfg.json") {
        std::ifstream f(path);
        if (!f.is_open()) { Save(path); return; }
        try {
            auto j = nlohmann::json::parse(f);
            #define JV(x) x = j.value(#x, x)
            JV(espEnabled); JV(espBoxes); JV(espHP); JV(espNames);
            JV(espWeapons); JV(espAtkState);
            JV(w2sOffsetX); JV(w2sOffsetY); JV(w2sFlipY);
            JV(autoDodge); JV(dodgeDelayMs); JV(dodgeGroundOnly); JV(dodgeRange);
            JV(threatDodge);
            JV(chaseEnabled); JV(lowHpThreshold);
            JV(dexBot); JV(comboDelayMs); JV(dexBotOnlyHit);
            JV(dexBotNoActive); JV(dexBotSwapLocal);
            JV(attackRange); JV(chaseRange);
            JV(autoGravCancel); JV(useSig); JV(sigRange);
            JV(weaponSnap); JV(snapRangeUnits);
            JV(randomizeTimings); JV(timingVarianceMs);
            #undef JV
        } catch (...) { Save(path); }
    }

    void Save(const std::string& path = "bhcheat_cfg.json") {
        nlohmann::json j;
        #define JS(x) j[#x] = x
        JS(espEnabled); JS(espBoxes); JS(espHP); JS(espNames);
        JS(espWeapons); JS(espAtkState);
        JS(w2sOffsetX); JS(w2sOffsetY); JS(w2sFlipY);
        JS(autoDodge); JS(dodgeDelayMs); JS(dodgeGroundOnly); JS(dodgeRange);
        JS(threatDodge);
        JS(chaseEnabled); JS(lowHpThreshold);
        JS(dexBot); JS(comboDelayMs); JS(dexBotOnlyHit);
        JS(dexBotNoActive); JS(dexBotSwapLocal);
        JS(attackRange); JS(chaseRange);
        JS(autoGravCancel); JS(useSig); JS(sigRange);
        JS(weaponSnap); JS(snapRangeUnits);
        JS(randomizeTimings); JS(timingVarianceMs);
        #undef JS
        std::ofstream f(path); f << j.dump(4);
    }
};

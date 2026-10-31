// weaponsnap.hpp — v2
// FIX : Tick() reçoit isDodging + isChasing depuis main.cpp (pas de ref ChaseAI)
// Si ChaseAI est en dodge → release immédiat
// Si ChaseAI est en chase d'ennemi → release immédiat (ennemi prioritaire sur arme)
// WeaponSnap n'opère que quand aucune priorité supérieure n'est active
#pragma once
#include "entities.hpp"
#include "input.hpp"
#include "config.hpp"
#include <vector>
#include <cmath>
#include <cstdint>

class WeaponSnap {
    Config&  cfg;
    WORD     lastSnapDir  = 0;
    uint64_t lastHoldTick = 0;

    void Release() {
        if (lastSnapDir) {
            Input::ReleaseKey(lastSnapDir);
            lastSnapDir = 0; lastHoldTick = 0;
        }
    }

public:
    WeaponSnap(Config& c) : cfg(c) {}

    // isDodging + isChasing exposés par ChaseAI via main.cpp
    void Tick(const std::vector<BHPlayer>& players,
              const std::vector<BHWeapon>& weapons,
              bool isDodging, bool isChasing) {

        if (!cfg.weaponSnap) { Release(); return; }
        if (isDodging)       { Release(); return; }  // dodge en cours → priorité 1
        if (isChasing)       { Release(); return; }  // chase ennemi → priorité 2

        // Trouver le local player
        const BHPlayer* local = nullptr;
        for (const auto& p : players)
            if (p.isLocal) { local = &p; break; }
        if (!local && !players.empty()) local = &players[0];
        if (!local) { Release(); return; }

        // Local déjà armé (weaponType > 0 = confirmé) → rien à faire
        if (local->weaponType > 0) { Release(); return; }

        // Arme la plus proche dans la range
        const BHWeapon* best     = nullptr;
        float           bestDist = cfg.snapRangeUnits;
        for (const auto& w : weapons) {
            float ddx = w.pos.x - local->worldPos.x;
            float ddy = w.pos.y - local->worldPos.y;
            float d   = sqrtf(ddx*ddx + ddy*ddy);
            if (d >= bestDist) continue;
            bestDist = d; best = &w;
        }

        if (!best) { Release(); return; }

        // Assez proche → ramasser sans mouvement
        if (bestDist < 80.f) { Release(); return; }

        // Chase vers l'arme — re-send HoldKey toutes les 150ms (AIR peut drop events)
        float ddx = best->pos.x - local->worldPos.x;
        WORD  want = ddx > 0 ? Input::KEY_RIGHT : Input::KEY_LEFT;
        uint64_t now = GetTickCount64();

        if (lastSnapDir != want || (now - lastHoldTick) > 150) {
            if (lastSnapDir && lastSnapDir != want)
                Input::ReleaseKey(lastSnapDir);
            Input::HoldKey(want);
            lastSnapDir  = want;
            lastHoldTick = now;
        }
    }
};

// weaponsnap.hpp — chase continu vers l'arme la plus proche
#pragma once
#include "entities.hpp"
#include "input.hpp"
#include "config.hpp"
#include <vector>
#include <thread>
#include <chrono>

class WeaponSnap {
    Config& cfg;
    WORD     lastSnapDir  = 0;
    uint64_t lastHoldTick = 0;

public:
    WeaponSnap(Config& c) : cfg(c) {}

    void Tick(const std::vector<BHPlayer>& players,
              const std::vector<BHWeapon>& weapons) {
        if (!cfg.weaponSnap) {
            if (lastSnapDir) { Input::ReleaseKey(lastSnapDir); lastSnapDir = 0; }
            return;
        }

        // Trouve le local
        const BHPlayer* local = nullptr;
        for (const auto& p : players)
            if (p.isLocal) { local = &p; break; }
        if (!local && !players.empty()) local = &players[0];
        if (!local) return;

        // Si le local a déjà une arme confirmée → stop
        // weaponType=-1 = FILL non confirmé → on cherche toujours
        // Une fois l'offset confirmé en CE, ce guard deviendra utile
        if (local->weaponType > 0) {
            if (lastSnapDir) { Input::ReleaseKey(lastSnapDir); lastSnapDir = 0; }
            return;
        }

        // Arme la plus proche dans la range
        const BHWeapon* best = nullptr;
        for (const auto& w : weapons) {
            float ddx = w.pos.x - local->worldPos.x;
            float ddy = w.pos.y - local->worldPos.y;
            float d   = sqrtf(ddx*ddx + ddy*ddy);
            if (d > cfg.snapRangeUnits) continue;
            if (!best) { best = &w; continue; }
            float bd = sqrtf(
                (best->pos.x-local->worldPos.x)*(best->pos.x-local->worldPos.x)+
                (best->pos.y-local->worldPos.y)*(best->pos.y-local->worldPos.y));
            if (d < bd) best = &w;
        }

        if (!best) {
            // Pas d'arme visible → stop mouvement snap
            if (lastSnapDir) { Input::ReleaseKey(lastSnapDir); lastSnapDir = 0; }
            return;
        }

        float ddx = best->pos.x - local->worldPos.x;
        float dist = sqrtf(ddx*ddx +
            (best->pos.y-local->worldPos.y)*(best->pos.y-local->worldPos.y));

        if (dist < 80.f) {
            // Assez proche → on est dessus, relâche
            if (lastSnapDir) { Input::ReleaseKey(lastSnapDir); lastSnapDir = 0; }
            return;
        }

        // Chase vers l'arme — re-send toutes les 150ms
        WORD want = ddx > 0 ? Input::KEY_RIGHT : Input::KEY_LEFT;
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

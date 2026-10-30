#pragma once
#include "entities.hpp"
#include "input.hpp"
#include "config.hpp"
#include <vector>
#include <thread>
#include <chrono>
#include <cmath>
#include <iostream>

class AutoDodge {
    Config& cfg;

    static WORD DodgeDir(const BHPlayer& local, const BHPlayer& enemy) {
        float dx = enemy.worldPos.x - local.worldPos.x;
        if (std::abs(dx) > 50.f)
            return dx > 0 ? Input::KEY_LEFT : Input::KEY_RIGHT;
        return Input::KEY_UP;
    }

public:
    AutoDodge(Config& c) : cfg(c) {}

    void Tick(const std::vector<BHPlayer>& players) {
        if (!cfg.autoDodge) return;
        if (players.size() < 2) return;

        // Cherche le local — celui dont atk change le moins
        // Provisoire : prend le joueur avec la plus petite distToLocal=0
        // (distToLocal est calculée depuis players[0] dans entities.hpp)
        // → on prend players[0] comme local ET on teste tous les autres
        // → si autododge se déclenche sur le mauvais perso, on inversera

        const BHPlayer* local = &players[0];

        for (size_t i = 1; i < players.size(); i++) {
            const BHPlayer& enemy = players[i];

            // Log pour debug
            std::cout << "[DODGE] enemy[" << i << "] atk="
                      << (int)enemy.atkState
                      << " dist=" << (int)enemy.distToLocal << "\n";

            if ((int)enemy.atkState == 0) continue;
            if (enemy.distToLocal > 600.f) continue;

            std::cout << "[DODGE] TRIGGER — ennemi " << i
                      << " attaque a dist=" << (int)enemy.distToLocal << "\n";

            int delay = Input::Jitter(cfg.dodgeDelayMs, cfg.timingVarianceMs);
            if (delay > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));

            WORD dir = DodgeDir(*local, enemy);
            Input::HoldKey(dir);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            Input::PressKey(Input::KEY_DODGE, 50, cfg.timingVarianceMs);
            Input::ReleaseKey(dir);
            break;
        }
    }
};

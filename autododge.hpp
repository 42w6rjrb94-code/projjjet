// autododge.hpp — C++17, Windows, external
#pragma once
#include "entities.hpp"
#include "input.hpp"
#include "config.hpp"
#include <vector>
#include <unordered_map>
#include <thread>
#include <chrono>
#include <cmath>
#include <iostream>

class AutoDodge {
    Config& cfg;

    // Dernier état par index ennemi — détecte la transition Startup→Active
    std::unordered_map<size_t, AttackState> prevState;

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

        constexpr float DODGE_RANGE = 350.f;

        const BHPlayer* local = &players[0];

        for (size_t i = 1; i < players.size(); i++) {
            const BHPlayer& enemy = players[i];
            AttackState prev = prevState[i]; // default-init à Idle=0
            AttackState curr = enemy.atkState;

            std::cout << "[DODGE] enemy[" << i << "] atk="
                      << (int)curr
                      << " dist=" << (int)enemy.distToLocal << "\n";

            // Mise à jour état précédent avant les guards
            prevState[i] = curr;

            if (enemy.distToLocal > DODGE_RANGE) continue;
            if (cfg.dodgeGroundOnly && !local->isGrounded) continue;

            // Trigger uniquement sur la transition vers Active (=2)
            // Idle→Active   : attaque légère rapide → dodge immédiat
            // Startup→Active: sig/heavy charge relâchée → dodge au bon moment
            bool transition =
                (curr == AttackState::Active) &&
                (prev != AttackState::Active);

            if (!transition) continue;

            std::cout << "[DODGE] TRIGGER — ennemi " << i
                      << " passe Active a dist=" << (int)enemy.distToLocal << "\n";

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

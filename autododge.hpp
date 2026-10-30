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
            AttackState prev = prevState[i];
            AttackState curr = enemy.atkState;

            std::cout << "[DODGE] enemy[" << i << "] atk="
                      << (int)curr
                      << " dist=" << (int)enemy.distToLocal << "\n";

            prevState[i] = curr;

            if (enemy.distToLocal > DODGE_RANGE) continue;
            if (cfg.dodgeGroundOnly && !local->isGrounded) continue;

            // Trigger sur Idle→non-Idle (onset unique de l'attaque)
            // BH expose pas Startup/Active séparément sur cet offset
            bool onset = (curr != AttackState::Idle) &&
                         (prev == AttackState::Idle);

            if (!onset) continue;

            std::cout << "[DODGE] TRIGGER — ennemi " << i
                      << " onset dist=" << (int)enemy.distToLocal << "\n";

            // dodgeDelayMs dans le menu Config règle le timing :
            // Légères  → 0–30ms
            // Lourdes  → 80–120ms
            // Sigs     → 250–350ms (couvre le startup de charge)
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

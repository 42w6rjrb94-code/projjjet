// autododge.hpp — C++17, Windows, external
// Même fix isLocal que DexBot : fallback players[0] si flag absent
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
    std::unordered_map<int, AttackState> prevState;

    static WORD DodgeDir(const BHPlayer& local, const BHPlayer& enemy) {
        float dx = enemy.worldPos.x - local.worldPos.x;
        if (std::abs(dx) > 50.f)
            return dx > 0 ? Input::KEY_LEFT : Input::KEY_RIGHT;
        return Input::KEY_UP;
    }

public:
    AutoDodge(Config& c) : cfg(c) {}

    void Tick(const std::vector<BHPlayer>& players) {
        if (!cfg.autoDodge)       return;
        if (players.size() < 2)   return;

        // Résolution local/enemy — même logique que DexBot, pas de static cache ici
        // (AutoDodge est stateless entre ticks pour la détection d'onset)
        int localIdx = -1;
        for (int i = 0; i < (int)players.size(); i++)
            if (players[i].isLocal) { localIdx = i; break; }
        if (localIdx < 0) localIdx = 0;  // fallback

        const BHPlayer& local = players[localIdx];

        constexpr float DODGE_RANGE = 510.f;

        for (int ei = 0; ei < (int)players.size(); ei++) {
            if (ei == localIdx) continue;
            const BHPlayer& enemy = players[ei];

            AttackState prev = prevState[ei];
            AttackState curr = enemy.atkState;
            prevState[ei] = curr;

            std::cout << "[DODGE] enemy[" << ei << "] atk="
                      << (int)curr << " dist=" << (int)enemy.distToLocal << "\n";

            if (enemy.distToLocal > DODGE_RANGE)       continue;
            if (cfg.dodgeGroundOnly && !local.isGrounded) continue;

            // Onset : Idle → non-Idle = début d'attaque ennemie
            bool onset = (curr != AttackState::Idle) && (prev == AttackState::Idle);
            if (!onset) continue;

            std::cout << "[DODGE] TRIGGER enemy[" << ei
                      << "] dist=" << (int)enemy.distToLocal << "\n";

            int delay = Input::Jitter(cfg.dodgeDelayMs, cfg.timingVarianceMs);
            if (delay > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));

            WORD dir = DodgeDir(local, enemy);
            Input::HoldKey(dir);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            Input::PressKey(Input::KEY_DODGE, 50, cfg.timingVarianceMs);
            Input::ReleaseKey(dir);
            break;
        }
    }
};

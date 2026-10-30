// autododge.hpp — C++17, Windows, external
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
    bool    lastAtkActive = false; // edge-trigger — dodge une fois par attaque

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

        const BHPlayer* local = &players[0];

        bool anyAtkNow = false;

        for (size_t i = 1; i < players.size(); i++) {
            const BHPlayer& enemy = players[i];

            // Seuil en world units — BH map ~10000 de large
            // Portée d'attaque typique ~800–2500 units
            constexpr float DODGE_RANGE = 3000.f;

            std::cout << "[DODGE] enemy[" << i << "] atk="
                      << (int)enemy.atkState
                      << " dist=" << (int)enemy.distToLocal << "\n";

            if ((int)enemy.atkState == 0) continue;
            if (enemy.distToLocal > DODGE_RANGE) continue;
            if (cfg.dodgeGroundOnly && !local->isGrounded) continue;

            anyAtkNow = true;

            // Edge-trigger : fire une seule fois à l'onset de l'attaque
            // pas à chaque frame pendant toute la durée
            if (lastAtkActive) break;

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

        lastAtkActive = anyAtkNow;
    }
};

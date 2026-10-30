#pragma once
#include "entities.hpp"
#include "input.hpp"
#include "config.hpp"
#include <vector>
#include <thread>
#include <chrono>
#include <cmath>

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
        if (players.empty()) return;

        // Premier joueur dans l'array = local provisoire
        const BHPlayer* local = &players[0];
        if (cfg.dodgeGroundOnly && !local->isGrounded) return;

        for (size_t i = 1; i < players.size(); i++) {
            const BHPlayer& enemy = players[i];

            // Skip si pas en train d'attaquer
            if ((int)enemy.atkState == 0) continue;

            // Skip si trop loin
            if (enemy.distToLocal > 300.f) continue;

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

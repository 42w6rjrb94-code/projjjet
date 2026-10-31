// autododge.hpp
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
        if (!cfg.autoDodge)     return;
        if (players.size() < 2) return;

        int localIdx = 0;
        for (int i = 0; i < (int)players.size(); i++)
            if (players[i].isLocal) { localIdx = i; break; }

        const BHPlayer& local = players[localIdx];
        const float DODGE_RANGE = cfg.dodgeRange; // ← configurable via slider

        for (int ei = 0; ei < (int)players.size(); ei++) {
            if (ei == localIdx) continue;
            const BHPlayer& enemy = players[ei];

            AttackState prev = prevState[ei];
            AttackState curr = enemy.atkState;
            prevState[ei] = curr;

            // Distance inline — indépendant de distToLocal
            float ddx = enemy.worldPos.x - local.worldPos.x;
            float ddy = enemy.worldPos.y - local.worldPos.y;
            float dist = sqrtf(ddx*ddx + ddy*ddy);

            std::cout << "[DODGE] enemy[" << ei << "] atk="
                      << (int)curr << " dist=" << (int)dist << "\n";

            if (dist > DODGE_RANGE) continue;
            if (cfg.dodgeGroundOnly && !local.isGrounded) continue;

            bool onset = (curr != AttackState::Idle) && (prev == AttackState::Idle);
            if (!onset) continue;

            std::cout << "[DODGE] TRIGGER dist=" << (int)dist << "\n";

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

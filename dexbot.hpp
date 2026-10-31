// dexbot.hpp — chase fix : dist inline + periodic re-send + swap toggle
#pragma once
#include "entities.hpp"
#include "input.hpp"
#include "config.hpp"
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>
#include <cmath>
#include <iostream>

class DexBot {
    Config& cfg;

    AttackState       prevEnemyAtk = AttackState::Idle;
    std::atomic<bool> attacking{false};
    WORD              lastMoveDir  = 0;
    uint64_t          lastHoldTick = 0;   // periodic re-send chase key
    int               cachedLocalIdx = -1;
    int               cachedEnemyIdx = -1;

    void FireAttack(WORD faceDir, int delayMs) {
        if (attacking.exchange(true)) return;
        std::thread([this, faceDir, delayMs]() {
            if (delayMs > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            if (faceDir) {
                Input::HoldKey(faceDir);
                std::this_thread::sleep_for(std::chrono::milliseconds(8));
            }
            Input::PressKey(Input::KEY_ATTACK, 30, 0);
            if (faceDir) Input::ReleaseKey(faceDir);
            int cd = 100 + std::max(0, cfg.comboDelayMs);
            if (cfg.randomizeTimings)
                cd += (rand() % std::max(1, cfg.timingVarianceMs));
            std::this_thread::sleep_for(std::chrono::milliseconds(cd));
            attacking = false;
        }).detach();
    }

    void ResolveIndices(const std::vector<BHPlayer>& players) {
        if (players.size() < 2) { cachedLocalIdx = -1; cachedEnemyIdx = -1; return; }
        if (cachedLocalIdx >= 0 && cachedEnemyIdx >= 0
            && cachedLocalIdx < (int)players.size()
            && cachedEnemyIdx < (int)players.size()) return;

        int li = -1, ei = -1;
        for (int i = 0; i < (int)players.size(); i++) {
            if (players[i].isLocal  && li < 0) li = i;
            else if (!players[i].isLocal && ei < 0) ei = i;
        }
        if (li < 0) li = 0;
        if (ei < 0) ei = (li == 0) ? 1 : 0;
        if (li == ei) ei = 1 - li;

        cachedLocalIdx = li;
        cachedEnemyIdx = ei;
        std::cout << "[DEXBOT] local=players[" << li << "] enemy=players[" << ei << "]\n";
    }

public:
    DexBot(Config& c) : cfg(c) {}

    void Tick(const std::vector<BHPlayer>& players) {
        if (!cfg.dexBot)          return;
        if (players.size() < 2)   return;

        ResolveIndices(players);
        if (cachedLocalIdx < 0 || cachedEnemyIdx < 0) return;

        int li = cachedLocalIdx;
        int ei = cachedEnemyIdx;

        // Swap si le chase part à l'envers — toggle dans menu DexBot
        if (cfg.dexBotSwapLocal) std::swap(li, ei);

        const BHPlayer& local = players[li];
        const BHPlayer& enemy = players[ei];

        // ── Dist inline — jamais basé sur distToLocal ────────────────────
        // distToLocal peut être 0 si local/enemy indices sont inversés dans entities
        float dx   = enemy.worldPos.x - local.worldPos.x;
        float dy   = enemy.worldPos.y - local.worldPos.y;
        float dist = sqrtf(dx*dx + dy*dy);

        // ── CHASE ────────────────────────────────────────────────────────
        // Re-send HoldKey toutes les 150ms même si direction inchangée
        // AIR game peut drop des keydown event si pas rafraîchi
        if (dist > cfg.attackRange && dist < cfg.chaseRange) {
            WORD want = dx > 0 ? Input::KEY_RIGHT : Input::KEY_LEFT;
            uint64_t now = GetTickCount64();
            bool needResend = (lastMoveDir != want) || (now - lastHoldTick > 150);
            if (needResend) {
                if (lastMoveDir && lastMoveDir != want)
                    Input::ReleaseKey(lastMoveDir);
                Input::HoldKey(want);
                lastMoveDir  = want;
                lastHoldTick = now;
            }
        } else {
            if (lastMoveDir) {
                Input::ReleaseKey(lastMoveDir);
                lastMoveDir  = 0;
                lastHoldTick = 0;
            }
        }

        // ── ÉTAT ─────────────────────────────────────────────────────────
        AttackState curr = enemy.atkState;
        bool justFinished = (curr == AttackState::Idle) && (prevEnemyAtk != AttackState::Idle);
        bool inRecovery   = (curr == AttackState::Recovery);
        bool isActive     = (curr == AttackState::Active);
        prevEnemyAtk = curr;

        // ── ATTAQUE ──────────────────────────────────────────────────────
        if (dist > cfg.attackRange) return;
        if (attacking)              return;

        bool doAttack;
        if (cfg.dexBotOnlyHit) {
            doAttack = justFinished || inRecovery;
        } else {
            doAttack = !(cfg.dexBotNoActive && isActive);
        }
        if (!doAttack) return;

        std::cout << "[DEXBOT] fire dist=" << (int)dist
                  << " state=" << (int)curr << "\n";

        if (lastMoveDir) { Input::ReleaseKey(lastMoveDir); lastMoveDir = 0; }

        WORD face = std::abs(dx) > 20.f
            ? (dx > 0 ? Input::KEY_RIGHT : Input::KEY_LEFT) : 0;

        int delay = cfg.randomizeTimings
            ? std::max(0, rand() % std::max(1, cfg.timingVarianceMs))
            : 0;
        FireAttack(face, delay);
    }
};

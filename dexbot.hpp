// dexbot.hpp — C++17, Windows, external
// FIX PRINCIPAL : isLocal null → plus jamais de return early
// Fallback players[0]/[1] garanti si isLocal non discriminé
// Attack mode : dexBotOnlyHit=false → attaque aussi en Idle (hitstun = Idle dans BH)
// Attack range / chase range configurables via menu
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

    // Indices stables local/enemy — définis une fois, ne changent pas
    // Évite le flip si isLocal se met à flicker entre ticks
    int cachedLocalIdx = -1;
    int cachedEnemyIdx = -1;

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

    // Résout indices local/enemy robustement
    // Priorité : flag isLocal → sinon i==0 = local, i==1 = enemy
    // Cache les indices pour éviter le flip
    void ResolveIndices(const std::vector<BHPlayer>& players) {
        if (players.size() < 2) { cachedLocalIdx = -1; cachedEnemyIdx = -1; return; }

        // Si on a déjà des indices valides ET les adresses sont stables → garde
        if (cachedLocalIdx >= 0 && cachedEnemyIdx >= 0
            && cachedLocalIdx < (int)players.size()
            && cachedEnemyIdx < (int)players.size()) return;

        // Cherche via isLocal flag
        int li = -1, ei = -1;
        for (int i = 0; i < (int)players.size(); i++) {
            if (players[i].isLocal && li < 0) li = i;
            else if (!players[i].isLocal && ei < 0) ei = i;
        }

        // FALLBACK : isLocal pas discriminé → 0=local, 1=enemy
        // BH cave : premier DP capturé = local dans la majorité des cas
        if (li < 0) li = 0;
        if (ei < 0) ei = (li == 0) ? 1 : 0;
        if (li == ei) ei = 1 - li;  // garantit différents

        cachedLocalIdx = li;
        cachedEnemyIdx = ei;

        std::cout << "[DEXBOT] local=players[" << li << "] addr=0x"
                  << std::hex << players[li].addr
                  << " enemy=players[" << ei << "] addr=0x"
                  << players[ei].addr << std::dec << "\n";
    }

public:
    DexBot(Config& c) : cfg(c) {}

    void Tick(const std::vector<BHPlayer>& players) {
        if (!cfg.dexBot)          return;
        if (players.size() < 2)   return;

        ResolveIndices(players);
        if (cachedLocalIdx < 0 || cachedEnemyIdx < 0) return;
        if (cachedLocalIdx >= (int)players.size())     return;
        if (cachedEnemyIdx >= (int)players.size())     return;

        const BHPlayer& local = players[cachedLocalIdx];
        const BHPlayer& enemy = players[cachedEnemyIdx];

        float dx   = enemy.worldPos.x - local.worldPos.x;
        float dist = enemy.distToLocal;

        // ── 1. CHASE ─────────────────────────────────────────────────────────
        // Indépendant de attacking — continue de chaser pendant le cooldown d'attaque
        if (dist > cfg.attackRange && dist < cfg.chaseRange) {
            WORD want = dx > 0 ? Input::KEY_RIGHT : Input::KEY_LEFT;
            if (lastMoveDir != want) {
                if (lastMoveDir) Input::ReleaseKey(lastMoveDir);
                Input::HoldKey(want);
                lastMoveDir = want;
            }
        } else {
            if (lastMoveDir) { Input::ReleaseKey(lastMoveDir); lastMoveDir = 0; }
        }

        // ── 2. ÉTAT ENNEMI ───────────────────────────────────────────────────
        AttackState curr = enemy.atkState;
        bool justFinished = (curr == AttackState::Idle)
                         && (prevEnemyAtk != AttackState::Idle);
        bool inRecovery   = (curr == AttackState::Recovery);
        bool inStartup    = (curr == AttackState::Startup);
        bool isActive     = (curr == AttackState::Active);
        prevEnemyAtk = curr;

        // ── 3. DÉCISION D'ATTAQUE ────────────────────────────────────────────
        if (dist > cfg.attackRange) return;
        if (attacking)              return;

        bool doAttack;
        if (cfg.dexBotOnlyHit) {
            // Mode punish strict : seulement fin d'animation ou recovery
            doAttack = justFinished || inRecovery;
        } else {
            // Mode agressif : attaque quand l'ennemi N'A PAS sa hitbox active
            // Idle = inclut hitstun (enemy vient d'être touché) → attaque
            // Startup = avant leur hitbox → on peut punch first
            // Recovery = leur attaque rate → punish
            // Active = DANGER, leur hitbox est dehors → skip si dexBotNoActive=true
            if (cfg.dexBotNoActive && isActive)
                doAttack = false;
            else
                doAttack = true;  // tout sauf Active si flag actif
        }

        if (!doAttack) return;

        std::cout << "[DEXBOT] fire | dist=" << (int)dist
                  << " state=" << (int)curr
                  << " punish=" << justFinished
                  << " recovery=" << inRecovery << "\n";

        if (lastMoveDir) { Input::ReleaseKey(lastMoveDir); lastMoveDir = 0; }

        WORD face = std::abs(dx) > 20.f
            ? (dx > 0 ? Input::KEY_RIGHT : Input::KEY_LEFT)
            : 0;

        int delay = cfg.randomizeTimings
            ? std::max(0, rand() % std::max(1, cfg.timingVarianceMs) - cfg.timingVarianceMs/2)
            : 0;
        FireAttack(face, delay);
    }
};

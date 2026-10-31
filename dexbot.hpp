// dexbot.hpp — C++17, Windows, external
// Fix: attack hold 30ms (était 80), cooldown 80ms (était 180-200), delayMs floor à 0
// Fix: chase indépendant de attacking (déjà bon, conservé)
// Fix: attaque en Idle si dexBotOnlyHit=false, pas seulement en Recovery
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

    static constexpr float ATTACK_RANGE = 270.f;
    static constexpr float CHASE_RANGE  = 2000.f;  // élargi — KYAN chase tôt

    void FireAttack(WORD faceDir, int delayMs) {
        if (attacking.exchange(true)) return;
        std::thread([this, faceDir, delayMs]() {
            if (delayMs > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));

            if (faceDir) {
                Input::HoldKey(faceDir);
                std::this_thread::sleep_for(std::chrono::milliseconds(8));
            }

            // Light attack — 30ms hold suffit pour BH, 80ms était trop long
            // L'ennemi a le temps de dash cancel sur 80ms
            Input::PressKey(Input::KEY_ATTACK, 30, 0);

            if (faceDir) Input::ReleaseKey(faceDir);

            // Cooldown = durée approx animation légère BH ~6 frames @60fps = ~100ms
            // cfg.comboDelayMs (default 0) s'ajoute pour le scaling manuel
            int cooldown = 100 + std::max(0, cfg.comboDelayMs);
            if (cfg.randomizeTimings)
                cooldown += (rand() % std::max(1, cfg.timingVarianceMs));
            std::this_thread::sleep_for(std::chrono::milliseconds(cooldown));

            attacking = false;
        }).detach();
    }

public:
    DexBot(Config& c) : cfg(c) {}

    void Tick(const std::vector<BHPlayer>& players) {
        if (!cfg.dexBot)          return;
        if (players.size() < 2)   return;

        // Local = players[0] (heuristique cave) ou isLocal flag (AVM2)
        const BHPlayer* local = nullptr;
        const BHPlayer* enemy = nullptr;
        for (const auto& p : players) {
            if (p.isLocal)  local = &p;
            else if (!enemy) enemy = &p;
        }
        if (!local || !enemy) return;

        float dx   = enemy->worldPos.x - local->worldPos.x;
        float dist = enemy->distToLocal;

        // ── 1. CHASE ─────────────────────────────────────────────────────────
        // Toujours actif, même si attacking — le bot continue de se déplacer
        // pendant le cooldown d'attaque pour coller l'ennemi
        if (dist > ATTACK_RANGE && dist < CHASE_RANGE) {
            WORD want = dx > 0 ? Input::KEY_RIGHT : Input::KEY_LEFT;
            if (lastMoveDir != want) {
                if (lastMoveDir) Input::ReleaseKey(lastMoveDir);
                Input::HoldKey(want);
                lastMoveDir = want;
            }
        } else if (dist <= ATTACK_RANGE || dist >= CHASE_RANGE) {
            if (lastMoveDir) {
                Input::ReleaseKey(lastMoveDir);
                lastMoveDir = 0;
            }
        }

        // ── 2. VULNERABILITY ─────────────────────────────────────────────────
        AttackState curr = enemy->atkState;
        bool justFinished = (curr == AttackState::Idle) &&
                            (prevEnemyAtk != AttackState::Idle);
        bool inRecovery   = (curr == AttackState::Recovery);
        prevEnemyAtk = curr;

        // ── 3. ATTACK ────────────────────────────────────────────────────────
        if (dist > ATTACK_RANGE) return;
        if (attacking)           return;

        bool doAttack;
        if (cfg.dexBotOnlyHit) {
            // Mode punish : attaque seulement en fin d'animation ennemie
            doAttack = justFinished || inRecovery;
        } else {
            // Mode agressif : attaque dès que l'ennemi est Idle (pas en attaque lui-même)
            doAttack = (curr == AttackState::Idle) || inRecovery || justFinished;
        }
        if (!doAttack) return;

        std::cout << "[DEXBOT] fire dist=" << (int)dist
                  << " state=" << (int)curr << " punish=" << justFinished << "\n";

        if (lastMoveDir) { Input::ReleaseKey(lastMoveDir); lastMoveDir = 0; }

        WORD face = std::abs(dx) > 20.f
            ? (dx > 0 ? Input::KEY_RIGHT : Input::KEY_LEFT)
            : 0;

        // Jitter minimal — pas de plancher à 16ms ici
        int delay = cfg.randomizeTimings
            ? std::max(0, (rand() % std::max(1, cfg.timingVarianceMs)) - cfg.timingVarianceMs/2)
            : 0;
        FireAttack(face, delay);
    }
};

// dexbot.hpp — C++17, Windows, external process
// Runtime : ~8ms/tick depuis main.cpp
// Nécessite : Input::KEY_ATTACK_LIGHT dans input.hpp (voir note bas)
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

    // World-unit thresholds — calibrés sur dist=350 de tes logs
    static constexpr float ATTACK_RANGE = 510.f;  // portée melee BH
    static constexpr float CHASE_RANGE  = 1500.f; // chase au-delà de cette dist

    void FireAttack(WORD faceDir, int delayMs) {
        if (attacking.exchange(true)) return; // une attaque à la fois
        std::thread([this, faceDir, delayMs]() {
            if (delayMs > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));

            // Face l'ennemi avant d'attaquer
            if (faceDir) {
                Input::HoldKey(faceDir);
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }

            // Légère — la plus rapide, safe en punish
            Input::PressKey(Input::KEY_ATTACK_LIGHT, 80, 0);

            if (faceDir) Input::ReleaseKey(faceDir);

            // Cooldown post-attaque : laisse l'animation sortir
            int cooldown = cfg.randomizeTimings
                ? 180 + (rand() % cfg.timingVarianceMs)
                : 200;
            std::this_thread::sleep_for(std::chrono::milliseconds(cooldown));

            attacking = false;
        }).detach();
    }

public:
    DexBot(Config& c) : cfg(c) {}

    void Tick(const std::vector<BHPlayer>& players) {
        if (!cfg.dexBot) return;
        if (players.size() < 2) return;

        const BHPlayer& local = players[0];
        const BHPlayer& enemy = players[1];

        AttackState curr = enemy.atkState;
        float dx   = enemy.worldPos.x - local.worldPos.x;
        float dist = enemy.distToLocal;

        // ── 1. CHASE — pression constante, force les erreurs ───────────
        // Logique : se coller à l'ennemi → il doit dodger ou attaquer
        // → autododge gère l'attaque entrante, dexbot punit la recovery
        if (!attacking) {
            if (dist > ATTACK_RANGE && dist < CHASE_RANGE) {
                WORD wantDir = dx > 0 ? Input::KEY_RIGHT : Input::KEY_LEFT;
                if (lastMoveDir != wantDir) {
                    if (lastMoveDir) Input::ReleaseKey(lastMoveDir);
                    Input::HoldKey(wantDir);
                    lastMoveDir = wantDir;
                }
            } else {
                // En portée d'attaque ou trop loin → stop mouvement
                if (lastMoveDir) {
                    Input::ReleaseKey(lastMoveDir);
                    lastMoveDir = 0;
                }
            }
        }

        // ── 2. VULNERABILITY DETECTION ─────────────────────────────────
        // justFinished : transition non-Idle→Idle = fin d'attaque = punish window
        // inRecovery   : state 3 si BH l'expose (pas garanti, bonus si oui)
        bool justFinished = (curr == AttackState::Idle) &&
                            (prevEnemyAtk != AttackState::Idle);
        bool inRecovery   = (curr == AttackState::Recovery);
        prevEnemyAtk = curr;

        // ── 3. ATTACK DECISION ─────────────────────────────────────────
        if (dist > ATTACK_RANGE) return;
        if (attacking)           return;

        bool doAttack = false;

        if (cfg.dexBotOnlyHit) {
            // Mode précis : punish uniquement sur window de recovery/fin d'attaque
            doAttack = justFinished || inRecovery;
        } else {
            // Mode agressif : attaque dès que l'ennemi est idle et à portée
            // Combiné avec le chase → force des dodges en continu
            doAttack = (curr == AttackState::Idle);
        }

        if (!doAttack) return;

        std::cout << "[DEXBOT] attack — dist=" << (int)dist
                  << " state=" << (int)curr
                  << " punish=" << justFinished << "\n";

        // Stop chase avant d'attaquer (évite input contradictoire)
        if (lastMoveDir) {
            Input::ReleaseKey(lastMoveDir);
            lastMoveDir = 0;
        }

        WORD faceDir = std::abs(dx) > 30.f
            ? (dx > 0 ? Input::KEY_RIGHT : Input::KEY_LEFT)
            : 0;

        int delay = Input::Jitter(cfg.comboDelayMs, cfg.timingVarianceMs);
        FireAttack(faceDir, delay);
    }
};

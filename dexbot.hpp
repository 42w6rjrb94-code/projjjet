// dexbot.hpp — v2
// FIX : Tick() accepte bool isDodging depuis main.cpp — ChaseAI a la priorité
// FIX : DexBot ne gère plus le chase (ChaseAI s'en charge), uniquement l'attaque
// ADD : gravity cancel aérien après combo (cfg.autoGravCancel)
// ADD : signature attack quand ennemi à haute % et dans sigRange (cfg.useSig)
// FIX : dist calculée inline, indépendant de distToLocal
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

    AttackState       prevEnemyAtk  = AttackState::Idle;
    std::atomic<bool> attacking{false};
    int               cachedLocalIdx = -1;
    int               cachedEnemyIdx = -1;

    // Attaque normale — grav cancel optionnel si local est en l'air
    void FireAttack(WORD faceDir, int delayMs,
                    bool gravCancel, bool localAirborne) {
        if (attacking.exchange(true)) return;
        Config& c   = cfg;
        bool    air = localAirborne;
        std::thread([this, faceDir, delayMs, gravCancel, air, &c]() {
            if (delayMs > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            if (faceDir) {
                Input::HoldKey(faceDir);
                std::this_thread::sleep_for(std::chrono::milliseconds(8));
            }
            Input::PressKey(Input::KEY_ATTACK, 30,
                c.randomizeTimings ? c.timingVarianceMs : 0);
            if (faceDir) Input::ReleaseKey(faceDir);

            // Gravity cancel : DOWN + ATTACK en l'air après l'attaque principale
            // Timing 45ms = après animation de la light, avant que le perso atterrisse
            // À affiner selon l'arme utilisée (50-70ms pour la plupart)
            if (gravCancel && air) {
                std::this_thread::sleep_for(std::chrono::milliseconds(48));
                Input::HoldKey(Input::KEY_DOWN);
                std::this_thread::sleep_for(std::chrono::milliseconds(8));
                Input::PressKey(Input::KEY_ATTACK, 30,
                    c.randomizeTimings ? c.timingVarianceMs : 0);
                Input::ReleaseKey(Input::KEY_DOWN);
            }

            int cd = 100 + std::max(0, c.comboDelayMs);
            if (c.randomizeTimings)
                cd += (rand() % std::max(1, c.timingVarianceMs));
            std::this_thread::sleep_for(std::chrono::milliseconds(cd));
            attacking = false;
        }).detach();
    }

    // Signature — animation plus longue, cooldown étendu
    void FireSig(WORD faceDir, int delayMs) {
        if (attacking.exchange(true)) return;
        Config& c = cfg;
        std::thread([this, faceDir, delayMs, &c]() {
            if (delayMs > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            if (faceDir) {
                Input::HoldKey(faceDir);
                std::this_thread::sleep_for(std::chrono::milliseconds(8));
            }
            Input::PressKey(Input::KEY_SIG, 35,
                c.randomizeTimings ? c.timingVarianceMs : 0);
            if (faceDir) Input::ReleaseKey(faceDir);
            // Sig animation ≈ 350-500ms selon le perso — pas d'attaque dans ce délai
            std::this_thread::sleep_for(std::chrono::milliseconds(380));
            attacking = false;
        }).detach();
    }

    void ResolveIndices(const std::vector<BHPlayer>& players) {
        if (players.size() < 2) { cachedLocalIdx = -1; cachedEnemyIdx = -1; return; }
        // Vérifier que les indices cachés sont toujours valides
        if (cachedLocalIdx >= 0 && cachedLocalIdx < (int)players.size()
            && cachedEnemyIdx >= 0 && cachedEnemyIdx < (int)players.size()
            && players[cachedLocalIdx].isLocal
            && !players[cachedEnemyIdx].isLocal) return;

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

    // isDodging provient de ChaseAI::Tick() via main.cpp — pas de dépendance croisée
    void Tick(const std::vector<BHPlayer>& players, bool isDodging) {
        if (!cfg.dexBot)          return;
        if (isDodging)            return;   // ChaseAI en dodge → priorité absolue
        if (players.size() < 2)   return;

        ResolveIndices(players);
        if (cachedLocalIdx < 0 || cachedEnemyIdx < 0) return;

        int li = cachedLocalIdx;
        int ei = cachedEnemyIdx;
        if (cfg.dexBotSwapLocal) std::swap(li, ei);

        const BHPlayer& local = players[li];
        const BHPlayer& enemy = players[ei];

        // Dist inline — jamais depuis distToLocal (peut être 0 si indices inversés)
        float dx   = enemy.worldPos.x - local.worldPos.x;
        float dy   = enemy.worldPos.y - local.worldPos.y;
        float dist = sqrtf(dx*dx + dy*dy);

        // Hors zone d'attaque → rien, ChaseAI gère le déplacement
        if (dist > cfg.attackRange) return;
        if (attacking) return;

        // ── État ennemi ────────────────────────────────────────────────────
        AttackState curr         = enemy.atkState;
        bool        justFinished = (curr == AttackState::Idle)
                                    && (prevEnemyAtk != AttackState::Idle);
        bool        inRecovery   = (curr == AttackState::Recovery);
        bool        isActive     = (curr == AttackState::Active);
        prevEnemyAtk = curr;

        // ── Décision attaque ───────────────────────────────────────────────
        bool doAttack;
        if (cfg.dexBotOnlyHit) {
            // Mode prudent : punir uniquement recovery ou fin d'animation
            doAttack = justFinished || inRecovery;
        } else {
            // Mode agressif : tout sauf active si dexBotNoActive
            doAttack = !(cfg.dexBotNoActive && isActive);
        }
        if (!doAttack) return;

        WORD face = std::abs(dx) > 20.f
            ? (dx > 0 ? Input::KEY_RIGHT : Input::KEY_LEFT) : 0;
        int delay = cfg.randomizeTimings
            ? std::max(0, rand() % std::max(1, cfg.timingVarianceMs)) : 0;

        // Sig si ennemi à haute % et dans sigRange
        // hp > 80 = il a pris des dégâts, sig peut KO
        bool doSig = cfg.useSig && dist < cfg.sigRange && enemy.hp > 80.f;

        if (doSig) {
            std::cout << "[DEXBOT] SIG d=" << (int)dist
                      << " hp=" << (int)enemy.hp << "\n";
            FireSig(face, delay);
        } else {
            std::cout << "[DEXBOT] ATK d=" << (int)dist
                      << " state=" << (int)curr << "\n";
            bool gc = cfg.autoGravCancel && !local.isGrounded;
            FireAttack(face, delay, gc, !local.isGrounded);
        }
    }
};

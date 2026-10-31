// dexbot.hpp — C++17, Windows, external
// Chase overhaul : 3 zones + hysterèse + no oscillation
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
    uint64_t          lastHoldTick = 0;
    int               cachedLocalIdx = -1;
    int               cachedEnemyIdx = -1;

    // Hysterèse : deux seuils par zone pour éviter l'oscillation
    // CHASE  : entre chaseRange et engageRange
    // ENGAGE : entre engageRange et attackRange  → marche lentement / stop
    // ATTACK : sous attackRange                  → attaque, plus de mouvement
    // On entre dans une zone quand on croise le seuil OUTER
    // On en sort seulement quand on croise le seuil INNER (plus petit)
    // → le bot ne peut pas osciller entre deux zones

    enum class Zone { Far, Chase, Engage, Attack };
    Zone currentZone = Zone::Far;

    Zone ComputeZone(float dist) {
        float chaseOuter  = cfg.chaseRange;
        float chaseInner  = cfg.chaseRange  * 0.85f;
        float engageOuter = cfg.attackRange * 2.2f;   // engage = 2.2x attack range
        float engageInner = cfg.attackRange * 1.8f;
        float attackOuter = cfg.attackRange;
        float attackInner = cfg.attackRange * 0.75f;

        // Transitions basées sur la zone actuelle + hysterèse
        switch (currentZone) {
            case Zone::Far:
                if (dist < chaseOuter)  return Zone::Chase;
                return Zone::Far;
            case Zone::Chase:
                if (dist > chaseOuter)  return Zone::Far;
                if (dist < engageOuter) return Zone::Engage;
                return Zone::Chase;
            case Zone::Engage:
                if (dist > chaseInner)  return Zone::Chase;
                if (dist < attackOuter) return Zone::Attack;
                return Zone::Engage;
            case Zone::Attack:
                if (dist > engageInner) return Zone::Engage;
                return Zone::Attack;
        }
        return Zone::Far;
    }

    void SetMoveDir(WORD want) {
        if (want == 0) {
            if (lastMoveDir) { Input::ReleaseKey(lastMoveDir); lastMoveDir = 0; lastHoldTick = 0; }
            return;
        }
        uint64_t now = GetTickCount64();
        bool needResend = (lastMoveDir != want) || (now - lastHoldTick > 120);
        if (needResend) {
            if (lastMoveDir && lastMoveDir != want)
                Input::ReleaseKey(lastMoveDir);
            Input::HoldKey(want);
            lastMoveDir  = want;
            lastHoldTick = now;
        }
    }

    void StopMove() { SetMoveDir(0); }

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
        std::cout << "[DEXBOT] local=[" << li << "] enemy=[" << ei << "]\n";
    }

public:
    DexBot(Config& c) : cfg(c) {}

    void Tick(const std::vector<BHPlayer>& players) {
        if (!cfg.dexBot)          return;
        if (players.size() < 2)   return;

        ResolveIndices(players);
        if (cachedLocalIdx < 0 || cachedEnemyIdx < 0) return;

        const BHPlayer& local = players[cachedLocalIdx];
        const BHPlayer& enemy = players[cachedEnemyIdx];

        float dx   = enemy.worldPos.x - local.worldPos.x;
        float dy   = enemy.worldPos.y - local.worldPos.y;
        float dist = sqrtf(dx*dx + dy*dy);
        WORD  toward = dx > 0 ? Input::KEY_RIGHT : Input::KEY_LEFT;

        // ── Mise à jour zone ─────────────────────────────────────────────
        currentZone = ComputeZone(dist);

        // ── Comportement par zone ────────────────────────────────────────
        switch (currentZone) {

            case Zone::Far:
                // Hors de portée — stop tout
                StopMove();
                break;

            case Zone::Chase:
                // Sprint vers l'ennemi
                if (!attacking) SetMoveDir(toward);
                break;

            case Zone::Engage:
                // Zone tampon : avance doucement mais arrête d'osciller
                // On ralentit : hold key seulement si on s'éloigne
                if (!attacking) {
                    // Marche vers l'ennemi mais avec une pause entre chaque hold
                    // → moins agressif, pas de rubber-band
                    uint64_t now = GetTickCount64();
                    if (lastMoveDir != toward || (now - lastHoldTick > 300)) {
                        SetMoveDir(toward);
                        // Relâche après 80ms → petit pas, pas de sprint
                        std::thread([this, toward]() {
                            std::this_thread::sleep_for(std::chrono::milliseconds(80));
                            if (lastMoveDir == toward) StopMove();
                        }).detach();
                    }
                }
                break;

            case Zone::Attack:
                // Stop mouvement — on est à portée, on attaque
                StopMove();
                break;
        }

        // ── État ennemi ──────────────────────────────────────────────────
        AttackState curr = enemy.atkState;
        bool justFinished = (curr == AttackState::Idle) && (prevEnemyAtk != AttackState::Idle);
        bool inRecovery   = (curr == AttackState::Recovery);
        bool isActive     = (curr == AttackState::Active);
        prevEnemyAtk = curr;

        // ── Attaque — uniquement en zone Attack ──────────────────────────
        if (currentZone != Zone::Attack) return;
        if (attacking) return;

        bool doAttack;
        if (cfg.dexBotOnlyHit) {
            doAttack = justFinished || inRecovery;
        } else {
            doAttack = !(cfg.dexBotNoActive && isActive);
        }
        if (!doAttack) return;

        WORD face = std::abs(dx) > 20.f ? toward : 0;
        int delay = cfg.randomizeTimings
            ? std::max(0, rand() % std::max(1, cfg.timingVarianceMs) - cfg.timingVarianceMs/2)
            : 0;
        FireAttack(face, delay);
    }
};

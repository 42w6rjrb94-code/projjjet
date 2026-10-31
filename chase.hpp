// chase.hpp — IA de mouvement prioritaire
// Priorité 1 : esquiver projectile (threat.isFast, dist <400)
// Priorité 2 : esquiver hazard stage (threat.isHazard, dist <220)
// Priorité 3 : esquiver attaque joueur (autoDodge — onset Idle→non-Idle)
// Priorité 4 : chaser l'ennemi le plus proche (chaseEnabled)
// Retourne IsDodging() → DexBot skip pendant le cooldown
#pragma once
#include "entities.hpp"
#include "input.hpp"
#include "config.hpp"
#include "threat.hpp"
#include <vector>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <chrono>
#include <cmath>

class ChaseAI {
    Config& cfg;

    std::atomic<bool> dodging_{false};
    WORD              heldKey = 0;
    int               cachedLocalIdx = -1;
    std::unordered_map<int,AttackState> prevAtk;

    void ReleaseHeld() {
        if (heldKey) { Input::ReleaseKey(heldKey); heldKey=0; }
    }

    void HoldDir(WORD k) {
        if (heldKey == k) return;
        ReleaseHeld();
        Input::HoldKey(k);
        heldKey = k;
    }

    // Lance le dodge sur un thread séparé → non bloquant
    // Atomic dodge_ reste true pendant toute la durée → DexBot.Tick est skippé
    void DoDodge(WORD dir, bool jump) {
        if (dodging_.exchange(true)) return;  // déjà en dodge
        ReleaseHeld();
        WORD d = dir; bool j = jump;
        Config& c = cfg;
        std::thread([this, d, j, &c]() {
            if (d) {
                Input::HoldKey(d);
                std::this_thread::sleep_for(std::chrono::milliseconds(12));
            }
            Input::PressKey(Input::KEY_DODGE, 50, c.timingVarianceMs);
            if (j) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                Input::PressKey(Input::KEY_JUMP, 55, c.timingVarianceMs);
            }
            if (d) Input::ReleaseKey(d);
            // Cooldown — empêche double-dodge pendant l'animation
            std::this_thread::sleep_for(std::chrono::milliseconds(140));
            dodging_ = false;
        }).detach();
    }

    void ResolveLocal(const std::vector<BHPlayer>& pl) {
        if (cachedLocalIdx >= 0 && cachedLocalIdx < (int)pl.size()) return;
        cachedLocalIdx = 0;
        for (int i=0;i<(int)pl.size();i++) if(pl[i].isLocal){cachedLocalIdx=i;return;}
    }

public:
    ChaseAI(Config& c) : cfg(c) {}
    ~ChaseAI() { ReleaseHeld(); }

    bool IsDodging() const { return dodging_.load(); }

    // Retourne true si un dodge est en cours → DexBot doit skip
    bool Tick(const std::vector<BHPlayer>& players,
               const std::vector<Threat>&   threats)
    {
        if (players.empty()) { ReleaseHeld(); return false; }

        // Si déjà en dodge : bloquer DexBot + skip le reste
        if (dodging_.load()) return true;

        ResolveLocal(players);
        if (cachedLocalIdx >= (int)players.size()) { ReleaseHeld(); return false; }

        const BHPlayer& local = players[cachedLocalIdx];
        float lx = local.worldPos.x, ly = local.worldPos.y;

        // ── 1. Esquiver projectile ─────────────────────────────────────────
        if (cfg.threatDodge) {
            const Threat* worst = nullptr;
            float worstScore = 0.f;

            for (const auto& t : threats) {
                if (!t.isFast) continue;
                // Calculer si le projectile se dirige vers nous
                float predDx = t.predX - lx, predDy = t.predY - ly;
                float predDist = std::sqrtf(predDx*predDx + predDy*predDy);
                // Score = danger si trajectoire prédite plus proche que position actuelle
                if (predDist < t.dist + 30.f && t.dist < 400.f) {
                    float score = (400.f - t.dist) / 400.f;
                    if (score > worstScore) { worstScore = score; worst = &t; }
                }
            }

            if (worst) {
                // Dodge perpendiculaire à la trajectory du projectile
                bool hThreat = std::abs(worst->vx) >= std::abs(worst->vy);
                WORD dir;
                if (hThreat) {
                    // Projectile horizontal → monter + dodge opposé
                    dir = (worst->x < lx) ? Input::KEY_RIGHT : Input::KEY_LEFT;
                } else {
                    // Projectile vertical → dodge latéral
                    dir = (worst->x < lx) ? Input::KEY_RIGHT : Input::KEY_LEFT;
                }
                bool jump = worst->vy > 8.f || (std::abs(worst->y - ly) < 120.f);
                DoDodge(dir, jump);
                return true;
            }

            // ── 2. Esquiver hazard stage ──────────────────────────────────
            for (const auto& t : threats) {
                if (!t.isHazard || t.dist > 220.f) continue;
                WORD dir = (t.x < lx) ? Input::KEY_RIGHT : Input::KEY_LEFT;
                DoDodge(dir, false);
                return true;
            }
        }

        // ── 3. Esquiver attaque joueur (autoDodge) ─────────────────────────
        if (cfg.autoDodge) {
            for (int ei = 0; ei < (int)players.size(); ei++) {
                if (ei == cachedLocalIdx) continue;
                const BHPlayer& e = players[ei];
                if (e.distToLocal > 500.f) continue;
                if (cfg.dodgeGroundOnly && !local.isGrounded) continue;

                AttackState prev = prevAtk[ei];
                AttackState curr = e.atkState;
                prevAtk[ei] = curr;

                bool onset = (curr != AttackState::Idle) && (prev == AttackState::Idle);
                if (!onset) continue;

                float ddx = e.worldPos.x - lx;
                WORD dir = (std::abs(ddx) > 50.f)
                    ? (ddx > 0 ? Input::KEY_LEFT : Input::KEY_RIGHT)
                    : Input::KEY_UP;

                int delay = Input::Jitter(cfg.dodgeDelayMs, cfg.timingVarianceMs);
                if (delay > 0)
                    std::this_thread::sleep_for(std::chrono::milliseconds(delay));

                DoDodge(dir, false);
                return true;
            }
        }

        // ── 4. Chase vers l'ennemi le plus proche ─────────────────────────
        if (cfg.chaseEnabled && players.size() >= 2) {
            const BHPlayer* tgt = nullptr;
            float minD = 9999.f;
            for (int i=0;i<(int)players.size();i++) {
                if (i==cachedLocalIdx) continue;
                if (players[i].distToLocal < minD) {
                    minD = players[i].distToLocal;
                    tgt  = &players[i];
                }
            }

            if (tgt && minD < cfg.chaseRange) {
                float ddx = tgt->worldPos.x - lx;
                float ddy = tgt->worldPos.y - ly;

                if (minD > cfg.attackRange) {
                    // Chase horizontal
                    HoldDir(ddx > 0 ? Input::KEY_RIGHT : Input::KEY_LEFT);
                    // Jump si l'ennemi est au-dessus
                    if (ddy > 180.f)
                        Input::PressKey(Input::KEY_JUMP, 55, cfg.timingVarianceMs);
                    return false;
                } else {
                    // Dans la zone d'attaque — stop mouvement, DexBot attaque
                    ReleaseHeld();
                    return false;
                }
            }
        }

        ReleaseHeld();
        return false;
    }

    void Stop()  { ReleaseHeld(); }
    void Reset() { cachedLocalIdx=-1; prevAtk.clear(); }
};

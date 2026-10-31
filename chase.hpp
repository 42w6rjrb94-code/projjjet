// chase.hpp — v2 — autorité de mouvement unique
// FIX MAJEUR : direction dodge projectile perpendiculaire à trajectoire réelle
//   avant : branche verticale identique à branche horizontale → dodge toujours
//           dans la même direction indépendamment du vecteur du projectile
//   après : horizontal → UP (perpendiculaire) ; vertical → latéral
// ADD : IsChasing() pour coordination WeaponSnap depuis main.cpp
// ADD : retraite défensive si local.hp > cfg.lowHpThreshold
// ADD : jump vers ennemi si delta Y > 180u
// ADD : fast-fall si local est au-dessus de l'ennemi dans zone d'attaque
// Priorité 1 projectile → 2 hazard → 3 attaque joueur → 4 chase → (WeaponSnap = main)
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
#include <iostream>

class ChaseAI {
    Config& cfg;

    std::atomic<bool> dodging_{false};
    std::atomic<bool> isChasing_{false};
    WORD              heldKey        = 0;
    int               cachedLocalIdx = -1;
    std::unordered_map<int,AttackState> prevAtk;

    void ReleaseHeld() {
        if (heldKey) { Input::ReleaseKey(heldKey); heldKey = 0; }
    }

    void HoldDir(WORD k) {
        if (heldKey == k) return;
        ReleaseHeld();
        Input::HoldKey(k);
        heldKey = k;
    }

    // Lance le dodge sur thread séparé — non bloquant
    // dodging_ reste true pendant l'animation → DexBot cède
    void DoDodge(WORD dir, bool addJump, int postCooldownMs = 160) {
        if (dodging_.exchange(true)) return;  // dodge déjà en cours
        ReleaseHeld();
        WORD d = dir; bool j = addJump; int cd = postCooldownMs;
        Config& c = cfg;
        std::thread([this, d, j, cd, &c]() {
            // Tenir la direction AVANT de presser dodge → dodge directionnel
            if (d) {
                Input::HoldKey(d);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            Input::PressKey(Input::KEY_DODGE, 55, c.timingVarianceMs);
            // Saut additionnel pour monter après le dodge
            if (j) {
                std::this_thread::sleep_for(std::chrono::milliseconds(18));
                Input::PressKey(Input::KEY_JUMP, 55, c.timingVarianceMs);
            }
            if (d) Input::ReleaseKey(d);
            // Cooldown animation — bloque DexBot pendant ce temps
            std::this_thread::sleep_for(std::chrono::milliseconds(cd));
            dodging_ = false;
        }).detach();
    }

    void ResolveLocal(const std::vector<BHPlayer>& pl) {
        if (cachedLocalIdx >= 0 && cachedLocalIdx < (int)pl.size()
            && pl[cachedLocalIdx].isLocal) return;
        cachedLocalIdx = 0;
        for (int i = 0; i < (int)pl.size(); i++)
            if (pl[i].isLocal) { cachedLocalIdx = i; return; }
    }

    // Direction dodge optimale pour un projectile donné
    // FIX : perpendiculaire à la VRAIE trajectoire, pas copie de la branche horizontale
    WORD ProjectileDodgeDir(const Threat& t, float lx, float ly, bool& addJump) {
        bool hThreat = std::abs(t.vx) >= std::abs(t.vy);

        if (hThreat) {
            // Projectile majoritairement horizontal (lance, boomerang, flèche…)
            // Esquive perpendiculaire = haut
            // Si à hauteur similaire → UP + saut pour sortir de la trajectoire
            if (std::abs(t.y - ly) < 110.f) {
                addJump = true;
                return Input::KEY_UP;   // UP + DODGE = air-dodge haut
            }
            // Projectile au-dessus/dessous → dodge latéral loin de la source
            return (t.x < lx) ? Input::KEY_RIGHT : Input::KEY_LEFT;
        } else {
            // Projectile majoritairement vertical (Orion pilier, mine tombante…)
            // Esquive perpendiculaire = latéral
            addJump = (t.vy > 0.f);  // tombe vers nous → saut additionnel
            // Choisir le côté opposé au décalage X du projectile
            return (t.x < lx) ? Input::KEY_RIGHT : Input::KEY_LEFT;
        }
    }

public:
    ChaseAI(Config& c) : cfg(c) {}
    ~ChaseAI() { ReleaseHeld(); }

    bool IsDodging() const { return dodging_.load(); }
    bool IsChasing() const { return isChasing_.load(); }

    // Tick principal — retourne true si en dodge (DexBot doit skip)
    bool Tick(const std::vector<BHPlayer>& players,
               const std::vector<Threat>&   threats)
    {
        isChasing_ = false;

        if (players.empty()) { ReleaseHeld(); return dodging_.load(); }
        if (dodging_.load()) return true;   // dodge en cours, rien d'autre

        ResolveLocal(players);
        if (cachedLocalIdx >= (int)players.size()) { ReleaseHeld(); return false; }

        const BHPlayer& local = players[cachedLocalIdx];
        float lx = local.worldPos.x, ly = local.worldPos.y;

        // ── 1. Projectile ─────────────────────────────────────────────────────
        if (cfg.threatDodge) {
            const Threat* worst  = nullptr;
            float         worstS = 0.f;

            for (const auto& t : threats) {
                if (!t.isFast) continue;
                // Le projectile se rapproche-t-il ? comparer dist actuelle vs prédite
                float pdx = t.predX - lx, pdy = t.predY - ly;
                float pDist = std::sqrtf(pdx*pdx + pdy*pdy);
                // Dangereux si trajectoire prédite + proche ET dans portée
                if (pDist < t.dist + 40.f && t.dist < 430.f) {
                    float score = (430.f - t.dist) / 430.f;
                    if (score > worstS) { worstS = score; worst = &t; }
                }
            }

            if (worst) {
                bool addJump = false;
                WORD dir = ProjectileDodgeDir(*worst, lx, ly, addJump);
                std::cout << "[CHASE] PROJ d=" << (int)worst->dist
                          << " v=(" << worst->vx << "," << worst->vy
                          << ") dir=" << (char)dir << " j=" << addJump << "\n";
                DoDodge(dir, addJump, 140);
                return true;
            }

            // ── 2. Hazard stage ─────────────────────────────────────────────
            for (const auto& t : threats) {
                if (!t.isHazard || t.dist > 235.f) continue;
                WORD dir  = (t.x < lx) ? Input::KEY_RIGHT : Input::KEY_LEFT;
                bool jump = (t.y > ly + 50.f);  // hazard en dessous → sauter
                std::cout << "[CHASE] HAZARD d=" << (int)t.dist << "\n";
                DoDodge(dir, jump, 160);
                return true;
            }
        }

        // ── 3. Attaque joueur ────────────────────────────────────────────────
        if (cfg.autoDodge) {
            for (int ei = 0; ei < (int)players.size(); ei++) {
                if (ei == cachedLocalIdx) continue;
                const BHPlayer& e = players[ei];

                float ddx  = e.worldPos.x - lx;
                float ddy  = e.worldPos.y - ly;
                float dist = std::sqrtf(ddx*ddx + ddy*ddy);
                if (dist > cfg.dodgeRange) continue;
                if (cfg.dodgeGroundOnly && !local.isGrounded) continue;

                AttackState prev = prevAtk[ei];
                AttackState curr = e.atkState;
                prevAtk[ei] = curr;

                bool onset = (curr != AttackState::Idle) && (prev == AttackState::Idle);
                if (!onset) continue;

                WORD dir  = (std::abs(ddx) > 50.f)
                    ? (ddx > 0 ? Input::KEY_LEFT : Input::KEY_RIGHT)
                    : Input::KEY_UP;
                bool jump = (dir == Input::KEY_UP);

                int delay = Input::Jitter(cfg.dodgeDelayMs, cfg.timingVarianceMs);
                if (delay > 0)
                    std::this_thread::sleep_for(std::chrono::milliseconds(delay));

                std::cout << "[CHASE] PLAYER DODGE dist=" << (int)dist
                          << " atk=" << (int)curr << "\n";
                DoDodge(dir, jump, 150);
                return true;
            }
        }

        // ── 4. Chase vers cible ──────────────────────────────────────────────
        if (cfg.chaseEnabled && players.size() >= 2) {
            // Mode défensif : hp local > threshold → s'éloigner au lieu d'approcher
            bool defensive = (cfg.lowHpThreshold > 0.f &&
                               local.hp > cfg.lowHpThreshold);

            const BHPlayer* tgt   = nullptr;
            float           minD  = 9999.f;
            for (int i = 0; i < (int)players.size(); i++) {
                if (i == cachedLocalIdx) continue;
                if (players[i].distToLocal < minD) {
                    minD = players[i].distToLocal;
                    tgt  = &players[i];
                }
            }

            if (tgt && minD < cfg.chaseRange) {
                isChasing_ = true;
                float ddx = tgt->worldPos.x - lx;
                float ddy = tgt->worldPos.y - ly;  // négatif = ennemi au-dessus

                if (defensive && minD < cfg.attackRange * 1.5f) {
                    // Retraite : direction opposée à l'ennemi
                    HoldDir(ddx > 0 ? Input::KEY_LEFT : Input::KEY_RIGHT);
                    if (ddy < -150.f)
                        Input::PressKey(Input::KEY_JUMP, 55, cfg.timingVarianceMs);
                    return false;
                }

                if (minD > cfg.attackRange) {
                    // Chase horizontal vers l'ennemi
                    HoldDir(ddx > 0 ? Input::KEY_RIGHT : Input::KEY_LEFT);

                    // Saut si ennemi significativement au-dessus (ddy < 0 = ennemi plus haut)
                    if (ddy < -180.f)
                        Input::PressKey(Input::KEY_JUMP, 55, cfg.timingVarianceMs);

                    // Fast-fall si local est au-dessus de l'ennemi dans la zone d'attaque
                    // Presse DOWN brièvement pour accélérer la descente
                    if (ddy > 150.f && minD < cfg.attackRange * 1.3f) {
                        Input::HoldKey(Input::KEY_DOWN);
                        std::this_thread::sleep_for(std::chrono::milliseconds(6));
                        Input::ReleaseKey(Input::KEY_DOWN);
                    }
                } else {
                    // Dans la zone d'attaque → arrêter le mouvement, DexBot prend le relais
                    ReleaseHeld();
                    isChasing_ = false;
                }
                return false;
            }
        }

        ReleaseHeld();
        isChasing_ = false;
        return false;
    }

    void Stop()  { ReleaseHeld(); dodging_ = false; isChasing_ = false; }
    void Reset() { cachedLocalIdx = -1; prevAtk.clear(); }
};

// threat.hpp — détection menaces depuis le ring buffer de la cave
// Toutes les entités passent par le même movsd (joueurs ET projectiles ET hazards)
// On lit le cave brut, on exclut les joueurs connus, on classifie par vitesse
#pragma once
#include "memory.hpp"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cmath>

struct Threat {
    uintptr_t addr  = 0;
    float x=0, y=0;
    float vx=0, vy=0;
    float speed     = 0;
    float dist      = 0;
    // Position prédite dans 3 frames — pour savoir si ça se dirige vers nous
    float predX=0, predY=0;
    bool  isFast    = false;  // projectile  (speed > 15 u/frame)
    bool  isHazard  = false;  // stage hazard (speed 0.8–15 u/frame)
};

class ThreatTracker {
    // Seuils calibrés pour Brawlhalla (coordonnées en double, ~1-3000 unités de scène)
    static constexpr float FAST_THRESH    = 15.f;   // au-delà = projectile
    static constexpr float SLOW_THRESH    = 0.8f;   // en dessous = statique / weapon au sol
    static constexpr float MAX_RANGE      = 650.f;  // ignorer au-delà

    struct Entry { float x=0,y=0; };
    std::unordered_map<uintptr_t,Entry> prev;

public:
    std::vector<Threat> Update(
        Memory&   mem,
        uintptr_t pointerArray,
        float     localX, float localY,
        const std::vector<uintptr_t>& playerAddrs)
    {
        std::vector<Threat> out;
        if (!pointerArray) return out;

        std::unordered_set<uintptr_t> seen;
        std::unordered_set<uintptr_t> pset(playerAddrs.begin(), playerAddrs.end());

        for (int i = 0; i < 50; i++) {
            uintptr_t dp = mem.Read<uintptr_t>(pointerArray + i*8);
            if (!dp || seen.count(dp) || pset.count(dp)) continue;
            seen.insert(dp);

            double posX = mem.Read<double>(dp + 0x148);
            double posY = mem.Read<double>(dp + 0x140);
            if (posX < -20000.0 || posX > 20000.0 ||
                posY < -20000.0 || posY > 20000.0) {
                prev.erase(dp); continue;
            }

            float fx = (float)posX, fy = (float)posY;
            float ddx = fx - localX, ddy = fy - localY;
            float dist = std::sqrtf(ddx*ddx + ddy*ddy);
            if (dist > MAX_RANGE) { prev.erase(dp); continue; }

            Threat t;
            t.addr = dp; t.x=fx; t.y=fy; t.dist=dist;

            auto it = prev.find(dp);
            if (it != prev.end()) {
                t.vx    = fx - it->second.x;
                t.vy    = fy - it->second.y;
                t.speed = std::sqrtf(t.vx*t.vx + t.vy*t.vy);
                t.predX = fx + t.vx * 4.f;
                t.predY = fy + t.vy * 4.f;
                t.isFast   = t.speed > FAST_THRESH;
                t.isHazard = t.speed > SLOW_THRESH && !t.isFast;
                it->second = {fx, fy};
            } else {
                prev[dp] = {fx, fy};
            }

            if (t.isFast || (t.isHazard && dist < 300.f))
                out.push_back(t);
        }

        // Nettoyer les DPs disparus
        for (auto it = prev.begin(); it != prev.end(); )
            it = seen.count(it->first) ? std::next(it) : prev.erase(it);

        return out;
    }

    void Clear() { prev.clear(); }
};

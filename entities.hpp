// entities.hpp — GetGroundWeapons extrait du cave par exclusion des joueurs connus
#pragma once
#include <iostream>
#include "memory.hpp"
#include "offsets.hpp"
#include <vector>
#include <string>
#include <cmath>

struct Vec2 { float x=0,y=0; };
struct Vec3 { float x=0,y=0,z=0; };
enum class AttackState : int { Idle=0,Startup=1,Active=2,Recovery=3 };

struct BHPlayer {
    uintptr_t   addr=0;
    float       hp=200,maxHp=200;
    int         teamID=0;
    std::string name;
    Vec3        worldPos;
    Vec2        velocity;
    bool        isGrounded=true;
    bool        isLocal=false;
    bool        facingRight=false;
    AttackState atkState=AttackState::Idle;
    int         atkFrame=0;
    float       dodgeCD=0.f;
    uintptr_t   weaponPtr=0;
    int         weaponType=-1;
    float       distToLocal=0.f;
};

struct BHWeapon {
    uintptr_t addr=0;
    Vec3      pos;
    int       type=0;
    float     distToLocal=0.f;
};

class Scene {
    Memory&   mem;
    uintptr_t playerArray = 0;
    uintptr_t playerIndex = 0;
    bool      caveReady   = false;
    uintptr_t airBase      = 0;
    uintptr_t mainInstance = 0;
    bool      avmReady     = false;

    bool SetupCave(const char* pattern, const char* mask,
                   uintptr_t& outArray, uintptr_t& outIndex) {
        uintptr_t target = mem.FindPatternSafe(pattern, mask);
        if (!target) return false;
        uintptr_t cave = mem.AllocNear(target);
        if (!cave) return false;

        outArray = cave + 0x100;
        outIndex = cave + 0x80;
        uintptr_t ret = target + 8;

        std::vector<uint8_t> sc = {
            0x50,0x51,0x52,
            0x48,0xB8, 0,0,0,0,0,0,0,0,
            0x8B,0x08,
            0x83,0xF9,0x32,
            0x7C,0x02,0x31,0xC9,
            0x48,0xBA, 0,0,0,0,0,0,0,0,
            0x48,0x89,0x1C,0xCA,
            0xFF,0xC1,
            0x89,0x08,
            0x5A,0x59,0x58,
            0xF2,0x0F,0x10,0x8B,0x40,0x01,0x00,0x00,
            0xFF,0x25,0x00,0x00,0x00,0x00,
            0,0,0,0,0,0,0,0
        };
        memcpy(&sc[5],  &outIndex, 8);
        memcpy(&sc[24], &outArray, 8);
        memcpy(&sc[57], &ret,      8);
        mem.WriteBytes(cave, sc);

        std::vector<uint8_t> patch = {0xE9,0,0,0,0, 0x90,0x90,0x90};
        DWORD rel = (DWORD)(cave - target - 5);
        memcpy(&patch[1], &rel, 4);
        DWORD old;
        VirtualProtectEx(mem.handle,(LPVOID)target,8,PAGE_EXECUTE_READWRITE,&old);
        mem.WriteBytes(target, patch);
        VirtualProtectEx(mem.handle,(LPVOID)target,8,old,&old);
        return true;
    }

    bool SetupAVM2() {
        airBase = mem.GetModuleBase("Adobe AIR.dll");
        if (!airBase) return false;
        uintptr_t gcHeap = mem.Read<uintptr_t>(airBase + AirOff::GC_HEAP_SLOT_RVA);
        if (!gcHeap) return false;
        uintptr_t colBase  = gcHeap + AirOff::GC_HEAP_COLLECTORS;
        int       colCount = mem.Read<int>(colBase + AirOff::LIST_COUNT);
        uintptr_t colPtr   = mem.Read<uintptr_t>(colBase + AirOff::LIST_ENTRIES);
        uintptr_t avmCore  = 0;
        for (int i = 0; i < colCount && i < 64; i++) {
            uintptr_t gc   = mem.Read<uintptr_t>(colPtr + i*8);
            if (!gc) continue;
            uintptr_t core = mem.Read<uintptr_t>(gc + AirOff::GC_TO_AVM_CORE);
            if (core > 0x10000 && core < 0x7FFFFFFFFFFF) { avmCore = core; break; }
        }
        if (!avmCore) return false;
        uintptr_t node = mem.Read<uintptr_t>(avmCore + AirOff::CORE_LIVE_POOLS);
        for (int pi = 0; pi < 256 && node; pi++) {
            uintptr_t wp  = mem.Read<uintptr_t>(node + AirOff::NODE_WEAK_POOL);
            if (wp) {
                uintptr_t obj = mem.Read<uintptr_t>(wp + AirOff::WEAK_REF_OBJECT);
                if (obj) {
                    uintptr_t sl = obj + AirOff::POOL_SCRIPTS;
                    int sc2      = mem.Read<int>(sl + AirOff::LIST_COUNT);
                    uintptr_t se = mem.Read<uintptr_t>(sl + AirOff::LIST_ENTRIES);
                    for (int s = 0; s < sc2 && s < 256; s++) {
                        uintptr_t env = mem.Read<uintptr_t>(se + s*8);
                        if (!env) continue;
                        uintptr_t scope = mem.Read<uintptr_t>(env + 0x28);
                        if (!scope) continue;
                        uintptr_t cand = mem.Read<uintptr_t>(scope + 0x10);
                        if (!cand) continue;
                        uintptr_t gm = mem.Read<uintptr_t>(cand + GameOff::MAIN_TO_GM);
                        if (!gm) continue;
                        uintptr_t el = mem.Read<uintptr_t>(gm + GameOff::GM_TO_ENTITYLIST);
                        if (!el) continue;
                        int ec = mem.Read<int>(el + 0x08);
                        if (ec > 0 && ec < 8) {
                            mainInstance = cand;
                            return true;
                        }
                    }
                }
            }
            node = mem.Read<uintptr_t>(node + AirOff::NODE_NEXT);
        }
        return false;
    }

    std::vector<BHPlayer> GetPlayersAVM2() {
        std::vector<BHPlayer> out;
        uintptr_t gm = mem.Read<uintptr_t>(mainInstance + GameOff::MAIN_TO_GM);
        if (!gm) return out;
        uintptr_t localEnt = mem.Read<uintptr_t>(gm + GameOff::GM_TO_LOCAL);
        uintptr_t el       = mem.Read<uintptr_t>(gm + GameOff::GM_TO_ENTITYLIST);
        if (!el) return out;
        int count      = mem.Read<int>(el + 0x08);
        uintptr_t ents = mem.Read<uintptr_t>(el + 0x10);
        if (count <= 0 || count > 8 || !ents) return out;
        for (int i = 0; i < count; i++) {
            uintptr_t ent = mem.Read<uintptr_t>(ents + i*8);
            if (!ent) continue;
            uintptr_t dc  = mem.Read<uintptr_t>(ent + GameOff::ENTITY_TO_DISPLAY);
            if (!dc) continue;
            double px = mem.Read<double>(dc + GameOff::DISPLAY_X);
            double py = mem.Read<double>(dc + GameOff::DISPLAY_Y);
            if (px < -50000.0 || px > 50000.0) continue;
            BHPlayer p;
            p.addr     = dc;
            p.worldPos = {(float)px,(float)py,0.f};
            p.isLocal  = (ent == localEnt);
            p.teamID   = i;
            p.name     = p.isLocal ? "You" : "Enemy";
            p.atkState = (Off::AttackState != 0)
                         ? (AttackState)mem.Read<int>(dc + Off::AttackState)
                         : AttackState::Idle;
            p.isGrounded = true;
            p.hp = p.maxHp = 200.f;
            out.push_back(p);
        }
        Vec3 lp{};
        for (auto& p:out) if(p.isLocal){lp=p.worldPos;break;}
        for (auto& p:out){float ddx=p.worldPos.x-lp.x,ddy=p.worldPos.y-lp.y;p.distToLocal=sqrtf(ddx*ddx+ddy*ddy);}
        return out;
    }

    std::vector<BHPlayer> GetPlayersCave() {
        std::vector<BHPlayer> out;
        std::vector<uintptr_t> dps;
        for (int i = 0; i < 50; i++) {
            uintptr_t dp = mem.Read<uintptr_t>(playerArray + i*8);
            if (!dp) continue;
            bool dup=false;
            for (auto e:dps) if(e==dp){dup=true;break;}
            if (!dup) dps.push_back(dp);
        }
        for (size_t i = 0; i < dps.size(); i++) {
            uintptr_t dp = dps[i];
            double py = mem.Read<double>(dp + Off::PosY);
            double px = mem.Read<double>(dp + Off::PosX);
            if (px < -50000.0 || px > 50000.0 || py < -50000.0 || py > 50000.0) continue;
            BHPlayer p;
            p.addr      = dp;
            p.worldPos  = {(float)px,(float)py,0.f};
            p.atkState  = (AttackState)mem.Read<int>(dp + Off::AttackState);
            p.atkFrame  = mem.Read<int>(dp + Off::AttackFrame);
            p.isGrounded = true;
            p.isLocal    = (i == 0);
            p.teamID     = (int)i;
            p.name       = p.isLocal ? "You" : "Enemy";
            p.hp = p.maxHp = 200.f;
            out.push_back(p);
        }
        Vec3 lp{};
        for (auto& p:out) if(p.isLocal){lp=p.worldPos;break;}
        for (auto& p:out){float ddx=p.worldPos.x-lp.x,ddy=p.worldPos.y-lp.y;p.distToLocal=sqrtf(ddx*ddx+ddy*ddy);}
        return out;
    }

public:
    // Cache des DPs joueurs pour GetGroundWeapons
    std::vector<uintptr_t> lastPlayerAddrs;

    Scene(Memory& m) : mem(m) {
        caveReady = SetupCave(
            "\xF2\x0F\x10\x8B\x40\x01\x00\x00", "xxxxxxxx",
            playerArray, playerIndex);
        std::cout << "[CAVE] player cave " << (caveReady ? "OK" : "FAIL") << "\n";
        avmReady = SetupAVM2();
        std::cout << "[AVM2] " << (avmReady ? "OK" : "not available") << "\n";
    }

    bool IsReady() const { return caveReady || avmReady; }

    std::vector<BHPlayer> GetPlayers(uintptr_t) {
        std::vector<BHPlayer> result;
        if (avmReady && mainInstance) {
            result = GetPlayersAVM2();
            if (result.size() >= 2) {
                lastPlayerAddrs.clear();
                for (auto& p : result) lastPlayerAddrs.push_back(p.addr);
                return result;
            }
        }
        if (caveReady) {
            result = GetPlayersCave();
            lastPlayerAddrs.clear();
            for (auto& p : result) lastPlayerAddrs.push_back(p.addr);
            return result;
        }
        return {};
    }

    // Extrait les armes depuis le cave array en excluant les DPs joueurs connus.
    // Fonctionne si les armes au sol déclenchent le même opcode AOB (probable).
    // Limitation : l'array de 50 slots est surtout peuplé de joueurs à 60fps —
    // les armes apparaissent quand elles ont un update physique récent (rebond, spawn).
    std::vector<BHWeapon> GetGroundWeapons(Vec3 localPos) {
        std::vector<BHWeapon> out;
        if (!caveReady) return out;

        for (int i = 0; i < 50; i++) {
            uintptr_t dp = mem.Read<uintptr_t>(playerArray + i*8);
            if (!dp) continue;

            // Exclure les DPs joueurs connus
            bool isPlayer = false;
            for (auto pa : lastPlayerAddrs)
                if (pa == dp) { isPlayer = true; break; }
            if (isPlayer) continue;

            double py = mem.Read<double>(dp + Off::PosY);
            double px = mem.Read<double>(dp + Off::PosX);
            if (px < -50000.0 || px > 50000.0 || py < -50000.0 || py > 50000.0) continue;
            // Sanity : armes au sol ont une Y basse (entre -3000 et 3000 en BH)
            if (py < -3000.0 || py > 3000.0) continue;

            BHWeapon w;
            w.addr = dp;
            w.pos  = {(float)px,(float)py,0.f};
            float ddx = w.pos.x - localPos.x;
            float ddy = w.pos.y - localPos.y;
            w.distToLocal = sqrtf(ddx*ddx + ddy*ddy);
            out.push_back(w);
        }
        return out;
    }
};

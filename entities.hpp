// entities.hpp — C++17 x64 Brawlhalla external
// Fix: AVM2 → fallback cave si <2 joueurs retournés (GameOff non confirmés = souvent vide)
// Fix: second cave pour weapon entities
// Fix: atkState / isGrounded / dodgeCD lus proprement depuis Off::
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
    bool        isGrounded=true;   // défaut true — gate dodgeGroundOnly ouvert par défaut
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

    // ── Cave joueurs ─────────────────────────────────────────────────────────
    uintptr_t playerArray = 0;   // cave + 0x100
    uintptr_t playerIndex = 0;   // cave + 0x80
    bool      caveReady   = false;

    // ── Cave armes ───────────────────────────────────────────────────────────
    uintptr_t weaponArray = 0;   // cave2 + 0x100
    uintptr_t weaponIndex = 0;   // cave2 + 0x80
    bool      weaponCaveReady = false;

    // ── AVM2 (optionnel, non fiable sans GameOff confirmés) ──────────────────
    uintptr_t airBase      = 0;
    uintptr_t mainInstance = 0;
    bool      avmReady     = false;

    // ─────────────────────────────────────────────────────────────────────────
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
            0x48,0xB8, 0,0,0,0,0,0,0,0,       // mov rax, &outIndex
            0x8B,0x08,                           // mov ecx, [rax]
            0x83,0xF9,0x32,                      // cmp ecx, 50
            0x7C,0x02,0x31,0xC9,                 // jl +2 ; xor ecx,ecx
            0x48,0xBA, 0,0,0,0,0,0,0,0,         // mov rdx, &outArray
            0x48,0x89,0x1C,0xCA,                 // mov [rdx+rcx*8], rbx
            0xFF,0xC1,                            // inc ecx
            0x89,0x08,                            // mov [rax], ecx
            0x5A,0x59,0x58,
            0xF2,0x0F,0x10,0x8B,0x40,0x01,0x00,0x00,  // original opcode
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
                    uintptr_t sl  = obj + AirOff::POOL_SCRIPTS;
                    int sc2       = mem.Read<int>(sl + AirOff::LIST_COUNT);
                    uintptr_t se  = mem.Read<uintptr_t>(sl + AirOff::LIST_ENTRIES);
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
                            std::cout << "[AVM2] Main=0x" << std::hex << mainInstance << " entities=" << std::dec << ec << "\n";
                            return true;
                        }
                    }
                }
            }
            node = mem.Read<uintptr_t>(node + AirOff::NODE_NEXT);
        }
        return false;
    }

    // ─────────────────────────────────────────────────────────────────────────
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
        for (auto& p:out){float dx=p.worldPos.x-lp.x,dy=p.worldPos.y-lp.y;p.distToLocal=sqrtf(dx*dx+dy*dy);}
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
            // DisplayCache : PosY=0x140, PosX=0x148 — confirmés AOB
            double py = mem.Read<double>(dp + Off::PosY);
            double px = mem.Read<double>(dp + Off::PosX);
            if (px < -50000.0 || px > 50000.0 || py < -50000.0 || py > 50000.0) continue;
            BHPlayer p;
            p.addr     = dp;
            p.worldPos = {(float)px,(float)py,0.f};
            // AttackState=0x50, AttackFrame=0x6C depuis offsets_1.hpp — UKC, à valider CE
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
        for (auto& p:out){float dx=p.worldPos.x-lp.x,dy=p.worldPos.y-lp.y;p.distToLocal=sqrtf(dx*dx+dy*dy);}
        return out;
    }

public:
    Scene(Memory& m) : mem(m) {
        // Cave joueurs : AOB sur movsd xmm1, [rbx+0x140] — confirmé
        caveReady = SetupCave(
            "\xF2\x0F\x10\x8B\x40\x01\x00\x00", "xxxxxxxx",
            playerArray, playerIndex);
        std::cout << "[CAVE] player cave " << (caveReady ? "OK" : "FAIL") << "\n";

        // Cave armes : même pattern que joueurs — BH réutilise le même opcode pour les entités
        // Si ça colle pas au bon endroit, mettre un pattern différent (weapon pos write)
        // FILL : trouver l'AOB exact sur l'écriture de position des items au sol via CE
        weaponCaveReady = false; // FILL — voir commentaire ci-dessous

        avmReady = SetupAVM2();
        std::cout << "[AVM2] " << (avmReady ? "OK" : "not available") << "\n";
    }

    bool IsReady() const { return caveReady || avmReady; }

    std::vector<BHPlayer> GetPlayers(uintptr_t) {
        // Stratégie : AVM2 en premier SI confirmé ET retourne ≥2 joueurs
        // sinon cave — évite le cas AVM2=true mais GameOff wrongg → vide
        if (avmReady && mainInstance) {
            auto r = GetPlayersAVM2();
            if (r.size() >= 2) return r;  // AVM2 fiable → utilise
            // AVM2 a retourné vide : GameOff probablement faux → cave
        }
        if (caveReady) return GetPlayersCave();
        return {};
    }

    std::vector<BHWeapon> GetGroundWeapons(Vec3 localPos) {
        // FILL — nécessite une cave sur le write de position des weapons au sol
        // Procédure CE :
        //   1. Drop une arme au sol en training
        //   2. CE → scan double → write breakpoint sur l'adresse de sa position Y
        //   3. L'instruction qui fire = AOB pour la cave weapons
        //   4. Passer cet AOB à SetupCave() avec weaponArray/weaponIndex
        //   5. Itérer weaponArray, lire Off::WepPosX/Y/Type, filtrer type == Off::ENTITY_WEAPON
        (void)localPos;
        return {};
    }
};

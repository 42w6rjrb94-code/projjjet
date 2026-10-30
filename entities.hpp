#pragma once
#include "memory.hpp"
#include "offsets.hpp"
#include <vector>
#include <string>
#include <cmath>

struct Vec2 { float x = 0, y = 0; };
struct Vec3 { float x = 0, y = 0, z = 0; };
enum class AttackState : int { Idle=0, Startup=1, Active=2, Recovery=3 };

struct BHPlayer {
    uintptr_t   addr        = 0;
    float       hp          = 0, maxHp = 0;
    int         teamID      = 0;
    std::string name;
    Vec3        worldPos;
    Vec2        velocity;
    bool        isGrounded  = false;
    bool        isLocal     = false;
    bool        facingRight = false;
    AttackState atkState    = AttackState::Idle;
    int         atkFrame    = 0;
    float       dodgeCD     = 0.f;
    uintptr_t   weaponPtr   = 0;
    int         weaponType  = -1;
    float       distToLocal = 0.f;
};

struct BHWeapon {
    uintptr_t addr = 0;
    Vec3      pos;
    int       type = 0;
    float     distToLocal = 0.f;
};

class Scene {
    Memory&   mem;
    uintptr_t pointerArray = 0; // cave + 0x100 : array[50] de DPs
    uintptr_t indexStore   = 0; // cave + 0x80  : index courant
    bool      caveReady    = false;

    bool SetupCodeCave() {
        // AOB : movsd xmm1, [rbx+0x140]
        const char* pattern = "\xF2\x0F\x10\x8B\x40\x01\x00\x00";
        const char* mask    = "xxxxxxxx";

        uintptr_t targetAddr = mem.FindPatternSafe(pattern, mask);
        if (!targetAddr) return false;

        uintptr_t caveAddr = mem.AllocNear(targetAddr);
        if (!caveAddr) return false;

        pointerArray         = caveAddr + 0x100;
        indexStore           = caveAddr + 0x80;
        uintptr_t returnAddr = targetAddr + 8;

        // Shellcode : push regs, save rbx dans array, pop regs,
        //             ré-exécuter l'opcode original, jmp retour
        std::vector<uint8_t> sc = {
            0x50, 0x51, 0x52,                                    // [0]  push rax,rcx,rdx
            0x48, 0xB8, 0,0,0,0,0,0,0,0,                        // [3]  mov rax, indexStore
            0x8B, 0x08,                                          // [13] mov ecx, [rax]
            0x83, 0xF9, 0x32,                                    // [15] cmp ecx, 50
            0x7C, 0x02, 0x31, 0xC9,                              // [18] jl +2; xor ecx,ecx
            0x48, 0xBA, 0,0,0,0,0,0,0,0,                        // [22] mov rdx, pointerArray
            0x48, 0x89, 0x1C, 0xCA,                              // [32] mov [rdx+rcx*8], rbx
            0xFF, 0xC1,                                          // [36] inc ecx
            0x89, 0x08,                                          // [38] mov [rax], ecx
            0x5A, 0x59, 0x58,                                    // [40] pop rdx,rcx,rax
            0xF2,0x0F,0x10,0x8B,0x40,0x01,0x00,0x00,            // [43] opcode original
            0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,                  // [51] jmp [rip+0]
            0,0,0,0,0,0,0,0                                      // [57] return addr
        };

        memcpy(&sc[5],  &indexStore,   8);
        memcpy(&sc[24], &pointerArray, 8);
        memcpy(&sc[57], &returnAddr,   8);
        mem.WriteBytes(caveAddr, sc);

        // Patch original : E9 (jmp rel32) + 3 NOP
        std::vector<uint8_t> patch = { 0xE9,0,0,0,0, 0x90,0x90,0x90 };
        DWORD rel = (DWORD)(caveAddr - targetAddr - 5);
        memcpy(&patch[1], &rel, 4);

        DWORD oldProt;
        VirtualProtectEx(mem.handle, (LPVOID)targetAddr, 8,
            PAGE_EXECUTE_READWRITE, &oldProt);
        mem.WriteBytes(targetAddr, patch);
        VirtualProtectEx(mem.handle, (LPVOID)targetAddr, 8,
            oldProt, &oldProt);

        return true;
    }

public:
    Scene(Memory& m) : mem(m) {
        caveReady = SetupCodeCave();
    }

    bool IsReady() const { return caveReady; }

    std::vector<BHPlayer> GetPlayers(uintptr_t /*localAddr — inutilisé*/) {
        std::vector<BHPlayer> out;
        if (!caveReady) return out;

        // Dédupliquer les DPs depuis l'array du code cave
        std::vector<uintptr_t> dps;
        for (int i = 0; i < 50; i++) {
            uintptr_t dp = mem.Read<uintptr_t>(pointerArray + i * 8);
            if (!dp) continue;
            bool dup = false;
            for (auto e : dps) if (e == dp) { dup = true; break; }
            if (!dup) dps.push_back(dp);
        }

        for (size_t i = 0; i < dps.size(); i++) {
            uintptr_t dp = dps[i];

            // Positions confirmées en DOUBLE
            double posX = mem.Read<double>(dp + Off::PosX);
            double posY = mem.Read<double>(dp + Off::PosY);

            // Filtre sanity — map Brawlhalla ~±5000 unités
            if (posX < -10000.0 || posX > 10000.0 ||
                posY < -10000.0 || posY > 10000.0) continue;

            BHPlayer p;
            p.addr      = dp;
            p.worldPos  = { (float)posX, (float)posY, 0.f };
            p.atkState  = (AttackState)mem.Read<int>(dp + Off::AttackState);
            p.atkFrame  = mem.Read<int>(dp + Off::AttackFrame);
            p.isLocal   = (i == 0); // premier DP capturé = joueur local
            p.teamID    = (int)i;
            p.name      = p.isLocal ? "You" : "Enemy";

            // HP non confirmés — placeholder 200 jusqu'au FILL des offsets
            p.hp    = 200.f;
            p.maxHp = 200.f;

            out.push_back(p);
        }

        // Distance depuis le local
        Vec3 lp{};
        for (auto& p : out) if (p.isLocal) { lp = p.worldPos; break; }
        for (auto& p : out) {
            float dx = p.worldPos.x - lp.x, dy = p.worldPos.y - lp.y;
            p.distToLocal = std::sqrtf(dx*dx + dy*dy);
        }
        return out;
    }

    std::vector<BHWeapon> GetGroundWeapons(Vec3 /*localPos*/) {
        return {}; // FILL : même boucle, filtrer EntityType == 1
    }
};

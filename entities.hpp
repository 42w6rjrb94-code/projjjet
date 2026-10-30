// entities.hpp
// C++17 · x64 · Brawlhalla external
// v2 changes: FILL-safe reads for atkState, isGrounded, dodgeCD, hp, vel, weaponPtr.
//   isGrounded defaults TRUE when Off::IsGrounded == 0 — "Ground only" guard now passes.
//   atkState defaults Idle when Off::AttackState == 0 — fill that offset before autododge fires.

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
    uintptr_t pointerArray = 0; // cave + 0x100 : circular array[50] of player DPs
    uintptr_t indexStore   = 0; // cave + 0x80  : current write index
    bool      caveReady    = false;

    bool SetupCodeCave() {
        // AOB: movsd xmm1, [rbx+0x140] — rbx = player entity DP, confirms Off::PosX = 0x140
        const char* pattern = "\xF2\x0F\x10\x8B\x40\x01\x00\x00";
        const char* mask    = "xxxxxxxx";

        uintptr_t targetAddr = mem.FindPatternSafe(pattern, mask);
        if (!targetAddr) return false;

        uintptr_t caveAddr = mem.AllocNear(targetAddr);
        if (!caveAddr) return false;

        pointerArray         = caveAddr + 0x100;
        indexStore           = caveAddr + 0x80;
        uintptr_t returnAddr = targetAddr + 8;

        // push regs, save rbx (DP) into circular array[50], pop regs,
        // re-execute original opcode, jmp back
        std::vector<uint8_t> sc = {
            0x50, 0x51, 0x52,                               // [0]  push rax, rcx, rdx
            0x48, 0xB8, 0,0,0,0,0,0,0,0,                   // [3]  mov rax, &indexStore
            0x8B, 0x08,                                     // [13] mov ecx, [rax]
            0x83, 0xF9, 0x32,                               // [15] cmp ecx, 50
            0x7C, 0x02, 0x31, 0xC9,                         // [18] jl +2; xor ecx,ecx
            0x48, 0xBA, 0,0,0,0,0,0,0,0,                   // [22] mov rdx, &pointerArray
            0x48, 0x89, 0x1C, 0xCA,                         // [32] mov [rdx+rcx*8], rbx
            0xFF, 0xC1,                                     // [36] inc ecx
            0x89, 0x08,                                     // [38] mov [rax], ecx
            0x5A, 0x59, 0x58,                               // [40] pop rdx, rcx, rax
            0xF2,0x0F,0x10,0x8B,0x40,0x01,0x00,0x00,       // [43] original opcode
            0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,             // [51] jmp [rip+0]
            0,0,0,0,0,0,0,0                                 // [57] return address
        };

        memcpy(&sc[5],  &indexStore,   8);
        memcpy(&sc[24], &pointerArray, 8);
        memcpy(&sc[57], &returnAddr,   8);
        mem.WriteBytes(caveAddr, sc);

        // Patch original: E9 rel32 jmp + 3 NOP padding (8 bytes total)
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

    std::vector<BHPlayer> GetPlayers(uintptr_t /*unused — local detection via cave ordering*/) {
        std::vector<BHPlayer> out;
        if (!caveReady) return out;

        // Deduplicate DPs from cave circular array
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

            // Position confirmed double — Off::PosX = 0x140 from AOB
            double posX = mem.Read<double>(dp + Off::PosX);
            double posY = mem.Read<double>(dp + Off::PosY);

            // Sanity gate — Brawlhalla map bounds ~±5000 units
            if (posX < -10000.0 || posX > 10000.0 ||
                posY < -10000.0 || posY > 10000.0) continue;

            BHPlayer p;
            p.addr     = dp;
            p.worldPos = { (float)posX, (float)posY, 0.f };

            // AttackState — defaults Idle when Off::AttackState == 0.
            // AutoDodge gate: enemy.atkState != Active → continues (skips enemy).
            // Off::AttackState must be filled before any dodge triggers.
            p.atkState = (Off::AttackState != 0)
                ? (AttackState)mem.Read<int>(dp + Off::AttackState)
                : AttackState::Idle;

            p.atkFrame = (Off::AttackFrame != 0)
                ? mem.Read<int>(dp + Off::AttackFrame)
                : 0;

            // DodgeCD — defaults 0.f (no cooldown) when Off::DodgeCD == 0.
            // AutoDodge fires freely without cooldown gating until filled.
            p.dodgeCD = (Off::DodgeCD != 0)
                ? mem.Read<float>(dp + Off::DodgeCD)
                : 0.f;

            // IsGrounded — defaults TRUE when Off::IsGrounded == 0.
            // Original kill gate: cfg.dodgeGroundOnly && !isGrounded → return.
            // Default=true opens that gate. Fill Off::IsGrounded before relying
            // on accurate air/ground discrimination.
            p.isGrounded = (Off::IsGrounded != 0)
                ? (mem.Read<uint8_t>(dp + Off::IsGrounded) != 0)
                : true;

            // Velocity — defaults 0 when unset; used by DexBot / WeaponSnap prediction
            p.velocity = {
                (Off::VelX != 0) ? mem.Read<float>(dp + Off::VelX) : 0.f,
                (Off::VelY != 0) ? mem.Read<float>(dp + Off::VelY) : 0.f
            };

            // HP — placeholder 200.f until Off::HP filled
            p.hp    = (Off::HP    != 0) ? mem.Read<float>(dp + Off::HP)    : 200.f;
            p.maxHp = (Off::MaxHP != 0) ? mem.Read<float>(dp + Off::MaxHP) : 200.f;

            // WeaponPtr
            p.weaponPtr = (Off::WeaponPtr != 0)
                ? mem.Read<uintptr_t>(dp + Off::WeaponPtr)
                : 0;

            // Local player detection.
            // Off::IsLocal filled → read from entity.
            // Unset → i==0 heuristic: assumes local player entity is first in the
            // game's update loop, which is standard for Unity player controllers.
            // Verify via debug output (check that dp[0] matches your position) before
            // trusting this in a live match.
            p.isLocal = (Off::IsLocal != 0)
                ? (mem.Read<uint8_t>(dp + Off::IsLocal) != 0)
                : (i == 0);

            p.teamID = (int)i;
            p.name   = p.isLocal ? "You" : "Enemy";

            out.push_back(p);
        }

        // Distance from local player
        Vec3 lp{};
        for (auto& p : out) if (p.isLocal) { lp = p.worldPos; break; }
        for (auto& p : out) {
            float dx = p.worldPos.x - lp.x, dy = p.worldPos.y - lp.y;
            p.distToLocal = std::sqrtf(dx*dx + dy*dy);
        }
        return out;
    }

    // Ground weapons — needs a separate entity hook.
    // Current player cave (AOB on movsd [rbx+0x140]) captures player entities only.
    //
    // To implement:
    //   1. Place a weapon on the ground in training mode.
    //   2. CE: scan its position as double, set a write breakpoint on that address.
    //   3. Breakpoint fires on the instruction writing weapon position — grab the
    //      register at that instruction = weapon entity DP base.
    //   4. Set up a second cave (pointerArray2) on that instruction, same pattern.
    //   5. Iterate pointerArray2 here, read Off::WepPosX / WepPosY / WepType.
    //   6. Filter where read WepType value == Off::ENTITY_WEAPON.
    //   Off::WepPosX, Off::WepPosY, Off::WepType need CE fills before this ships.
    std::vector<BHWeapon> GetGroundWeapons(Vec3 localPos) {
        (void)localPos;
        return {}; // FILL: weapon entity cave — see comment above
    }
};

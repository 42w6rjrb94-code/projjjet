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
    Memory& mem;
public:
    Scene(Memory& m) : mem(m) {}

    Vec3 ReadTransformPos(uintptr_t t) {
        float mat[16]{};
        ReadProcessMemory(mem.handle, (LPCVOID)(t + Off::TransformMatrix),
                          mat, sizeof(mat), nullptr);
        return { mat[12], mat[13], mat[14] };
    }

    std::vector<BHPlayer> GetPlayers(uintptr_t localAddr) {
        std::vector<BHPlayer> out;
        uintptr_t sm  = mem.Read<uintptr_t>(mem.gameBase + Off::SceneManager);
        if (!sm) return out;
        uintptr_t sc  = mem.Read<uintptr_t>(sm + 0x18);
        uintptr_t roots = mem.Read<uintptr_t>(sc + 0x08);
        int32_t cnt   = mem.Read<int32_t>(sc + 0x10);
        if (cnt <= 0 || cnt > 32) return out;

        for (int i = 0; i < cnt; i++) {
            uintptr_t go = mem.Read<uintptr_t>(roots + i * 0x08);
            if (!go) continue;
            uintptr_t arr = mem.Read<uintptr_t>(go + 0x30);
            int32_t   cc  = mem.Read<int32_t>(go + 0x40);
            if (cc <= 0 || cc > 64) continue;
            for (int c = 0; c < cc; c++) {
                uintptr_t cell = mem.Read<uintptr_t>(arr + c * 0x10);
                uintptr_t comp = mem.Read<uintptr_t>(cell + 0x10);
                if (!comp) continue;
                uintptr_t klass   = mem.Read<uintptr_t>(comp);
                uintptr_t namePtr = mem.Read<uintptr_t>(klass + 0x48);
                char kn[64]{};
                ReadProcessMemory(mem.handle, (LPCVOID)namePtr, kn, 63, nullptr);
                if (strcmp(kn, "BrawlPlayer") != 0) continue;

                BHPlayer p;
                p.addr        = comp;
                p.hp          = mem.Read<float>(comp + 0x10 + Off::HP);
                p.maxHp       = mem.Read<float>(comp + 0x10 + Off::MaxHP);
                p.teamID      = mem.Read<int32_t>(comp + 0x10 + Off::TeamID);
                p.isGrounded  = mem.Read<bool>(comp + 0x10 + Off::IsGrounded);
                p.facingRight = mem.Read<bool>(comp + 0x10 + Off::FacingRight);
                p.atkState    = mem.Read<AttackState>(comp + 0x10 + Off::AttackState);
                p.atkFrame    = mem.Read<int32_t>(comp + 0x10 + Off::AttackFrame);
                p.dodgeCD     = mem.Read<float>(comp + 0x10 + Off::DodgeCooldown);
                p.velocity    = mem.Read<Vec2>(comp + 0x10 + Off::Velocity);
                p.weaponPtr   = mem.Read<uintptr_t>(comp + 0x10 + Off::CurrentWeapon);
                if (p.weaponPtr)
                    p.weaponType = mem.Read<int32_t>(p.weaponPtr + 0x10 + Off::WeaponType);
                uintptr_t ns  = mem.Read<uintptr_t>(comp + 0x10 + Off::PlayerName);
                p.name        = mem.ReadMonoStr(ns);
                uintptr_t tr  = mem.Read<uintptr_t>(comp + 0x10 + Off::Transform);
                p.worldPos    = ReadTransformPos(tr);
                p.isLocal     = (comp == localAddr);
                out.push_back(p);
            }
        }
        Vec3 lp{};
        for (auto& p : out) if (p.isLocal) { lp = p.worldPos; break; }
        for (auto& p : out) {
            float dx = p.worldPos.x - lp.x, dy = p.worldPos.y - lp.y;
            p.distToLocal = std::sqrtf(dx*dx + dy*dy);
        }
        return out;
    }

    std::vector<BHWeapon> GetGroundWeapons(Vec3 localPos) {
        // FILL: même boucle que GetPlayers, filtre EntityType == 1
        return {};
    }
};

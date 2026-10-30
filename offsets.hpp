#pragma once
#include <cstdint>

namespace Off {
    // ── Entity Data Pointer via code cave ───────────────────────────────
    // AOB : F2 0F 10 8B 40 01 00 00  (movsd xmm1, [rbx+0x140])
    // rbx au moment de l'instruction = DP de l'entité

    // Positions — DOUBLE (8 bytes) — confirmés UKC
    constexpr uintptr_t PosX          = 0x140;
    constexpr uintptr_t PosY          = 0x138;

    // Combat — int (4 bytes) — confirmés UKC
    constexpr uintptr_t AttackLight   = 0x30;
    constexpr uintptr_t AttackHeavy   = 0x34;
    constexpr uintptr_t AttackState   = 0x50;
    constexpr uintptr_t AttackFrame   = 0x6C;

    // Non confirmés — à remplir via CE Dissect Data Structure sur le DP
    constexpr uintptr_t HP            = 0x00;   // FILL
    constexpr uintptr_t MaxHP         = 0x04;   // FILL
    constexpr uintptr_t TeamID        = 0x08;   // FILL
    constexpr uintptr_t IsGrounded    = 0x0C;   // FILL
    constexpr uintptr_t FacingRight   = 0x10;   // FILL
    constexpr uintptr_t PlayerName    = 0x14;   // FILL
    constexpr uintptr_t DodgeCooldown = 0x18;   // FILL
    constexpr uintptr_t Velocity      = 0x38;   // FILL
    constexpr uintptr_t CurrentWeapon = 0x48;   // FILL
    constexpr uintptr_t WeaponType    = 0x10;   // FILL
    constexpr uintptr_t EntityType    = 0x58;   // FILL

    // Non utilisés avec l'approche AOB — gardés pour compatibilité
    constexpr uintptr_t Transform       = 0x20;
    constexpr uintptr_t TransformMatrix = 0x38;
    constexpr uintptr_t SceneManager    = 0x0;  // FILL — non utilisé
    constexpr uintptr_t CameraX        = 0x0;   // FILL
    constexpr uintptr_t CameraY        = 0x0;   // FILL
    constexpr uintptr_t CameraZoom     = 0x0;   // FILL
}

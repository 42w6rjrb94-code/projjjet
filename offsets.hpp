
#pragma once
#include <cstdint>

namespace Off {

    // Positions
    constexpr uintptr_t PosX = 0x140;
    constexpr uintptr_t PosY = 0x138;

    // Combat
    constexpr uintptr_t AttackLight = 0x30;
    constexpr uintptr_t AttackHeavy = 0x34;
    constexpr uintptr_t AttackState = 0x50;
    constexpr uintptr_t AttackFrame = 0x6C;

    // Données non vérifiées : désactivées par défaut
    constexpr uintptr_t HP = 0x00;
    constexpr uintptr_t MaxHP = 0x00;
    constexpr uintptr_t TeamID = 0x00;
    constexpr uintptr_t IsGrounded = 0x00;
    constexpr uintptr_t FacingRight = 0x00;
    constexpr uintptr_t PlayerName = 0x00;
    constexpr uintptr_t DodgeCooldown = 0x00;

    constexpr uintptr_t Velocity = 0x00;
    constexpr uintptr_t VelX = 0x00;
    constexpr uintptr_t VelY = 0x00;

    constexpr uintptr_t CurrentWeapon = 0x00;
    constexpr uintptr_t WeaponPtr = 0x00;
    constexpr uintptr_t WeaponType = 0x00;
    constexpr uintptr_t EntityType = 0x00;
    constexpr uintptr_t IsLocal = 0x00;

    // Compatibilité
    constexpr uintptr_t Transform = 0x20;
    constexpr uintptr_t TransformMatrix = 0x38;
    constexpr uintptr_t SceneManager = 0x00;

    constexpr uintptr_t CameraX = 0x00;
    constexpr uintptr_t CameraY = 0x00;
    constexpr uintptr_t CameraZoom = 0x00;
}

#pragma once
#include <cstdint>

namespace Off {
    constexpr uintptr_t SceneManager  = 0x0; // FILL
    constexpr uintptr_t HP            = 0x00;
    constexpr uintptr_t MaxHP         = 0x04;
    constexpr uintptr_t TeamID        = 0x08;
    constexpr uintptr_t IsGrounded    = 0x0C;
    constexpr uintptr_t PlayerName    = 0x14;
    constexpr uintptr_t Transform     = 0x20;
    constexpr uintptr_t AttackState   = 0x28;
    constexpr uintptr_t AttackFrame   = 0x2C;
    constexpr uintptr_t DodgeCooldown = 0x30;
    constexpr uintptr_t FacingRight   = 0x34;
    constexpr uintptr_t Velocity      = 0x38;
    constexpr uintptr_t CurrentWeapon = 0x48;
    constexpr uintptr_t EntityType    = 0x50;
    constexpr uintptr_t WeaponType    = 0x10;
    constexpr uintptr_t WeaponTransform = 0x18;
    constexpr uintptr_t TransformMatrix = 0x38;
    constexpr uintptr_t CameraX      = 0x0; // FILL
    constexpr uintptr_t CameraY      = 0x0; // FILL
    constexpr uintptr_t CameraZoom   = 0x0; // FILL
}

#pragma once
#include <cstdint>

namespace Off {
    // ── Entity Data Pointer ──────────────────────────────────────────────
    // Trouvé via AOB : F2 0F 10 8B 40 01 00 00
    // rbx au moment de l'instruction = DP de l'entité

    // Positions — DOUBLE (8 bytes), pas float
    constexpr uintptr_t PosX          = 0x140;   // double — confirmé
    constexpr uintptr_t PosY          = 0x138;   // double — confirmé

    // Combat — int (4 bytes)
    constexpr uintptr_t AttackLight   = 0x30;    // int 0/1 — confirmé
    constexpr uintptr_t AttackHeavy   = 0x34;    // int 0/1 — confirmé
    constexpr uintptr_t AttackState   = 0x50;    // int 0/1 — confirmé
    constexpr uintptr_t AttackFrame   = 0x6C;    // move ID  — confirmé

    // Non confirmés — à tester en jeu avec CE dissect structure
    constexpr uintptr_t HP            = 0x00;    // FILL
    constexpr uintptr_t MaxHP         = 0x04;    // FILL
    constexpr uintptr_t TeamID        = 0x08;    // FILL
    constexpr uintptr_t IsGrounded    = 0x0C;    // FILL
    constexpr uintptr_t FacingRight   = 0x10;    // FILL
    constexpr uintptr_t PlayerName    = 0x14;    // FILL
    constexpr uintptr_t DodgeCooldown = 0x18;    // FILL
    constexpr uintptr_t CurrentWeapon = 0x48;    // FILL

    // Camera — approximée depuis positions joueurs (voir main.cpp)
    // Pas d'offset statique trouvé — caméra Adobe AIR dynamique
}

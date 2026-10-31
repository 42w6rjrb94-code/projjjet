// main.cpp — v3 — C++17, Windows
// INTEGRATION : ChaseAI + ThreatTracker câblés, renderer.Frame() reçoit threats,
// DexBot et WeaponSnap coordonnés via bool isDodging/isChasing (pas de ref croisée)
// AutoDodge standalone supprimé — ChaseAI le remplace intégralement
#include "hwid.hpp"
#include "memory.hpp"
#include "config.hpp"
#include "entities.hpp"
#include "input.hpp"
#include "threat.hpp"
#include "chase.hpp"
#include "dexbot.hpp"
#include "weaponsnap.hpp"
#include "renderer.hpp"
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

static std::atomic<bool> g_run{ true };

void HotKeyLoop(bool& menu, Renderer& rend) {
    while (g_run) {
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            menu = !menu;
            rend.SetInteractive(menu);
            Sleep(200);
        }
        if (GetAsyncKeyState(VK_END) & 1) g_run = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

int main() {
    SetConsoleTitleW(L"BH Cheat");

    std::string hwid = HWID::Generate();
    std::cout << "HWID: " << hwid << "\n";
    if (!HWID::IsAuthorized(hwid)) {
        MessageBoxW(nullptr, L"Licence invalide.", L"BH Cheat", MB_ICONERROR);
        return 1;
    }

    Config cfg; cfg.Load();

    Memory mem;
    while (!mem.Attach(L"Brawlhalla.exe")) {
        std::cout << "Attente Brawlhalla...\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    std::cout << "Attache. gameBase=0x" << std::hex << mem.gameBase << "\n";

    Scene scene(mem);
    if (!scene.IsReady()) {
        std::cerr << "Cave + AVM2 echecs — quitte\n";
        return 1;
    }

    // ── Modules ─────────────────────────────────────────────────────────────
    // ChaseAI : autorité unique sur touches directionnelles
    // DexBot  : attaque seulement, skip si isDodging
    // WeaponSnap : snap arme, skip si isDodging || isChasing
    ThreatTracker threatTracker;
    ChaseAI       chaseAI(cfg);
    DexBot        dexBot(cfg);
    WeaponSnap    weaponSnap(cfg);
    Renderer      renderer;

    if (!renderer.Init()) { std::cerr << "Overlay impossible\n"; return 1; }
    renderer.SetInteractive(true);

    bool menuOpen = true;
    std::thread hotkeys(HotKeyLoop, std::ref(menuOpen), std::ref(renderer));

    uintptr_t unused  = 0;
    float     camX    = 0.f, camY = 0.f;
    float     zoom    = 6.f;
    int       dbgTick = 0;

    MSG msg{};
    while (g_run) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) g_run = false;
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }

        // ── Entités ─────────────────────────────────────────────────────────
        auto players = scene.GetPlayers(unused);
        auto weapons = scene.GetGroundWeapons(
            players.empty() ? Vec3{} : players[0].worldPos);

        // Résoudre local player — camera + addrs pour ThreatTracker
        float localX = 0.f, localY = 0.f;
        bool  foundLocal = false;
        std::vector<uintptr_t> playerAddrs;

        for (const auto& p : players) {
            playerAddrs.push_back(p.addr);
            if (p.isLocal) {
                localX = p.worldPos.x; localY = p.worldPos.y;
                camX   = localX;       camY   = localY;
                foundLocal = true;
            }
        }
        if (!foundLocal && !players.empty()) {
            float sx=0.f, sy=0.f;
            for (const auto& p : players) { sx+=p.worldPos.x; sy+=p.worldPos.y; }
            camX = sx/(float)players.size(); camY = sy/(float)players.size();
            localX = camX; localY = camY;
        }

        // ── ThreatTracker ────────────────────────────────────────────────────
        // Lit cave array brut, exclut les addrs joueurs, classifie par vélocité
        // Retourne projectiles (isFast) + hazards (isHazard) dans la portée locale
        auto threats = threatTracker.Update(
            mem, scene.GetPointerArray(), localX, localY, playerAddrs);

        // ── ChaseAI — tick mouvement ─────────────────────────────────────────
        // Retourne isDodging → DexBot et WeaponSnap skippent pendant l'animation
        bool isDodging = chaseAI.Tick(players, threats);
        bool isChasing = chaseAI.IsChasing();

        // ── DexBot — attaque uniquement ──────────────────────────────────────
        dexBot.Tick(players, isDodging);

        // ── WeaponSnap — cède à ChaseAI ─────────────────────────────────────
        weaponSnap.Tick(players, weapons, isDodging, isChasing);

        // Debug ~1/sec
        if (++dbgTick % 120 == 0) {
            int nF=0, nH=0;
            for (auto& t:threats){if(t.isFast)nF++;if(t.isHazard)nH++;}
            std::cout << std::dec
                      << "ply=" << players.size()
                      << " prj=" << nF << " haz=" << nH
                      << " dg=" << isDodging << " ch=" << isChasing
                      << " cam=(" << (int)camX << "," << (int)camY << ")";
            for (const auto& p:players)
                std::cout << " | " << p.name
                          << " A=" << (int)p.atkState
                          << " d=" << (int)p.distToLocal;
            std::cout << "\n";
        }

        renderer.Frame(players, weapons, threats, cfg, menuOpen, camX, camY, zoom);
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

    hotkeys.join();
    chaseAI.Stop();
    renderer.Shutdown();
    cfg.Save();
    return 0;
}

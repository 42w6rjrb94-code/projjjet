// main.cpp — C++17, Windows
// Fix: camera suit le local player (pas la moyenne — décalait tout le W2S)
// Fix: zoom par référence au renderer → slider ESP modifie en direct
#include "hwid.hpp"
#include "memory.hpp"
#include "config.hpp"
#include "entities.hpp"
#include "input.hpp"
#include "autododge.hpp"
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

    AutoDodge  autoDodge(cfg);
    DexBot     dexBot(cfg);
    WeaponSnap weaponSnap(cfg);
    Renderer   renderer;

    if (!renderer.Init()) {
        std::cerr << "Overlay impossible\n";
        return 1;
    }
    renderer.SetInteractive(true);

    bool menuOpen = true;
    std::thread hotkeys(HotKeyLoop, std::ref(menuOpen), std::ref(renderer));

    uintptr_t unused  = 0;
    float     camX    = 0.f;
    float     camY    = 0.f;
    float     zoom    = 6.f;  // calibrer via slider ESP tab jusqu'à ce que les boxes collent
    int       dbgTick = 0;

    MSG msg{};
    while (g_run) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) g_run = false;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        auto players = scene.GetPlayers(unused);
        auto weapons = scene.GetGroundWeapons(
            players.empty() ? Vec3{} : players[0].worldPos);

        // Camera sur local player — pas la moyenne
        // Moyenne = repère se décale quand ennemi est loin = boxes décalées
        bool foundLocal = false;
        for (const auto& p : players) {
            if (p.isLocal) {
                camX = p.worldPos.x;
                camY = p.worldPos.y;
                foundLocal = true;
                break;
            }
        }
        // Fallback si isLocal pas encore discriminé (début de cave, 0 frames capturées)
        if (!foundLocal && !players.empty()) {
            float sx=0.f, sy=0.f;
            for (const auto& p : players) { sx+=p.worldPos.x; sy+=p.worldPos.y; }
            camX = sx / (float)players.size();
            camY = sy / (float)players.size();
        }

        // Debug ~1/sec (120 ticks × 8ms = 960ms)
        if (++dbgTick % 120 == 0) {
            std::cout << "players=" << std::dec << players.size()
                      << " weapons=" << weapons.size()
                      << " cam=(" << (int)camX << "," << (int)camY << ")";
            for (const auto& p : players)
                std::cout << " | " << p.name
                          << " atk=" << (int)p.atkState
                          << " d=" << (int)p.distToLocal;
            std::cout << "\n";
        }

        autoDodge .Tick(players);
        dexBot    .Tick(players);
        weaponSnap.Tick(players, weapons);

        renderer.Frame(players, weapons, cfg, menuOpen, camX, camY, zoom);
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

    hotkeys.join();
    renderer.Shutdown();
    cfg.Save();
    return 0;
}

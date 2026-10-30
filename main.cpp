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
        MessageBoxW(nullptr,
            L"Licence invalide.",
            L"BH Cheat", MB_ICONERROR);
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
        std::cerr << "Code cave injection failed — AOB not found\n";
        return 1;
    }
    std::cout << "Code cave OK\n";

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

    uintptr_t localAddr = 0;
    float camX = 0.f, camY = 0.f, zoom = 6.f;

    MSG msg{};
    int debugTick = 0;

    while (g_run) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) g_run = false;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        auto players = scene.GetPlayers(localAddr);
        auto weapons = scene.GetGroundWeapons(
            players.empty() ? Vec3{} : players[0].worldPos);

        // Debug toutes les 60 frames (~0.5 sec)
        if (++debugTick % 60 == 0) {
            std::cout << "players=" << std::dec << players.size();
            for (auto& p : players) {
                std::cout << " | dp=0x" << std::hex << p.addr
                          << " x=" << std::dec << (int)p.worldPos.x
                          << " y=" << (int)p.worldPos.y
                          << " atk=" << (int)p.atkState;
            }
            std::cout << "\n";
        }

        if (!players.empty()) {
            float sx = 0.f, sy = 0.f;
            for (auto& p : players) {
                sx += p.worldPos.x;
                sy += p.worldPos.y;
            }
            camX = sx / (float)players.size();
            camY = sy / (float)players.size();
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

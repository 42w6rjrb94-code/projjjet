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

void HotKeyLoop(bool& menu) {
    while (g_run) {
        if (GetAsyncKeyState(VK_INSERT) & 1) menu = !menu;
        if (GetAsyncKeyState(VK_END)    & 1) g_run = false;
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
    std::cout << "Attache. monoBase=0x" << std::hex << mem.monoBase << "\n";

    Scene      scene(mem);
    AutoDodge  autoDodge(cfg);
    DexBot     dexBot(cfg);
    WeaponSnap weaponSnap(cfg);
    Renderer   renderer;

    if (!renderer.Init()) {
        std::cerr << "Overlay impossible\n"; return 1;
    }

    bool menuOpen = true;
    std::thread hotkeys(HotKeyLoop, std::ref(menuOpen));

    // FILL: pointer scan CE pour ces 4 valeurs
    uintptr_t localAddr = 0x0;
    float camX=0.f, camY=0.f, zoom=6.f;

    MSG msg{};
    while (g_run) {
        while (PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){
            if (msg.message==WM_QUIT) g_run=false;
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
        auto players = scene.GetPlayers(localAddr);
        auto weapons = scene.GetGroundWeapons(
            players.empty() ? Vec3{} : players[0].worldPos);

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

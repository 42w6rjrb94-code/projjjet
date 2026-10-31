// renderer.hpp — C++17, Windows, D3D11 + ImGui
// Ajouts v2: espAtkState (couleur par état), stream hide OBS, zoom slider live
// Fix: zoom passé par référence → slider modifie en direct sans redémarrer
// WDA_EXCLUDEFROMCAPTURE — Windows 10 2004+ requis
#pragma once
#include <d3d11.h>
#include <dwmapi.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include "entities.hpp"
#include "config.hpp"
#include <vector>
#include <cmath>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011  // Win10 2004+
#endif

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT WINAPI OverlayWndProc(HWND hWnd, UINT msg,
                                      WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

class Renderer {
    // Labels + couleurs par AttackState — Recovery = bleu = punish window
    static constexpr const char* ATK_LABELS[] = {
        "IDLE", "STARTUP", "ACTIVE", "RECOV"
    };
    static constexpr ImU32 ATK_COLORS[] = {
        IM_COL32(120,120,120,200),  // Idle     — gris
        IM_COL32(255,200,  0,255),  // Startup  — jaune
        IM_COL32(255, 60, 60,255),  // Active   — rouge vif = danger
        IM_COL32(100,200,255,255),  // Recovery — bleu = punish
    };

public:
    HWND                    overlay = nullptr;
    HWND                    target  = nullptr;
    ID3D11Device*           dev     = nullptr;
    ID3D11DeviceContext*    ctx     = nullptr;
    IDXGISwapChain*         sc      = nullptr;
    ID3D11RenderTargetView* rtv     = nullptr;
    int  W = 1920, H = 1080;
    bool streamHide = true;  // actif par défaut — toggle dans tab Config

    bool Init() {
        target = FindWindowW(nullptr, L"Brawlhalla");
        if (!target) return false;
        RECT r; GetClientRect(target, &r);
        W = r.right  > 0 ? r.right  : 1920;
        H = r.bottom > 0 ? r.bottom : 1080;

        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc   = OverlayWndProc;
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"BHOvr";
        RegisterClassExW(&wc);

        overlay = CreateWindowExW(
            WS_EX_TOPMOST|WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE,
            L"BHOvr", L"", WS_POPUP, 0, 0, W, H,
            nullptr, nullptr, wc.hInstance, nullptr);
        if (!overlay) return false;

        // Cache l'overlay de OBS / Medal / toute capture par SetWindowDisplayAffinity
        // Si le build target est < Win10 2004, commenter cette ligne
        SetWindowDisplayAffinity(overlay, WDA_EXCLUDEFROMCAPTURE);

        SetLayeredWindowAttributes(overlay, RGB(0,0,0), 0, LWA_COLORKEY);
        MARGINS m{-1}; DwmExtendFrameIntoClientArea(overlay, &m);

        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount        = 2;
        sd.BufferDesc.Width   = W; sd.BufferDesc.Height = H;
        sd.BufferDesc.Format  = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow       = overlay;
        sd.SampleDesc.Count   = 1;
        sd.Windowed           = TRUE;
        sd.SwapEffect         = DXGI_SWAP_EFFECT_DISCARD;
        D3D_FEATURE_LEVEL fl;
        D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE,
            nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
            &sd, &sc, &dev, &fl, &ctx);

        ID3D11Texture2D* bb;
        sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb);
        dev->CreateRenderTargetView(bb, nullptr, &rtv);
        bb->Release();
        ShowWindow(overlay, SW_SHOW);

        IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGui::StyleColorsDark();
        auto& s = ImGui::GetStyle();
        s.WindowRounding = 6.f; s.FrameRounding = 4.f;
        auto* c = s.Colors;
        c[ImGuiCol_WindowBg]      = {0.07f,0.07f,0.09f,0.95f};
        c[ImGuiCol_TitleBgActive] = {0.18f,0.08f,0.38f,1.f};
        c[ImGuiCol_CheckMark]     = {0.6f,0.2f,1.0f,1.f};
        c[ImGuiCol_SliderGrab]    = {0.6f,0.2f,1.0f,1.f};
        c[ImGuiCol_Button]        = {0.18f,0.08f,0.38f,1.f};
        c[ImGuiCol_ButtonHovered] = {0.28f,0.12f,0.55f,1.f};
        c[ImGuiCol_Tab]           = {0.12f,0.05f,0.25f,1.f};
        c[ImGuiCol_TabSelected]   = {0.28f,0.12f,0.55f,1.f};
        ImGui_ImplWin32_Init(overlay);
        ImGui_ImplDX11_Init(dev, ctx);
        return true;
    }

    void SetInteractive(bool on) {
        if (!overlay) return;
        LONG_PTR ex = GetWindowLongPtrW(overlay, GWL_EXSTYLE);
        if (on) {
            ex &= ~(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
            SetWindowLongPtrW(overlay, GWL_EXSTYLE, ex);
            SetForegroundWindow(overlay);
        } else {
            ex |= WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
            SetWindowLongPtrW(overlay, GWL_EXSTYLE, ex);
        }
        // Certains drivers resetent l'affinity au changement de style — ré-appliquer
        if (streamHide)
            SetWindowDisplayAffinity(overlay, WDA_EXCLUDEFROMCAPTURE);
    }

    // World → Screen
    // BH : 0,0 = centre de map, Y vers le haut
    // zoom à calibrer via le slider ESP jusqu'à ce que les boxes collent
    bool W2S(float wx, float wy,
             float camX, float camY, float zoom,
             ImVec2& out) const {
        float sx = (wx - camX) * zoom + W * 0.5f;
        float sy =  H * 0.5f  - (wy - camY) * zoom;
        if (sx < -300.f || sx > W + 300.f) return false;
        if (sy < -300.f || sy > H + 300.f) return false;
        out = {sx, sy};
        return true;
    }

    // zoom par référence — ImGui::SliderFloat le modifie directement
    void Frame(const std::vector<BHPlayer>& players,
               const std::vector<BHWeapon>& weapons,
               Config& cfg, bool& menuOpen,
               float camX, float camY, float& zoom) {

        // Suit la fenêtre BH si elle bouge
        if (target) {
            RECT r; GetWindowRect(target, &r);
            SetWindowPos(overlay, HWND_TOPMOST,
                r.left, r.top, r.right-r.left, r.bottom-r.top,
                SWP_NOACTIVATE);
        }

        float cl[4]{};
        ctx->OMSetRenderTargets(1, &rtv, nullptr);
        ctx->ClearRenderTargetView(rtv, cl);
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        auto* dl = ImGui::GetBackgroundDrawList();

        // ── ESP ──────────────────────────────────────────────────────────────
        if (cfg.espEnabled) {

            for (const auto& p : players) {
                if (p.isLocal) continue;

                ImVec2 s;
                if (!W2S(p.worldPos.x, p.worldPos.y, camX, camY, zoom, s)) continue;

                ImU32 col = IM_COL32(255, 80, 80, 255);
                float hw = 20.f, hh = 35.f;

                // Box
                if (cfg.espBoxes)
                    dl->AddRect({s.x-hw, s.y-hh}, {s.x+hw, s.y+hh},
                                col, 0, 0, 1.5f);

                // HP bar verticale — gauche de la box
                if (cfg.espHP && p.maxHp > 0) {
                    float ratio = std::max(0.f, std::min(1.f, p.hp / p.maxHp));
                    float barH  = hh * 2.f;
                    // Fond
                    dl->AddRectFilled({s.x-hw-7, s.y-hh},
                                     {s.x-hw-3, s.y-hh+barH},
                                     IM_COL32(40,40,40,180));
                    // Remplissage vert→rouge selon ratio
                    dl->AddRectFilled({s.x-hw-7, s.y-hh+barH*(1.f-ratio)},
                                     {s.x-hw-3, s.y-hh+barH},
                                     IM_COL32((int)(255*(1-ratio)),
                                              (int)(255*ratio), 0, 255));
                }

                // Nom + HP
                if (cfg.espNames) {
                    char lbl[128];
                    snprintf(lbl, sizeof(lbl), "%s %.0f/%.0f",
                             p.name.c_str(), p.hp, p.maxHp);
                    dl->AddText({s.x-hw, s.y-hh-16}, col, lbl);
                }

                // AttackState — couleur varie par état, barre rouge si Active
                if (cfg.espAtkState) {
                    int idx = std::max(0, std::min(3, (int)p.atkState));
                    char albl[16];
                    snprintf(albl, sizeof(albl), "[%s]", ATK_LABELS[idx]);
                    dl->AddText({s.x-hw, s.y+hh+3}, ATK_COLORS[idx], albl);

                    // Barre fine rouge sous box quand Active (hitbox ouverte)
                    if (p.atkState == AttackState::Active)
                        dl->AddRectFilled({s.x-hw, s.y+hh+1},
                                         {s.x+hw, s.y+hh+3},
                                         IM_COL32(255,60,60,220));
                }

                // Point centre + distance
                dl->AddCircleFilled(s, 3.f, col);
                char dlbl[16];
                snprintf(dlbl, sizeof(dlbl), "%.0f", p.distToLocal);
                dl->AddText({s.x+hw+3, s.y-7}, IM_COL32(180,180,180,180), dlbl);
            }

            // Armes au sol — cercle jaune + croix
            if (cfg.espWeapons) {
                for (const auto& w : weapons) {
                    ImVec2 s;
                    if (!W2S(w.pos.x, w.pos.y, camX, camY, zoom, s)) continue;
                    dl->AddCircle(s, 10.f, IM_COL32(255,220,0,220), 12, 2.f);
                    dl->AddLine({s.x-6,s.y},{s.x+6,s.y}, IM_COL32(255,220,0,180), 1.f);
                    dl->AddLine({s.x,s.y-6},{s.x,s.y+6}, IM_COL32(255,220,0,180), 1.f);
                    char wlbl[16];
                    snprintf(wlbl, sizeof(wlbl), "%.0f", w.distToLocal);
                    dl->AddText({s.x+13,s.y-7}, IM_COL32(255,220,0,180), wlbl);
                }
            }
        }

        // ── Menu ─────────────────────────────────────────────────────────────
        if (menuOpen) {
            ImGui::SetNextWindowSize({340, 400}, ImGuiCond_Once);
            ImGui::SetNextWindowPos ({30,  30},  ImGuiCond_Once);
            ImGui::Begin("  BH Cheat  v2.0", &menuOpen, ImGuiWindowFlags_NoResize);

            if (ImGui::BeginTabBar("tabs")) {

                if (ImGui::BeginTabItem("ESP")) {
                    ImGui::Checkbox("ESP actif",    &cfg.espEnabled);
                    ImGui::Checkbox("Boxes",         &cfg.espBoxes);
                    ImGui::Checkbox("HP bar",        &cfg.espHP);
                    ImGui::Checkbox("Noms",          &cfg.espNames);
                    ImGui::Checkbox("Atk State",     &cfg.espAtkState);
                    ImGui::Checkbox("Armes au sol",  &cfg.espWeapons);
                    ImGui::Separator();

                    // Debug live — positions et états en temps réel
                    ImGui::Text("Players lus: %d", (int)players.size());
                    for (const auto& p : players) {
                        int idx = std::max(0,std::min(3,(int)p.atkState));
                        ImGui::TextColored(
                            ImVec4(
                                (ATK_COLORS[idx]>>0 & 0xFF)/255.f,
                                (ATK_COLORS[idx]>>8 & 0xFF)/255.f,
                                (ATK_COLORS[idx]>>16& 0xFF)/255.f, 1.f),
                            "%s | x=%.0f y=%.0f | %s | d=%.0f",
                            p.name.c_str(),
                            p.worldPos.x, p.worldPos.y,
                            ATK_LABELS[idx],
                            p.distToLocal);
                    }
                    ImGui::Text("Weapons: %d", (int)weapons.size());
                    ImGui::Separator();

                    // Calibrage W2S en live — glisse jusqu'à ce que les boxes collent
                    ImGui::SliderFloat("Zoom W2S", &zoom, 0.5f, 20.f);
                    ImGui::TextDisabled("cam %.0f / %.0f", camX, camY);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Auto Dodge")) {
                    ImGui::Checkbox("Auto Dodge",  &cfg.autoDodge);
                    ImGui::Checkbox("Ground only", &cfg.dodgeGroundOnly);
                    ImGui::SliderInt("Délai (ms)", &cfg.dodgeDelayMs, 0, 350);
                    ImGui::TextDisabled("0-30=legeres  80-120=lourdes  250-350=sigs");
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("DexBot")) {
                    ImGui::Checkbox("DexBot",           &cfg.dexBot);
                    ImGui::Checkbox("Punish only",      &cfg.dexBotOnlyHit);
                    ImGui::TextDisabled("OFF = agressif (Idle aussi)");
                    ImGui::SliderInt("Cooldown extra (ms)", &cfg.comboDelayMs, 0, 200);
                    ImGui::TextDisabled("0 = latence minimale (~130ms/cycle)");
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Weapon")) {
                    ImGui::Checkbox("Weapon Snap",   &cfg.weaponSnap);
                    ImGui::SliderFloat("Range",      &cfg.snapRangeUnits, 50, 800);
                    ImGui::TextDisabled("Cave armes : FILL requis");
                    ImGui::TextDisabled("ESP armes desactive jusqu'au FILL");
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Config")) {
                    ImGui::Checkbox("Timings aleatoires", &cfg.randomizeTimings);
                    ImGui::SliderInt("Variance (ms)",      &cfg.timingVarianceMs, 0, 20);
                    ImGui::Separator();

                    bool prev = streamHide;
                    ImGui::Checkbox("Stream hide (OBS/Medal)", &streamHide);
                    if (streamHide != prev) {
                        // Toggle live sans redémarrer
                        SetWindowDisplayAffinity(overlay,
                            streamHide ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
                    }

                    ImGui::Separator();
                    if (ImGui::Button("Sauvegarder")) cfg.Save();
                    ImGui::SameLine();
                    if (ImGui::Button("Charger"))     cfg.Load();
                    ImGui::Separator();
                    ImGui::TextDisabled("INSERT = menu  |  END = quitter");
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        sc->Present(0, 0);
    }

    void Shutdown() {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        if (rtv) rtv->Release();
        if (sc)  sc->Release();
        if (ctx) ctx->Release();
        if (dev) dev->Release();
        if (overlay) DestroyWindow(overlay);
    }
};

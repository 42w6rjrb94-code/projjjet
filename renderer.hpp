// renderer.hpp — v2 : ajout Shutdown() + Combat tab complété + nouveaux champs Config
// Frame() signature inchangée (threats était déjà là, main.cpp ne le passait pas — fixed)
#pragma once
#include <d3d11.h>
#include <dwmapi.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include "entities.hpp"
#include "threat.hpp"
#include "config.hpp"
#include <vector>
#include <cmath>
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"dwmapi.lib")

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT WINAPI OverlayWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return true;
    if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(h, m, w, l);
}

class Renderer {
    static constexpr const char* ATK_LABELS[]  = {"IDLE","STARTUP","ACTIVE","RECOV"};
    static constexpr ImU32       ATK_COLORS[]  = {
        IM_COL32(120,120,120,200),
        IM_COL32(255,200,  0,255),
        IM_COL32(255, 60, 60,255),
        IM_COL32(100,200,255,255),
    };
public:
    HWND                    overlay = nullptr, target = nullptr;
    ID3D11Device*           dev     = nullptr;
    ID3D11DeviceContext*    ctx     = nullptr;
    IDXGISwapChain*         sc      = nullptr;
    ID3D11RenderTargetView* rtv     = nullptr;
    int W = 1920, H = 1080;
    bool streamHide = true;

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

        SetWindowDisplayAffinity(overlay, WDA_EXCLUDEFROMCAPTURE);
        SetLayeredWindowAttributes(overlay, RGB(0,0,0), 0, LWA_COLORKEY);
        MARGINS m{-1}; DwmExtendFrameIntoClientArea(overlay, &m);

        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount          = 2;
        sd.BufferDesc.Width     = W; sd.BufferDesc.Height = H;
        sd.BufferDesc.Format    = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage          = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow         = overlay;
        sd.SampleDesc.Count     = 1;
        sd.Windowed             = TRUE;
        sd.SwapEffect           = DXGI_SWAP_EFFECT_DISCARD;
        D3D_FEATURE_LEVEL fl;
        D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE,
            nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &sc, &dev, &fl, &ctx);
        ID3D11Texture2D* bb;
        sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb);
        dev->CreateRenderTargetView(bb, nullptr, &rtv); bb->Release();
        ShowWindow(overlay, SW_SHOW);

        IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGui::StyleColorsDark();
        auto& s = ImGui::GetStyle();
        s.WindowRounding = 6.f; s.FrameRounding = 4.f;
        auto* c = s.Colors;
        c[ImGuiCol_WindowBg]       = {0.07f,0.07f,0.09f,0.95f};
        c[ImGuiCol_TitleBgActive]  = {0.18f,0.08f,0.38f,1.f};
        c[ImGuiCol_CheckMark]      = {0.6f, 0.2f, 1.0f, 1.f};
        c[ImGuiCol_SliderGrab]     = {0.6f, 0.2f, 1.0f, 1.f};
        c[ImGuiCol_Button]         = {0.18f,0.08f,0.38f,1.f};
        c[ImGuiCol_ButtonHovered]  = {0.28f,0.12f,0.55f,1.f};
        c[ImGuiCol_Tab]            = {0.12f,0.05f,0.25f,1.f};
        c[ImGuiCol_TabSelected]    = {0.28f,0.12f,0.55f,1.f};
        ImGui_ImplWin32_Init(overlay);
        ImGui_ImplDX11_Init(dev, ctx);
        return true;
    }

    void Shutdown() {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        if (rtv) { rtv->Release(); rtv = nullptr; }
        if (sc)  { sc->Release();  sc  = nullptr; }
        if (ctx) { ctx->Release(); ctx = nullptr; }
        if (dev) { dev->Release(); dev = nullptr; }
        if (overlay) { DestroyWindow(overlay); overlay = nullptr; }
        UnregisterClassW(L"BHOvr", GetModuleHandleW(nullptr));
    }

    void SetInteractive(bool on) {
        if (!overlay) return;
        LONG_PTR ex = GetWindowLongPtrW(overlay, GWL_EXSTYLE);
        if (on) { ex &= ~(WS_EX_TRANSPARENT|WS_EX_NOACTIVATE); SetForegroundWindow(overlay); }
        else      ex |=   WS_EX_TRANSPARENT|WS_EX_NOACTIVATE;
        SetWindowLongPtrW(overlay, GWL_EXSTYLE, ex);
        if (streamHide) SetWindowDisplayAffinity(overlay, WDA_EXCLUDEFROMCAPTURE);
    }

    bool W2S(float wx, float wy, float camX, float camY, float zoom,
             const Config& cfg, ImVec2& out) const {
        float sx = (wx-camX)*zoom + W*0.5f + cfg.w2sOffsetX;
        float sy = cfg.w2sFlipY
            ? H*0.5f - (wy-camY)*zoom + cfg.w2sOffsetY
            : (wy-camY)*zoom + H*0.5f + cfg.w2sOffsetY;
        if (sx<-300||sx>W+300||sy<-300||sy>H+300) return false;
        out = {sx, sy}; return true;
    }

    void Frame(const std::vector<BHPlayer>& players,
               const std::vector<BHWeapon>& weapons,
               const std::vector<Threat>&   threats,
               Config& cfg, bool& menuOpen,
               float camX, float camY, float& zoom) {

        if (target) {
            RECT r; GetWindowRect(target, &r);
            SetWindowPos(overlay, HWND_TOPMOST,
                r.left, r.top, r.right-r.left, r.bottom-r.top, SWP_NOACTIVATE);
        }
        float cl[4]{};
        ctx->OMSetRenderTargets(1, &rtv, nullptr);
        ctx->ClearRenderTargetView(rtv, cl);
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        auto* dl = ImGui::GetBackgroundDrawList();

        if (cfg.espEnabled) {
            // ── Joueurs ──────────────────────────────────────────────────────
            for (const auto& p : players) {
                if (p.isLocal) continue;
                ImVec2 s;
                if (!W2S(p.worldPos.x, p.worldPos.y, camX, camY, zoom, cfg, s)) continue;
                ImU32 col = IM_COL32(255,80,80,255);
                float hw=20.f, hh=35.f;
                if (cfg.espBoxes)
                    dl->AddRect({s.x-hw,s.y-hh},{s.x+hw,s.y+hh}, col, 0, 0, 1.5f);
                if (cfg.espHP && p.maxHp > 0) {
                    float ratio = std::max(0.f, std::min(1.f, p.hp/p.maxHp));
                    float bH    = hh*2.f;
                    dl->AddRectFilled({s.x-hw-7,s.y-hh},{s.x-hw-3,s.y-hh+bH},
                        IM_COL32(40,40,40,180));
                    dl->AddRectFilled({s.x-hw-7,s.y-hh+bH*(1-ratio)},{s.x-hw-3,s.y-hh+bH},
                        IM_COL32((int)(255*(1-ratio)),(int)(255*ratio),0,255));
                }
                if (cfg.espNames) {
                    char lb[128];
                    snprintf(lb, sizeof(lb), "%s %.0f/%.0f",
                        p.name.c_str(), p.hp, p.maxHp);
                    dl->AddText({s.x-hw, s.y-hh-16}, col, lb);
                }
                if (cfg.espAtkState) {
                    int idx = std::max(0, std::min(3, (int)p.atkState));
                    char al[16]; snprintf(al, sizeof(al), "[%s]", ATK_LABELS[idx]);
                    dl->AddText({s.x-hw, s.y+hh+3}, ATK_COLORS[idx], al);
                    if (p.atkState == AttackState::Active)
                        dl->AddRectFilled({s.x-hw,s.y+hh+1},{s.x+hw,s.y+hh+3},
                            IM_COL32(255,60,60,220));
                }
                dl->AddCircleFilled(s, 3.f, col);
                char dlbl[32]; snprintf(dlbl, sizeof(dlbl), "d=%.0f", p.distToLocal);
                dl->AddText({s.x+hw+3, s.y-7}, IM_COL32(200,200,200,200), dlbl);
            }
            // ── Armes au sol ─────────────────────────────────────────────────
            if (cfg.espWeapons) {
                for (const auto& w : weapons) {
                    ImVec2 s;
                    if (!W2S(w.pos.x, w.pos.y, camX, camY, zoom, cfg, s)) continue;
                    dl->AddCircle(s, 10.f, IM_COL32(255,220,0,220), 12, 2.f);
                }
            }
            // ── Menaces — orange=projectile, jaune=hazard ────────────────────
            for (const auto& t : threats) {
                ImVec2 s;
                if (!W2S(t.x, t.y, camX, camY, zoom, cfg, s)) continue;
                if (t.isFast) {
                    dl->AddCircleFilled(s, 6.f, IM_COL32(255,100,0,240));
                    dl->AddLine(s, {s.x+t.vx*5.f, s.y+t.vy*5.f},
                        IM_COL32(255,160,0,180), 2.f);
                } else if (t.isHazard) {
                    float r = 7.f;
                    dl->AddQuad({s.x,s.y-r},{s.x+r,s.y},{s.x,s.y+r},{s.x-r,s.y},
                        IM_COL32(255,230,0,200), 2.f);
                }
            }
        }

        // ── Menu ─────────────────────────────────────────────────────────────
        if (menuOpen) {
            ImGui::SetNextWindowSize({400,520}, ImGuiCond_Once);
            ImGui::SetNextWindowPos({30,30},    ImGuiCond_Once);
            ImGui::Begin("  BH Cheat  v3.0", &menuOpen, ImGuiWindowFlags_NoResize);
            if (ImGui::BeginTabBar("tabs")) {

                // ── Onglet ESP ────────────────────────────────────────────────
                if (ImGui::BeginTabItem("ESP")) {
                    ImGui::Checkbox("ESP actif",  &cfg.espEnabled);
                    ImGui::Checkbox("Boxes",       &cfg.espBoxes);
                    ImGui::Checkbox("HP bar",      &cfg.espHP);
                    ImGui::Checkbox("Noms",        &cfg.espNames);
                    ImGui::Checkbox("Atk State",   &cfg.espAtkState);
                    ImGui::Checkbox("Armes",       &cfg.espWeapons);
                    ImGui::Separator();
                    ImGui::Text("W2S Calibration");
                    ImGui::SliderFloat("Zoom",     &zoom,          0.05f, 20.f);
                    ImGui::SliderFloat("Offset X", &cfg.w2sOffsetX,-500.f, 500.f);
                    ImGui::SliderFloat("Offset Y", &cfg.w2sOffsetY,-500.f, 500.f);
                    ImGui::Checkbox("Flip Y (Y-up world)", &cfg.w2sFlipY);
                    ImGui::Separator();
                    // Debug live
                    int nF=0,nH=0;
                    for (auto& t:threats){if(t.isFast)nF++;if(t.isHazard)nH++;}
                    ImGui::Text("players=%d  prj=%d  haz=%d",
                        (int)players.size(), nF, nH);
                    ImGui::Text("cam=(%.0f,%.0f) zoom=%.2f", camX, camY, zoom);
                    for (const auto& p : players) {
                        int idx = std::max(0, std::min(3, (int)p.atkState));
                        ImGui::TextColored(
                            {(ATK_COLORS[idx]&0xFF)/255.f,
                             ((ATK_COLORS[idx]>>8)&0xFF)/255.f,
                             ((ATK_COLORS[idx]>>16)&0xFF)/255.f, 1.f},
                            "%s wx=%.0f wy=%.0f d=%.0f %s",
                            p.name.c_str(), p.worldPos.x, p.worldPos.y,
                            p.distToLocal, ATK_LABELS[idx]);
                    }
                    ImGui::EndTabItem();
                }

                // ── Onglet Combat ─────────────────────────────────────────────
                if (ImGui::BeginTabItem("Combat")) {
                    // Threat Dodge
                    ImGui::TextColored({1.f,0.4f,0.f,1.f},"=== Threat Dodge ===");
                    ImGui::Checkbox("Threat Dodge (projectiles+hazards)", &cfg.threatDodge);
                    ImGui::TextDisabled("Points orange=proj  losanges=hazard");
                    ImGui::Separator();

                    // Auto Dodge (attaque joueur)
                    ImGui::TextColored({1.f,0.8f,0.f,1.f},"=== Auto Dodge joueur ===");
                    ImGui::Checkbox("Auto Dodge",          &cfg.autoDodge);
                    ImGui::Checkbox("Ground only",         &cfg.dodgeGroundOnly);
                    ImGui::SliderInt("Delai dodge (ms)",   &cfg.dodgeDelayMs, 0, 350);
                    ImGui::SliderFloat("Dodge range",      &cfg.dodgeRange, 100.f, 1000.f);
                    ImGui::TextDisabled("0-30=légères  80-120=lourdes");
                    ImGui::Separator();

                    // Chase AI
                    ImGui::TextColored({0.3f,1.f,0.5f,1.f},"=== Chase AI ===");
                    ImGui::Checkbox("Chase activé",        &cfg.chaseEnabled);
                    ImGui::SliderFloat("Chase range",      &cfg.chaseRange, 100.f, 8000.f);
                    ImGui::SliderFloat("Mode défensif hp", &cfg.lowHpThreshold, 0.f, 500.f);
                    ImGui::TextDisabled("0=off  >0=retraite si hp local > seuil");
                    ImGui::Separator();

                    // DexBot
                    ImGui::TextColored({0.5f,0.8f,1.f,1.f},"=== DexBot (attaque) ===");
                    ImGui::Checkbox("DexBot",              &cfg.dexBot);
                    ImGui::Checkbox("Only punish hits",    &cfg.dexBotOnlyHit);
                    ImGui::Checkbox("Skip actives",        &cfg.dexBotNoActive);
                    ImGui::Checkbox("Swap local/enemy",    &cfg.dexBotSwapLocal);
                    ImGui::SliderFloat("Attack range",     &cfg.attackRange, 50.f, 800.f);
                    ImGui::SliderInt("Combo delay (ms)",   &cfg.comboDelayMs, 0, 300);
                    ImGui::Separator();

                    // Grav cancel + Sig
                    ImGui::TextColored({0.8f,0.4f,1.f,1.f},"=== Grav Cancel / Sig ===");
                    ImGui::Checkbox("Auto Grav Cancel",    &cfg.autoGravCancel);
                    ImGui::TextDisabled("DOWN+ATTACK en l'air après combo aérien");
                    ImGui::Checkbox("Use Sig",             &cfg.useSig);
                    ImGui::SliderFloat("Sig range",        &cfg.sigRange, 50.f, 500.f);
                    ImGui::TextDisabled("Sig si ennemi hp>80 dans la portée");
                    ImGui::Separator();

                    // WeaponSnap
                    ImGui::TextColored({1.f,1.f,0.3f,1.f},"=== Weapon Snap ===");
                    ImGui::Checkbox("Weapon Snap",         &cfg.weaponSnap);
                    ImGui::SliderFloat("Snap range",       &cfg.snapRangeUnits, 100.f, 1000.f);
                    ImGui::TextDisabled("Inactif si chase ennemi ou dodge");
                    ImGui::Separator();

                    // Timings
                    ImGui::TextColored({0.6f,0.6f,0.6f,1.f},"=== Timings ===");
                    ImGui::Checkbox("Randomize timings",   &cfg.randomizeTimings);
                    ImGui::SliderInt("Variance (ms)",      &cfg.timingVarianceMs, 0, 30);
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        sc->Present(1, 0);
    }
};

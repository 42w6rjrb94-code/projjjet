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

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT WINAPI OverlayWndProc(HWND hWnd, UINT msg,
                                      WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    switch (msg) {
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

class Renderer {
public:
    HWND overlay=nullptr, target=nullptr;
    ID3D11Device* dev=nullptr;
    ID3D11DeviceContext* ctx=nullptr;
    IDXGISwapChain* sc=nullptr;
    ID3D11RenderTargetView* rtv=nullptr;
    int W=1920, H=1080;

    bool Init() {
        target = FindWindowW(nullptr, L"Brawlhalla");
        if (!target) return false;
        RECT r; GetClientRect(target, &r);
        W=r.right; H=r.bottom;
        if (W<=0) W=1920;
        if (H<=0) H=1080;

        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc   = OverlayWndProc;
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"BHOvr";
        RegisterClassExW(&wc);

        overlay=CreateWindowExW(
            WS_EX_TOPMOST|WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE,
            L"BHOvr",L"",WS_POPUP,0,0,W,H,
            nullptr,nullptr,wc.hInstance,nullptr);
        if (!overlay) return false;
        SetLayeredWindowAttributes(overlay,RGB(0,0,0),0,LWA_COLORKEY);
        MARGINS m{-1}; DwmExtendFrameIntoClientArea(overlay,&m);

        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount=2;
        sd.BufferDesc.Width=W; sd.BufferDesc.Height=H;
        sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow=overlay; sd.SampleDesc.Count=1; sd.Windowed=TRUE;
        sd.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
        D3D_FEATURE_LEVEL fl;
        D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,
            nullptr,0,nullptr,0,D3D11_SDK_VERSION,&sd,&sc,&dev,&fl,&ctx);
        ID3D11Texture2D* bb;
        sc->GetBuffer(0,__uuidof(ID3D11Texture2D),(void**)&bb);
        dev->CreateRenderTargetView(bb,nullptr,&rtv); bb->Release();
        ShowWindow(overlay,SW_SHOW);

        IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGui::StyleColorsDark();
        auto& s=ImGui::GetStyle();
        s.WindowRounding=6.f; s.FrameRounding=4.f;
        auto* c=s.Colors;
        c[ImGuiCol_WindowBg]       ={0.07f,0.07f,0.09f,0.95f};
        c[ImGuiCol_TitleBgActive]  ={0.18f,0.08f,0.38f,1.f};
        c[ImGuiCol_CheckMark]      ={0.6f,0.2f,1.0f,1.f};
        c[ImGuiCol_SliderGrab]     ={0.6f,0.2f,1.0f,1.f};
        c[ImGuiCol_Button]         ={0.18f,0.08f,0.38f,1.f};
        c[ImGuiCol_ButtonHovered]  ={0.28f,0.12f,0.55f,1.f};
        c[ImGuiCol_Tab]            ={0.12f,0.05f,0.25f,1.f};
        c[ImGuiCol_TabSelected]    ={0.28f,0.12f,0.55f,1.f};
        ImGui_ImplWin32_Init(overlay);
        ImGui_ImplDX11_Init(dev,ctx);
        return true;
    }

    void SetInteractive(bool interactive) {
        if (!overlay) return;
        LONG_PTR style = GetWindowLongPtrW(overlay, GWL_EXSTYLE);
        if (interactive) {
            style &= ~WS_EX_TRANSPARENT;
            style &= ~WS_EX_NOACTIVATE;
            SetWindowLongPtrW(overlay, GWL_EXSTYLE, style);
            SetForegroundWindow(overlay);
        } else {
            style |= WS_EX_TRANSPARENT;
            style |= WS_EX_NOACTIVATE;
            SetWindowLongPtrW(overlay, GWL_EXSTYLE, style);
        }
    }

    // Coordonnées monde Brawlhalla → pixel écran
    // Les coords monde sont ~0-3000 en X, ~0-2500 en Y
    // On mappe sur la taille de la fenêtre de jeu
    bool W2S(float wx, float wy,
             float camX, float camY, float zoom,
             ImVec2& out) {
        float sx = (wx - camX) * zoom + W * 0.5f;
        float sy = H * 0.5f - (wy - camY) * zoom;
        if (sx < -200.f || sx > W+200.f) return false;
        if (sy < -200.f || sy > H+200.f) return false;
        out = {sx, sy};
        return true;
    }

    void Frame(const std::vector<BHPlayer>& players,
               const std::vector<BHWeapon>& weapons,
               Config& cfg, bool& menuOpen,
               float camX, float camY, float zoom) {

        // Suivre la fenêtre Brawlhalla
        if (target) {
            RECT r; GetWindowRect(target, &r);
            SetWindowPos(overlay, HWND_TOPMOST,
                r.left, r.top,
                r.right-r.left, r.bottom-r.top,
                SWP_NOACTIVATE);
        }

        float cl[4]{};
        ctx->OMSetRenderTargets(1, &rtv, nullptr);
        ctx->ClearRenderTargetView(rtv, cl);
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        auto* dl = ImGui::GetBackgroundDrawList();

        if (cfg.espEnabled) {
            for (size_t i = 0; i < players.size(); i++) {
                const auto& p = players[i];
                if (p.isLocal) continue;

                ImVec2 s;
                if (!W2S(p.worldPos.x, p.worldPos.y,
                         camX, camY, zoom, s)) continue;

                // Rouge pour ennemis, vert pour alliés
                ImU32 col = IM_COL32(255, 80, 80, 255);

                float hw = 20.f, hh = 35.f;

                if (cfg.espBoxes)
                    dl->AddRect(
                        {s.x-hw, s.y-hh},
                        {s.x+hw, s.y+hh},
                        col, 0, 0, 2.f);

                if (cfg.espHP && p.maxHp > 0) {
                    float ratio = p.hp / p.maxHp;
                    dl->AddRectFilled(
                        {s.x-hw-7, s.y+hh},
                        {s.x-hw-3, s.y-hh},
                        IM_COL32(40,40,40,180));
                    dl->AddRectFilled(
                        {s.x-hw-7, s.y+hh},
                        {s.x-hw-3, s.y+hh - hh*2*ratio},
                        IM_COL32(
                            (int)(255*(1-ratio)),
                            (int)(255*ratio), 0, 255));
                }

                if (cfg.espNames) {
                    char lbl[128];
                    snprintf(lbl, sizeof(lbl), "%s [%.0f/%.0f]",

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

        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc=DefWindowProcW;
        wc.hInstance=GetModuleHandleW(nullptr);
        wc.lpszClassName=L"BHOvr";
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
        c[ImGuiCol_WindowBg]      ={0.07f,0.07f,0.09f,0.95f};
        c[ImGuiCol_TitleBgActive] ={0.18f,0.08f,0.38f,1.f};
        c[ImGuiCol_CheckMark]     ={0.6f,0.2f,1.0f,1.f};
        c[ImGuiCol_SliderGrab]    ={0.6f,0.2f,1.0f,1.f};
        c[ImGuiCol_Button]        ={0.18f,0.08f,0.38f,1.f};
        c[ImGuiCol_ButtonHovered] ={0.28f,0.12f,0.55f,1.f};
        c[ImGuiCol_Tab]           ={0.12f,0.05f,0.25f,1.f};
        c[ImGuiCol_TabActive]     ={0.28f,0.12f,0.55f,1.f};
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

    bool W2S(float wx,float wy,float camX,float camY,float zoom,ImVec2& out){
        float sx=(wx-camX)*zoom+W*0.5f;
        float sy=H*0.5f-(wy-camY)*zoom;
        if(sx<0||sx>W||sy<0||sy>H) return false;
        out={sx,sy}; return true;
    }

    void Frame(const std::vector<BHPlayer>& players,
               const std::vector<BHWeapon>& weapons,
               Config& cfg, bool& menuOpen,
               float camX,float camY,float zoom){
        if(target){
            RECT r; GetWindowRect(target,&r);
            SetWindowPos(overlay,HWND_TOPMOST,r.left,r.top,
                r.right-r.left,r.bottom-r.top,SWP_NOACTIVATE);
        }
        float cl[4]{}; ctx->OMSetRenderTargets(1,&rtv,nullptr);
        ctx->ClearRenderTargetView(rtv,cl);
        ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
        auto* dl=ImGui::GetBackgroundDrawList();

        if(cfg.espEnabled){
            for(const auto& p:players){
                if(p.isLocal) continue;
                ImVec2 s;
                if(!W2S(p.worldPos.x,p.worldPos.y,camX,camY,zoom,s)) continue;
                ImU32 col=p.teamID==0
                    ?IM_COL32(255,80,80,255):IM_COL32(80,255,80,255);
                float hw=30.f*zoom,hh=50.f*zoom;
                if(cfg.espBoxes)
                    dl->AddRect({s.x-hw,s.y-hh},{s.x+hw,s.y+hh},col,0,0,2.f);
                if(cfg.espHP&&p.maxHp>0){
                    float ratio=p.hp/p.maxHp;
                    dl->AddRectFilled({s.x-hw-7,s.y+hh},{s.x-hw-3,s.y-hh},
                        IM_COL32(40,40,40,180));
                    dl->AddRectFilled({s.x-hw-7,s.y+hh},
                        {s.x-hw-3,s.y+hh-hh*2*ratio},
                        IM_COL32(int(255*(1-ratio)),int(255*ratio),0,255));
                }
                if(cfg.espNames){
                    char lbl[128];
                    snprintf(lbl,sizeof(lbl),"%s [%.0f/%.0f]",
                        p.name.c_str(),p.hp,p.maxHp);
                    dl->AddText({s.x-hw,s.y-hh-16},col,lbl);
                }
            }
            for(const auto& w:weapons){
                if(!cfg.espWeapons) break;
                ImVec2 s;
                if(!W2S(w.pos.x,w.pos.y,camX,camY,zoom,s)) continue;
                dl->AddCircle(s,8.f,IM_COL32(255,220,0,255),12,2.f);
            }
        }

        if(menuOpen){
            ImGui::SetNextWindowSize({320,340},ImGuiCond_Once);
            ImGui::SetNextWindowPos({30,30},ImGuiCond_Once);
            ImGui::Begin("  BH Cheat  v1.0",&menuOpen,ImGuiWindowFlags_NoResize);
            if(ImGui::BeginTabBar("tabs")){
                if(ImGui::BeginTabItem("ESP")){
                    ImGui::Checkbox("ESP actif",   &cfg.espEnabled);
                    ImGui::Checkbox("Boxes",        &cfg.espBoxes);
                    ImGui::Checkbox("HP bar",       &cfg.espHP);
                    ImGui::Checkbox("Noms",         &cfg.espNames);
                    ImGui::Checkbox("Armes au sol", &cfg.espWeapons);
                    ImGui::EndTabItem();
                }
                if(ImGui::BeginTabItem("Auto Dodge")){
                    ImGui::Checkbox("Auto Dodge",   &cfg.autoDodge);
                    ImGui::Checkbox("Ground only",  &cfg.dodgeGroundOnly);
                    ImGui::SliderInt("Délai (ms)",  &cfg.dodgeDelayMs,0,150);
                    ImGui::EndTabItem();
                }
                if(ImGui::BeginTabItem("DexBot")){
                    ImGui::Checkbox("DexBot",           &cfg.dexBot);
                    ImGui::Checkbox("Sur hitstun seul", &cfg.dexBotOnlyHit);
                    ImGui::SliderInt("Inter-input (ms)",&cfg.comboDelayMs,16,200);
                    ImGui::EndTabItem();
                }
                if(ImGui::BeginTabItem("Weapon")){
                    ImGui::Checkbox("Weapon Snap",     &cfg.weaponSnap);
                    ImGui::SliderFloat("Range",        &cfg.snapRangeUnits,50,800);
                    ImGui::EndTabItem();
                }
                if(ImGui::BeginTabItem("Config")){
                    ImGui::Checkbox("Timings aléatoires",&cfg.randomizeTimings);
                    ImGui::SliderInt("Variance (ms)",    &cfg.timingVarianceMs,0,30);
                    ImGui::Separator();
                    if(ImGui::Button("Sauvegarder")) cfg.Save();
                    ImGui::SameLine();
                    if(ImGui::Button("Charger"))     cfg.Load();
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
        sc->Present(0,0);
    }

    void Shutdown(){
        ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        if(rtv) rtv->Release(); if(sc) sc->Release();
        if(ctx) ctx->Release(); if(dev) dev->Release();
        if(overlay) DestroyWindow(overlay);
    }
};

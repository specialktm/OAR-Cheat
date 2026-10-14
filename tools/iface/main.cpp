#include <windows.h>
#include <d3d11.h>
#include <cstdio>
#include <cstring>
#include <string>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#include "../../cheat/vendor/imgui_dock/imgui.h"
#include "../../cheat/vendor/imgui_dock/backends/imgui_impl_win32.h"
#include "../../cheat/vendor/imgui_dock/backends/imgui_impl_dx11.h"
#include "../../shared/shm.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static ID3D11Device* g_dev = nullptr;
static ID3D11DeviceContext* g_ctx = nullptr;
static IDXGISwapChain* g_swap = nullptr;
static ID3D11RenderTargetView* g_rtv = nullptr;
static HWND g_hwnd = nullptr;
static bool g_smoke = false;
static int g_smokeFrames = 0;
static HANDLE g_map = nullptr;
static shm::Region* g_shm = nullptr;

static LRESULT __stdcall WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return 0;
    if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcA(h, m, w, l);
}

static void apply_take_style() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 10.0f;
    s.ChildRounding = 7.0f;
    s.FrameRounding = 5.0f;
    s.PopupRounding = 5.0f;
    s.ScrollbarSize = 3.0f;
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.086f, 0.098f, 0.118f, 1.0f);
    c[ImGuiCol_ChildBg] = ImVec4(0.114f, 0.129f, 0.157f, 1.0f);
    c[ImGuiCol_PopupBg] = ImVec4(0.102f, 0.102f, 0.102f, 1.0f);
    c[ImGuiCol_Border] = ImVec4(0.180f, 0.216f, 0.259f, 1.0f);
    c[ImGuiCol_Text] = ImVec4(0.914f, 0.937f, 0.957f, 1.0f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.545f, 0.592f, 0.647f, 1.0f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.0f, 0.75f, 1.0f, 1.0f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.2f, 0.82f, 1.0f, 1.0f);
    c[ImGuiCol_Button] = ImVec4(0.137f, 0.161f, 0.188f, 1.0f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.0f, 0.75f, 1.0f, 1.0f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.0f, 0.58f, 0.78f, 1.0f);
    c[ImGuiCol_Header] = ImVec4(0.149f, 0.149f, 0.149f, 1.0f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.22f, 0.22f, 0.22f, 1.0f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.28f, 0.28f, 0.28f, 1.0f);
    c[ImGuiCol_FrameBg] = ImVec4(0.137f, 0.161f, 0.188f, 1.0f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.169f, 0.2f, 0.239f, 1.0f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.216f, 0.259f, 0.31f, 1.0f);
}

static bool init_d3d(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC d{};
    d.BufferCount = 2;
    d.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    d.BufferDesc.RefreshRate.Numerator = 60;
    d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    d.OutputWindow = hwnd;
    d.SampleDesc.Count = 1;
    d.Windowed = TRUE;
    d.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL lvl;
    const D3D_FEATURE_LEVEL lvls[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        lvls, 2, D3D11_SDK_VERSION, &d, &g_swap, &g_dev, &lvl, &g_ctx))) return false;
    ID3D11Texture2D* back = nullptr;
    if (FAILED(g_swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back))) return false;
    HRESULT hr = g_dev->CreateRenderTargetView(back, nullptr, &g_rtv);
    back->Release();
    return SUCCEEDED(hr);
}

static void open_shm() {
    g_map = OpenFileMappingA(FILE_MAP_READ, FALSE, shm::MAP_NAME);
    if (g_map) g_shm = (shm::Region*)MapViewOfFile(g_map, FILE_MAP_READ, 0, 0, sizeof(shm::Region));
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i)
        if (!strcmp(argv[i], "--smoke")) g_smoke = true;

    WNDCLASSA wc{ CS_CLASSDC, WndProc, 0, 0, GetModuleHandleA(nullptr),
        nullptr, nullptr, nullptr, nullptr, "OAR_IFACE" };
    RegisterClassA(&wc);
    g_hwnd = CreateWindowA(wc.lpszClassName, "OAR Interface", WS_OVERLAPPEDWINDOW,
        200, 200, 960, 620, nullptr, nullptr, wc.hInstance, nullptr);
    if (!g_hwnd) { printf("window failed\n"); return 1; }
    if (!init_d3d(g_hwnd)) { printf("d3d failed\n"); return 2; }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigViewportsNoAutoMerge = true;
    apply_take_style();
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_dev, g_ctx);
    open_shm();
    if (!g_smoke) ShowWindow(g_hwnd, SW_SHOWDEFAULT);

    bool running = true;
    MSG msg{};
    while (running) {
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) running = false;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!running) break;
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
        ImGui::Begin("OAR Interface");
        ImGui::Text("docking shell online");
        ImGui::Text("display %.0fx%.0f", io.DisplaySize.x, io.DisplaySize.y);
        if (g_shm && g_shm->snap.magic == shm::MAGIC && g_shm->snap.ver == shm::VERSION)
            ImGui::Text("shm frame %u players %u guards %u loot %u",
                g_shm->snap.frame, g_shm->snap.np, g_shm->snap.ng, g_shm->snap.nl);
        else
            ImGui::TextDisabled("shm not connected (inject the no-render dll)");
        ImGui::End();
        ImGui::Render();
        const float clear[4] = { 0.05f, 0.06f, 0.08f, 1.0f };
        g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_ctx->ClearRenderTargetView(g_rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
        g_swap->Present(1, 0);
        if (g_smoke && ++g_smokeFrames >= 5) {
            printf("smoke ok\n");
            break;
        }
    }

    if (g_shm) UnmapViewOfFile(g_shm);
    if (g_map) CloseHandle(g_map);
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (g_rtv) g_rtv->Release();
    if (g_ctx) g_ctx->Release();
    if (g_dev) g_dev->Release();
    if (g_swap) g_swap->Release();
    DestroyWindow(g_hwnd);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);
    return 0;
}

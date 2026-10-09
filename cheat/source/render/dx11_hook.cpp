#include "pch/pch.h"
#include <cstring>
#include "render/dx11_hook.hpp"
#include "render/blur.hpp"
#include "core/hook.hpp"
#include "config/config.hpp"
#include "core/loading.hpp"
#include "features/esp.hpp"
#include "ui/menu.hpp"
#include "ui/listmenu.hpp"
#include "features/capture.hpp"

#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#include "../../vendor/imgui/imgui.h"
#include "../../vendor/imgui/imgui_internal.h"
#include "../../vendor/imgui/backends/imgui_impl_win32.h"
#include "../../vendor/imgui/backends/imgui_impl_dx11.h"
#include "../../vendor/fonts/inter.h"

float dpi_scale = 1.0f;
ImFont* tab_text2 = nullptr;
ImFont* tab_text3 = nullptr;
#include "ui/tabs.hpp"
#include "logger/logger.hpp"
#include "world/actor_iter.hpp"
#include "SDK/Engine_classes.hpp"
#include "steam/steam_user.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace dx11 {

using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
using ResizeBuffersFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

static HWND g_hwnd = nullptr;
static WNDPROC g_origWndProc = nullptr;
static ID3D11Device* g_device = nullptr;
static ID3D11DeviceContext* g_ctx = nullptr;
static IDXGISwapChain* g_swap = nullptr;
static ID3D11RenderTargetView* g_rtv = nullptr;
static ImFont* g_titleFont = nullptr;
static ImFont* g_icoFont = nullptr;
static bool g_initImgui = false;
static bool g_ready = false;

static hook::Detour g_presentHook;
static hook::Detour g_resizeHook;

static std::atomic<bool> g_unloading{ false };
static std::atomic<int> g_inflight{ 0 };
static bool g_tornDown = false;

static void create_rtv() {
    if (!g_swap || g_rtv) return;
    ID3D11Texture2D* back = nullptr;
    if (SUCCEEDED(g_swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back)) && back) {
        g_device->CreateRenderTargetView(back, nullptr, &g_rtv);
        back->Release();
    }
}
static void release_rtv() {
    if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
}

static void setup_imgui() {
    ImGui::CreateContext();
    ui::apply_modern_style();
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_device, g_ctx);
    create_rtv();

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    ImFont* main = io.Fonts->AddFontFromMemoryTTF(
        (void*)inter, (int)sizeof(inter), 16.0f);
    if (!main)
        main = io.Fonts->AddFontFromFileTTF(
            "C:\\Windows\\Fonts\\segoeui.ttf", 16.0f);
    if (main) io.FontDefault = main;
    else io.Fonts->AddFontDefault();
    ImFontConfig merge;
    merge.MergeMode = true;
    merge.PixelSnapH = true;
    const ImWchar* ranges[] = {
        io.Fonts->GetGlyphRangesJapanese(),
        io.Fonts->GetGlyphRangesChineseSimplifiedCommon(),
        io.Fonts->GetGlyphRangesCyrillic(),
        io.Fonts->GetGlyphRangesThai(),
        io.Fonts->GetGlyphRangesVietnamese(),
    };
    for (int i = 0; i < 5; ++i)
        io.Fonts->AddFontFromFileTTF(
            "C:\\Windows\\Fonts\\segoeui.ttf", 16.0f, &merge, ranges[i]);
    {
        static const ImWchar emoji_ranges[] = { 0x1F300, 0x1F9FF, 0x2600, 0x26FF, 0 };
        ImFontConfig emoji_cfg;
        emoji_cfg.MergeMode = true;
        emoji_cfg.PixelSnapH = true;
        emoji_cfg.GlyphMinAdvanceX = 16.0f;
        emoji_cfg.GlyphOffset = ImVec2(0, 3);
        io.Fonts->AddFontFromFileTTF(
            "C:\\Windows\\Fonts\\seguiemj.ttf", 20.0f, &emoji_cfg, emoji_ranges);
    }
    g_titleFont = io.Fonts->AddFontFromMemoryTTF(
        (void*)inter, (int)sizeof(inter), 22.0f);
    g_icoFont = main ? main : (ImFont*)io.FontDefault;
    tab_text2 = io.Fonts->AddFontFromMemoryTTF(
        (void*)inter, (int)sizeof(inter), 24.0f);
    tab_text3 = io.Fonts->AddFontFromMemoryTTF(
        (void*)inter, (int)sizeof(inter), 40.0f);
    g_initImgui = true;
    g_ready = true;
}

static const char* current_tab_title() {
    const std::vector<tabs::Tab>& all = tabs::list();
    int cur = tabs::current();
    if (cur < 0 || cur >= (int)all.size()) return "<none>";
    return all[cur].title ? all[cur].title : "<null>";
}
static LRESULT __stdcall WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (g_unloading.load())
        return CallWindowProcA(g_origWndProc, h, m, w, l);
    int menuVk = g_cfg.keyMenu.load();
    if (m == WM_KEYDOWN && menuVk != 0 && w == (WPARAM)menuVk) {
        bool held = (l & (1u<<30)) != 0;
        if (!held && loading::done() && g_cfg.keyModeMenu.load() == 0) {
            bool o = g_cfg.menuOpen.load();
            g_cfg.menuOpen.store(!o);
        }
    }
    if (g_cfg.menuOpen.load()) {

        ImGui_ImplWin32_WndProcHandler(h, m, w, l);

        if (g_cfg.uiStyle.load() == 1)
            return CallWindowProcA(g_origWndProc, h, m, w, l);
        ImGuiIO& io = ImGui::GetIO();

        switch (m) {
        case WM_INPUT:
        case WM_MOUSEMOVE:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:
        case WM_KEYDOWN: case WM_SYSKEYDOWN:
        case WM_CHAR: case WM_SYSCHAR: case WM_DEADCHAR: case WM_SYSDEADCHAR:
            return 0;
        default:

            if (io.WantCaptureMouse &&
                (m >= WM_MOUSEFIRST && m <= WM_MOUSELAST)) return 0;
            if (io.WantCaptureKeyboard &&
                ((m >= WM_KEYFIRST && m <= WM_KEYLAST) || m == WM_CHAR)) return 0;
            break;
        }
    }
    return CallWindowProcA(g_origWndProc, h, m, w, l);
}

static HRESULT __stdcall present_impl(IDXGISwapChain* sc, UINT sync, UINT flags) {
    static bool first = true;
    if (first) {
        first = false;
        g_swap = sc;
        sc->GetDevice(__uuidof(ID3D11Device), (void**)&g_device);
        g_device->GetImmediateContext(&g_ctx);

        DXGI_SWAP_CHAIN_DESC d{};
        sc->GetDesc(&d);
        g_hwnd = d.OutputWindow;
        g_origWndProc = (WNDPROC)SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, (LONG_PTR)WndProc);

        setup_imgui();
    } else if (g_swap != sc) {
        g_swap = sc;
    }
    if (!g_initImgui) return g_presentHook.orig<PresentFn>()(sc, sync, flags);
    if (!g_rtv) create_rtv();

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    esp::begin_present();

    __try {
        if (ui::visibility() > 0.004f && g_cfg.uiStyle.load() == 0)
            ui::render_backdrop();
        if (g_cfg.uiStyle.load() == 1 && g_cfg.menuOpen.load()) listui::handle_keys();
        capture::present_upload();
        steamServiceHelper::render_upload();
        esp::render_items();
        esp::render_players();
        esp::render_guards();
        esp::render_npcs();
        ui::render();
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    ImGui::Render();
    g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    return g_presentHook.orig<PresentFn>()(sc, sync, flags);
}

static HRESULT __stdcall hkPresent(IDXGISwapChain* sc, UINT sync, UINT flags) {

    g_inflight.fetch_add(1);
    if (g_unloading.load()) {
        HRESULT hr = g_presentHook.orig<PresentFn>()(sc, sync, flags);
        g_inflight.fetch_sub(1);
        return hr;
    }
    HRESULT hr = present_impl(sc, sync, flags);
    g_inflight.fetch_sub(1);
    return hr;
}

static HRESULT __stdcall resize_impl(IDXGISwapChain* sc, UINT bc, UINT w, UINT h, DXGI_FORMAT f, UINT fl) {
    release_rtv();
    blur::on_resize();
    HRESULT hr = g_resizeHook.orig<ResizeBuffersFn>()(sc, bc, w, h, f, fl);
    create_rtv();
    return hr;
}

static HRESULT __stdcall hkResizeBuffers(IDXGISwapChain* sc, UINT bc, UINT w, UINT h, DXGI_FORMAT f, UINT fl) {
    g_inflight.fetch_add(1);
    HRESULT hr;
    if (g_unloading.load())
        hr = g_resizeHook.orig<ResizeBuffersFn>()(sc, bc, w, h, f, fl);
    else
        hr = resize_impl(sc, bc, w, h, f, fl);
    g_inflight.fetch_sub(1);
    return hr;
}

bool is_ready() { return g_ready; }
void* swapchain() { return g_swap; }
void* device() { return g_device; }
void* title_font() { return g_titleFont; }
void* icon_font() { return g_icoFont; }
void* get_rtv() { return g_rtv; }
void* ctx() { return g_ctx; }
void* present_target() { return g_presentHook.target(); }
bool present_hooked() { return g_presentHook.valid(); }
void* resize_target() { return g_resizeHook.target(); }
bool resize_hooked() { return g_resizeHook.valid(); }

bool init() {
    if (g_ready) return true;
    if (!hook::init()) return false;

    WNDCLASSA wc{ CS_CLASSDC, DefWindowProcA, 0, 0, GetModuleHandleA(nullptr),
        nullptr, nullptr, nullptr, nullptr, "OAR_DUMMY" };

    bool registered = RegisterClassA(&wc) != 0;
    if (!registered && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    HWND dummy = CreateWindowA(wc.lpszClassName, "tmp", WS_OVERLAPPEDWINDOW,
        0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
    if (!dummy) {
        if (registered) UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return false;
    }

    DXGI_SWAP_CHAIN_DESC d{};
    d.BufferCount = 1;
    d.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    d.OutputWindow = dummy;
    d.SampleDesc.Count = 1;
    d.Windowed = TRUE;
    d.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    IDXGISwapChain* sc = nullptr;
    D3D_FEATURE_LEVEL lvl;
    const D3D_FEATURE_LEVEL lvls[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        lvls, 2, D3D11_SDK_VERSION, &d, &sc, &dev, &lvl, &ctx);

    bool ok = false;
    if (SUCCEEDED(hr) && sc) {
        void** vt = *reinterpret_cast<void***>(sc);
        void* present = vt[8];
        void* resize = vt[13];
        if (present && g_presentHook.create(present, (void*)hkPresent)) g_presentHook.enable();
        if (resize && g_resizeHook.create(resize, (void*)hkResizeBuffers)) g_resizeHook.enable();
        ok = g_presentHook.valid();
        sc->Release();
    }
    if (dev) dev->Release();
    if (ctx) ctx->Release();
    DestroyWindow(dummy);
    if (registered) UnregisterClassA(wc.lpszClassName, wc.hInstance);
    return ok;
}

void shutdown() {
    if (g_tornDown) return;
    g_unloading.store(true);

    for (int i = 0; i < 1000 && g_inflight.load() > 0; ++i) Sleep(10);
    __try {
        WNDPROC orig = g_origWndProc;
        if (orig && g_hwnd)
            SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, (LONG_PTR)orig);
        MH_DisableHook(MH_ALL_HOOKS);
        g_presentHook.remove();
        g_resizeHook.remove();
        blur::shutdown();
        if (g_initImgui) {
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            g_initImgui = false;
        }
        release_rtv();
        if (g_ctx) { g_ctx->Release(); g_ctx = nullptr; }
        if (g_device) { g_device->Release(); g_device = nullptr; }
        g_ready = false;
        g_tornDown = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    hook::shutdown();
}

bool is_unloading() { return g_unloading.load(); }

}

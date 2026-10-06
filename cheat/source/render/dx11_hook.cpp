#include "pch/pch.h"
#include <cstring>
#include "render/dx11_hook.hpp"
#include "render/blur.hpp"
#include "core/hook.hpp"
#include "core/config.hpp"
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
#include "ui/tabs.hpp"
#include "logger/logger.hpp"

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
static bool g_initImgui = false;
static bool g_ready = false;

static hook::Detour g_presentHook;
static hook::Detour g_resizeHook;

// Unload safety: detours can be in-flight on the render thread while the
// worker thread tears down. Flag + refcount lets in-flight calls drain
// before any hook/state is removed, so FreeLibrary never unmaps live code.
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

// ImGui setup factored out so the recovery path can rebuild it.
static void setup_imgui() {
    ImGui::CreateContext();
    ui::apply_modern_style();
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_device, g_ctx);
    create_rtv();
    g_initImgui = true;
    g_ready = true;
}

// ---- frame integrity gate -------------------------------------------
// An AV swallowed mid-window (stale actor read inside a tab) leaves the
// Begin/End stack dangling, which used to surface one frame later as a
// render-thread fatal inside EndFrame recovery ("Missing EndChild").
// We snapshot ImGui's own recovery state right after NewFrame (which opens
// the implicit fallback window BY DESIGN - an "empty stack" expectation
// here is wrong and used to false-trip every frame) and compare it before
// Render. Mismatch => abandon the frame on a FRESH context and pair it
// with an empty NewFrame/Render so the next frame's sanity check
// ("Forgot to call Render()?") still passes. Skipping Render without
// pairing is itself fatal - never do that.
// All helpers keep globals only (ImGuiErrorRecoveryState has a ctor, so it
// must never be a local in a function containing __try under /EHsc).
static ImGuiErrorRecoveryState g_frameStart{};
static ImGuiErrorRecoveryState g_frameEnd{};
static unsigned g_errStart = 0;
static bool g_haveStart = false;
static unsigned snapshot_err() {
    ImGuiContext* ctx = ImGui::GetCurrentContext();
    __try { return ctx ? (unsigned)ctx->ErrorCountCurrentFrame : 0xFFFFFFFFu; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0xFFFFFFFFu; }
}
static void snapshot_frame_start() {
    __try {
        ImGui::ErrorRecoveryStoreState(&g_frameStart);
        g_errStart = snapshot_err();
        g_haveStart = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { g_haveStart = false; }
}
static bool frame_balanced(bool& outStackMismatch) {
    outStackMismatch = true;
    __try {
        if (!g_haveStart) return false;
        ImGui::ErrorRecoveryStoreState(&g_frameEnd);
        if (memcmp(&g_frameStart, &g_frameEnd, sizeof(g_frameStart)) != 0) return false;
        outStackMismatch = false;
        return snapshot_err() == g_errStart;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static const char* current_tab_title() {
    const std::vector<tabs::Tab>& all = tabs::list();
    int cur = tabs::current();
    if (cur < 0 || cur >= (int)all.size()) return "<none>";
    return all[cur].title ? all[cur].title : "<null>";
}
static uint64_t g_lastRecoverMs = 0;
static void log_corrupt(const char* tab, bool stackMismatch) {
    uint64_t now = GetTickCount64();
    if (now - g_lastRecoverMs < 2000) return; // don't flood the console
    g_lastRecoverMs = now;
    if (g_logger) g_logger->send(levels::critical,
        "ImGui frame unbalanced, rebuilding context (tab={} {})", tab,
        stackMismatch ? "stack-mismatch" : "new-errors-logged");
}
static void rebuild_imgui_guarded() {
    __try {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_initImgui = false;
        setup_imgui();
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}
// Abandon the corrupt frame on a fresh context and pair it with an empty
// NewFrame/Render: every NewFrame MUST be paired or the next frame's sanity
// check fatals. Skipping Render is never an option.
static void recover_frame(const char* tab, bool stackMismatch) {
    log_corrupt(tab, stackMismatch);
    rebuild_imgui_guarded();
    __try {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGui::Render();
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static LRESULT __stdcall WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (g_unloading.load())
        return CallWindowProcA(g_origWndProc, h, m, w, l);
    int menuVk = g_cfg.keyMenu.load();
    if (m == WM_KEYDOWN && menuVk != 0 && w == (WPARAM)menuVk) {
        bool o = g_cfg.menuOpen.load();
        g_cfg.menuOpen.store(!o);
    }
    if (g_cfg.menuOpen.load()) {
        // Let ImGui consume first so menu interaction (clicks, typing) works.
        ImGui_ImplWin32_WndProcHandler(h, m, w, l);
        ImGuiIO& io = ImGui::GetIO();
        // Swallow game input while open: mouse buttons/motion/wheel and
        // keyboard/char traffic the game would otherwise act on (camera,
        // movement, shooting). System/size/focus messages still pass through.
        switch (m) {
        case WM_MOUSEMOVE:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
        case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP:
        case WM_CHAR: case WM_SYSCHAR: case WM_DEADCHAR: case WM_SYSDEADCHAR:
            return 0;
        default:
            // If ImGui explicitly wants capture, also eat it.
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
        g_swap = sc; // defensive
    }
    if (!g_initImgui) return g_presentHook.orig<PresentFn>()(sc, sync, flags);
    if (!g_rtv) create_rtv();

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    snapshot_frame_start();
    bool stackMismatch = true;
    esp::begin_present();

    __try {
        if (g_cfg.menuOpen.load() && g_cfg.uiStyle.load() == 0) ui::render_backdrop();
        if (g_cfg.uiStyle.load() == 1 && g_cfg.menuOpen.load()) listui::handle_keys();
        capture::present_upload();
        esp::render_items();
        esp::render_players();
        esp::render_guards();
        ui::render();
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    // Pairing invariant: every NewFrame MUST reach Render/EndFrame, or the
    // next frame's sanity check fatals. On imbalance, rebuild on a fresh
    // context and pair with an empty frame instead of skipping Render.
    if (!frame_balanced(stackMismatch))
        recover_frame(current_tab_title(), stackMismatch);
    else
        ImGui::Render();
    g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    return g_presentHook.orig<PresentFn>()(sc, sync, flags);
}

static HRESULT __stdcall hkPresent(IDXGISwapChain* sc, UINT sync, UINT flags) {
    // Count first, then check: teardown waits for zero, so orig is valid
    // for every call that observes unloading==true.
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
void* present_target() { return g_presentHook.target(); }
bool present_hooked() { return g_presentHook.valid(); }
void* resize_target() { return g_resizeHook.target(); }
bool resize_hooked() { return g_resizeHook.valid(); }

bool init() {
    if (g_ready) return true;
    if (!hook::init()) return false;

    // Dummy device to steal vtable (standard DX11 hook discovery).
    WNDCLASSA wc{ CS_CLASSDC, DefWindowProcA, 0, 0, GetModuleHandleA(nullptr),
        nullptr, nullptr, nullptr, nullptr, "OAR_DUMMY" };
    // Tolerate a leftover registration from a previous life (crashed unload).
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
    // Drain in-flight detour calls before touching hooks or state.
    for (int i = 0; i < 100 && g_inflight.load() > 0; ++i) Sleep(10);
    __try {
        if (g_origWndProc && g_hwnd)
            SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, (LONG_PTR)g_origWndProc);
        g_origWndProc = nullptr;
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

} // namespace dx11

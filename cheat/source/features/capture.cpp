#include "pch/pch.h"
#include "features/capture.hpp"
#include "world/actor_iter.hpp"
#include "render/dx11_hook.hpp"
#include "util/files.hpp"
#include "SDK/Basic.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/Engine_structs.hpp"
#include "SDK/CoreUObject_classes.hpp"

#include <d3d11.h>
#include <wincodec.h>

namespace capture {

static std::atomic<bool> g_active{ false };
static std::mutex g_tgtMtx;
static float g_tx = 0, g_ty = 0, g_tz = 0;
static bool g_hasTarget = false;

static void* g_cap = nullptr;
static void* g_rt = nullptr;
static uint64_t g_lastMs = 0;
static float g_yawDeg = 0.0f;
static bool g_evenPhase = false;

static ID3D11Texture2D* g_tex = nullptr;
static ID3D11ShaderResourceView* g_srv = nullptr;
static std::mutex g_pxMtx;
static std::vector<uint8_t> g_px;
static int g_pxW = 0, g_pxH = 0;
static bool g_dirty = false;

static SDK::UWorld* seh_world() {
    __try { return static_cast<SDK::UWorld*>(actors::game_world()); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static SDK::UClass* seh_cap_static() {
    __try { return SDK::ASceneCapture2D::StaticClass(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static SDK::AActor* seh_begin(SDK::UObject* world, SDK::UClass* cls, const SDK::FTransform& tf) {
    __try {
        return SDK::UGameplayStatics::BeginDeferredActorSpawnFromClass(
            world, cls, tf,
            SDK::ESpawnActorCollisionHandlingMethod::AlwaysSpawn, nullptr);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static bool seh_finish(SDK::AActor* a, const SDK::FTransform& tf) {
    __try { SDK::UGameplayStatics::FinishSpawningActor(a, tf); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static SDK::USceneCaptureComponent2D* seh_comp(SDK::ASceneCapture2D* a) {
    __try { return a->CaptureComponent2D; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static SDK::UTextureRenderTarget2D* seh_mkert(SDK::UObject* world) {
    __try {
        SDK::FLinearColor cc;
        cc.R = 0; cc.G = 0; cc.B = 0; cc.A = 1;
        return SDK::UKismetRenderingLibrary::CreateRenderTarget2D(
            world, 256, 256, SDK::ETextureRenderTargetFormat::RTF_RGBA8, cc, false);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static bool seh_wire(SDK::USceneCaptureComponent2D* c, SDK::UTextureRenderTarget2D* rt) {
    __try { c->TextureTarget = rt; c->FOVAngle = 50.0f; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_teleport(SDK::AActor* a, const SDK::FVector& loc, const SDK::FRotator& rot) {
    __try { a->K2_SetActorLocation(loc, false, nullptr, true); a->K2_SetActorRotation(rot, true); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static SDK::FRotator seh_lookat(const SDK::FVector& s, const SDK::FVector& t, bool& ok) {
    __try { ok = true; return SDK::UKismetMathLibrary::FindLookAtRotation(s, t); }
    __except (EXCEPTION_EXECUTE_HANDLER) { ok = false; return SDK::FRotator{}; }
}
static bool seh_capture(SDK::USceneCaptureComponent2D* c) {
    __try { c->CaptureScene(); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_export(SDK::UObject* world, SDK::UTextureRenderTarget2D* rt,
        const wchar_t* path, const wchar_t* name) {
    __try {
        SDK::FString p(path), n(name);
        SDK::UKismetRenderingLibrary::ExportRenderTarget(world, rt, p, n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_destroy(SDK::AActor* a) {
    __try { a->K2_DestroyActor(); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_release(SDK::UTextureRenderTarget2D* rt) {
    __try { SDK::UKismetRenderingLibrary::ReleaseRenderTarget2D(rt); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void set_active(bool on) { g_active.store(on); }
void set_target(float x, float y, float z) {
    std::lock_guard<std::mutex> lk(g_tgtMtx);
    g_tx = x; g_ty = y; g_tz = z;
    g_hasTarget = true;
}

static bool ensure_all() {
    if (IS_VALID_PTR(g_cap) && IS_VALID_PTR(g_rt)) return true;
    auto* world = seh_world();
    if (!IS_VALID_PTR(world)) return false;
    SDK::UClass* cls = seh_cap_static();
    if (!IS_VALID_PTR(cls)) return false;
    SDK::FTransform tf{};
    float* q = reinterpret_cast<float*>(&tf);
    q[0] = q[1] = q[2] = 0.0f; q[3] = 1.0f;
    tf.Translation.X = 0; tf.Translation.Y = 0; tf.Translation.Z = -5000;
    tf.Scale3D.X = 1; tf.Scale3D.Y = 1; tf.Scale3D.Z = 1;
    auto* a = seh_begin(world, cls, tf);
    if (!IS_VALID_PTR(a) || !seh_finish(a, tf)) return false;
    auto* comp = seh_comp(static_cast<SDK::ASceneCapture2D*>(a));
    if (!IS_VALID_PTR(comp)) return false;
    auto* rt = seh_mkert(world);
    if (!IS_VALID_PTR(rt) || !seh_wire(comp, rt)) return false;
    g_cap = a;
    g_rt = rt;
    return true;
}

static std::vector<uint8_t> decode_png(const wchar_t* file, int& w, int& h) {
    std::vector<uint8_t> out;
    w = h = 0;
    static bool coInit = false;
    if (!coInit) {
        coInit = true;
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    }
    IWICImagingFactory* fac = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&fac))) || !fac)
        return out;
    IWICBitmapDecoder* dec = nullptr;
    HRESULT hr = fac->CreateDecoderFromFilename(file, nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnDemand, &dec);
    if (SUCCEEDED(hr) && dec) {
        IWICBitmapFrameDecode* fr = nullptr;
        if (SUCCEEDED(dec->GetFrame(0, &fr)) && fr) {
            UINT sw = 0, sh = 0;
            if (SUCCEEDED(fr->GetSize(&sw, &sh)) && sw > 0 && sh > 0 && sw <= 512 && sh <= 512) {
                WICPixelFormatGUID fmt{};
                if (SUCCEEDED(fr->GetPixelFormat(&fmt))) {
                    GUID want = GUID_WICPixelFormat32bppBGRA;
                    IWICFormatConverter* cv = nullptr;
                    IWICBitmapSource* src = fr;
                    src->AddRef();
                    if (!IsEqualGUID(fmt, want)) {
                        if (SUCCEEDED(fac->CreateFormatConverter(&cv)) && cv) {
                            if (SUCCEEDED(cv->Initialize(fr, want, WICBitmapDitherTypeNone,
                                    nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
                                fr->Release();
                                src = cv;
                            } else { cv->Release(); cv = nullptr; }
                        }
                        if (!cv) { src->Release(); src = nullptr; }
                    }
                    if (src) {
                        out.resize((size_t)sw * sh * 4);
                        if (SUCCEEDED(src->CopyPixels(nullptr, sw * 4, (UINT)out.size(), out.data()))) {

                            for (size_t i = 0; i < out.size(); i += 4) {
                                uint8_t t = out[i];
                                out[i] = out[i + 2];
                                out[i + 2] = t;
                            }
                            w = (int)sw; h = (int)sh;
                        } else out.clear();
                        src->Release();
                    }
                }
            }
            fr->Release();
        }
        dec->Release();
    }
    fac->Release();
    return out;
}

void tick() {
    if (!g_active.load()) return;
    uint64_t now = GetTickCount64();
    if (now - g_lastMs < 600) return;
    g_lastMs = now;
    if (!ensure_all()) return;
    auto* world = seh_world();
    if (!IS_VALID_PTR(world)) return;
    float tx, ty, tz;
    {
        std::lock_guard<std::mutex> lk(g_tgtMtx);
        if (!g_hasTarget) return;
        tx = g_tx; ty = g_ty; tz = g_tz;
    }
    g_yawDeg += 15.0f;
    if (g_yawDeg >= 360.0f) g_yawDeg -= 360.0f;
    const float R = 0.01745329252f;
    SDK::FVector campos{};
    campos.X = tx + cosf(g_yawDeg * R) * 300.0f;
    campos.Y = ty + sinf(g_yawDeg * R) * 300.0f;
    campos.Z = tz + 90.0f;
    SDK::FVector tgt{};
    tgt.X = tx; tgt.Y = ty; tgt.Z = tz + 60.0f;
    bool ok = false;
    SDK::FRotator rot = seh_lookat(campos, tgt, ok);
    auto* cap = static_cast<SDK::AActor*>(g_cap);
    auto* comp = seh_comp(static_cast<SDK::ASceneCapture2D*>(cap));
    if (!IS_VALID_PTR(comp)) { g_cap = nullptr; return; }
    if (g_evenPhase) {
        std::wstring dir = fileManager::baseDirW();
        if (dir.empty()) return;
        std::wstring wdir = dir + L"\\";

        if (seh_export(world, static_cast<SDK::UTextureRenderTarget2D*>(g_rt),
                wdir.c_str(), L"oar_cap")) {
            std::wstring png = wdir + L"oar_cap.png";
            std::wstring bmp = wdir + L"oar_cap.bmp";
            const wchar_t* cands[2] = { png.c_str(), bmp.c_str() };
            for (auto* f : cands) {
                if (GetFileAttributesW(f) == INVALID_FILE_ATTRIBUTES) continue;
                int w = 0, h = 0;
                auto px = decode_png(f, w, h);
                if (!px.empty()) {
                    std::lock_guard<std::mutex> lk(g_pxMtx);
                    g_px = std::move(px);
                    g_pxW = w; g_pxH = h;
                    g_dirty = true;
                }
                break;
            }
        }
    } else {
        if (ok) seh_teleport(cap, campos, rot);
        seh_capture(comp);
    }
    g_evenPhase = !g_evenPhase;
}

void present_upload() {
    std::vector<uint8_t> px;
    int w = 0, h = 0;
    {
        std::lock_guard<std::mutex> lk(g_pxMtx);
        if (!g_dirty || g_px.empty()) return;
        px = std::move(g_px);
        w = g_pxW; h = g_pxH;
        g_px.clear();
        g_dirty = false;
    }
    auto* sc = reinterpret_cast<IDXGISwapChain*>(dx11::swapchain());
    if (!sc || w <= 0 || h <= 0 || w > 512 || h > 512) return;
    if ((int)px.size() != w * h * 4) return;
    ID3D11Device* dev = nullptr;
    sc->GetDevice(__uuidof(ID3D11Device), (void**)&dev);
    if (!dev) return;
    ID3D11DeviceContext* ctx = nullptr;
    dev->GetImmediateContext(&ctx);
    static int tw = 0, th = 0;
    if (!g_tex || !g_srv || tw != w || th != h) {
        if (g_srv) { g_srv->Release(); g_srv = nullptr; }
        if (g_tex) { g_tex->Release(); g_tex = nullptr; }
        tw = th = 0;
        D3D11_TEXTURE2D_DESC d{};
        d.Width = (UINT)w; d.Height = (UINT)h;
        d.MipLevels = 1; d.ArraySize = 1;
        d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (SUCCEEDED(dev->CreateTexture2D(&d, nullptr, &g_tex)) &&
            SUCCEEDED(dev->CreateShaderResourceView(g_tex, nullptr, &g_srv))) {
            tw = w; th = h;
        }
    }
    if (g_tex && ctx)
        ctx->UpdateSubresource(g_tex, 0, nullptr, px.data(), (UINT)(w * 4), 0);
    if (ctx) ctx->Release();
    dev->Release();
}

void* srv() { return g_srv; }
bool has_image() { return g_srv != nullptr; }

void shutdown() {
    set_active(false);
    if (IS_VALID_PTR(g_cap)) seh_destroy(static_cast<SDK::AActor*>(g_cap));
    if (IS_VALID_PTR(g_rt))
        seh_release(static_cast<SDK::UTextureRenderTarget2D*>(g_rt));
    g_cap = nullptr;
    g_rt = nullptr;
    if (g_srv) { g_srv->Release(); g_srv = nullptr; }
    if (g_tex) { g_tex->Release(); g_tex = nullptr; }
}

}

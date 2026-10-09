#include "pch/pch.h"
#include "render/blur.hpp"

#include <d3d11.h>
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

#include "../../vendor/imgui/imgui.h"
#include "ui/menu.hpp"

namespace blur {

static ID3D11Device* g_dev = nullptr;
static ID3D11VertexShader* g_vs = nullptr;
static ID3D11PixelShader* g_down = nullptr;
static ID3D11PixelShader* g_blur = nullptr;
static ID3D11SamplerState* g_smp = nullptr;
static ID3D11Buffer* g_cb = nullptr;

static ID3D11Texture2D* g_src = nullptr;
static ID3D11ShaderResourceView* g_srcSrv = nullptr;
static ID3D11Texture2D* g_half = nullptr;
static ID3D11RenderTargetView* g_halfRtv = nullptr;
static ID3D11ShaderResourceView* g_halfSrv = nullptr;
static ID3D11Texture2D* g_tmp = nullptr;
static ID3D11RenderTargetView* g_tmpRtv = nullptr;
static ID3D11ShaderResourceView* g_tmpSrv = nullptr;
static UINT g_w = 0, g_h = 0;
static bool g_ok = false;

static float g_alpha = 1.0f;

static float g_radius = 0.5f;

struct CB { float texel[2]; float dir[2]; };

static const char* kVS =
    "void VS(uint id : SV_VertexID, out float4 pos : SV_Position, out float2 uv : TEXCOORD0) {"
    " float2 p = float2((id << 1) & 2, id & 2);"
    " pos = float4(p * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);"
    " uv = p; }";
static const char* kDown =
    "Texture2D t : register(t0); SamplerState s : register(s0);"
    "float4 PS(float4 p : SV_Position, float2 uv : TEXCOORD0) : SV_Target {"
    " return t.Sample(s, uv); }";
static const char* kBlur =
    "cbuffer CB : register(b0) { float2 texel; float2 dir; };"
    "Texture2D t : register(t0); SamplerState s : register(s0);"
    "float4 PS(float4 p : SV_Position, float2 uv : TEXCOORD0) : SV_Target {"
    " float2 d = dir * texel;"
    " float3 c = t.Sample(s, uv).rgb * 0.2270270270;"
    " c += t.Sample(s, uv + d * 1.3846153846).rgb * 0.3162162162;"
    " c += t.Sample(s, uv - d * 1.3846153846).rgb * 0.3162162162;"
    " c += t.Sample(s, uv + d * 3.2307692308).rgb * 0.0702702703;"
    " c += t.Sample(s, uv - d * 3.2307692308).rgb * 0.0702702703;"
    " return float4(c, 1.0); }";

static ID3DBlob* compile(const char* src, const char* entry, const char* model) {
    ID3DBlob* code = nullptr;
    ID3DBlob* err = nullptr;
    HRESULT hr = D3DCompile(src, strlen(src), nullptr, nullptr, nullptr,
        entry, model, D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3,
        0, &code, &err);
    if (err) err->Release();
    if (FAILED(hr)) { if (code) code->Release(); return nullptr; }
    return code;
}

static void release_sized() {
    if (g_srcSrv) { g_srcSrv->Release(); g_srcSrv = nullptr; }
    if (g_src) { g_src->Release(); g_src = nullptr; }
    if (g_halfSrv) { g_halfSrv->Release(); g_halfSrv = nullptr; }
    if (g_halfRtv) { g_halfRtv->Release(); g_halfRtv = nullptr; }
    if (g_half) { g_half->Release(); g_half = nullptr; }
    if (g_tmpSrv) { g_tmpSrv->Release(); g_tmpSrv = nullptr; }
    if (g_tmpRtv) { g_tmpRtv->Release(); g_tmpRtv = nullptr; }
    if (g_tmp) { g_tmp->Release(); g_tmp = nullptr; }
    g_w = g_h = 0;
}

static bool init_static(ID3D11Device* dev) {
    if (g_ok && g_dev == dev) return true;
    shutdown();
    g_dev = dev;
    if (!g_dev) return false;

    ID3DBlob* vsb = compile(kVS, "VS", "vs_5_0");
    ID3DBlob* dnb = compile(kDown, "PS", "ps_5_0");
    ID3DBlob* blb = compile(kBlur, "PS", "ps_5_0");
    if (!vsb || !dnb || !blb) {
        if (vsb) vsb->Release();
        if (dnb) dnb->Release();
        if (blb) blb->Release();
        return false;
    }
    if (FAILED(g_dev->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &g_vs))) {
        vsb->Release(); dnb->Release(); blb->Release(); return false;
    }
    vsb->Release();
    if (FAILED(g_dev->CreatePixelShader(dnb->GetBufferPointer(), dnb->GetBufferSize(), nullptr, &g_down))) {
        dnb->Release(); blb->Release(); return false;
    }
    dnb->Release();
    if (FAILED(g_dev->CreatePixelShader(blb->GetBufferPointer(), blb->GetBufferSize(), nullptr, &g_blur))) {
        blb->Release(); return false;
    }
    blb->Release();

    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxAnisotropy = 1;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(g_dev->CreateSamplerState(&sd, &g_smp))) return false;

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = sizeof(CB);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(g_dev->CreateBuffer(&bd, nullptr, &g_cb))) return false;

    g_ok = true;
    return true;
}

static ID3D11Texture2D* make_tex(UINT w, UINT h, DXGI_FORMAT fmt, UINT bind) {
    D3D11_TEXTURE2D_DESC d{};
    d.Width = w; d.Height = h;
    d.MipLevels = 1; d.ArraySize = 1;
    d.Format = fmt;
    d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = bind;
    ID3D11Texture2D* t = nullptr;
    if (FAILED(g_dev->CreateTexture2D(&d, nullptr, &t))) return nullptr;
    return t;
}

static bool ensure(IDXGISwapChain* sc) {
    ID3D11Texture2D* back = nullptr;
    if (FAILED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back)) || !back)
        return false;
    D3D11_TEXTURE2D_DESC bd{};
    back->GetDesc(&bd);
    UINT w = bd.Width, h = bd.Height;
    DXGI_FORMAT fmt = bd.Format;
    back->Release();
    if (w < 16 || h < 16) return false;
    if (g_src && w == g_w && h == g_h) return true;
    release_sized();
    UINT hw = w / 4, hh = h / 4;
    if (hw < 8) hw = 8;
    if (hh < 8) hh = 8;
    g_src = make_tex(w, h, fmt, D3D11_BIND_SHADER_RESOURCE);
    g_half = make_tex(hw, hh, fmt, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    g_tmp = make_tex(hw, hh, fmt, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    if (!g_src || !g_half || !g_tmp) { release_sized(); return false; }
    if (FAILED(g_dev->CreateShaderResourceView(g_src, nullptr, &g_srcSrv))) { release_sized(); return false; }
    if (FAILED(g_dev->CreateRenderTargetView(g_half, nullptr, &g_halfRtv))) { release_sized(); return false; }
    if (FAILED(g_dev->CreateShaderResourceView(g_half, nullptr, &g_halfSrv))) { release_sized(); return false; }
    if (FAILED(g_dev->CreateRenderTargetView(g_tmp, nullptr, &g_tmpRtv))) { release_sized(); return false; }
    if (FAILED(g_dev->CreateShaderResourceView(g_tmp, nullptr, &g_tmpSrv))) { release_sized(); return false; }
    g_w = w; g_h = h;
    return true;
}

static void set_cb(ID3D11DeviceContext* ctx, float tx, float ty, float dx, float dy) {
    D3D11_MAPPED_SUBRESOURCE m{};
    if (SUCCEEDED(ctx->Map(g_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        CB* c = (CB*)m.pData;
        c->texel[0] = tx; c->texel[1] = ty;
        c->dir[0] = dx; c->dir[1] = dy;
        ctx->Unmap(g_cb, 0);
    }
}

static bool compute(void* swapchain) {
    auto* sc = reinterpret_cast<IDXGISwapChain*>(swapchain);
    if (!sc) return false;
    ID3D11Device* dev = nullptr;
    sc->GetDevice(__uuidof(ID3D11Device), (void**)&dev);
    if (!dev) return false;
    bool devMine = (dev != g_dev);
    if (!init_static(dev)) { dev->Release(); return false; }
    dev->Release();
    if (!ensure(sc)) return false;

    ID3D11DeviceContext* ctx = nullptr;
    g_dev->GetImmediateContext(&ctx);
    if (!ctx) return false;

    ID3D11RenderTargetView* oldRtv = nullptr;
    ID3D11DepthStencilView* oldDsv = nullptr;
    ctx->OMGetRenderTargets(1, &oldRtv, &oldDsv);
    UINT nvp = 1;
    D3D11_VIEWPORT oldVp{};
    ctx->RSGetViewports(&nvp, &oldVp);
    ID3D11VertexShader* oldVs = nullptr;
    ID3D11Buffer* oldVb = nullptr;
    UINT oldStride = 0, oldOff = 0;
    ctx->VSGetShader(&oldVs, nullptr, nullptr);
    ctx->IAGetVertexBuffers(0, 1, &oldVb, &oldStride, &oldOff);
    ID3D11PixelShader* oldPs = nullptr;
    ctx->PSGetShader(&oldPs, nullptr, nullptr);
    ID3D11SamplerState* oldSmp = nullptr;
    ctx->PSGetSamplers(0, 1, &oldSmp);
    ID3D11ShaderResourceView* oldSrv = nullptr;
    ctx->PSGetShaderResources(0, 1, &oldSrv);
    ID3D11Buffer* oldCb = nullptr;
    ctx->PSGetConstantBuffers(0, 1, &oldCb);
    D3D11_PRIMITIVE_TOPOLOGY oldTopo{};
    ctx->IAGetPrimitiveTopology(&oldTopo);
    ID3D11InputLayout* oldIl = nullptr;
    ctx->IAGetInputLayout(&oldIl);

    bool ok = false;
    do {
        ID3D11Texture2D* back = nullptr;
        if (FAILED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back)) || !back) break;
        ctx->CopyResource(g_src, back);
        back->Release();

        ctx->IASetInputLayout(nullptr);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->VSSetShader(g_vs, nullptr, 0);
        ctx->PSSetSamplers(0, 1, &g_smp);

        D3D11_VIEWPORT vp{};
        vp.Width = (float)(g_w / 4); vp.Height = (float)(g_h / 4);
        vp.MaxDepth = 1.0f;
        ctx->RSSetViewports(1, &vp);

        ctx->OMSetRenderTargets(1, &g_halfRtv, nullptr);
        ctx->PSSetShader(g_down, nullptr, 0);
        ctx->PSSetShaderResources(0, 1, &g_srcSrv);
        ctx->Draw(3, 0);

        ID3D11ShaderResourceView* nullSrv = nullptr;
        ctx->PSSetShader(g_blur, nullptr, 0);
        ctx->PSSetConstantBuffers(0, 1, &g_cb);
        float tx = (1.0f / (g_w / 4.0f)) * g_radius;
        float ty = (1.0f / (g_h / 4.0f)) * g_radius;
        set_cb(ctx, tx, ty, 1.0f, 0.0f);
        ctx->OMSetRenderTargets(1, &g_tmpRtv, nullptr);
        ID3D11ShaderResourceView* s0 = g_halfSrv;
        ctx->PSSetShaderResources(0, 1, &s0);
        ctx->Draw(3, 0);
        ctx->PSSetShaderResources(0, 1, &nullSrv);
        set_cb(ctx, tx, ty, 0.0f, 1.0f);
        ctx->OMSetRenderTargets(1, &g_halfRtv, nullptr);
        ID3D11ShaderResourceView* s1 = g_tmpSrv;
        ctx->PSSetShaderResources(0, 1, &s1);
        ctx->Draw(3, 0);
        ctx->PSSetShaderResources(0, 1, &nullSrv);
        ok = true;
    } while (0);

    ctx->OMSetRenderTargets(1, &oldRtv, oldDsv);
    if (nvp) ctx->RSSetViewports(nvp, &oldVp);
    ctx->VSSetShader(oldVs, nullptr, 0);
    ctx->PSSetShader(oldPs, nullptr, 0);
    if (oldSmp) ctx->PSSetSamplers(0, 1, &oldSmp);
    ctx->PSSetShaderResources(0, 1, &oldSrv);
    if (oldCb) ctx->PSSetConstantBuffers(0, 1, &oldCb);
    ctx->IASetPrimitiveTopology(oldTopo);
    ctx->IASetInputLayout(oldIl);
    if (oldRtv) oldRtv->Release();
    if (oldDsv) oldDsv->Release();
    if (oldVs) oldVs->Release();
    if (oldVb) oldVb->Release();
    if (oldPs) oldPs->Release();
    if (oldSmp) oldSmp->Release();
    if (oldSrv) oldSrv->Release();
    if (oldCb) oldCb->Release();
    if (oldIl) oldIl->Release();

    ctx->Release();
    return ok;
}

bool backdrop(void* swapchain) {
    if (!compute(swapchain) || !g_halfSrv) return false;
    ImVec2 vp2 = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddImage(
        (ImTextureID)g_halfSrv, ImVec2(0, 0), vp2, ImVec2(0, 0), ImVec2(1, 1),
        IM_COL32(255, 255, 255, (int)(255 * g_alpha)));
    return true;
}

bool backdrop_region(void* swapchain, float x, float y, float w, float h) {
    if (!compute(swapchain) || !g_halfSrv || !g_w || !g_h) return false;
    if (w <= 0 || h <= 0) return false;
    ImVec2 uv0(x / (float)g_w, y / (float)g_h);
    ImVec2 uv1((x + w) / (float)g_w, (y + h) / (float)g_h);
    ImGui::GetBackgroundDrawList()->AddImage((ImTextureID)g_halfSrv,
        ImVec2(x, y), ImVec2(x + w, y + h), uv0, uv1,
        IM_COL32(255, 255, 255, (int)(255 * g_alpha)));
    return true;
}

bool ready() { return g_ok && g_halfSrv; }
void set_radius(float r) { g_radius = r < 0.05f ? 0.05f : (r > 1.5f ? 1.5f : r); }
float radius() { return g_radius; }

void set_alpha(float a) { g_alpha = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a); }
float alpha() { return g_alpha; }

void on_resize() { release_sized(); }

void shutdown() {
    release_sized();
    if (g_cb) { g_cb->Release(); g_cb = nullptr; }
    if (g_smp) { g_smp->Release(); g_smp = nullptr; }
    if (g_vs) { g_vs->Release(); g_vs = nullptr; }
    if (g_down) { g_down->Release(); g_down = nullptr; }
    if (g_blur) { g_blur->Release(); g_blur = nullptr; }
    g_dev = nullptr;
    g_ok = false;
}

}

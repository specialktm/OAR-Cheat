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

static int g_computeFrame = -1;

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
static const char* kGlass =
    "cbuffer CB : register(b0) { float2 bufSize; float2 rectMin; float2 rectSize; float radius; float refr; float4 tint; };"
    "Texture2D t : register(t0); SamplerState s : register(s0);"
    "float sdBox(float2 p) {"
    " float2 c = rectSize * 0.5;"
    " float2 q = abs(p - c) - c + radius;"
    " return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius; }"
    "float4 PS(float4 p : SV_Position) : SV_Target {"
    " float2 lp = p.xy - rectMin;"
    " float sd = sdBox(lp);"
    " float e = 1.0;"
    " float2 g = float2(sdBox(lp + float2(e, 0.0)) - sdBox(lp - float2(e, 0.0)), sdBox(lp + float2(0.0, e)) - sdBox(lp - float2(0.0, e)));"
    " float2 n = g / max(length(g), 0.0001);"
    " float rim = 1.0 - smoothstep(0.0, 7.0, abs(sd));"
    " float2 fuv = (p.xy + n * rim * refr) / bufSize;"
    " float3 col = t.Sample(s, fuv).rgb * tint.rgb;"
    " float spec = pow(1.0 - smoothstep(0.0, 3.0, abs(sd)), 2.0);"
    " col += spec * 0.16;"
    " float top = (1.0 - smoothstep(0.0, rectSize.y * 0.5, lp.y)) * (1.0 - smoothstep(0.0, 1.5, sd));"
    " col += top * 0.04;"
    " float alpha = (1.0 - smoothstep(0.0, 1.5, sd)) * tint.a;"
    " return float4(col, alpha); }";

struct GCB { float bufSize[2]; float rectMin[2]; float rectSize[2]; float radius; float refr; float tint[4]; };
struct GlassRect { float x, y, w, h, r; };

static ID3D11PixelShader* g_glass = nullptr;
static ID3D11Buffer* g_gcb = nullptr;
static ID3D11BlendState* g_blend = nullptr;
static ID3D11RasterizerState* g_rs = nullptr;
static std::vector<GlassRect> g_glassRects;

static void release_glass() {
    if (g_glass) { g_glass->Release(); g_glass = nullptr; }
    if (g_gcb) { g_gcb->Release(); g_gcb = nullptr; }
    if (g_blend) { g_blend->Release(); g_blend = nullptr; }
    if (g_rs) { g_rs->Release(); g_rs = nullptr; }
    g_glassRects.clear();
}

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

    ID3DBlob* glb = compile(kGlass, "PS", "ps_5_0");
    if (!glb) return false;
    if (FAILED(g_dev->CreatePixelShader(glb->GetBufferPointer(), glb->GetBufferSize(), nullptr, &g_glass))) {
        glb->Release(); return false;
    }
    glb->Release();

    D3D11_BUFFER_DESC gd{};
    gd.ByteWidth = sizeof(GCB);
    gd.Usage = D3D11_USAGE_DYNAMIC;
    gd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    gd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(g_dev->CreateBuffer(&gd, nullptr, &g_gcb))) return false;

    D3D11_BLEND_DESC bld{};
    bld.RenderTarget[0].BlendEnable = TRUE;
    bld.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bld.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bld.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bld.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bld.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bld.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bld.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(g_dev->CreateBlendState(&bld, &g_blend))) return false;

    D3D11_RASTERIZER_DESC rsd{};
    rsd.FillMode = D3D11_FILL_SOLID;
    rsd.CullMode = D3D11_CULL_NONE;
    rsd.ScissorEnable = TRUE;
    rsd.DepthClipEnable = TRUE;
    if (FAILED(g_dev->CreateRasterizerState(&rsd, &g_rs))) return false;

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
    int frame = ImGui::GetFrameCount();
    if (frame == g_computeFrame && g_halfSrv) return true;
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
        g_computeFrame = frame;
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

bool ready() { return g_ok && g_halfSrv && g_glass && g_gcb && g_blend && g_rs; }
void set_radius(float r) { g_radius = r < 0.05f ? 0.05f : (r > 1.5f ? 1.5f : r); }
float radius() { return g_radius; }

float shell_alpha() { return (g_ok && g_halfSrv && g_glass && g_gcb && g_blend && g_rs) ? 0.55f : 1.0f; }

void glass_rect(float x, float y, float w, float h, float r) {
    if (w <= 0.0f || h <= 0.0f) return;
    if (g_glassRects.size() > 256) return;
    GlassRect gr{ x, y, w, h, r };
    g_glassRects.push_back(gr);
}

static void set_gcb(ID3D11DeviceContext* ctx, const GlassRect& gr) {
    D3D11_MAPPED_SUBRESOURCE m{};
    if (SUCCEEDED(ctx->Map(g_gcb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        GCB* c = (GCB*)m.pData;
        c->bufSize[0] = (float)g_w; c->bufSize[1] = (float)g_h;
        c->rectMin[0] = gr.x; c->rectMin[1] = gr.y;
        c->rectSize[0] = gr.w; c->rectSize[1] = gr.h;
        c->radius = gr.r;
        c->refr = 14.0f;
        c->tint[0] = 0.80f; c->tint[1] = 0.84f; c->tint[2] = 0.90f; c->tint[3] = 1.0f;
        ctx->Unmap(g_gcb, 0);
    }
}

void glass_flush(void* rtv) {
    if (g_glassRects.empty() || !g_dev || !g_glass || !g_gcb || !g_blend || !g_rs || !g_halfSrv || !g_w || !g_h)
        { g_glassRects.clear(); return; }
    ID3D11DeviceContext* ctx = nullptr;
    g_dev->GetImmediateContext(&ctx);
    if (!ctx) { g_glassRects.clear(); return; }
    ID3D11RenderTargetView* oldRtv = nullptr;
    ID3D11DepthStencilView* oldDsv = nullptr;
    ctx->OMGetRenderTargets(1, &oldRtv, &oldDsv);
    UINT nvp = 1;
    D3D11_VIEWPORT oldVp{};
    ctx->RSGetViewports(&nvp, &oldVp);
    UINT nnr = 0;
    ctx->RSGetScissorRects(&nnr, nullptr);
    D3D11_RECT oldSr[8]{};
    if (nnr > 8) nnr = 8;
    ctx->RSGetScissorRects(&nnr, oldSr);
    ID3D11VertexShader* oldVs = nullptr;
    ctx->VSGetShader(&oldVs, nullptr, nullptr);
    ID3D11Buffer* oldVb = nullptr;
    UINT oldStride = 0, oldOff = 0;
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
    ID3D11RasterizerState* oldRs = nullptr;
    ctx->RSGetState(&oldRs);
    float oldBlend[4]{};
    UINT oldMask = 0;
    ID3D11BlendState* oldBl = nullptr;
    ctx->OMGetBlendState(&oldBl, oldBlend, &oldMask);

    ID3D11RenderTargetView* target = reinterpret_cast<ID3D11RenderTargetView*>(rtv);
    ctx->OMSetRenderTargets(1, &target, nullptr);
    D3D11_VIEWPORT vp{};
    vp.Width = (float)g_w; vp.Height = (float)g_h;
    vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);
    ctx->RSSetState(g_rs);
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(g_vs, nullptr, 0);
    ctx->PSSetShader(g_glass, nullptr, 0);
    ctx->PSSetSamplers(0, 1, &g_smp);
    ctx->PSSetConstantBuffers(0, 1, &g_gcb);
    float bf[4] = { 0, 0, 0, 0 };
    ctx->OMSetBlendState(g_blend, bf, 0xFFFFFFFF);
    ID3D11ShaderResourceView* srv = g_halfSrv;
    ctx->PSSetShaderResources(0, 1, &srv);
    for (size_t i = 0; i < g_glassRects.size(); ++i) {
        const GlassRect& gr = g_glassRects[i];
        LONG x0 = (LONG)(gr.x < 0.0f ? 0.0f : gr.x);
        LONG y0 = (LONG)(gr.y < 0.0f ? 0.0f : gr.y);
        LONG x1 = (LONG)(gr.x + gr.w > (float)g_w ? (float)g_w : gr.x + gr.w);
        LONG y1 = (LONG)(gr.y + gr.h > (float)g_h ? (float)g_h : gr.y + gr.h);
        if (x1 <= x0 || y1 <= y0) continue;
        D3D11_RECT sr{ x0, y0, x1, y1 };
        ctx->RSSetScissorRects(1, &sr);
        set_gcb(ctx, gr);
        ctx->Draw(3, 0);
    }
    ID3D11ShaderResourceView* nullSrv = nullptr;
    ctx->PSSetShaderResources(0, 1, &nullSrv);
    ctx->OMSetRenderTargets(1, &oldRtv, oldDsv);
    if (nvp) ctx->RSSetViewports(nvp, &oldVp);
    ctx->RSSetScissorRects(nnr, oldSr);
    ctx->RSSetState(oldRs);
    ctx->OMSetBlendState(oldBl, oldBlend, oldMask);
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
    if (oldRs) oldRs->Release();
    if (oldBl) oldBl->Release();
    g_glassRects.clear();
}
void set_radius(float r) { g_radius = r < 0.05f ? 0.05f : (r > 1.5f ? 1.5f : r); }
float radius() { return g_radius; }

void set_alpha(float a) { g_alpha = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a); }
float alpha() { return g_alpha; }

void on_resize() { release_sized(); }

void shutdown() {
    release_glass();
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

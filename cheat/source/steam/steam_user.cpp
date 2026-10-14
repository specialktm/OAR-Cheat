#include "pch/pch.h"
#include "steam/steam_user.hpp"
#include "render/dx11_hook.hpp"

#include <psapi.h>
#include <mutex>
#include <vector>

#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "psapi.lib")

using Fn_Accessor = void* (*)();
using Fn_PersonaName = const char* (*)(void*);
using Fn_SteamID = unsigned long long (*)(void*);
using Fn_Avatar = int (*)(void*, unsigned long long);
using Fn_ImageSize = bool (*)(void*, int, unsigned int*, unsigned int*);
using Fn_ImageRGBA = bool (*)(void*, int, void*, int);

static void* s_friends = nullptr;
static void* s_user = nullptr;
static void* s_utils = nullptr;
static Fn_PersonaName s_getName = nullptr;
static Fn_SteamID s_getID = nullptr;
static Fn_Avatar s_getAvatar = nullptr;
static Fn_Avatar s_getAvatarM = nullptr;
static Fn_ImageSize s_imgSize = nullptr;
static Fn_ImageRGBA s_imgRGBA = nullptr;
static std::atomic<bool> s_resolved{ false };

static bool s_SteamAPI_Init = false;

static HMODULE find_steam_module() {

    HMODULE mods[512];
    DWORD needed = 0;
    if (!EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &needed))
        return nullptr;
    DWORD count = needed / sizeof(HMODULE);
    if (count > 512) count = 512;
    for (DWORD i = 0; i < count; ++i) {
        HMODULE h = mods[i];
        if (!h) continue;
        if (GetProcAddress(h, "SteamAPI_SteamFriends_v017") ||
            GetProcAddress(h, "SteamAPI_SteamFriends") ||
            GetProcAddress(h, "SteamAPI_Init"))
            return h;
    }
    return nullptr;
}

static void* accessor(HMODULE h, const char* base) {
    if (auto p = GetProcAddress(h, base)) return ((Fn_Accessor)p)();
    char name[64];
    for (int v = 1; v <= 40; ++v) {
        snprintf(name, sizeof(name), "%s_v%03d", base, v);
        if (auto q = GetProcAddress(h, name)) return ((Fn_Accessor)q)();
    }
    return nullptr;
}

template<typename T>
static T method(HMODULE h, const char* name) {
    if (auto p = GetProcAddress(h, name)) return (T)p;
    return nullptr;
}

static void resolve() {
    if (s_resolved.load()) return;
    HMODULE h = find_steam_module();
    if (!h) { s_resolved.store(true); return; }
    s_SteamAPI_Init = GetProcAddress(h, "SteamAPI_Init") != nullptr;
    s_friends = accessor(h, "SteamAPI_SteamFriends");
    s_user = accessor(h, "SteamAPI_SteamUser");
    s_utils = accessor(h, "SteamAPI_SteamUtils");
    s_getName = method<Fn_PersonaName>(h, "SteamAPI_ISteamFriends_GetPersonaName");
    s_getID = method<Fn_SteamID>(h, "SteamAPI_ISteamUser_GetSteamID");
    s_getAvatar = method<Fn_Avatar>(h, "SteamAPI_ISteamFriends_GetLargeFriendAvatar");
    s_getAvatarM = method<Fn_Avatar>(h, "SteamAPI_ISteamFriends_GetMediumFriendAvatar");
    s_imgSize = method<Fn_ImageSize>(h, "SteamAPI_ISteamUtils_GetImageSize");
    s_imgRGBA = method<Fn_ImageRGBA>(h, "SteamAPI_ISteamUtils_GetImageRGBA");
    s_resolved.store(true);
}

static char s_name[64] = { 0 };
static char s_id[24] = { 0 };
static std::atomic<bool> s_nameReady{ false };

static std::mutex s_mtx;
static std::vector<unsigned char> s_px;
static unsigned int s_pw = 0, s_ph = 0;
static bool s_pending = false;

static ID3D11Texture2D* s_tex = nullptr;
static ID3D11ShaderResourceView* s_srv = nullptr;

static void sanitize_ascii(char* dst, size_t cap) {
    size_t w = 0;
    for (size_t r = 0; dst[r] && w + 1 < cap; ++r) {
        unsigned char c = (unsigned char)dst[r];
        if (c >= 32 && c < 127)
            dst[w++] = (char)c;
    }
    dst[w] = 0;
    if (!w)
        strcpy_s(dst, cap, "player");
}

static void fallback_name() {
    DWORD n = (DWORD)sizeof(s_name);
    if (!GetUserNameA(s_name, &n) || n == 0)
        strcpy_s(s_name, "player");
    else if (n < sizeof(s_name))
        s_name[n] = 0;
    sanitize_ascii(s_name, sizeof(s_name));
    s_nameReady.store(true);
}

static bool s_gotAvatar = false;
static int s_avatarTries = 0;

static bool grab_avatar(int tries) {
    if (s_gotAvatar) return true;
    if (!s_friends || !s_user || !s_utils) return false;
    if (!s_getID || !s_getAvatar || !s_imgSize || !s_imgRGBA) return false;
    unsigned long long id = s_getID(s_user);
    if (!id) return false;

    int handle = -1;
    for (int i = 0; i < tries && handle < 0; ++i) {
        handle = s_getAvatar(s_friends, id);
        if (handle < 0 && s_getAvatarM)
            handle = s_getAvatarM(s_friends, id);
        if (handle < 0 && i + 1 < tries) Sleep(150);
    }
    if (handle < 0) return false;
    unsigned int w = 0, h = 0;
    if (!s_imgSize(s_utils, handle, &w, &h) || !w || !h || w > 1024 || h > 1024)
        return false;
    size_t bytes = (size_t)w * h * 4;
    std::vector<unsigned char> px(bytes);
    if (!s_imgRGBA(s_utils, handle, px.data(), (int)bytes)) return false;
    std::lock_guard<std::mutex> lk(s_mtx);
    s_px.swap(px);
    s_pw = w; s_ph = h;
    s_pending = true;
    s_gotAvatar = true;
    return true;
}

void steamServiceHelper::fetch() {
    resolve();    if (!s_resolved.load() || !s_friends || !s_getName) { fallback_name(); return; }
    const char* pn = s_getName(s_friends);
    if (pn && pn[0]) {
        strncpy_s(s_name, pn, _TRUNCATE);
        sanitize_ascii(s_name, sizeof(s_name));
        s_nameReady.store(true);
    } else {
        fallback_name();
    }
    if (s_user && s_getID) {
        unsigned long long id = s_getID(s_user);
        if (id) snprintf(s_id, sizeof(s_id), "%llu", id);
    }
    grab_avatar(10);
    if (!s_gotAvatar) ++s_avatarTries;
}

void steamServiceHelper::retry_avatar() {
    if (s_gotAvatar || !s_resolved.load()) return;
    grab_avatar(1);
    if (!s_gotAvatar) ++s_avatarTries;
}

bool steamServiceHelper::avatar_failed() {
    return !s_gotAvatar && s_avatarTries >= 6;
}

void steamServiceHelper::render_upload() {
    std::vector<unsigned char> px;
    unsigned int w = 0, h = 0;
    {
        std::lock_guard<std::mutex> lk(s_mtx);
        if (!s_pending || s_px.empty()) return;
        px.swap(s_px);
        w = s_pw; h = s_ph;
        s_pending = false;
    }
    auto* dev = static_cast<ID3D11Device*>(dx11::device());
    if (!dev || !w || !h) return;
    if (s_srv) { s_srv->Release(); s_srv = nullptr; }
    if (s_tex) { s_tex->Release(); s_tex = nullptr; }
    D3D11_TEXTURE2D_DESC d{};
    d.Width = w; d.Height = h;
    d.MipLevels = 1; d.ArraySize = 1;
    d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = px.data();
    sd.SysMemPitch = w * 4;
    if (FAILED(dev->CreateTexture2D(&d, &sd, &s_tex)) || !s_tex) return;
    dev->CreateShaderResourceView(s_tex, nullptr, &s_srv);
}

const char* steamServiceHelper::name() {
    if (!s_nameReady.load()) return "player";
    return s_name[0] ? s_name : "player";
}

const char* steamServiceHelper::id_text() { return s_id; }

void* steamServiceHelper::avatar() { return s_srv; }
bool steamServiceHelper::resolved() { return s_resolved.load(); }

void steamServiceHelper::shutdown() {
    std::lock_guard<std::mutex> lk(s_mtx);
    s_px.clear(); s_pending = false;
    if (s_srv) { s_srv->Release(); s_srv = nullptr; }
    if (s_tex) { s_tex->Release(); s_tex = nullptr; }
}

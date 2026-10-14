#include "pch/pch.h"
#include "features/oar_lua.hpp"
#include "features/entities.hpp"
#include "features/esp.hpp"
#include "features/manager.hpp"
#include "features/exploits.hpp"
#include "features/lua_draw.hpp"
#include "render/dx11_hook.hpp"
#include "util/files.hpp"
#include "lua.h"
#include "lualib.h"
#include "luacode.h"
#include "Luau/Compiler.h"
#include <winhttp.h>

#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>

namespace lua
{
namespace
{
struct live_t
{
    std::string name;
    lua_State* L = nullptr;
    std::atomic<bool> kill{ false };
    std::atomic<bool> done{ false };
    std::thread worker;
    struct task_entry
    {
        lua_State* co = nullptr;
        int argc = 0;
        bool started = false;
        bool dead = false;
        int anchor = 0;
        std::chrono::steady_clock::time_point wake{};
    };
    struct sched_t
    {
        std::vector<task_entry> entries;
        int next_anchor = 0;
        std::chrono::steady_clock::time_point last_pass{};
        struct frame_conn { int id = 0; bool dead = false; };
        struct player_conn { int id = 0; bool added = true; bool dead = false; };
        struct mouse_conn { int id = 0; int kind = 0; bool dead = false; };
        std::vector<frame_conn> frame_conns;
        std::vector<player_conn> player_conns;
        std::vector<mouse_conn> mouse_conns;
        std::unordered_set<int> dead_conns;
        int next_conn = 0;
        std::unordered_set<std::string> known_players;
        std::uint64_t last_generation = 0;
        bool players_primed = false;
        bool mouse_primed = false;
        bool mouse_l = false, mouse_r = false;
        double mouse_x = 0.0, mouse_y = 0.0;
    };
    std::unique_ptr<sched_t> sched = std::make_unique<sched_t>();
};

inline std::chrono::steady_clock::time_point wake_in(double secs)
{
    if (!(secs > 0.0))
        secs = 0.0;
    return std::chrono::steady_clock::now() +
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(secs));
}

live_t* self_of(lua_State* L)
{
    lua_Callbacks* cb = lua_callbacks(L);
    return cb ? (live_t*)cb->userdata : nullptr;
}

std::mutex mtx;
std::unordered_map<std::string, std::shared_ptr<live_t>> live;
std::vector<std::string> output;
constexpr std::size_t OUTPUT_CAP = 400;

void register_dvec_meta(lua_State* L);
void register_drawing_meta(lua_State* L);
int l_loadstring(lua_State* L);
int l_rs_connect(lua_State* L);
int l_player_conn(lua_State* L);
int l_get_mouse(lua_State* L);
int l_mouse_index(lua_State* L);
void push_vec3(lua_State* L, float x, float y, float z);
void hold_fn(lua_State* L, int id);
void drop_fn(lua_State* L, int id);
void push_fn(lua_State* L, int id);
void fire_callback(lua_State* L, live_t* sc, int fn_id, int nargs);

struct LSnapP { char name[32] = {}; int hp = 0, maxhp = 100; float x = 0, y = 0, z = 0, dist = 0; bool downed = false, vis = true, self = false; };
struct LSnapG { float x = 0, y = 0, z = 0; int hp = 0; float dist = 0; bool dead = false, alerted = false, armed = false, vis = true, guard = false; };
struct LSnapL { char label[48] = {}; float x = 0, y = 0, z = 0; int value = 0; float dist = 0; bool money = false; };
struct LSnap { std::vector<LSnapP> ps; std::vector<LSnapG> gs; std::vector<LSnapL> ls; };
std::mutex slock;
LSnap g_snap;
std::atomic<std::uint64_t> g_gen{ 0 };
std::uint64_t snap_gen()
{
    return g_gen.load(std::memory_order_acquire);
}

void push_out_locked(const std::string& line)
{
    output.push_back(line);
    while (output.size() > OUTPUT_CAP)
        output.erase(output.begin());
}

void push_out(const std::string& line)
{
    std::lock_guard<std::mutex> lock(mtx);
    push_out_locked(line);
}

bool killed(lua_State* L)
{
    live_t* sc = self_of(L);
    if (sc && sc->kill.load(std::memory_order_acquire)) return true;
    return !programRunning.load(std::memory_order_acquire);
}

void interrupt_hook(lua_State* L, int)
{
    if (killed(L))
        luaL_errorL(L, "stopped");
}

int l_wait(lua_State* L)
{
    const double secs = luaL_optnumber(L, 1, 0.0);
    live_t* sc = self_of(L);
    const int is_main = lua_pushthread(L);
    lua_pop(L, 1);
    if (!is_main && sc)
    {
        for (auto& e : sc->sched->entries)
        {
            if (e.co == L && !e.dead)
            {
                e.wake = wake_in(secs);
                break;
            }
        }
        return lua_yield(L, 0);
    }
    const auto until = std::chrono::steady_clock::now() +
        std::chrono::duration<double>(secs < 0.0 ? 0.0 : secs);
    while (std::chrono::steady_clock::now() < until)
    {
        if (killed(L))
            luaL_errorL(L, "stopped");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return 0;
}

int anchor_thread(lua_State* L, live_t* sc)
{
    lua_getfield(L, LUA_REGISTRYINDEX, "__oar_threads");
    if (!lua_istable(L, -1))
    {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, "__oar_threads");
    }
    lua_pushvalue(L, -2);
    const int slot = ++sc->sched->next_anchor;
    lua_rawseti(L, -2, slot);
    lua_pop(L, 1);
    return slot;
}

void release_thread(lua_State* L, int slot)
{
    if (slot <= 0)
        return;
    lua_getfield(L, LUA_REGISTRYINDEX, "__oar_threads");
    if (lua_istable(L, -1))
    {
        lua_pushnil(L);
        lua_rawseti(L, -2, slot);
    }
    lua_pop(L, 1);
}

int l_task_spawn(lua_State* L)
{
    const int fnidx = lua_isfunction(L, 1) ? 1 : 2;
    luaL_checktype(L, fnidx, LUA_TFUNCTION);
    live_t* sc = self_of(L);
    if (!sc)
        luaL_errorL(L, "task.spawn outside a script");
    const int nargs = lua_gettop(L) - fnidx;
    lua_State* co = lua_newthread(L);
    const int slot = anchor_thread(L, sc);
    lua_pushvalue(L, fnidx);
    lua_xmove(L, co, 1);
    for (int i = 0; i < nargs; i++)
    {
        lua_pushvalue(L, fnidx + 1 + i);
        lua_xmove(L, co, 1);
    }
    live_t::task_entry e;
    e.co = co;
    e.argc = nargs;
    e.anchor = slot;
    e.wake = std::chrono::steady_clock::now();
    sc->sched->entries.push_back(e);
    return 1;
}

int l_task_defer(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);
    live_t* sc = self_of(L);
    if (!sc)
        luaL_errorL(L, "task.defer outside a script");
    const int nargs = lua_gettop(L) - 1;
    lua_State* co = lua_newthread(L);
    const int slot = anchor_thread(L, sc);
    lua_pushvalue(L, 1);
    lua_xmove(L, co, 1);
    for (int i = 0; i < nargs; i++)
    {
        lua_pushvalue(L, 2 + i);
        lua_xmove(L, co, 1);
    }
    live_t::task_entry e;
    e.co = co;
    e.argc = nargs;
    e.anchor = slot;
    e.wake = std::chrono::steady_clock::now();
    sc->sched->entries.push_back(e);
    return 1;
}

int l_task_delay(lua_State* L)
{
    const double secs = luaL_optnumber(L, 1, 0.0);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    live_t* sc = self_of(L);
    if (!sc)
        luaL_errorL(L, "task.delay outside a script");
    const int nargs = lua_gettop(L) - 2;
    lua_State* co = lua_newthread(L);
    const int slot = anchor_thread(L, sc);
    lua_pushvalue(L, 2);
    lua_xmove(L, co, 1);
    for (int i = 0; i < nargs; i++)
    {
        lua_pushvalue(L, 3 + i);
        lua_xmove(L, co, 1);
    }
    live_t::task_entry e;
    e.co = co;
    e.argc = nargs;
    e.anchor = slot;
    e.wake = wake_in(secs);
    sc->sched->entries.push_back(e);
    return 1;
}

int l_task_cancel(lua_State* L)
{
    live_t* sc = self_of(L);
    if (!sc)
        return 0;
    for (auto& e : sc->sched->entries)
    {
        if (e.dead)
            continue;
        lua_pushthread(e.co);
        const int eq = lua_rawequal(L, -1, 1);
        lua_pop(L, 1);
        if (eq)
        {
            e.dead = true;
            break;
        }
    }
    return 0;
}

int l_print(lua_State* L)
{
    const char* tag = lua_tostring(L, lua_upvalueindex(1));
    const int n = lua_gettop(L);
    std::string msg;
    for (int i = 1; i <= n; i++)
    {
        std::size_t len = 0;
        const char* s = luaL_tolstring(L, i, &len);
        if (i > 1)
            msg.push_back('\t');
        msg.append(s ? s : "nil", len);
        lua_pop(L, 1);
    }
    push_out(std::string("[") + (tag ? tag : "?") + "] " + msg);
    return 0;
}

int l_warn(lua_State* L)
{
    const int n = lua_gettop(L);
    std::string msg = "[warn]";
    for (int i = 1; i <= n; i++)
    {
        std::size_t len = 0;
        const char* s = luaL_tolstring(L, i, &len);
        msg.push_back(' ');
        msg.append(s ? s : "nil", len);
        lua_pop(L, 1);
    }
    push_out(msg);
    return 0;
}

int l_notify(lua_State* L)
{
    std::size_t mlen = 0, tlen = 0;
    const char* msg = luaL_checklstring(L, 1, &mlen);
    const char* title = luaL_optlstring(L, 2, "OAR", &tlen);
    push_out(std::string("[") + (title ? std::string(title, tlen) : "OAR") + "] " + (msg ? std::string(msg, mlen) : ""));
    return 0;
}

int l_tick(lua_State* L)
{
    static const auto start = std::chrono::steady_clock::now();
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    lua_pushnumber(L, secs);
    return 1;
}

int l_typeof(lua_State* L)
{
    if (lua_isuserdata(L, 1))
    {
        if (lua_getmetatable(L, 1) != 0)
        {
            lua_getfield(L, -1, "__type");
            if (lua_isstring(L, -1))
            {
                lua_pushvalue(L, -1);
                lua_remove(L, -2);
                lua_remove(L, -2);
                return 1;
            }
            lua_pop(L, 2);
        }
    }
    if (lua_istable(L, 1))
    {
        lua_getfield(L, 1, "x"); const bool nx = lua_isnumber(L, -1) != 0; lua_pop(L, 1);
        lua_getfield(L, 1, "y"); const bool ny = lua_isnumber(L, -1) != 0; lua_pop(L, 1);
        lua_getfield(L, 1, "z"); const bool nz = lua_isnumber(L, -1) != 0; lua_pop(L, 1);
        if (nx && ny && nz)
        {
            lua_pushstring(L, "Vector3");
            return 1;
        }
    }
    lua_pushstring(L, lua_typename(L, lua_type(L, 1)));
    return 1;
}

int l_identifyexecutor(lua_State* L)
{
    lua_pushstring(L, "OAR");
    lua_pushstring(L, "1.0");
    return 2;
}

int l_getfenv(lua_State* L)
{
    (void)L;
    lua_pushvalue(L, LUA_GLOBALSINDEX);
    return 1;
}

int l_setfenv(lua_State* L)
{
    (void)L;
    luaL_errorL(L, "setfenv is not supported (Luau has no per-function environments)");
    return 0;
}

int l_os_clock(lua_State* L)
{
    static const auto start = std::chrono::steady_clock::now();
    lua_pushnumber(L, std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
    return 1;
}

int l_os_time(lua_State* L)
{
    (void)L;
    lua_pushinteger(L, (int)std::time(nullptr));
    return 1;
}

int l_os_difftime(lua_State* L)
{
    lua_pushinteger(L, (int)luaL_checknumber(L, 1) - (int)luaL_checknumber(L, 2));
    return 1;
}

enum { DVT_VEC2 = 1, DVT_VEC3, DVT_COLOR3 };
struct dvec_t
{
    int tag = 0;
    double x = 0.0, y = 0.0, z = 0.0;
};

const char* dvec_meta(int tag)
{
    return tag == DVT_VEC2 ? "OARVec2" : (tag == DVT_VEC3 ? "OARVec3" : "OARColor3");
}

void push_dvec(lua_State* L, int tag, double x, double y, double z, const char* tname)
{
    dvec_t* d = (dvec_t*)lua_newuserdata(L, sizeof(dvec_t));
    d->tag = tag;
    d->x = x;
    d->y = y;
    d->z = z;
    lua_getfield(L, LUA_REGISTRYINDEX, tname);
    lua_setmetatable(L, -2);
}

dvec_t* check_dvec(lua_State* L, int idx, int tag)
{
    dvec_t* d = (dvec_t*)lua_touserdata(L, idx);
    if (!d || d->tag != tag)
        luaL_errorL(L, "datatype mismatch");
    return d;
}

int l_dvec_index(lua_State* L)
{
    dvec_t* d = (dvec_t*)lua_touserdata(L, 1);
    std::size_t len = 0;
    const char* k = lua_tolstring(L, 2, &len);
    const std::string key = k ? std::string(k, len) : std::string();
    if (!d)
    {
        lua_pushnil(L);
        return 1;
    }
    const bool is_col = (d->tag == DVT_COLOR3);
    if ((!is_col && key == "X") || (is_col && key == "R")) { lua_pushnumber(L, d->x); return 1; }
    if ((!is_col && key == "Y") || (is_col && key == "G")) { lua_pushnumber(L, d->y); return 1; }
    if ((!is_col && key == "Z") || (is_col && key == "B")) { lua_pushnumber(L, d->z); return 1; }
    if (key == "Magnitude")
    {
        if (d->tag == DVT_COLOR3) { lua_pushnumber(L, 0.0); return 1; }
        lua_pushnumber(L, std::sqrt(d->x * d->x + d->y * d->y + d->z * d->z));
        return 1;
    }
    if (key == "Unit" && !is_col)
    {
        const double m = std::sqrt(d->x * d->x + d->y * d->y + d->z * d->z);
        if (m > 0.0)
            push_dvec(L, d->tag, d->x / m, d->y / m, d->z / m, dvec_meta(d->tag));
        else
            push_dvec(L, d->tag, 0.0, 0.0, 0.0, dvec_meta(d->tag));
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

int l_dvec_add(lua_State* L) { dvec_t* a = check_dvec(L, 1, ((dvec_t*)lua_touserdata(L, 1))->tag); dvec_t* b = check_dvec(L, 2, a->tag); push_dvec(L, a->tag, a->x + b->x, a->y + b->y, a->z + b->z, dvec_meta(a->tag)); return 1; }
int l_dvec_sub(lua_State* L) { dvec_t* a = check_dvec(L, 1, ((dvec_t*)lua_touserdata(L, 1))->tag); dvec_t* b = check_dvec(L, 2, a->tag); push_dvec(L, a->tag, a->x - b->x, a->y - b->y, a->z - b->z, dvec_meta(a->tag)); return 1; }
int l_dvec_unm(lua_State* L) { dvec_t* a = check_dvec(L, 1, ((dvec_t*)lua_touserdata(L, 1))->tag); push_dvec(L, a->tag, -a->x, -a->y, -a->z, dvec_meta(a->tag)); return 1; }
int l_dvec_eq(lua_State* L) { dvec_t* a = (dvec_t*)lua_touserdata(L, 1); dvec_t* b = (dvec_t*)lua_touserdata(L, 2); lua_pushboolean(L, a && b && a->tag == b->tag && a->x == b->x && a->y == b->y && a->z == b->z); return 1; }
int l_dvec_tostring(lua_State* L)
{
    dvec_t* d = (dvec_t*)lua_touserdata(L, 1);
    char buf[96];
    if (!d) { lua_pushstring(L, "?"); return 1; }
    if (d->tag == DVT_COLOR3) snprintf(buf, sizeof(buf), "%g, %g, %g", d->x, d->y, d->z);
    else if (d->tag == DVT_VEC2) snprintf(buf, sizeof(buf), "%g, %g", d->x, d->y);
    else snprintf(buf, sizeof(buf), "%g, %g, %g", d->x, d->y, d->z);
    lua_pushstring(L, buf);
    return 1;
}
int l_dvec_mul(lua_State* L)
{
    if (lua_isuserdata(L, 1) && lua_isnumber(L, 2)) { dvec_t* a = check_dvec(L, 1, ((dvec_t*)lua_touserdata(L, 1))->tag); const double s = lua_tonumber(L, 2); push_dvec(L, a->tag, a->x * s, a->y * s, a->z * s, dvec_meta(a->tag)); return 1; }
    if (lua_isnumber(L, 1) && lua_isuserdata(L, 2)) { dvec_t* b = check_dvec(L, 2, ((dvec_t*)lua_touserdata(L, 2))->tag); const double s = lua_tonumber(L, 1); push_dvec(L, b->tag, b->x * s, b->y * s, b->z * s, dvec_meta(b->tag)); return 1; }
    dvec_t* a = check_dvec(L, 1, ((dvec_t*)lua_touserdata(L, 1))->tag); dvec_t* b = check_dvec(L, 2, a->tag); push_dvec(L, a->tag, a->x * b->x, a->y * b->y, a->z * b->z, dvec_meta(a->tag)); return 1;
}
int l_dvec_div(lua_State* L)
{
    if (lua_isuserdata(L, 1) && lua_isnumber(L, 2)) { dvec_t* a = check_dvec(L, 1, ((dvec_t*)lua_touserdata(L, 1))->tag); const double s = lua_tonumber(L, 2); push_dvec(L, a->tag, a->x / s, a->y / s, a->z / s, dvec_meta(a->tag)); return 1; }
    dvec_t* a = check_dvec(L, 1, ((dvec_t*)lua_touserdata(L, 1))->tag); dvec_t* b = check_dvec(L, 2, a->tag); push_dvec(L, a->tag, a->x / b->x, a->y / b->y, a->z / b->z, dvec_meta(a->tag)); return 1;
}

template<typename T>
static bool mem_rd(std::uint64_t a, T& o)
{
    __try {
        if (!IS_VALID_PTR((void*)(uintptr_t)a)) return false;
        o = *(T*)(uintptr_t)a;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
template<typename T>
static bool mem_wr(std::uint64_t a, const T& v)
{
    __try {
        if (!IS_VALID_PTR((void*)(uintptr_t)a)) return false;
        *(T*)(uintptr_t)a = v;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

std::uint64_t mem_addr(lua_State* L, int idx)
{
    const double d = luaL_checknumber(L, idx);
    if (!(d > 0.0) || d >= 18446744073709551616.0)
        luaL_errorL(L, "bad address");
    return (std::uint64_t)d;
}

int l_memory_read(lua_State* L)
{
    std::size_t tlen = 0;
    const char* t = luaL_checklstring(L, 1, &tlen);
    const std::string type(t ? t : "", tlen);
    const std::uint64_t addr = mem_addr(L, 2);
    if (type == "float") { float v = 0; if (!mem_rd(addr, v)) return 0; lua_pushnumber(L, v); return 1; }
    if (type == "double" || type == "number") { double v = 0; if (!mem_rd(addr, v)) return 0; lua_pushnumber(L, v); return 1; }
    if (type == "int" || type == "int32" || type == "i32") { std::int32_t v = 0; if (!mem_rd(addr, v)) return 0; lua_pushinteger(L, v); return 1; }
    if (type == "bool" || type == "boolean") { bool v = false; if (!mem_rd(addr, v)) return 0; lua_pushboolean(L, v); return 1; }
    if (type == "byte" || type == "uint8" || type == "u8") { std::uint8_t v = 0; if (!mem_rd(addr, v)) return 0; lua_pushinteger(L, v); return 1; }
    if (type == "string")
    {
        std::string s;
        for (int i = 0; i < 256; i++)
        {
            char c = 0;
            if (!mem_rd(addr + (std::uint64_t)i, c) || c == '\0') break;
            s.push_back(c);
        }
        lua_pushlstring(L, s.data(), s.size());
        return 1;
    }
    luaL_errorL(L, "memory_read: unknown type (float/double/int/bool/byte/string)");
    return 0;
}

int l_memory_write(lua_State* L)
{
    std::size_t tlen = 0;
    const char* t = luaL_checklstring(L, 1, &tlen);
    const std::string type(t ? t : "", tlen);
    const std::uint64_t addr = mem_addr(L, 2);
    if (type == "float") { float v = (float)luaL_checknumber(L, 3); lua_pushboolean(L, mem_wr(addr, v)); return 1; }
    if (type == "double" || type == "number") { double v = luaL_checknumber(L, 3); lua_pushboolean(L, mem_wr(addr, v)); return 1; }
    if (type == "int" || type == "int32" || type == "i32") { std::int32_t v = (std::int32_t)luaL_checkinteger(L, 3); lua_pushboolean(L, mem_wr(addr, v)); return 1; }
    if (type == "bool" || type == "boolean") { bool v = lua_toboolean(L, 3) != 0; lua_pushboolean(L, mem_wr(addr, v)); return 1; }
    if (type == "byte" || type == "uint8" || type == "u8") { std::uint8_t v = (std::uint8_t)luaL_checkinteger(L, 3); lua_pushboolean(L, mem_wr(addr, v)); return 1; }
    luaL_errorL(L, "memory_write: unknown type (float/double/int/bool/byte)");
    return 0;
}

bool http_fetch(const std::string& url, const std::string& body, const std::string& content_type, std::string& out)
{
    const std::string pre = "https://";
    std::string u = url;
    if (u.rfind(pre, 0) == 0)
        u = u.substr(pre.size());
    else if (u.rfind("http://", 0) == 0)
        u = u.substr(7);
    else
        return false;
    const std::size_t slash = u.find('/');
    if (slash == std::string::npos)
        return false;
    const std::wstring host(u.begin(), u.begin() + slash);
    const std::wstring path(u.begin() + slash, u.end());

    HINTERNET ses = WinHttpOpen(L"OARLua/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses)
        return false;
    WinHttpSetTimeouts(ses, 8000, 8000, 8000, 8000);
    HINTERNET con = WinHttpConnect(ses, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!con)
    {
        WinHttpCloseHandle(ses);
        return false;
    }
    const bool post = !body.empty();
    HINTERNET req = WinHttpOpenRequest(con, post ? L"POST" : L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!req)
    {
        WinHttpCloseHandle(con);
        WinHttpCloseHandle(ses);
        return false;
    }
    std::wstring hdrs;
    if (post)
        hdrs = L"Content-Type: " + std::wstring(content_type.begin(), content_type.end()) + L"\r\n";
    bool ok = false;
    if (WinHttpSendRequest(req, hdrs.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : hdrs.c_str(), (DWORD)hdrs.size(),
        post ? (LPVOID)body.data() : WINHTTP_NO_REQUEST_DATA, post ? (DWORD)body.size() : 0, post ? (DWORD)body.size() : 0, 0) &&
        WinHttpReceiveResponse(req, nullptr))
    {
        DWORD avail = 0, got = 0;
        std::vector<char> chunk(16384);
        const std::size_t cap = 4u * 1024u * 1024u;
        while (WinHttpQueryDataAvailable(req, &avail) && avail != 0 && out.size() < cap)
        {
            std::size_t take = avail > 262144u ? 262144u : avail;
            if (take > chunk.size())
                chunk.resize(take);
            if (!WinHttpReadData(req, chunk.data(), (DWORD)take, &got) || got == 0)
                break;
            out.append(chunk.data(), got);
        }
        ok = !out.empty();
    }
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return ok;
}

int l_httpget(lua_State* L)
{
    std::size_t len = 0;
    const char* url = luaL_checklstring(L, 1, &len);
    std::string body;
    if (!http_fetch(std::string(url, len), "", "", body))
    {
        lua_pushnil(L);
        lua_pushstring(L, "httpget failed");
        return 2;
    }
    lua_pushlstring(L, body.data(), body.size());
    return 1;
}

int l_httppost(lua_State* L)
{
    std::size_t ulen = 0, dlen = 0, clen = 0;
    const char* url = luaL_checklstring(L, 1, &ulen);
    const char* data = luaL_checklstring(L, 2, &dlen);
    const char* ctype = luaL_optlstring(L, 3, "application/json", &clen);
    std::string body;
    if (!http_fetch(std::string(url, ulen), std::string(data, dlen), std::string(ctype ? ctype : "", clen), body))
    {
        lua_pushnil(L);
        lua_pushstring(L, "httppost failed");
        return 2;
    }
    lua_pushlstring(L, body.data(), body.size());
    return 1;
}

int l_game_httpget(lua_State* L)
{
    std::size_t len = 0;
    const char* url = nullptr;
    if (lua_type(L, 2) == LUA_TSTRING)
        url = lua_tolstring(L, 2, &len);
    else if (lua_type(L, 1) == LUA_TSTRING)
        url = lua_tolstring(L, 1, &len);
    else
        luaL_errorL(L, "usage: game:HttpGet(url)");
    std::string body;
    if (!url || !http_fetch(std::string(url, len), "", "", body))
    {
        lua_pushnil(L);
        return 1;
    }
    lua_pushlstring(L, body.data(), body.size());
    return 1;
}

int l_worldtoscreen(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    float wv[3] = {};
    const char* keys[3] = { "x", "y", "z" };
    for (int i = 0; i < 3; ++i)
    {
        lua_getfield(L, 1, keys[i]);
        wv[i] = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
    }
    struct WVec { float x, y, z; };
    WVec w{ wv[0], wv[1], wv[2] };
    esp::ScreenPos sp{};
    bool ok = false;
    __try {
        ok = esp::world_to_screen(&w, sp);
    } __except (EXCEPTION_EXECUTE_HANDLER) { ok = false; }
    lua_pushnumber(L, sp.x);
    lua_pushnumber(L, sp.y);
    lua_pushboolean(L, ok && sp.visible);
    return 3;
}

int l_ismouse1pressed(lua_State* L)
{
    (void)L;
    lua_pushboolean(L, (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
    return 1;
}
int l_ismouse2pressed(lua_State* L)
{
    (void)L;
    lua_pushboolean(L, (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
    return 1;
}
int l_isgameactive(lua_State* L)
{
    (void)L;
    bool active = false;
    __try {
        HWND win = (HWND)dx11::window();
        active = win && (GetForegroundWindow() == win);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    lua_pushboolean(L, active);
    return 1;
}
int l_iskeypressed(lua_State* L)
{
    const int vk = (int)luaL_checknumber(L, 1);
    if (vk < 0 || vk > 255)
    {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, (GetAsyncKeyState(vk) & 0x8000) != 0);
    return 1;
}
int l_setgameinput(lua_State* L)
{
    (void)L;
    lua_pushboolean(L, 1);
    return 1;
}

bool mouse_client_pos(double& x, double& y)
{
    POINT p{};
    if (!GetCursorPos(&p))
        return false;
    HWND win = nullptr;
    __try { win = (HWND)dx11::window(); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    if (win)
    {
        POINT c = p;
        if (ScreenToClient(win, &c))
        {
            x = (double)c.x;
            y = (double)c.y;
            return true;
        }
    }
    x = (double)p.x;
    y = (double)p.y;
    return true;
}

void push_mouse(lua_State* L)
{
    lua_newtable(L);
    lua_newtable(L);
    lua_pushcfunction(L, l_mouse_index, "");
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);
}

int l_mouse_index(lua_State* L)
{
    std::size_t len = 0;
    const char* k = lua_tolstring(L, 2, &len);
    const std::string key = k ? std::string(k, len) : std::string();
    if (key == "X" || key == "Y")
    {
        double x = 0, y = 0;
        mouse_client_pos(x, y);
        lua_pushnumber(L, key == "X" ? x : y);
        return 1;
    }
    if (key == "Button1Down" || key == "Button1Up" || key == "Move")
    {
        live_t* sc = self_of(L);
        if (!sc)
            luaL_errorL(L, "mouse event outside a script");
        const int kind = key == "Button1Down" ? 0 : (key == "Button1Up" ? 1 : 2);
        lua_newtable(L);
        lua_pushinteger(L, kind);
        lua_pushcclosure(L, [](lua_State* L) -> int {
            const int k2 = (int)lua_tointeger(L, lua_upvalueindex(1));
            luaL_checktype(L, 1, LUA_TFUNCTION);
            live_t* sc2 = self_of(L);
            if (!sc2)
                luaL_errorL(L, "Connect outside a script");
            lua_pushvalue(L, 1);
            const int id = ++sc2->sched->next_conn;
            hold_fn(L, id);
            live_t::sched_t::mouse_conn mc;
            mc.id = id;
            mc.kind = k2;
            mc.dead = false;
            sc2->sched->mouse_conns.push_back(mc);
            lua_newtable(L);
            lua_pushinteger(L, id);
            lua_pushcclosure(L, [](lua_State* L) -> int {
                live_t* sc3 = self_of(L);
                if (sc3)
                    sc3->sched->dead_conns.insert((int)lua_tointeger(L, lua_upvalueindex(1)));
                return 0;
            }, "", 1);
            lua_setfield(L, -2, "Disconnect");
            return 1;
        }, "", 1);
        lua_setfield(L, -2, "Connect");
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

int l_get_mouse(lua_State* L)
{
    (void)L;
    push_mouse(L);
    return 1;
}

void hold_fn(lua_State* L, int id)
{
    lua_getfield(L, LUA_REGISTRYINDEX, "__oar_fns");
    if (!lua_istable(L, -1))
    {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, "__oar_fns");
    }
    lua_pushvalue(L, -2);
    lua_rawseti(L, -2, id);
    lua_pop(L, 2);
}

void drop_fn(lua_State* L, int id)
{
    lua_getfield(L, LUA_REGISTRYINDEX, "__oar_fns");
    if (lua_istable(L, -1))
    {
        lua_pushnil(L);
        lua_rawseti(L, -2, id);
    }
    lua_pop(L, 1);
}

void push_fn(lua_State* L, int id)
{
    lua_getfield(L, LUA_REGISTRYINDEX, "__oar_fns");
    if (lua_istable(L, -1))
        lua_rawgeti(L, -1, id);
    else
        lua_pushnil(L);
    lua_remove(L, -2);
}

void fire_callback(lua_State* L, live_t* sc, int fn_id, int nargs)
{
    push_fn(L, fn_id);
    if (!lua_isfunction(L, -1))
    {
        lua_pop(L, 1 + nargs);
        return;
    }
    lua_insert(L, -(nargs + 1));
    lua_State* co = lua_newthread(L);
    const int slot = anchor_thread(L, sc);
    lua_insert(L, -(nargs + 2));
    for (int i = 0; i <= nargs; i++)
    {
        lua_pushvalue(L, -(nargs + 1) + i);
        lua_xmove(L, co, 1);
    }
    live_t::task_entry e;
    e.co = co;
    e.argc = nargs;
    e.anchor = slot;
    e.wake = std::chrono::steady_clock::now();
    sc->sched->entries.push_back(std::move(e));
    lua_pop(L, 1 + (nargs + 1));
}

int l_rs_connect(lua_State* L)
{
    const int fnidx = lua_isfunction(L, 1) ? 1 : 2;
    luaL_checktype(L, fnidx, LUA_TFUNCTION);
    live_t* sc = self_of(L);
    if (!sc)
        luaL_errorL(L, "Connect outside a script");
    lua_pushvalue(L, fnidx);
    const int id = ++sc->sched->next_conn;
    hold_fn(L, id);
    sc->sched->frame_conns.push_back({ id, false });
    lua_newtable(L);
    lua_pushinteger(L, id);
    lua_pushcclosure(L, [](lua_State* L) -> int {
        live_t* sc = self_of(L);
        if (sc)
            sc->sched->dead_conns.insert((int)lua_tointeger(L, lua_upvalueindex(1)));
        return 0;
    }, "", 1);
    lua_setfield(L, -2, "Disconnect");
    return 1;
}

int l_player_conn(lua_State* L)
{
    const int added = (int)lua_tointeger(L, lua_upvalueindex(1));
    const int fnidx = lua_isfunction(L, 1) ? 1 : 2;
    luaL_checktype(L, fnidx, LUA_TFUNCTION);
    live_t* sc = self_of(L);
    if (!sc)
        luaL_errorL(L, "Connect outside a script");
    lua_pushvalue(L, fnidx);
    const int id = ++sc->sched->next_conn;
    hold_fn(L, id);
    sc->sched->player_conns.push_back({ id, added != 0, false });
    lua_newtable(L);
    lua_pushinteger(L, id);
    lua_pushcclosure(L, [](lua_State* L) -> int {
        live_t* sc = self_of(L);
        if (sc)
            sc->sched->dead_conns.insert((int)lua_tointeger(L, lua_upvalueindex(1)));
        return 0;
    }, "", 1);
    lua_setfield(L, -2, "Disconnect");
    return 1;
}

int l_player_eq(lua_State* L)
{
    lua_getfield(L, 1, "name");
    std::size_t alen = 0;
    const char* sa = lua_tolstring(L, -1, &alen);
    const std::string a = sa ? std::string(sa, alen) : std::string();
    lua_getfield(L, 2, "name");
    std::size_t blen = 0;
    const char* sb = lua_tolstring(L, -1, &blen);
    const std::string b = sb ? std::string(sb, blen) : std::string();
    lua_pushboolean(L, !a.empty() && a == b);
    return 1;
}

void ensure_player_meta(lua_State* L)
{
    lua_getfield(L, LUA_REGISTRYINDEX, "OARPlayerMeta");
    if (lua_istable(L, -1))
    {
        lua_pop(L, 1);
        return;
    }
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushcfunction(L, l_player_eq, "");
    lua_setfield(L, -2, "__eq");
    lua_pushstring(L, "Instance");
    lua_setfield(L, -2, "__type");
    lua_setfield(L, LUA_REGISTRYINDEX, "OARPlayerMeta");
}

void push_svc_player(lua_State* L, const LSnapP& e)
{
    lua_newtable(L);
    lua_pushstring(L, e.name); lua_setfield(L, -2, "Name");
    lua_pushstring(L, e.name); lua_setfield(L, -2, "DisplayName");
    lua_pushnumber(L, e.hp); lua_setfield(L, -2, "Health");
    lua_pushnumber(L, e.maxhp); lua_setfield(L, -2, "MaxHealth");
    push_vec3(L, e.x, e.y, e.z); lua_setfield(L, -2, "Position");
    lua_pushcfunction(L, l_get_mouse, ""); lua_setfield(L, -2, "GetMouse");
    lua_pushstring(L, e.name); lua_setfield(L, -2, "name");
    lua_pushnumber(L, e.hp); lua_setfield(L, -2, "health");
    push_vec3(L, e.x, e.y, e.z); lua_setfield(L, -2, "pos");
    ensure_player_meta(L);
    lua_getfield(L, LUA_REGISTRYINDEX, "OARPlayerMeta");
    lua_setmetatable(L, -2);
}

int l_players_getplayers(lua_State* L)
{
    (void)L;
    std::vector<LSnapP> copy;
    {
        std::lock_guard<std::mutex> lock(slock);
        copy = g_snap.ps;
    }
    lua_newtable(L);
    int idx = 1;
    for (auto& e : copy)
    {
        if (idx > 200)
            break;
        if (e.self) continue;
        push_svc_player(L, e);
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

void push_players_service(lua_State* L)
{
    LSnapP self{};
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(slock);
        for (auto& e : g_snap.ps) {
            if (e.self) { self = e; found = true; break; }
        }
    }
    if (found) push_svc_player(L, self);
    else { lua_newtable(L); }
    lua_setfield(L, -2, "LocalPlayer");
    lua_pushcfunction(L, l_players_getplayers, "");
    lua_setfield(L, -2, "GetPlayers");
    lua_newtable(L);
    lua_pushinteger(L, 1);
    lua_pushcclosure(L, l_player_conn, "", 1);
    lua_setfield(L, -2, "Connect");
    lua_setfield(L, -2, "PlayerAdded");
    lua_newtable(L);
    lua_pushinteger(L, 0);
    lua_pushcclosure(L, l_player_conn, "", 1);
    lua_setfield(L, -2, "Connect");
    lua_setfield(L, -2, "PlayerRemoving");
}

void push_runservice(lua_State* L)
{
    lua_newtable(L);
    const char* evs[3] = { "Heartbeat", "RenderStepped", "Stepped" };
    for (int i = 0; i < 3; i++)
    {
        lua_newtable(L);
        lua_pushcclosure(L, l_rs_connect, "", 0);
        lua_setfield(L, -2, "Connect");
        lua_setfield(L, -2, evs[i]);
    }
}

int l_game_getservice(lua_State* L)
{
    const char* svc = nullptr;
    std::size_t len = 0;
    if (lua_type(L, 2) == LUA_TSTRING)
        svc = lua_tolstring(L, 2, &len);
    else if (lua_type(L, 1) == LUA_TSTRING)
        svc = lua_tolstring(L, 1, &len);
    else
        luaL_errorL(L, "usage: game:GetService(\"Players\")");
    const std::string name = svc ? std::string(svc, len) : std::string();
    if (name == "Players")
    {
        lua_newtable(L);
        push_players_service(L);
        lua_setfield(L, -2, "Players");
        lua_newtable(L);
        push_players_service(L);
        return 1;
    }
    if (name == "RunService")
    {
        push_runservice(L);
        return 1;
    }
    luaL_errorL(L, ("GetService: unknown service \"" + name + "\"").c_str());
    return 0;
}

int l_game_index(lua_State* L)
{
    std::size_t len = 0;
    const char* key = lua_tolstring(L, 2, &len);
    const std::string name = key ? std::string(key, len) : std::string();
    if (name == "Players")
    {
        lua_newtable(L);
        push_players_service(L);
        lua_setfield(L, -2, "Players");
        lua_newtable(L);
        push_players_service(L);
        return 1;
    }
    if (name == "RunService")
    {
        push_runservice(L);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

struct draw_ud
{
    int id = 0;
};

void push_drawing(lua_State* L, int id)
{
    draw_ud* u = (draw_ud*)lua_newuserdata(L, sizeof(draw_ud));
    u->id = id;
    lua_getfield(L, LUA_REGISTRYINDEX, "OARDrawingMeta");
    lua_setmetatable(L, -2);
}

int draw_id_of(lua_State* L, int idx)
{
    draw_ud* u = (draw_ud*)lua_touserdata(L, idx);
    if (!u || !u->id)
        luaL_errorL(L, "expected Drawing object");
    return u->id;
}

bool read_dvec2(lua_State* L, int idx, double& x, double& y)
{
    dvec_t* d = (dvec_t*)lua_touserdata(L, idx);
    if (!d || d->tag != DVT_VEC2)
        return false;
    x = d->x;
    y = d->y;
    return true;
}

int l_draw_index(lua_State* L)
{
    const int id = draw_id_of(L, 1);
    std::size_t len = 0;
    const char* k = lua_tolstring(L, 2, &len);
    const std::string key = k ? std::string(k, len) : std::string();
    drawing::item_t it;
    if (!drawing::get(id, it))
    {
        lua_pushnil(L);
        return 1;
    }
    if (key == "Remove" || key == "Destroy")
    {
        lua_pushinteger(L, id);
        lua_pushcclosure(L, [](lua_State* L) -> int {
            drawing::remove((int)lua_tointeger(L, lua_upvalueindex(1)));
            return 0;
        }, "", 1);
        return 1;
    }
    if (key == "Color") { push_dvec(L, DVT_COLOR3, it.r, it.g, it.b, "OARColor3"); return 1; }
    if (key == "Position" || key == "From" || key == "PointA") { push_dvec(L, DVT_VEC2, it.x1, it.y1, 0.0, "OARVec2"); return 1; }
    if (key == "To" || key == "PointB") { push_dvec(L, DVT_VEC2, it.x2, it.y2, 0.0, "OARVec2"); return 1; }
    if (key == "PointC") { push_dvec(L, DVT_VEC2, it.x3, it.y3, 0.0, "OARVec2"); return 1; }
    if (key == "Size")
    {
        if (it.kind == drawing::kind_t::Text) { lua_pushnumber(L, it.fontsize); return 1; }
        push_dvec(L, DVT_VEC2, it.x2, it.y2, 0.0, "OARVec2");
        return 1;
    }
    if (key == "Transparency") { lua_pushnumber(L, it.transparency); return 1; }
    if (key == "Thickness") { lua_pushnumber(L, it.thickness); return 1; }
    if (key == "FontSize") { lua_pushnumber(L, it.fontsize); return 1; }
    if (key == "Radius") { lua_pushnumber(L, it.radius); return 1; }
    if (key == "NumSides") { lua_pushinteger(L, it.numsides); return 1; }
    if (key == "Rounding" || key == "Corner") { lua_pushnumber(L, it.rounding); return 1; }
    if (key == "ZIndex") { lua_pushinteger(L, it.zindex); return 1; }
    if (key == "Visible") { lua_pushboolean(L, it.visible); return 1; }
    if (key == "Filled") { lua_pushboolean(L, it.filled); return 1; }
    if (key == "Center") { lua_pushboolean(L, it.center); return 1; }
    if (key == "Outline") { lua_pushboolean(L, it.outline); return 1; }
    if (key == "Text") { lua_pushstring(L, it.text.c_str()); return 1; }
    lua_pushnil(L);
    return 1;
}

int l_draw_newindex(lua_State* L)
{
    const int id = draw_id_of(L, 1);
    std::size_t len = 0;
    const char* k = lua_tolstring(L, 2, &len);
    const std::string key = k ? std::string(k, len) : std::string();
    drawing::item_t it;
    if (!drawing::get(id, it))
        luaL_errorL(L, "Drawing object was removed");
    if (key == "Color")
    {
        dvec_t* d = (dvec_t*)lua_touserdata(L, 3);
        if (!d || d->tag != DVT_COLOR3)
            luaL_errorL(L, "Color expects a Color3");
        it.r = (float)d->x;
        it.g = (float)d->y;
        it.b = (float)d->z;
    }
    else if (key == "Position" || key == "From" || key == "PointA")
    {
        double x = 0, y = 0;
        if (!read_dvec2(L, 3, x, y))
            luaL_errorL(L, "Position expects a Vector2");
        it.x1 = (float)x;
        it.y1 = (float)y;
    }
    else if (key == "To" || key == "PointB")
    {
        double x = 0, y = 0;
        if (!read_dvec2(L, 3, x, y))
            luaL_errorL(L, "To expects a Vector2");
        it.x2 = (float)x;
        it.y2 = (float)y;
    }
    else if (key == "PointC")
    {
        double x = 0, y = 0;
        if (!read_dvec2(L, 3, x, y))
            luaL_errorL(L, "PointC expects a Vector2");
        it.x3 = (float)x;
        it.y3 = (float)y;
    }
    else if (key == "Size")
    {
        if (it.kind == drawing::kind_t::Text)
            it.fontsize = (float)luaL_checknumber(L, 3);
        else
        {
            double x = 0, y = 0;
            if (!read_dvec2(L, 3, x, y))
                luaL_errorL(L, "Size expects a Vector2");
            it.x2 = (float)x;
            it.y2 = (float)y;
        }
    }
    else if (key == "Transparency") it.transparency = (float)luaL_checknumber(L, 3);
    else if (key == "Thickness") it.thickness = (float)luaL_checknumber(L, 3);
    else if (key == "FontSize") it.fontsize = (float)luaL_checknumber(L, 3);
    else if (key == "Radius") it.radius = (float)luaL_checknumber(L, 3);
    else if (key == "NumSides") it.numsides = (int)luaL_checkinteger(L, 3);
    else if (key == "Rounding" || key == "Corner") it.rounding = (float)luaL_checknumber(L, 3);
    else if (key == "ZIndex") it.zindex = (int)luaL_checkinteger(L, 3);
    else if (key == "Visible") it.visible = lua_toboolean(L, 3) != 0;
    else if (key == "Filled") it.filled = lua_toboolean(L, 3) != 0;
    else if (key == "Center") it.center = lua_toboolean(L, 3) != 0;
    else if (key == "Outline") it.outline = lua_toboolean(L, 3) != 0;
    else if (key == "Font") { }
    else if (key == "Text")
    {
        std::size_t tl = 0;
        const char* t = lua_tolstring(L, 3, &tl);
        it.text.assign(t ? t : "", tl);
    }
    else
        luaL_errorL(L, ("Drawing: unknown property \"" + key + "\"").c_str());
    drawing::set(id, it);
    return 0;
}

int l_draw_gc(lua_State* L)
{
    draw_ud* u = (draw_ud*)lua_touserdata(L, 1);
    if (u && u->id)
        drawing::remove(u->id);
    return 0;
}

int l_drawing_new(lua_State* L)
{
    std::size_t len = 0;
    const char* type = luaL_checklstring(L, 1, &len);
    const char* owner = lua_tostring(L, lua_upvalueindex(1));
    const int id = drawing::create(owner ? owner : "?", type ? std::string(type, len) : "");
    if (!id)
        luaL_errorL(L, "Drawing.new: unknown type (Line/Text/Square/Circle/Triangle)");
    push_drawing(L, id);
    return 1;
}

void register_dvec_meta(lua_State* L)
{
    struct reg { const char* name; const char* type; };
    const reg regs[3] = { { "OARVec2", "Vector2" }, { "OARVec3", "Vector3" }, { "OARColor3", "Color3" } };
    for (int i = 0; i < 3; i++)
    {
        lua_newtable(L);
        lua_pushcfunction(L, l_dvec_index, ""); lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, l_dvec_add, ""); lua_setfield(L, -2, "__add");
        lua_pushcfunction(L, l_dvec_sub, ""); lua_setfield(L, -2, "__sub");
        lua_pushcfunction(L, l_dvec_mul, ""); lua_setfield(L, -2, "__mul");
        lua_pushcfunction(L, l_dvec_div, ""); lua_setfield(L, -2, "__div");
        lua_pushcfunction(L, l_dvec_unm, ""); lua_setfield(L, -2, "__unm");
        lua_pushcfunction(L, l_dvec_eq, ""); lua_setfield(L, -2, "__eq");
        lua_pushcfunction(L, l_dvec_tostring, ""); lua_setfield(L, -2, "__tostring");
        lua_pushstring(L, regs[i].type); lua_setfield(L, -2, "__type");
        lua_setfield(L, LUA_REGISTRYINDEX, regs[i].name);
    }
}

void register_drawing_meta(lua_State* L)
{    struct reg { const char* name; const char* type; };
    const reg regs[3] = { { "OARVec2", "Vector2" }, { "OARVec3", "Vector3" }, { "OARColor3", "Color3" } };
    for (int i = 0; i < 3; i++)
    {
        lua_newtable(L);
        lua_pushcfunction(L, l_dvec_index, ""); lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, l_dvec_add, ""); lua_setfield(L, -2, "__add");
        lua_pushcfunction(L, l_dvec_sub, ""); lua_setfield(L, -2, "__sub");
        lua_pushcfunction(L, l_dvec_mul, ""); lua_setfield(L, -2, "__mul");
        lua_pushcfunction(L, l_dvec_div, ""); lua_setfield(L, -2, "__div");
        lua_pushcfunction(L, l_dvec_unm, ""); lua_setfield(L, -2, "__unm");
        lua_pushcfunction(L, l_dvec_eq, ""); lua_setfield(L, -2, "__eq");
        lua_pushcfunction(L, l_dvec_tostring, ""); lua_setfield(L, -2, "__tostring");
        lua_pushstring(L, regs[i].type); lua_setfield(L, -2, "__type");
        lua_setfield(L, LUA_REGISTRYINDEX, regs[i].name);
    }
}

int l_vector2_new(lua_State* L)
{
    push_dvec(L, DVT_VEC2, luaL_checknumber(L, 1), luaL_checknumber(L, 2), 0.0, "OARVec2");
    return 1;
}
int l_vector3_new(lua_State* L)
{
    push_dvec(L, DVT_VEC3, luaL_checknumber(L, 1), luaL_checknumber(L, 2), luaL_checknumber(L, 3), "OARVec3");
    return 1;
}
int l_color3_new(lua_State* L)
{
    push_dvec(L, DVT_COLOR3, luaL_checknumber(L, 1), luaL_checknumber(L, 2), luaL_checknumber(L, 3), "OARColor3");
    return 1;
}
int l_color3_rgb(lua_State* L)
{
    push_dvec(L, DVT_COLOR3, luaL_checknumber(L, 1) / 255.0, luaL_checknumber(L, 2) / 255.0, luaL_checknumber(L, 3) / 255.0, "OARColor3");
    return 1;
}
int l_color3_hsv(lua_State* L)
{
    double h = luaL_checknumber(L, 1), s = luaL_checknumber(L, 2), v = luaL_checknumber(L, 3);
    h = h - std::floor(h);
    const double c = v * s;
    const double x = c * (1.0 - std::fabs(std::fmod(h * 6.0, 2.0) - 1.0));
    const double m = v - c;
    double r = 0, g = 0, b = 0;
    const int sector = (int)(h * 6.0);
    switch (sector % 6)
    {
    case 0: r = c; g = x; break;
    case 1: r = x; g = c; break;
    case 2: g = c; b = x; break;
    case 3: g = x; b = c; break;
    case 4: r = x; b = c; break;
    default: r = c; b = x; break;
    }
    push_dvec(L, DVT_COLOR3, r + m, g + m, b + m, "OARColor3");
    return 1;
}

void push_vec3(lua_State* L, float x, float y, float z)
{
    lua_newtable(L);
    lua_pushnumber(L, x); lua_setfield(L, -2, "x");
    lua_pushnumber(L, y); lua_setfield(L, -2, "y");
    lua_pushnumber(L, z); lua_setfield(L, -2, "z");
}

int l_game_players(lua_State* L)
{
    std::vector<LSnapP> copy;
    {
        std::lock_guard<std::mutex> lock(slock);
        copy = g_snap.ps;
    }
    lua_newtable(L);
    int idx = 1;
    for (auto& e : copy)
    {
        if (idx > 200)
            break;
        lua_newtable(L);
        lua_pushstring(L, e.name); lua_setfield(L, -2, "name");
        lua_pushnumber(L, e.hp); lua_setfield(L, -2, "health");
        lua_pushnumber(L, e.maxhp); lua_setfield(L, -2, "max_health");
        lua_pushnumber(L, e.dist); lua_setfield(L, -2, "dist");
        lua_pushboolean(L, e.downed); lua_setfield(L, -2, "downed");
        lua_pushboolean(L, e.vis); lua_setfield(L, -2, "visible");
        push_vec3(L, e.x, e.y, e.z); lua_setfield(L, -2, "pos");
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

int l_game_guards(lua_State* L)
{
    std::vector<LSnapG> copy;
    {
        std::lock_guard<std::mutex> lock(slock);
        copy = g_snap.gs;
    }
    lua_newtable(L);
    int idx = 1;
    for (auto& e : copy)
    {
        if (idx > 200)
            break;
        lua_newtable(L);
        lua_pushnumber(L, e.hp); lua_setfield(L, -2, "health");
        lua_pushnumber(L, e.dist); lua_setfield(L, -2, "dist");
        lua_pushboolean(L, e.dead); lua_setfield(L, -2, "dead");
        lua_pushboolean(L, e.alerted); lua_setfield(L, -2, "alerted");
        lua_pushboolean(L, e.armed); lua_setfield(L, -2, "armed");
        lua_pushboolean(L, e.vis); lua_setfield(L, -2, "visible");
        push_vec3(L, e.x, e.y, e.z); lua_setfield(L, -2, "pos");
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

int l_game_loot(lua_State* L)
{
    std::vector<LSnapL> copy;
    {
        std::lock_guard<std::mutex> lock(slock);
        copy = g_snap.ls;
    }
    lua_newtable(L);
    int idx = 1;
    for (auto& e : copy)
    {
        if (idx > 200)
            break;
        lua_newtable(L);
        lua_pushstring(L, e.label); lua_setfield(L, -2, "label");
        lua_pushnumber(L, e.value); lua_setfield(L, -2, "value");
        lua_pushnumber(L, e.dist); lua_setfield(L, -2, "dist");
        lua_pushboolean(L, e.money); lua_setfield(L, -2, "money");
        push_vec3(L, e.x, e.y, e.z); lua_setfield(L, -2, "pos");
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

int l_game_count(lua_State* L)
{
    size_t n = 0;
    {
        std::lock_guard<std::mutex> lock(slock);
        n = g_snap.ps.size();
    }
    lua_pushinteger(L, (int)n);
    return 1;
}

int l_game_local(lua_State* L)
{
    LSnapP self{};
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(slock);
        for (auto& e : g_snap.ps) {
            if (e.self) { self = e; found = true; break; }
        }
    }
    if (!found)
    {
        lua_pushnil(L);
        return 1;
    }
    lua_newtable(L);
    lua_pushstring(L, self.name); lua_setfield(L, -2, "name");
    lua_pushnumber(L, self.hp); lua_setfield(L, -2, "health");
    lua_pushnumber(L, self.maxhp); lua_setfield(L, -2, "max_health");
    push_vec3(L, self.x, self.y, self.z); lua_setfield(L, -2, "pos");
    return 1;
}

int l_oar_act(lua_State* L)
{
    const char* id = lua_tostring(L, lua_upvalueindex(1));
    if (id && id[0])
        feat::queue_run(id);
    return 0;
}

int l_oar_slomo(lua_State* L)
{
    exploits::set_slomo((float)luaL_optnumber(L, 1, 1.0));
    return 0;
}

int l_oar_fov(lua_State* L)
{
    exploits::set_fov((float)luaL_optnumber(L, 1, 90.0));
    return 0;
}

int l_oar_summon(lua_State* L)
{
    std::size_t len = 0;
    const char* s = luaL_checklstring(L, 1, &len);
    if (s && len > 0 && len < 64)
    {
        char b[64] = {};
        memcpy(b, s, len);
        exploits::summon_item(b);
    }
    return 0;
}

static const struct { const char* name; const char* id; } kActs[] = {
    { "vacuum", "vacuum" }, { "phones", "phones" }, { "cameras", "cameras" },
    { "doors", "doors" }, { "guns", "guns" }, { "complete", "complete" },
    { "wingame", "wingame" }, { "ammo", "ammo" }, { "xp", "xp" },
    { "immunity", "immunity" }, { "dance", "dance" }, { "floppy", "floppy" },
    { "cops", "cops" }, { "randmask", "randmask" }, { "endtimer", "endtimer" },
    { "kickself", "kickself" }, { "ghost", "ghost" }, { "debugcam", "debugcam" },
    { "playersonly", "playersonly" }, { "cheats", "cheats" }, { "depthchams", "depthchams" },
    { nullptr, nullptr }
};

void register_dvec_meta(lua_State* L);
void register_drawing_meta(lua_State* L);
int l_loadstring(lua_State* L);
int l_rs_connect(lua_State* L);
int l_player_conn(lua_State* L);
int l_get_mouse(lua_State* L);
void hold_fn(lua_State* L, int id);
void drop_fn(lua_State* L, int id);
void fire_callback(lua_State* L, live_t* sc, int fn_id, int nargs);

void register_api(lua_State* L, const std::string& tag)
{
    lua_pushstring(L, tag.c_str());
    lua_pushcclosure(L, l_print, "print", 1);
    lua_setglobal(L, "print");

    lua_pushcfunction(L, l_wait, "");
    lua_setglobal(L, "wait");

    lua_newtable(L);
    lua_pushcfunction(L, l_game_players, ""); lua_setfield(L, -2, "players");
    lua_pushcfunction(L, l_game_guards, ""); lua_setfield(L, -2, "guards");
    lua_pushcfunction(L, l_game_loot, ""); lua_setfield(L, -2, "loot");
    lua_pushcfunction(L, l_game_count, ""); lua_setfield(L, -2, "count");
    lua_pushcfunction(L, l_game_local, ""); lua_setfield(L, -2, "local");
    lua_pushcfunction(L, l_game_getservice, ""); lua_setfield(L, -2, "GetService");
    lua_pushcfunction(L, l_game_httpget, ""); lua_setfield(L, -2, "HttpGet");
    lua_newtable(L);
    lua_pushcfunction(L, l_game_index, ""); lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);
    lua_setglobal(L, "game");

    lua_newtable(L);
    for (int i = 0; kActs[i].name; ++i)
    {
        lua_pushstring(L, kActs[i].id);
        lua_pushcclosure(L, l_oar_act, "", 1);
        lua_setfield(L, -2, kActs[i].name);
    }
    lua_pushcfunction(L, l_oar_slomo, ""); lua_setfield(L, -2, "slomo");
    lua_pushcfunction(L, l_oar_fov, ""); lua_setfield(L, -2, "fov");
    lua_pushcfunction(L, l_oar_summon, ""); lua_setfield(L, -2, "summon");
    lua_setglobal(L, "oar");

    lua_pushcfunction(L, l_loadstring, ""); lua_setglobal(L, "loadstring");
    lua_pushcfunction(L, l_loadstring, ""); lua_setglobal(L, "load");
    lua_pushcfunction(L, l_getfenv, ""); lua_setglobal(L, "getfenv");
    lua_pushcfunction(L, l_setfenv, ""); lua_setglobal(L, "setfenv");
    lua_pushcfunction(L, l_tick, ""); lua_setglobal(L, "tick");
    lua_pushcfunction(L, l_typeof, ""); lua_setglobal(L, "typeof");
    lua_pushcfunction(L, l_identifyexecutor, ""); lua_setglobal(L, "identifyexecutor");
    lua_pushcfunction(L, l_notify, ""); lua_setglobal(L, "notify");
    lua_pushcfunction(L, l_warn, ""); lua_setglobal(L, "warn");
    lua_pushcfunction(L, l_httpget, ""); lua_setglobal(L, "httpget");
    lua_pushcfunction(L, l_httppost, ""); lua_setglobal(L, "httppost");
    lua_pushcfunction(L, l_worldtoscreen, ""); lua_setglobal(L, "WorldToScreen");
    lua_pushcfunction(L, l_ismouse1pressed, ""); lua_setglobal(L, "ismouse1pressed");
    lua_pushcfunction(L, l_ismouse2pressed, ""); lua_setglobal(L, "ismouse2pressed");
    lua_pushcfunction(L, l_iskeypressed, ""); lua_setglobal(L, "iskeypressed");
    lua_pushcfunction(L, l_isgameactive, ""); lua_setglobal(L, "isgameactive");
    lua_pushcfunction(L, l_setgameinput, ""); lua_setglobal(L, "setgameinput");
    lua_pushcfunction(L, l_memory_read, ""); lua_setglobal(L, "memory_read");
    lua_pushcfunction(L, l_memory_write, ""); lua_setglobal(L, "memory_write");
    lua_pushcfunction(L, l_get_mouse, ""); lua_setglobal(L, "getmouse");

    register_dvec_meta(L);
    lua_newtable(L);
    lua_pushcfunction(L, l_vector2_new, ""); lua_setfield(L, -2, "new");
    lua_setglobal(L, "Vector2");
    lua_newtable(L);
    lua_pushcfunction(L, l_vector3_new, ""); lua_setfield(L, -2, "new");
    lua_setglobal(L, "Vector3");
    lua_newtable(L);
    lua_pushcfunction(L, l_color3_new, ""); lua_setfield(L, -2, "new");
    lua_pushcfunction(L, l_color3_rgb, ""); lua_setfield(L, -2, "fromRGB");
    lua_pushcfunction(L, l_color3_hsv, ""); lua_setfield(L, -2, "fromHSV");
    lua_setglobal(L, "Color3");

    register_drawing_meta(L);
    lua_newtable(L);
    lua_pushstring(L, tag.c_str());
    lua_pushcclosure(L, l_drawing_new, "", 1);
    lua_setfield(L, -2, "new");
    lua_newtable(L);
    const char* fonts[] = { "UI", "Monospace", "System" };
    for (auto f : fonts)
    {
        lua_pushstring(L, f);
        lua_setfield(L, -2, f);
    }
    lua_setfield(L, -2, "Fonts");
    lua_setglobal(L, "Drawing");

    lua_newtable(L);
    lua_pushcfunction(L, l_os_clock, ""); lua_setfield(L, -2, "clock");
    lua_pushcfunction(L, l_os_time, ""); lua_setfield(L, -2, "time");
    lua_pushcfunction(L, l_os_difftime, ""); lua_setfield(L, -2, "difftime");
    lua_setglobal(L, "os");

    lua_newtable(L);
    lua_pushcfunction(L, l_task_spawn, ""); lua_setfield(L, -2, "spawn");
    lua_pushcfunction(L, l_task_delay, ""); lua_setfield(L, -2, "delay");
    lua_pushcfunction(L, l_task_defer, ""); lua_setfield(L, -2, "defer");
    lua_pushcfunction(L, l_task_cancel, ""); lua_setfield(L, -2, "cancel");
    lua_pushcfunction(L, l_wait, ""); lua_setfield(L, -2, "wait");
    lua_setglobal(L, "task");
    lua_getglobal(L, "table");
    if (lua_istable(L, -1))
    {
        lua_getfield(L, -1, "unpack");
        lua_setglobal(L, "unpack");
    }
    lua_pop(L, 1);
}

void sandbox_ours(lua_State* L)
{
    static const char* libs[] = { "string", "table", "math", "utf8", "coroutine", "buffer", "vector", "bit32", "debug", nullptr };
    for (int i = 0; libs[i]; i++)
    {
        lua_getglobal(L, libs[i]);
        if (lua_istable(L, -1))
            lua_setreadonly(L, -1, true);
        lua_pop(L, 1);
    }
    static const char* ours[] = { "os", nullptr };
    for (int i = 0; ours[i]; i++)
    {
        lua_getglobal(L, ours[i]);
        if (lua_istable(L, -1))
            lua_setreadonly(L, -1, true);
        lua_pop(L, 1);
    }
    static const char* metas[] = { "OARVec2", "OARVec3", "OARColor3", "OARDrawingMeta", nullptr };
    for (int i = 0; metas[i]; i++)
    {
        lua_getfield(L, LUA_REGISTRYINDEX, metas[i]);
        if (lua_istable(L, -1))
            lua_setreadonly(L, -1, true);
        lua_pop(L, 1);
    }
    lua_pushliteral(L, "");
    if (lua_getmetatable(L, -1))
    {
        lua_setreadonly(L, -1, true);
        lua_pop(L, 2);
    }
    else
    {
        lua_pop(L, 1);
    }
}

static const char* const kNoFastcall[] = { "typeof", "unpack", "table.unpack", nullptr };

std::string compile_source(const std::string& source)
{
    Luau::CompileOptions opts;
    opts.disabledBuiltins = kNoFastcall;
    return Luau::compile(source, opts);
}

int l_loadstring(lua_State* L)
{
    std::size_t len = 0;
    const char* src = luaL_checklstring(L, 1, &len);
    std::size_t nlen = 0;
    const char* name = luaL_optlstring(L, 2, "loadstring", &nlen);
    std::string bytecode;
    try
    {
        bytecode = compile_source(std::string(src, len));
    }
    catch (const std::exception& e)
    {
        lua_pushnil(L);
        lua_pushstring(L, e.what());
        return 2;
    }
    const std::string chunkname = (name && nlen) ? std::string(name, nlen) : std::string("loadstring");
    if (luau_load(L, chunkname.c_str(), bytecode.data(), bytecode.size(), 0) != 0)
    {
        lua_pushnil(L);
        lua_insert(L, -2);
        return 2;
    }
    return 1;
}

void script_main(std::shared_ptr<live_t> sc, std::string source)
{
    lua_State* L = luaL_newstate();
    if (!L)
    {
        push_out("[" + sc->name + "] error: could not create VM state");
        sc->done = true;
        return;
    }
    sc->L = L;
    lua_callbacks(L)->userdata = sc.get();

    luaL_openlibs(L);
    register_api(L, sc->name);

    static const char* strip[] = { "io", "dofile", "loadfile", "require", "collectgarbage", "newproxy", nullptr };
    for (int i = 0; strip[i]; i++)
    {
        lua_pushnil(L);
        lua_setglobal(L, strip[i]);
    }
    sandbox_ours(L);

    if (sc->kill.load(std::memory_order_acquire) || !programRunning.load(std::memory_order_acquire))
    {
        push_out("[" + sc->name + "] stopped");
        sc->done = true;
        lua_close(L);
        sc->L = nullptr;
        return;
    }

    lua_callbacks(L)->interrupt = &interrupt_hook;

    std::string bytecode;
    try
    {
        bytecode = compile_source(source);
    }
    catch (const std::exception& e)
    {
        push_out("[" + sc->name + "] compile error: " + e.what());
        sc->done = true;
        lua_close(L);
        sc->L = nullptr;
        return;
    }

    const std::string chunk = "@" + sc->name;
    bool loaded = false;
    bool stopped_loading = false;
    try
    {
        loaded = (luau_load(L, chunk.c_str(), bytecode.data(), bytecode.size(), 0) == 0);
    }
    catch (...)
    {
        stopped_loading = true;
        push_out("[" + sc->name + "] stopped");
    }
    if (loaded)
    {
        lua_State* co = lua_newthread(L);
        const int mainslot = anchor_thread(L, sc.get());
        lua_pushvalue(L, -2);
        lua_xmove(L, co, 1);
        lua_pop(L, 2);
        live_t::task_entry main_entry;
        main_entry.co = co;
        main_entry.argc = 0;
        main_entry.anchor = mainslot;
        main_entry.wake = std::chrono::steady_clock::now();
        sc->sched->entries.push_back(std::move(main_entry));

        sc->sched->entries.reserve(32);
        bool reported = false;
        sc->sched->last_pass = std::chrono::steady_clock::now();
        for (;;)
        {
            if (sc->kill.load(std::memory_order_acquire) || !programRunning.load(std::memory_order_acquire))
            {
                push_out("[" + sc->name + "] stopped");
                reported = true;
                break;
            }
            bool any_live = false;
            bool did_work = false;
            bool has_frame = false;
            const auto now = std::chrono::steady_clock::now();
            double dt = std::chrono::duration<double>(now - sc->sched->last_pass).count();
            if (!(dt >= 0.0) || dt > 0.25)
                dt = 0.016;
            sc->sched->last_pass = now;

            for (auto& c : sc->sched->frame_conns)
            {
                if (c.dead || sc->sched->dead_conns.count(c.id))
                {
                    if (!c.dead)
                        drop_fn(L, c.id);
                    c.dead = true;
                    continue;
                }
                any_live = true;
                has_frame = true;
                lua_pushnumber(L, dt);
                fire_callback(L, sc.get(), c.id, 1);
            }

            {
                const std::uint64_t gen = snap_gen();
                if (!sc->sched->players_primed || gen != sc->sched->last_generation)
                {
                    sc->sched->last_generation = gen;
                    std::vector<std::string> cur;
                    {
                        std::lock_guard<std::mutex> lock(slock);
                        cur.reserve(g_snap.ps.size());
                        for (auto& e : g_snap.ps) {
                            if (e.self || !e.name[0]) continue;
                            cur.push_back(e.name);
                        }
                    }
                    std::vector<std::string> joined;
                    for (auto& n : cur) {
                        if (sc->sched->players_primed && sc->sched->known_players.find(n) == sc->sched->known_players.end())
                            joined.push_back(n);
                    }
                    std::vector<std::string> left;
                    if (sc->sched->players_primed)
                        for (auto& n : sc->sched->known_players)
                            if (std::find(cur.begin(), cur.end(), n) == cur.end())
                                left.push_back(n);
                    sc->sched->known_players.clear();
                    for (auto& n : cur) sc->sched->known_players.insert(n);
                    sc->sched->players_primed = true;
                    for (auto& c : sc->sched->player_conns)
                    {
                        if (c.dead || sc->sched->dead_conns.count(c.id))
                        {
                            if (!c.dead)
                                drop_fn(L, c.id);
                            c.dead = true;
                            continue;
                        }
                        any_live = true;
                        if (c.added)
                        {
                            for (auto& n : joined)
                            {
                                lua_pushstring(L, n.c_str());
                                fire_callback(L, sc.get(), c.id, 1);
                            }
                        }
                        else
                        {
                            for (auto& n : left)
                            {
                                lua_pushstring(L, n.c_str());
                                fire_callback(L, sc.get(), c.id, 1);
                            }
                        }
                    }
                }
                else
                {
                    for (auto& c : sc->sched->frame_conns)
                        if (!c.dead && !sc->sched->dead_conns.count(c.id)) { any_live = true; break; }
                    if (!any_live)
                        for (auto& c : sc->sched->player_conns)
                            if (!c.dead && !sc->sched->dead_conns.count(c.id)) { any_live = true; break; }
                }
            }
            {
                double mx = 0, my = 0;
                const bool has_mouse = mouse_client_pos(mx, my);
                const bool lb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
                if (has_mouse)
                {
                    if (!sc->sched->mouse_primed)
                    {
                        sc->sched->mouse_primed = true;
                        sc->sched->mouse_l = lb;
                        sc->sched->mouse_x = mx;
                        sc->sched->mouse_y = my;
                    }
                    else
                    {
                        const bool down_edge = lb && !sc->sched->mouse_l;
                        const bool up_edge = !lb && sc->sched->mouse_l;
                        const bool moved = mx != sc->sched->mouse_x || my != sc->sched->mouse_y;
                        sc->sched->mouse_l = lb;
                        sc->sched->mouse_x = mx;
                        sc->sched->mouse_y = my;
                        if (down_edge || up_edge || moved)
                        {
                            for (auto& c : sc->sched->mouse_conns)
                            {
                                if (c.dead || sc->sched->dead_conns.count(c.id))
                                {
                                    if (!c.dead)
                                        drop_fn(L, c.id);
                                    c.dead = true;
                                    continue;
                                }
                                if ((c.kind == 0 && down_edge) || (c.kind == 1 && up_edge) || (c.kind == 2 && moved))
                                    fire_callback(L, sc.get(), c.id, 0);
                            }
                        }
                        else
                        {
                            for (auto& c : sc->sched->mouse_conns)
                                if (!c.dead && !sc->sched->dead_conns.count(c.id)) { any_live = true; break; }
                        }
                    }
                }
            }

            for (std::size_t i = 0; i < sc->sched->entries.size(); i++)
            {
                auto& e = sc->sched->entries[i];
                if (e.dead)
                    continue;
                any_live = true;
                if (now < e.wake)
                    continue;
                int nargs = 0;
                if (!e.started)
                {
                    e.started = true;
                    nargs = e.argc;
                }
                const int st = lua_resume(e.co, L, nargs);
                did_work = true;
                if (st == LUA_OK)
                {
                    e.dead = true;
                    release_thread(L, e.anchor);
                    e.anchor = 0;
                }
                else if (st == LUA_YIELD)
                {
                }
                else
                {
                    e.dead = true;
                    release_thread(L, e.anchor);
                    e.anchor = 0;
                    lua_callbacks(L)->interrupt = nullptr;
                    std::size_t len = 0;
                    const char* err = lua_tolstring(e.co, -1, &len);
                    std::string msg = err ? std::string(err, len) : std::string("unknown");
                    luaL_traceback(L, e.co, msg.c_str(), 1);
                    std::size_t tlen = 0;
                    const char* tb = lua_tolstring(L, -1, &tlen);
                    if (tb && tlen > 0 && tlen < 8192)
                        msg.assign(tb, tlen);
                    lua_pop(L, 1);
                    push_out("[" + sc->name + "] runtime error: " + msg);
                    reported = true;
                    bool more = false;
                    for (auto& o : sc->sched->entries)
                        if (!o.dead) { more = true; break; }
                    if (more)
                        lua_callbacks(L)->interrupt = &interrupt_hook;
                }
            }
            if (!any_live)
                break;
            {
                auto& v = sc->sched->entries;
                v.erase(std::remove_if(v.begin(), v.end(), [](const live_t::task_entry& e) { return e.dead; }), v.end());
            }
            if (has_frame)
            {
                const auto budget_until = now + std::chrono::milliseconds(8);
                const auto cur = std::chrono::steady_clock::now();
                if (cur < budget_until)
                    std::this_thread::sleep_until(budget_until);
            }
            else if (!did_work)
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        lua_settop(L, 0);
        if (!reported)
            push_out("[" + sc->name + "] finished");
    }
    else
    {
        lua_callbacks(L)->interrupt = nullptr;
        if (!stopped_loading)
        {
            std::size_t len = 0;
            const char* err = lua_tolstring(L, -1, &len);
            push_out("[" + sc->name + "] load error: " + (err ? std::string(err, len) : "unknown"));
        }
    }

    sc->done = true;
    lua_callbacks(L)->interrupt = nullptr;
    drawing::clear_owner(sc->name);
    lua_close(L);
    sc->L = nullptr;
    std::lock_guard<std::mutex> lock(mtx);
    auto it = live.find(sc->name);
    if (it != live.end() && it->second == sc)
        live.erase(it);
}

const char* EXAMPLE =
    "-- OAR Lua example (runs in the cheat, actions apply in game)\n"
    "print(\"players online: \" .. game.count())\n"
    "for _, p in ipairs(game.players()) do\n"
    "    print(p.name .. \" hp=\" .. p.health)\n"
    "end\n"
    "for i = 1, 3 do\n"
    "    print(\"tick \" .. i)\n"
    "    wait(1)\n"
    "end\n"
    "-- oar.vacuum()\n"
    "-- oar.doors()\n"
    "-- local d = Drawing.new(\"Text\") d.Text = \"hi\" d.Visible = true\n"
    "-- game:GetService(\"RunService\").Heartbeat:Connect(function(dt) end)\n";

}

std::string scripts_dir()
{
    static std::string dir;
    static bool done = false;
    if (!done)
    {
        done = true;
        dir = fileManager::baseDir() + "\\Scripts";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        std::string ex = dir + "\\example.lua";
        DWORD a = GetFileAttributesA(ex.c_str());
        if (a == INVALID_FILE_ATTRIBUTES)
        {
            std::ofstream f(ex, std::ios::binary | std::ios::trunc);
            if (f) f.write(EXAMPLE, strlen(EXAMPLE));
        }
    }
    return dir;
}

std::vector<std::string> list_scripts()
{
    std::vector<std::string> out;
    std::error_code ec;
    for (auto& e : std::filesystem::directory_iterator(scripts_dir(), ec))
    {
        if (!e.is_regular_file(ec)) continue;
        if (e.path().extension() != ".lua") continue;
        out.push_back(e.path().filename().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<run_info> list_runs()
{
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<run_info> out;
    for (auto& kv : live)
    {
        run_info r;
        r.name = kv.first;
        r.running = !kv.second->done.load();
        out.push_back(r);
    }
    return out;
}

bool is_running(const std::string& name)
{
    std::lock_guard<std::mutex> lock(mtx);
    auto it = live.find(name);
    return it != live.end() && !it->second->done.load();
}

bool valid_script_name(const std::string& name)
{
    if (name.empty() || name.size() > 128)
        return false;
    if (name.find('\\') != std::string::npos || name.find('/') != std::string::npos || name.find("..") != std::string::npos)
        return false;
    return true;
}

bool spawn_locked(const std::string& name, const std::string& source)
{
    auto it = live.find(name);
    if (it != live.end())
    {
        if (!it->second->done.load())
            it->second->kill.store(true, std::memory_order_release);
    }
    auto sc = std::make_shared<live_t>();
    sc->name = name;
    live[name] = sc;
    push_out_locked("[" + name + "] started");
    std::thread([sc, source] {
        try
        {
            script_main(sc, source);
        }
        catch (const std::exception& e)
        {
            push_out("[" + sc->name + "] error: worker crashed (" + std::string(e.what()) + ")");
            sc->done = true;
        }
        catch (...)
        {
            push_out("[" + sc->name + "] error: worker crashed (unknown)");
            sc->done = true;
        }
    }).detach();
    return true;
}

bool start(const std::string& name)
{
    if (!valid_script_name(name))
        return false;
    {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = live.find(name);
        if (it != live.end() && !it->second->done.load())
            return false;
    }
    const std::string path = scripts_dir() + "\\" + name;
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    const std::string source = ss.str();
    if (source.empty() || source.size() > 1024u * 1024u)
        return false;

    std::lock_guard<std::mutex> lock(mtx);
    return spawn_locked(name, source);
}

bool execute_string(const std::string& source)
{
    if (source.empty() || source.size() > 1024u * 1024u)
        return false;
    std::lock_guard<std::mutex> lock(mtx);
    return spawn_locked("executor", source);
}

bool read_file(const std::string& name, std::string& out)
{
    if (!valid_script_name(name))
        return false;
    std::ifstream f(scripts_dir() + "\\" + name, std::ios::binary);
    if (!f)
        return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return out.size() <= 1024u * 1024u;
}

bool write_file(const std::string& name, const std::string& data)
{
    if (!valid_script_name(name) || data.size() > 1024u * 1024u)
        return false;
    std::ofstream f(scripts_dir() + "\\" + name, std::ios::binary | std::ios::trunc);
    if (!f)
        return false;
    f.write(data.data(), (std::streamsize)data.size());
    return !!f;
}

void stop(const std::string& name)
{
    std::lock_guard<std::mutex> lock(mtx);
    auto it = live.find(name);
    if (it == live.end())
        return;
    it->second->kill.store(true, std::memory_order_release);
}

void stop_all()
{
    std::vector<std::shared_ptr<live_t>> all;
    {
        std::lock_guard<std::mutex> lock(mtx);
        for (auto& kv : live)
        {
            kv.second->kill.store(true, std::memory_order_release);
            all.push_back(kv.second);
        }
    }
    for (int i = 0; i < 120; ++i)
    {
        bool any = false;
        for (auto& sc : all) {
            if (!sc->done.load()) { any = true; break; }
        }
        if (!any) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    std::lock_guard<std::mutex> lock(mtx);
    for (auto it = live.begin(); it != live.end();)
    {
        if (it->second->done.load())
            it = live.erase(it);
        else
            ++it;
    }
}

void clear_output()
{
    std::lock_guard<std::mutex> lock(mtx);
    output.clear();
}

std::vector<std::string> get_output()
{
    std::lock_guard<std::mutex> lock(mtx);
    return output;
}

static void snap_players(LSnap& s) {
    __try {
        for (auto& e : esp::playerManager::players()) {
            if (s.ps.size() >= 128) break;
            LSnapP p{};
            strncpy_s(p.name, e.name.c_str(), 31);
            p.hp = e.hp; p.maxhp = e.maxHp;
            p.x = e.x; p.y = e.y; p.z = e.z; p.dist = e.distM;
            p.downed = e.downed; p.vis = e.visible; p.self = e.self;
            s.ps.push_back(p);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}
static void snap_guards(LSnap& s) {
    __try {
        for (auto& e : esp::playerManager::npcs()) {
            if (s.gs.size() >= 128) break;
            LSnapG g{};
            g.x = e.x; g.y = e.y; g.z = e.z;
            g.hp = e.hp; g.dist = e.distM;
            g.dead = e.dead; g.alerted = e.alerted; g.armed = e.armed;
            g.vis = e.visible; g.guard = e.guard;
            s.gs.push_back(g);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}
static void snap_loot(LSnap& s) {
    __try {
        for (auto& e : esp::itemManager::items()) {
            if (s.ls.size() >= 256) break;
            LSnapL l{};
            strncpy_s(l.label, e.label.c_str(), 47);
            l.x = e.x; l.y = e.y; l.z = e.z;
            l.value = e.value; l.dist = e.distM; l.money = e.money;
            s.ls.push_back(l);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void snapshot()
{
    LSnap s;
    snap_players(s);
    snap_guards(s);
    snap_loot(s);
    {
        std::lock_guard<std::mutex> lock(slock);
        g_snap.ps.swap(s.ps);
        g_snap.gs.swap(s.gs);
        g_snap.ls.swap(s.ls);
        g_gen.fetch_add(1, std::memory_order_release);
    }
}

}

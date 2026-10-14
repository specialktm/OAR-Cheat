#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <cstdint>
#include "../../cheat/source/util/obf.h"

static void usage() {
    printf("usage:\n");
    printf("  injector unload [modname] [procname]\n");
    printf("  injector inject [dllpath] [procname]\n");
    printf("  injector reload [dllpath] [procname] [modname]\n");
    printf("  injector watch [dllpath] [procname] [modname]\n");
    printf("defaults: mod cheat.dll, proc OAR-Win64-Shipping.exe\n");
}

static bool ieq(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
    return true;
}

static DWORD find_proc(const std::vector<std::string>& names) {
    DWORD pids[1024] = {};
    DWORD needed = 0;
    if (!EnumProcesses(pids, sizeof(pids), &needed)) return 0;
    DWORD count = needed / sizeof(DWORD);
    char path[MAX_PATH];
    std::vector<std::pair<DWORD, std::string>> procs;
    for (DWORD i = 0; i < count; ++i) {
        HANDLE hp = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pids[i]);
        if (!hp) continue;
        DWORD n = GetModuleFileNameExA(hp, nullptr, path, sizeof(path));
        CloseHandle(hp);
        if (!n) continue;
        std::string full = path;
        size_t p = full.find_last_of("\\/");
        procs.push_back({ pids[i], (p == std::string::npos) ? full : full.substr(p + 1) });
    }
    for (auto& nm : names) {
        for (auto& pr : procs) {
            if (ieq(pr.second, nm)) return pr.first;
        }
    }
    return 0;
}

static uintptr_t remote_mod_base(DWORD pid, const std::string& modname) {
    HANDLE hp = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hp) return 0;
    HMODULE mods[1024];
    DWORD needed = 0;
    uintptr_t base = 0;
    char path[MAX_PATH];
    if (EnumProcessModules(hp, mods, sizeof(mods), &needed)) {
        DWORD count = needed / sizeof(HMODULE);
        if (count > 1024) count = 1024;
        for (DWORD i = 0; i < count; ++i) {
            if (!GetModuleFileNameExA(hp, mods[i], path, sizeof(path))) continue;
            std::string full = path;
            size_t p = full.find_last_of("\\/");
            std::string bn = (p == std::string::npos) ? full : full.substr(p + 1);
            if (ieq(bn, modname)) { base = (uintptr_t)mods[i]; break; }
        }
    }
    CloseHandle(hp);
    return base;
}

static bool file_writable(const std::string& path) {
    HANDLE h = CreateFileA(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    CloseHandle(h);
    return true;
}

static std::string resolve_dll(const char* arg) {
    if (arg && arg[0]) return arg;
    const char* cands[] = {
        "bin\\Developer\\cheat.dll",
        "..\\bin\\Developer\\cheat.dll",
        "..\\..\\bin\\Developer\\cheat.dll",
    };
    char exe[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exe, sizeof(exe));
    std::string dir = exe;
    size_t p = dir.find_last_of("\\/");
    std::string exedir = (p == std::string::npos) ? "" : dir.substr(0, p + 1);
    for (auto c : cands) {
        DWORD a = GetFileAttributesA(c);
        if (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY)) return c;
        if (!exedir.empty()) {
            std::string q = exedir + c;
            a = GetFileAttributesA(q.c_str());
            if (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY)) return q;
        }
    }
    return cands[0];
}

static bool read_remote(HANDLE hp, uintptr_t addr, void* out, size_t n) {
    SIZE_T got = 0;
    return ReadProcessMemory(hp, (LPCVOID)addr, out, n, &got) && got == n;
}

static uintptr_t remote_export(HANDLE hp, uintptr_t base, const char* name) {
    uint8_t dos[64] = {};
    if (!read_remote(hp, base, dos, sizeof(dos))) return 0;
    if (*(uint16_t*)dos != 0x5A4D) return 0;
    uint32_t peoff = *(uint32_t*)(dos + 0x3C);
    uint8_t nth[8] = {};
    if (!read_remote(hp, base + peoff, nth, sizeof(nth))) return 0;
    if (*(uint32_t*)nth != 0x00004550) return 0;
    uint16_t magic = 0;
    if (!read_remote(hp, base + peoff + 24, &magic, sizeof(magic))) return 0;
    uintptr_t expdir = 0;
    uint32_t expsz = 0;
    if (magic == 0x20B) {
        uint8_t dd[8] = {};
        if (!read_remote(hp, base + peoff + 24 + 112, dd, sizeof(dd))) return 0;
        expdir = base + *(uint32_t*)dd;
        expsz = *(uint32_t*)(dd + 4);
    } else if (magic == 0x10B) {
        uint8_t dd[8] = {};
        if (!read_remote(hp, base + peoff + 24 + 96, dd, sizeof(dd))) return 0;
        expdir = base + *(uint32_t*)dd;
        expsz = *(uint32_t*)(dd + 4);
    } else return 0;
    if (!expdir || !expsz) return 0;
    struct EDIR { uint32_t flags, stamp; uint16_t maj, min; uint32_t name, base, n, nn, af, anf, aof; };
    EDIR ed{};
    if (!read_remote(hp, expdir, &ed, sizeof(ed))) return 0;
    if (!ed.nn || ed.nn > 4096) return 0;
    std::vector<uint32_t> names(ed.nn);
    if (!read_remote(hp, base + ed.anf, names.data(), ed.nn * 4)) return 0;
    std::vector<uint16_t> ords(ed.nn);
    if (!read_remote(hp, base + ed.aof, ords.data(), ed.nn * 2)) return 0;
    std::vector<uint32_t> funcs(ed.n);
    if (!read_remote(hp, base + ed.af, funcs.data(), ed.n * 4)) return 0;
    char sname[64];
    for (uint32_t i = 0; i < ed.nn; ++i) {
        memset(sname, 0, sizeof(sname));
        if (!read_remote(hp, base + names[i], sname, sizeof(sname) - 1)) continue;
        if (strcmp(sname, name) == 0) {
            uint16_t o = ords[i];
            if ((uint32_t)o >= ed.n) return 0;
            return base + funcs[o];
        }
    }
    return 0;
}

static std::string dll_path_for_mod(DWORD pid, const std::string& modname) {
    HANDLE hp = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hp) return "";
    HMODULE mods[1024];
    DWORD needed = 0;
    std::string path;
    char buf[MAX_PATH];
    if (EnumProcessModules(hp, mods, sizeof(mods), &needed)) {
        DWORD count = needed / sizeof(HMODULE);
        if (count > 1024) count = 1024;
        for (DWORD i = 0; i < count; ++i) {
            if (!GetModuleFileNameExA(hp, mods[i], buf, sizeof(buf))) continue;
            std::string full = buf;
            size_t p = full.find_last_of("\\/");
            std::string bn = (p == std::string::npos) ? full : full.substr(p + 1);
            if (ieq(bn, modname)) { path = full; break; }
        }
    }
    CloseHandle(hp);
    return path;
}

static int do_unload(DWORD pid, const std::string& modname) {
    uintptr_t base = remote_mod_base(pid, modname);
    if (!base) { printf("module %s not loaded\n", modname.c_str()); return 0; }
    HANDLE hp = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_READ, FALSE, pid);
    if (!hp) { printf("OpenProcess failed %lu\n", GetLastError()); return 3; }
    uintptr_t fn = remote_export(hp, base, "unload");
    if (!fn) { printf("export unload not found in loaded %s\n", modname.c_str()); CloseHandle(hp); return 2; }
    HANDLE th = CreateRemoteThread(hp, nullptr, 0, (LPTHREAD_START_ROUTINE)fn, nullptr, 0, nullptr);
    if (!th) { printf("CreateRemoteThread failed %lu\n", GetLastError()); CloseHandle(hp); return 4; }
    WaitForSingleObject(th, 3000);
    CloseHandle(th);
    std::string disk = dll_path_for_mod(pid, modname);
    int rc = 5;
    for (int i = 0; i < 80; ++i) {
        if (!remote_mod_base(pid, modname)) {
            if (disk.empty() || file_writable(disk)) {
                printf("unloaded, dll writable\n");
                rc = 0;
                break;
            }
            Sleep(250);
            continue;
        }
        Sleep(250);
    }
    CloseHandle(hp);
    if (rc) printf("timeout waiting for unload\n");
    return rc;
}

static int do_inject(DWORD pid, const std::string& dllpath, const std::string& modname) {
    if (remote_mod_base(pid, modname)) { printf("module %s already loaded\n", modname.c_str()); return 0; }
    char full[MAX_PATH] = {};
    if (!GetFullPathNameA(dllpath.c_str(), sizeof(full), full, nullptr)) {
        printf("bad dll path\n");
        return 2;
    }
    HANDLE hp = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!hp) { printf("OpenProcess failed %lu\n", GetLastError()); return 3; }
    size_t n = strlen(full) + 1;
    void* mem = VirtualAllocEx(hp, nullptr, n, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!mem) { printf("VirtualAllocEx failed %lu\n", GetLastError()); CloseHandle(hp); return 4; }
    int rc = 5;
    if (!WriteProcessMemory(hp, mem, full, n, nullptr)) {
        printf("WriteProcessMemory failed %lu\n", GetLastError());
    } else {
        HMODULE k32 = GetModuleHandleA("kernel32.dll");
        FARPROC ll = k32 ? GetProcAddress(k32, "LoadLibraryA") : nullptr;
        if (!ll) {
            printf("LoadLibraryA resolve failed\n");
        } else {
            HANDLE th = CreateRemoteThread(hp, nullptr, 0, (LPTHREAD_START_ROUTINE)ll, mem, 0, nullptr);
            if (!th) {
                printf("CreateRemoteThread failed %lu\n", GetLastError());
            } else {
                WaitForSingleObject(th, 15000);
                DWORD code = 0;
                GetExitCodeThread(th, &code);
                CloseHandle(th);
                if (code && remote_mod_base(pid, modname)) {
                    printf("injected %s\n", modname.c_str());
                    rc = 0;
                } else {
                    printf("remote LoadLibrary failed\n");
                }
            }
        }
    }
    VirtualFreeEx(hp, mem, 0, MEM_RELEASE);
    CloseHandle(hp);
    return rc;
}

static bool file_stamp(const std::string& path, uint64_t& out) {
    WIN32_FILE_ATTRIBUTE_DATA d{};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &d)) return false;
    uint64_t mt = ((uint64_t)d.ftLastWriteTime.dwHighDateTime << 32) | d.ftLastWriteTime.dwLowDateTime;
    uint64_t sz = ((uint64_t)d.nFileSizeHigh << 32) | d.nFileSizeLow;
    out = mt ^ (sz * 1000000007ull);
    return true;
}

static int do_watch(const std::string& dllpath, const std::vector<std::string>& procs, const std::string& modname) {
    char full[MAX_PATH] = {};
    if (!GetFullPathNameA(dllpath.c_str(), sizeof(full), full, nullptr)) {
        printf("bad dll path\n");
        return 2;
    }
    printf("watching %s\n", full);
    uint64_t stamp = 0;
    file_stamp(full, stamp);
    DWORD lastPid = 0;
    bool haveGame = false;
    for (;;) {
        DWORD pid = find_proc(procs);
        if (!pid) {
            if (haveGame) { printf("game closed, waiting...\n"); haveGame = false; }
            lastPid = 0;
            Sleep(1000);
            continue;
        }
        haveGame = true;
        if (pid != lastPid) {
            printf("game found pid %lu\n", (unsigned long)pid);
            Sleep(4000);
            if (!remote_mod_base(pid, modname))
                do_inject(pid, full, modname);
            lastPid = pid;
            file_stamp(full, stamp);
            Sleep(500);
            continue;
        }
        uint64_t cur = 0;
        if (file_stamp(full, cur) && cur != stamp) {
            Sleep(1000);
            uint64_t cur2 = 0;
            if (!file_stamp(full, cur2) || cur2 != cur || !file_writable(full)) {
                Sleep(500);
                continue;
            }
            printf("rebuild detected, reloading...\n");
            if (remote_mod_base(pid, modname)) {
                if (do_unload(pid, modname)) { Sleep(1000); continue; }
            }
            do_inject(pid, full, modname);
            file_stamp(full, stamp);
        }
        Sleep(500);
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) { usage(); return 1; }
    std::string verb = argv[1];
    auto proc_list = [&](int idx) {
        std::vector<std::string> v;
        if (idx < argc && argv[idx][0]) v.push_back(argv[idx]);
        v.push_back(PRJ_STR("OAR-Win64-Shipping.exe"));
        v.push_back(PRJ_STR("OAR.exe"));
        return v;
    };
    if (verb == "unload") {
        std::string mod = (argc > 2 && argv[2][0]) ? argv[2] : PRJ_STR("cheat.dll");
        DWORD pid = find_proc(proc_list(3));
        if (!pid) { printf("game process not found\n"); return 6; }
        printf("pid %lu\n", (unsigned long)pid);
        return do_unload(pid, mod);
    }
    if (verb == "inject") {
        std::string dll = resolve_dll(argc > 2 ? argv[2] : nullptr);
        DWORD pid = find_proc(proc_list(3));
        if (!pid) { printf("game process not found\n"); return 6; }
        printf("pid %lu\n", (unsigned long)pid);
        return do_inject(pid, dll, PRJ_STR("cheat.dll"));
    }
    if (verb == "reload") {
        std::string dll = resolve_dll(argc > 2 ? argv[2] : nullptr);
        std::string mod = (argc > 4 && argv[4][0]) ? argv[4] : PRJ_STR("cheat.dll");
        DWORD pid = find_proc(proc_list(3));
        if (!pid) { printf("game process not found\n"); return 6; }
        printf("pid %lu\n", (unsigned long)pid);
        int r = do_unload(pid, mod);
        if (r) return r;
        return do_inject(pid, dll, mod);
    }
    if (verb == "watch") {
        std::string dll = resolve_dll(argc > 2 ? argv[2] : nullptr);
        std::string mod = (argc > 4 && argv[4][0]) ? argv[4] : PRJ_STR("cheat.dll");
        return do_watch(dll, proc_list(3), mod);
    }
    usage();
    return 1;
}

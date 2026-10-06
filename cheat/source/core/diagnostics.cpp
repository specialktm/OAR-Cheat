#include "pch/pch.h"
#include "core/diagnostics.hpp"
#include "core/memory.hpp"
#include "core/offsets.hpp"
#include "core/pattern.hpp"
#include "core/actor_iter.hpp"
#include "render/dx11_hook.hpp"
#include "SDK/Basic.hpp"
#include "SDK/CoreUObject_classes.hpp"
#include "logger/logger.hpp"

namespace diag {

static Report g_rep{};
static bool g_done = false;
static std::mutex g_repMtx;

// ---- SEH leaves (no string/vector locals) ----
static uintptr_t seh_scan(const char* s) {
    __try { return sig::scan_module(s); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
static uintptr_t seh_read_ptr(uintptr_t absAddr) {
    __try { return *reinterpret_cast<uintptr_t*>(absAddr); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
static int seh_gnum() {
    __try { return SDK::UObject::GObjects->Num(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
static void* seh_pc() {
    __try { return actors::local_player_controller(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

static void log_addr(const char* what, uintptr_t absAddr, bool extra_valid, const char* extra) {
    if (!g_logger) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)absAddr);
    if (extra)
        g_logger->send(levels::debug, "[diag] {} = {} ({}: {})", what, buf,
            extra, extra_valid ? "valid" : "NULL");
    else
        g_logger->send(levels::debug, "[diag] {} = {}", what, buf);
}

void report() {
    if (g_done) return;
    g_done = true;
    Report r{};
    r.imageBase = mem::image_base();
    if (g_logger) {
        char b[32];
        snprintf(b, sizeof(b), "0x%llX", (unsigned long long)r.imageBase);
        g_logger->send(levels::info, "[diag] image base = {}", b);
    }

    uintptr_t gwHit = seh_scan(offsets::SIG_GWORLD);
    r.gwSigHit = gwHit != 0;
    r.gwRva = mem::gworld_rva();
    r.gwAbs = r.imageBase + r.gwRva;
    r.gwPtr = seh_read_ptr(r.gwAbs);
    r.gwValid = IS_VALID_PTR((void*)r.gwPtr);
    log_addr(r.gwSigHit ? "GWorld (sig)" : "GWorld (fallback RVA)",
        r.gwAbs, r.gwValid, "deref");

    uintptr_t goHit = seh_scan(offsets::SIG_GOBJECTS);
    r.goSigHit = goHit != 0;
    r.goRva = mem::gobjects_rva();
    r.goAbs = r.imageBase + r.goRva;
    r.goNum = seh_gnum();
    log_addr(r.goSigHit ? "GObjects (sig)" : "GObjects (fallback RVA)", r.goAbs,
        r.goNum > 0, "count>0");
    if (g_logger) g_logger->send(levels::debug, "[diag] GObjects->Num() = {}", r.goNum);

    uintptr_t peHit = seh_scan(offsets::SIG_PROCESS_EVENT);
    r.peSigHit = peHit != 0;
    r.peAbs = r.imageBase + (uintptr_t)offsets::ProcessEvent;
    log_addr("ProcessEvent (offset)", r.peAbs, r.peSigHit, "sig-hit");

    void* pc = seh_pc();
    r.pcValid = IS_VALID_PTR(pc);
    if (g_logger) g_logger->send(levels::debug, "[diag] local PlayerController: {}",
        r.pcValid ? "valid" : "NULL (join a lobby)");

    r.presentTarget = dx11::present_target();
    r.presentValid = dx11::present_hooked();
    log_addr("Present hook target", (uintptr_t)r.presentTarget, r.presentValid, "hooked");
    r.resizeTarget = dx11::resize_target();
    r.resizeValid = dx11::resize_hooked();
    log_addr("ResizeBuffers hook target", (uintptr_t)r.resizeTarget, r.resizeValid, "hooked");

    { std::lock_guard<std::mutex> lk(g_repMtx); g_rep = r; }
}

Report last() {
    std::lock_guard<std::mutex> lk(g_repMtx);
    return g_rep;
}

} // namespace diag

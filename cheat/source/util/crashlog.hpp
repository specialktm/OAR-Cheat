#pragma once
#include "pch/pch.h"
#include <windows.h>
#include <dbghelp.h>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <exception>
#include <chrono>
#include <ctime>
#include "../util/files.hpp"
namespace crashlog {
inline std::mutex& mtx() { static std::mutex m; return m; }
inline std::filesystem::path& dir() { static std::filesystem::path d; return d; }
inline bool& ready() { static bool r = false; return r; }
inline LPTOP_LEVEL_EXCEPTION_FILTER& prev_filter() { static LPTOP_LEVEL_EXCEPTION_FILTER f = nullptr; return f; }
inline terminate_handler& prev_term() { static terminate_handler f = nullptr; return f; }
inline void*& prev_inv_p() { static void* f = nullptr; return f; }
inline std::string now_stamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{}; localtime_s(&tm, &t);
    char b[20]; std::strftime(b, sizeof(b), "%Y-%m-%d %H:%M:%S", &tm);
    return b;
}
inline void append_raw(const std::string& file, const std::string& line) {
    std::lock_guard<std::mutex> l(mtx());
    if (dir().empty()) return;
    std::ofstream f(dir() / file, std::ios::app);
    if (f) { f << line << "\n"; }
}
inline void append_line(const char* label, const std::string& msg) {
    append_raw("console.log", "[" + now_stamp() + "][" + (label ? label : "?") + "] " + msg);
}
static void write_stack() {
    void* frames[48];
    USHORT n = CaptureStackBackTrace(0, 48, frames, nullptr);
    HANDLE proc = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    if (SymInitialize(proc, nullptr, TRUE)) {
        char symbuf[sizeof(SYMBOL_INFO) + 256];
        SYMBOL_INFO* sym = (SYMBOL_INFO*)symbuf;
        sym->SizeOfStruct = sizeof(SYMBOL_INFO); sym->MaxNameLen = 255;
        for (USHORT i = 0; i < n; i++) {
            DWORD64 disp = 0;
            char line[320];
            if (SymFromAddr(proc, (DWORD64)frames[i], &disp, sym))
                snprintf(line, sizeof(line), "#%02d %p %s+0x%llx", i, frames[i], sym->Name, disp);
            else
                snprintf(line, sizeof(line), "#%02d %p <no symbol>", i, frames[i]);
            append_raw("crash.log", line);
        }
        SymCleanup(proc);
    } else {
        for (USHORT i = 0; i < n; i++) {
            char line[64]; snprintf(line, sizeof(line), "#%02d %p", i, frames[i]);
            append_raw("crash.log", line);
        }
    }
}
static LONG WINAPI filter(EXCEPTION_POINTERS* ep) {
    DWORD code = ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0;
    void* addr = ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionAddress : nullptr;
    char head[256];
    snprintf(head, sizeof(head), "==== CRASH %s code=0x%08X addr=%p ====", now_stamp().c_str(), code, addr);
    append_raw("crash.log", head);
    write_stack();
    append_raw("crash.log", "==== END CRASH ====");
    if (prev_filter()) return prev_filter()(ep);
    return EXCEPTION_EXECUTE_HANDLER;
}
static void on_terminate() {
    try {
        if (auto e = std::current_exception()) std::rethrow_exception(e);
        else append_raw("crash.log", "==== TERMINATE " + now_stamp() + " (no active exception) ====");
    } catch (const std::exception& ex) {
        append_raw("crash.log", "==== TERMINATE " + now_stamp() + " std: " + ex.what() + " ====");
    } catch (...) {
        append_raw("crash.log", "==== TERMINATE " + now_stamp() + " (unknown non-std exception) ====");
    }
    write_stack();
    append_raw("crash.log", "==== END TERMINATE ====");
    if (prev_term()) { prev_term()(); return; }
    std::abort();
}
static void __cdecl on_invparam(const wchar_t* expr, const wchar_t* func,
    const wchar_t* file, unsigned int line, uintptr_t) {
    char buf[512];
    snprintf(buf, sizeof(buf), "==== INVPARAM %s expr=%ls func=%ls file=%ls line=%u ====",
        now_stamp().c_str(), expr ? expr : L"?", func ? func : L"?",
        file ? file : L"?", line);
    append_raw("crash.log", buf);
    write_stack();
    append_raw("crash.log", "==== END INVPARAM ====");
}
inline void init() {
    try {
        std::filesystem::path d = fileManager::baseDir();
        std::error_code ec;
        std::filesystem::create_directories(d, ec);
        dir() = d;
        ready() = true;
    } catch (...) { return; }
    prev_filter() = SetUnhandledExceptionFilter(filter);
    prev_term() = std::set_terminate(on_terminate);
    prev_inv_p() = (void*)_set_invalid_parameter_handler(on_invparam);
    append_raw("console.log", "---- session start " + now_stamp() + " ----");
}
inline void shutdown() {
    if (!ready()) return;
    ready() = false;
    SetUnhandledExceptionFilter(prev_filter());
    if (prev_term()) std::set_terminate(prev_term());
    if (prev_inv_p()) _set_invalid_parameter_handler((_invalid_parameter_handler)prev_inv_p());
    prev_filter() = nullptr; prev_term() = nullptr; prev_inv_p() = nullptr;
}
}

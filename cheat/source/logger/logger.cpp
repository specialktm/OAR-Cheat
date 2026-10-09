#include "pch/pch.h"
#include "logger/logger.hpp"

void logger::initialize(const char* title)
{

    if (GetConsoleWindow())
        m_owned = false;
    else
        m_owned = AllocConsole() != 0;

    freopen_s(&m_file, "CONOUT$", "w", stdout);
    freopen_s(&m_file, "CONOUT$", "w", stderr);
    freopen_s(&m_file, "CONIN$", "r", stdin);
    std::cout.clear();
    std::clog.clear();
    std::cerr.clear();
    std::cin.clear();

    h_console_out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h_console_out == INVALID_HANDLE_VALUE)
        return;

    if (!GetConsoleMode(h_console_out, &m_console_mode))
        return;

    m_console_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(h_console_out, m_console_mode);

    SetStdHandle(STD_OUTPUT_HANDLE, h_console_out);
    SetStdHandle(STD_ERROR_HANDLE, h_console_out);
    SetStdHandle(STD_INPUT_HANDLE, GetStdHandle(STD_INPUT_HANDLE));

    std::wcout.clear();
    std::wclog.clear();
    std::wcerr.clear();
    std::wcin.clear();

    SetConsoleTitleA(title);
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    SetLayeredWindowAttributes(GetConsoleWindow(), NULL, 255, LWA_ALPHA);

#ifdef Developer
    set_level(levels::developer);
#endif
    this->send(levels::info, "Logger Created: {}", title);
}

void logger::uninitialize()
{
    this->send(levels::info, "Goodbye, You may now close this window.");

    if (m_file) fclose(m_file);
    m_file = nullptr;

    if (m_owned) FreeConsole();
    m_owned = false;
}

void logger::old_send(levels level, const std::string& msg)
{
    std::cout << level_to_string(level) << " " << msg << "\33[m" << '\n';
}

void logger::set_level(levels level)
{
    m_log_level = level;
}

std::string logger::level_to_string(levels level)
{
    switch (level)
    {
        case levels::developer:
            return "\33[38;5;105m[Developer]";
        case levels::debug:
            return "\33[38;5;63;5m[Debug]\33[m";
        case levels::info:
            return "\33[1m\33[38;5;237m[Info]";
        case levels::success:
            return "\33[1m\33[38;5;119m[Success]";
        case levels::warn:
            return "\33[1m\33[38;5;136m[Warn]";
        case levels::error:
            return "\33[1m\33[38;5;196m[Error]\33[m";
        case levels::critical:
            return "\33[1m\33[38;5;124m[Critical]\33[m";
        default:
            return "Unhandled Level";
    }
}

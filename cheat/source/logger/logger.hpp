#pragma once
#include "pch/pch.h"
#define APP_NAME "OS Cheats | OAR | "
#define APP_VERSION "1.1"

enum class levels
{
    developer,
    debug,
    info,
    success,
    warn,
    error,
    critical
};

class logger
{
public:
    logger()
    {
        this->initialize(APP_NAME APP_VERSION);
    }
    ~logger()
    {
        this->uninitialize();
    }
public:
    void old_send(levels level, const std::string& msg);

    void set_level(levels level);
    std::string level_to_string(levels level);


    template <typename... args>
    void send(levels level, const std::string& format, args... msg)
    {
        std::string formatted = std::vformat(format, std::make_format_args(msg...));
        old_send(level, formatted);
    }
private:
    void initialize(const char* title);
    void uninitialize();

    levels m_log_level = levels::info;
    FILE* m_file = nullptr;
    HANDLE h_console_out = nullptr;
    DWORD m_console_mode = 0;
};

inline logger* g_logger;
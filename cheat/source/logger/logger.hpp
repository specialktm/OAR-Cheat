#pragma once
/*
* File Name: Logger.hpp
* Purpose: Contains the function definitions for the logger.
* Author VoDKa
* Date: 12.07.2024 | EU Format | 07/12/2024
* Time: 01:48 : 1:48 AM
*/

// Includes
#include "pch/pch.h"

#define LOGGER_NAME "OS Cheats | On Top! | v1.0"


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
        this->initialize(LOGGER_NAME);
    }
    ~logger()
    {
        this->flush();
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
    void flush();
    void initialize(const char* title);
    void uninitialize();

    levels m_log_level = levels::info;
    FILE* m_file = nullptr;
    HANDLE h_console_out = nullptr;
    DWORD m_console_mode = 0;
};

inline std::shared_ptr<logger> g_logger;

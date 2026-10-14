#pragma once
#include "pch/pch.h"
#include <string>
#include <vector>

namespace lua
{
std::string scripts_dir();
std::vector<std::string> list_scripts();

struct run_info
{
    std::string name;
    bool running;
};
std::vector<run_info> list_runs();

bool start(const std::string& name);
bool execute_string(const std::string& source);
bool read_file(const std::string& name, std::string& out);
bool write_file(const std::string& name, const std::string& data);

void stop(const std::string& name);
void stop_all();
bool is_running(const std::string& name);

void clear_output();
std::vector<std::string> get_output();

void snapshot();
}

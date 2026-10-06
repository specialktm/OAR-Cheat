#pragma once
#include "pch/pch.h"
// Config persistence: simple key=value files under %TEMP%/opencode.
// Render thread only (file IO, no game calls). Returns true on success.
namespace cfgio {
std::string path_for(const std::string& name);
bool save(const std::string& name, std::string& err);
bool load(const std::string& name, std::string& err);
} // namespace cfgio

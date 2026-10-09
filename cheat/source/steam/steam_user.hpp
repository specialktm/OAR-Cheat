#pragma once
#include "pch/pch.h"

class steamServiceHelper {
public:
    steamServiceHelper() = delete;

    static void fetch();
    static void retry_avatar();
    static void render_upload();
    static const char* name();
    static const char* id_text();
    static void* avatar();
    static bool avatar_failed();
    static bool resolved();
    static void shutdown();
};

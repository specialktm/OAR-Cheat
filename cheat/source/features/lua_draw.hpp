#pragma once
#include "../../vendor/imgui/imgui.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <algorithm>
#include <cstdint>

namespace drawing
{
enum class kind_t
{
    Line,
    Text,
    Square,
    Circle,
    Triangle
};

struct item_t
{
    int id = 0;
    kind_t kind = kind_t::Line;
    std::string owner;
    bool visible = false;
    float r = 1.f, g = 1.f, b = 1.f;
    float transparency = 0.f;
    float x1 = 0.f, y1 = 0.f;
    float x2 = 0.f, y2 = 0.f;
    float x3 = 0.f, y3 = 0.f;
    float thickness = 1.f;
    float fontsize = 14.f;
    float radius = 20.f;
    int numsides = 32;
    bool filled = false;
    bool center = false;
    bool outline = false;
    float rounding = 0.f;
    int zindex = 0;
    std::string text;
};

inline std::mutex& mtx()
{
    static std::mutex m;
    return m;
}

inline std::unordered_map<int, item_t>& items()
{
    static std::unordered_map<int, item_t> m;
    return m;
}

inline int& next_id()
{
    static int n = 0;
    return n;
}

inline int create(const std::string& owner, const std::string& type)
{
    kind_t k;
    if (type == "Line") k = kind_t::Line;
    else if (type == "Text") k = kind_t::Text;
    else if (type == "Square") k = kind_t::Square;
    else if (type == "Circle") k = kind_t::Circle;
    else if (type == "Triangle") k = kind_t::Triangle;
    else return 0;
    std::lock_guard<std::mutex> lock(mtx());
    const int id = ++next_id();
    item_t it;
    it.id = id;
    it.kind = k;
    it.owner = owner;
    items()[id] = it;
    return id;
}

inline bool get(int id, item_t& out)
{
    std::lock_guard<std::mutex> lock(mtx());
    auto it = items().find(id);
    if (it == items().end())
        return false;
    out = it->second;
    return true;
}

inline bool set(int id, const item_t& in)
{
    std::lock_guard<std::mutex> lock(mtx());
    auto it = items().find(id);
    if (it == items().end())
        return false;
    item_t copy = in;
    copy.id = id;
    copy.owner = it->second.owner;
    copy.kind = it->second.kind;
    it->second = copy;
    return true;
}

inline void remove(int id)
{
    std::lock_guard<std::mutex> lock(mtx());
    items().erase(id);
}

inline void clear_owner(const std::string& owner)
{
    std::lock_guard<std::mutex> lock(mtx());
    for (auto it = items().begin(); it != items().end();)
    {
        if (it->second.owner == owner)
            it = items().erase(it);
        else
            ++it;
    }
}

inline std::size_t count()
{
    std::lock_guard<std::mutex> lock(mtx());
    return items().size();
}

inline void render_all(ImDrawList* draw)
{
    if (!draw)
        return;
    std::vector<item_t> snap;
    {
        std::lock_guard<std::mutex> lock(mtx());
        snap.reserve(items().size());
        for (auto& kv : items())
            if (kv.second.visible)
                snap.push_back(kv.second);
    }
    if (snap.empty())
        return;
    std::stable_sort(snap.begin(), snap.end(), [](const item_t& a, const item_t& b) {
        return a.zindex < b.zindex;
    });
    ImFont* font = ImGui::GetFont();
    const float ui_font = ImGui::GetFontSize();
    for (auto& it : snap)
    {
        float a = it.transparency;
        if (a < 0.f) a = 0.f;
        if (a > 1.f) a = 1.f;
        if (a <= 0.f)
            continue;
        const ImU32 col = IM_COL32((int)(it.r * 255.f), (int)(it.g * 255.f), (int)(it.b * 255.f), (int)(a * 255.f));
        const ImU32 out_col = IM_COL32(0, 0, 0, (int)(a * 255.f));
        switch (it.kind)
        {
        case kind_t::Line:
            draw->AddLine(ImVec2(it.x1, it.y1), ImVec2(it.x2, it.y2), col, it.thickness);
            break;
        case kind_t::Square:
            if (it.filled)
                draw->AddRectFilled(ImVec2(it.x1, it.y1), ImVec2(it.x1 + it.x2, it.y1 + it.y2), col, it.rounding);
            else
                draw->AddRect(ImVec2(it.x1, it.y1), ImVec2(it.x1 + it.x2, it.y1 + it.y2), col, it.rounding, 0, it.thickness);
            break;
        case kind_t::Circle:
            if (it.filled)
                draw->AddCircleFilled(ImVec2(it.x1, it.y1), it.radius, col, it.numsides > 0 ? it.numsides : 32);
            else
                draw->AddCircle(ImVec2(it.x1, it.y1), it.radius, col, it.numsides > 0 ? it.numsides : 32, it.thickness);
            break;
        case kind_t::Text:
        {
            const float size = it.fontsize > 0.f ? it.fontsize : ui_font;
            ImVec2 pos(it.x1, it.y1);
            if (it.center)
            {
                const ImVec2 sz = font->CalcTextSizeA(size, FLT_MAX, 0.f, it.text.c_str());
                pos.x -= sz.x * 0.5f;
            }
            if (it.outline)
            {
                for (int dx = -1; dx <= 1; ++dx)
                    for (int dy = -1; dy <= 1; ++dy)
                        if (dx || dy)
                            draw->AddText(font, size, ImVec2(pos.x + (float)dx, pos.y + (float)dy), out_col, it.text.c_str());
            }
            draw->AddText(font, size, pos, col, it.text.c_str());
            break;
        }
        case kind_t::Triangle:
            if (it.filled)
                draw->AddTriangleFilled(ImVec2(it.x1, it.y1), ImVec2(it.x2, it.y2), ImVec2(it.x3, it.y3), col);
            else
                draw->AddTriangle(ImVec2(it.x1, it.y1), ImVec2(it.x2, it.y2), ImVec2(it.x3, it.y3), col, it.thickness);
            break;
        }
    }
}
}

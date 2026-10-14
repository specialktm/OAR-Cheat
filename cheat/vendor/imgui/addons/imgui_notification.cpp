#include "imgui_notification.h"
#include "../../../source/render/blur.hpp"

#include <algorithm>
#include <cmath>

namespace {
float snap_px(float v) { return floorf(v + 0.5f); }
float fast_smooth(float current, float target, float speed, float dt)
{
    return ImLerp(current, target, 1.0f - expf(-speed * dt));
}
}

namespace notify {

float Notification::getAlpha() const
{
    if (dying)
        return slideOut;

    if (elapsed < fadeIn)
        return elapsed / fadeIn;

    return 1.0f;
}

float Notification::getTimerRemaining() const
{
    if (isProgress())
        return ImClamp(progressValue, 0.0f, 1.0f);

    float activeTime = lifetime - fadeIn - fadeOut;
    float activeElapsed = ImClamp(elapsed - fadeIn, 0.0f, activeTime);
    if (activeTime <= 0.0f)
        return 0.0f;
    return 1.0f - (activeElapsed / activeTime);
}

bool Notification::isExpired() const
{
    if (isProgress())
        return false;
    return elapsed >= lifetime - fadeOut;
}

bool Notification::isProgress() const
{
    return progressValue >= 0.0f && progressValue < 1.0f;
}

float NotificationManager::getScale() const
{
    return ImGui::GetIO().DisplaySize.y / referenceHeight;
}

ImVec4 NotificationManager::defaultColor(Type type) const
{
    switch (type)
    {
    case Type::Dev:    return ImVec4(0.0f, 0.75f, 1.0f, 1.0f);
    case Type::Debug:        return ImVec4(0.6f, 0.6f, 0.65f, 1.0f);
    case Type::Info:         return ImVec4(66.f / 255.f, 150.f / 255.f, 240.f / 255.f, 1.0f);
    case Type::Success:      return ImVec4(45.f / 255.f, 195.f / 255.f, 110.f / 255.f, 1.0f);
    case Type::Warning:      return ImVec4(235.f / 255.f, 190.f / 255.f, 90.f / 255.f, 1.0f);
    case Type::Error:        return ImVec4(225.f / 255.f, 70.f / 255.f, 85.f / 255.f, 1.0f);
    case Type::Critical:     return ImVec4(255.f / 255.f, 90.f / 255.f, 60.f / 255.f, 1.0f);
    case Type::Announcement: return ImVec4(235.f / 255.f, 150.f / 255.f, 60.f / 255.f, 1.0f);
    }
    return ImVec4(1, 1, 1, 1);
}

std::string NotificationManager::typeHeader(Type type) const
{
    switch (type)
    {
    case Type::Dev:    return "Developer";
    case Type::Debug:        return "Debug";
    case Type::Info:         return "Information";
    case Type::Success:      return "Success";
    case Type::Warning:      return "Warning";
    case Type::Error:        return "Error";
    case Type::Critical:     return "Critical Error";
    case Type::Announcement: return "Announcement";
    }
    return "Information";
}

std::string NotificationManager::typeIcon(Type type) const
{
    switch (type)
    {
    case Type::Dev:    return ICON_MDI_DEV_TO;
    case Type::Debug:        return ICON_MDI_BUG;
    case Type::Info:         return ICON_MDI_INFORMATION;
    case Type::Success:      return ICON_MDI_CHECK_CIRCLE;
    case Type::Warning:      return ICON_MDI_ALERT;
    case Type::Error:        return ICON_MDI_CLOSE_CIRCLE;
    case Type::Critical:     return ICON_MDI_PISTOL;
    case Type::Announcement: return ICON_MDI_BULLHORN;
    }
    return ICON_MDI_INFORMATION;
}

void NotificationManager::pushHistory(const Notification& n)
{
    HistoryEntry entry;
    entry.header = n.header;
    entry.message = n.message;
    entry.icon = n.icon;
    entry.type = n.type;
    entry.color = n.color;
    entry.duplicateCount = n.duplicateCount;
    m_History.emplace_back(std::move(entry));

    if (m_History.size() > maxHistory)
        m_History.erase(m_History.begin());
}

void NotificationManager::add(const std::string& message, Type type, float duration)
{
    Notification n;
    n.id = m_NextId++;
    n.header = typeHeader(type);
    n.message = message;
    n.icon = typeIcon(type);
    n.type = type;
    n.color = defaultColor(type);
    n.fadeIn = fadeInTime;
    n.fadeOut = fadeOutTime;
    n.lifetime = (duration > 0.0f ? duration : defaultDuration) + fadeInTime + fadeOutTime;

    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Pending.emplace_back(std::move(n));
}

void NotificationManager::addCustom(const std::string& header, const std::string& message, const ImVec4& color, const char* icon, Type type, float duration)
{
    Notification n;
    n.id = m_NextId++;
    n.header = header;
    n.message = message;
    n.icon = icon ? icon : ICON_MDI_INFORMATION;
    n.type = type;
    n.color = color;
    n.fadeIn = fadeInTime;
    n.fadeOut = fadeOutTime;
    n.lifetime = (duration > 0.0f ? duration : defaultDuration) + fadeInTime + fadeOutTime;

    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Pending.emplace_back(std::move(n));
}

uint32_t NotificationManager::addProgress(const std::string& header, const std::string& message, Type type)
{
    Notification n;
    n.id = m_NextId++;
    n.header = header;
    n.message = message;
    n.icon = typeIcon(type);
    n.type = type;
    n.color = defaultColor(type);
    n.fadeIn = fadeInTime;
    n.fadeOut = fadeOutTime;
    n.lifetime = 999999.0f;
    n.progressValue = 0.0f;

    uint32_t id = n.id;
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Pending.emplace_back(std::move(n));
    return id;
}

void NotificationManager::updateProgress(uint32_t id, float value)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (auto& n : m_Active)
    {
        if (n.id == id)
        {
            n.progressValue = ImClamp(value, 0.0f, 0.999f);
            return;
        }
    }
    for (auto& n : m_Pending)
    {
        if (n.id == id)
        {
            n.progressValue = ImClamp(value, 0.0f, 0.999f);
            return;
        }
    }
}

void NotificationManager::completeProgress(uint32_t id, float lingerDuration)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (auto& n : m_Active)
    {
        if (n.id == id)
        {
            n.progressValue = 1.0f;
            n.elapsed = 0.0f;
            n.lifetime = lingerDuration + n.fadeIn + n.fadeOut;
            return;
        }
    }
    for (auto& n : m_Pending)
    {
        if (n.id == id)
        {
            n.progressValue = 1.0f;
            n.elapsed = 0.0f;
            n.lifetime = lingerDuration + n.fadeIn + n.fadeOut;
            return;
        }
    }
}

float NotificationManager::estimateHeight(const Notification& n, ImFont* font, float fontSize, float headerFontSize, float iconFontSize, float padY, float lineThick, float headerGap, float iconGap, float wrapW, float progressBarH) const
{
    ImVec2 hdrSz = font->CalcTextSizeA(headerFontSize, FLT_MAX, 0.0f, n.header.c_str());
    ImVec2 msgSz = font->CalcTextSizeA(fontSize, wrapW, wrapW, n.message.c_str());
    ImVec2 icoSz = font->CalcTextSizeA(iconFontSize, FLT_MAX, 0.0f, n.icon.c_str());
    float h = lineThick + padY + hdrSz.y + headerGap + msgSz.y + iconGap + icoSz.y + padY;
    if (n.progressValue >= 0.0f)
        h += progressBarH + headerGap;
    return h;
}

void NotificationManager::drainPending(float availableH, float scale, ImFont* font, float fontSize, float headerFontSize, float iconFontSize, float padY, float lineThick, float headerGap, float iconGap, float wrapW)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    float scaledSpacing = spacing * scale;
    float progressBarH = 3.0f * scale;

    while (!m_Pending.empty())
    {
        float h = estimateHeight(m_Pending.front(), font, fontSize, headerFontSize, iconFontSize, padY, lineThick, headerGap, iconGap, wrapW, progressBarH);
        if (availableH < h + scaledSpacing)
            break;

        m_Active.emplace_back(std::move(m_Pending.front()));
        m_Pending.erase(m_Pending.begin());
        availableH -= (h + scaledSpacing);
    }
}

void NotificationManager::mergeDuplicates()
{
    for (size_t i = 0; i < m_Active.size();)
    {
        bool merged = false;
        for (size_t j = i + 1; j < m_Active.size();)
        {
            if (!m_Active[i].dying && !m_Active[j].dying &&
                m_Active[i].message == m_Active[j].message &&
                m_Active[i].type == m_Active[j].type &&
                m_Active[i].progressValue < 0.0f &&
                m_Active[j].progressValue < 0.0f)
            {
                m_Active[i].elapsed = 0.0f;
                m_Active[i].lifetime = ImMax(m_Active[i].lifetime, m_Active[j].lifetime);
                m_Active[i].duplicateCount += m_Active[j].duplicateCount;
                m_Active.erase(m_Active.begin() + j);
                merged = true;
            }
            else
            {
                ++j;
            }
        }
        if (!merged)
            ++i;
    }
}

void NotificationManager::render()
{
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImGuiIO& io = ImGui::GetIO();
    ImFont* font = ImGui::GetFont();
    ImVec2      screen = io.DisplaySize;
    float       scale = getScale();

    const float fontSize = ImGui::GetFontSize() * 0.82f * scale;
    const float headerFontSize = fontSize * 0.95f;
    const float iconFontSize = fontSize * 1.4f;
    const float badgeFontSize = fontSize * 0.75f;
    const float padX = 7.0f * scale;
    const float padY = 5.0f * scale;
    const float lineThick = snap_px(2.0f * scale);
    const float headerGap = 2.0f * scale;
    const float iconGap = 2.0f * scale;
    const float lineInset = 3.0f * scale;
    const float scaledMarginX = margin.x * scale;
    const float scaledMarginY = margin.y * scale;
    const float scaledSpacing = spacing * scale;
    const float maxBoxW = screen.x * maxWidthFraction;
    const float wrapW = ImMax(0.0f, maxBoxW - padX * 2.0f);
    const float totalAvailable = screen.y - scaledMarginY * 2.0f;
    const float scaledRound = rounding * scale;
    const float scaledBorder = snap_px(borderThickness * scale);
    const float progressBarH = snap_px(3.0f * scale);

    for (size_t i = 0; i < m_Active.size();)
    {
        Notification& n = m_Active[i];

        if (n.dying)
        {
            n.slideOut = fast_smooth(n.slideOut, 0.0f, 10.0f, io.DeltaTime);
            if (n.slideOut < 0.01f)
            {
                pushHistory(n);
                m_Active.erase(m_Active.begin() + i);
                continue;
            }
            ++i;
            continue;
        }

        n.elapsed += io.DeltaTime;
        if (n.isExpired())
        {
            n.dying = true;
            ++i;
            continue;
        }
        ++i;
    }

    mergeDuplicates();

    float usedH = 0.0f;
    for (auto& n : m_Active)
        usedH += estimateHeight(n, font, fontSize, headerFontSize, iconFontSize, padY, lineThick, headerGap, iconGap, wrapW, progressBarH) + scaledSpacing;

    drainPending(totalAvailable - usedH, scale, font, fontSize, headerFontSize, iconFontSize, padY, lineThick, headerGap, iconGap, wrapW);

    if (m_Active.empty())
        return;

    int   dir = (anchor == 2 || anchor == 3) ? -1 : 1;
    float yAccum = 0.0f;

    for (size_t i = 0; i < m_Active.size(); ++i)
    {
        Notification& n = m_Active[i];

        ImVec2 hdrSz = font->CalcTextSizeA(headerFontSize, FLT_MAX, 0.0f, n.header.c_str());
        ImVec2 msgSz = font->CalcTextSizeA(fontSize, wrapW, wrapW, n.message.c_str());
        ImVec2 icoSz = font->CalcTextSizeA(iconFontSize, FLT_MAX, 0.0f, n.icon.c_str());

        float contentW = ImMax(hdrSz.x, msgSz.x);
        float boxW = ImClamp(contentW + padX * 2.0f, 100.0f * scale, maxBoxW);
        float boxH = lineThick + padY + hdrSz.y + headerGap + msgSz.y + iconGap + icoSz.y + padY;
        if (n.progressValue >= 0.0f)
            boxH += progressBarH + headerGap;

        if (!n.heightInit)
        {
            n.lerpedHeight = 0.0f;
            n.heightInit = true;
        }
        n.lerpedHeight = fast_smooth(n.lerpedHeight, boxH, 12.0f, io.DeltaTime);
        n.slideProgress = fast_smooth(n.slideProgress, 1.0f, 8.0f, io.DeltaTime);

        float curH = snap_px(n.lerpedHeight);

        if (yAccum + curH + scaledMarginY > totalAvailable)
            break;

        ImVec2 boxMin;
        if (anchor == 1 || anchor == 3)
            boxMin.x = screen.x - scaledMarginX - boxW;
        else
            boxMin.x = scaledMarginX;

        if (dir > 0)
            boxMin.y = scaledMarginY + yAccum;
        else
            boxMin.y = screen.y - scaledMarginY - curH - yAccum;

        float slideIn = (1.0f - n.slideProgress) * (boxW + scaledMarginX);
        float slideOff = n.dying ? (1.0f - n.slideOut) * (boxW + scaledMarginX) : 0.0f;
        float totalSlide = slideIn + slideOff;

        if (anchor == 1 || anchor == 3)
            boxMin.x += totalSlide;
        else
            boxMin.x -= totalSlide;

        boxMin.x = snap_px(boxMin.x);
        boxMin.y = snap_px(boxMin.y);
        ImVec2 boxMax = ImVec2(
            snap_px(boxMin.x + boxW),
            snap_px(boxMin.y + curH)
        );

        float alpha = n.getAlpha() * ImClamp(n.slideProgress, 0.0f, 1.0f);

        ImU32 colTop = IM_COL32(0, 0, 0, (int)(200.0f * alpha));
        ImU32 colBot = IM_COL32(0, 0, 0, (int)(80.0f * alpha));
        dl->AddRectFilledMultiColor(boxMin, boxMax, colTop, colTop, colBot, colBot);

        ImVec4 borderCol = n.color;
        borderCol.w = alpha * 0.4f;
        dl->AddRect(boxMin, boxMax, ImGui::GetColorU32(borderCol), scaledRound, 0, scaledBorder);

        float  timerFrac = n.getTimerRemaining();
        float  lineW = (boxW - lineInset * 2.0f) * timerFrac;
        ImVec4 lineCol = n.color;
        lineCol.w = alpha;

        dl->AddRectFilled(
            ImVec2(snap_px(boxMin.x + lineInset), snap_px(boxMin.y)),
            ImVec2(snap_px(boxMin.x + lineInset + lineW), snap_px(boxMin.y + lineThick)),
            ImGui::GetColorU32(lineCol),
            scaledRound
        );

        ImVec4 clipVec = { boxMin.x, boxMin.y, boxMax.x, boxMax.y };

        ImVec4 headerCol = n.color;
        headerCol.w = alpha;
        ImVec2 headerPos = ImVec2(
            snap_px(boxMin.x + padX),
            snap_px(boxMin.y + lineThick + padY)
        );
        dl->AddText(font, headerFontSize, headerPos, ImGui::GetColorU32(headerCol),
            n.header.c_str(), nullptr, 0.0f, &clipVec);

        ImVec4 msgCol = ImVec4(1.0f, 1.0f, 1.0f, alpha * 0.85f);
        ImVec2 msgPos = ImVec2(
            snap_px(headerPos.x),
            snap_px(headerPos.y + hdrSz.y + headerGap)
        );
        dl->AddText(font, fontSize, msgPos, ImGui::GetColorU32(msgCol),
            n.message.c_str(), nullptr, wrapW, &clipVec);

        float contentBottom = msgPos.y + msgSz.y;

        if (n.progressValue >= 0.0f)
        {
            float barY = snap_px(contentBottom + headerGap);
            float barX0 = snap_px(boxMin.x + padX);
            float barX1 = snap_px(boxMax.x - padX);

            ImVec4 barBg = n.color;
            barBg.w = alpha * 0.15f;
            dl->AddRectFilled(
                ImVec2(barX0, barY),
                ImVec2(barX1, barY + progressBarH),
                ImGui::GetColorU32(barBg),
                progressBarH * 0.5f
            );

            float fillW = (barX1 - barX0) * ImClamp(n.progressValue, 0.0f, 1.0f);
            ImVec4 barFill = n.color;
            barFill.w = alpha * 0.9f;
            dl->AddRectFilled(
                ImVec2(barX0, barY),
                ImVec2(barX0 + fillW, barY + progressBarH),
                ImGui::GetColorU32(barFill),
                progressBarH * 0.5f
            );

            contentBottom = barY + progressBarH;
        }

        ImVec4 iconCol = n.color;
        iconCol.w = alpha * 0.35f;
        ImVec2 iconPos = ImVec2(
            snap_px(boxMax.x - padX - icoSz.x),
            snap_px(boxMax.y - padY - icoSz.y)
        );
        dl->AddText(font, iconFontSize, iconPos, ImGui::GetColorU32(iconCol),
            n.icon.c_str(), nullptr, 0.0f, &clipVec);

        if (n.duplicateCount > 1)
        {
            char badge[16];
            snprintf(badge, sizeof(badge), "x%d", n.duplicateCount);
            ImVec2 badgeSz = font->CalcTextSizeA(badgeFontSize, FLT_MAX, 0.0f, badge);

            float badgePadX = 4.0f * scale;
            float badgePadY = 2.0f * scale;
            float badgeW = badgeSz.x + badgePadX * 2.0f;
            float badgeH = badgeSz.y + badgePadY * 2.0f;

            ImVec2 badgeMin = ImVec2(
                snap_px(boxMax.x - badgeW - padX * 0.5f),
                snap_px(boxMin.y + lineThick + padY * 0.5f)
            );
            ImVec2 badgeMax = ImVec2(badgeMin.x + badgeW, badgeMin.y + badgeH);

            ImVec4 badgeBg = n.color;
            badgeBg.w = alpha * 0.8f;
            dl->AddRectFilled(badgeMin, badgeMax, ImGui::GetColorU32(badgeBg), badgeH * 0.5f);

            ImVec2 badgeTextPos = ImVec2(
                snap_px(badgeMin.x + badgePadX),
                snap_px(badgeMin.y + badgePadY)
            );
            dl->AddText(font, badgeFontSize, badgeTextPos,
                IM_COL32(0, 0, 0, 255), badge);
        }

        yAccum += curH + scaledSpacing;
    }
}

void NotificationManager::renderHistory()
{
    if (!historyOpen)
        return;

    ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Notification History", &historyOpen))
    {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Clear History"))
        m_History.clear();

    ImGui::Separator();

    for (int i = (int)m_History.size() - 1; i >= 0; --i)
    {
        const HistoryEntry& e = m_History[i];
        ImGui::PushID(i);

        ImGui::TextColored(e.color, "%s %s", e.icon.c_str(), e.header.c_str());
        if (e.duplicateCount > 1)
        {
            ImGui::SameLine();
            ImGui::Text("x%d", e.duplicateCount);
        }

        ImGui::TextWrapped("%s", e.message.c_str());
        ImGui::Separator();
        ImGui::PopID();
    }

    ImGui::End();
}

void NotificationManager::clear()
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Active.clear();
    m_Pending.clear();
}

size_t NotificationManager::count() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Active.size() + m_Pending.size();
}

}

static notify::Type level_type(NotifyLevel t)
{
    switch (t)
    {
    case NotifyLevel::Success:      return notify::Type::Success;
    case NotifyLevel::Info:         return notify::Type::Info;
    case NotifyLevel::Warn:         return notify::Type::Warning;
    case NotifyLevel::Error:        return notify::Type::Error;
    case NotifyLevel::Announcement: return notify::Type::Announcement;
    default:                         return notify::Type::Info;
    }
}

static ImVec4 level_color(NotifyLevel t)
{
    switch (t)
    {
    case NotifyLevel::Success:      return ImVec4(45.f / 255.f, 195.f / 255.f, 110.f / 255.f, 1.f);
    case NotifyLevel::Info:         return ImVec4(66.f / 255.f, 150.f / 255.f, 240.f / 255.f, 1.f);
    case NotifyLevel::Warn:         return ImVec4(235.f / 255.f, 190.f / 255.f, 90.f / 255.f, 1.f);
    case NotifyLevel::Error:        return ImVec4(225.f / 255.f, 70.f / 255.f, 85.f / 255.f, 1.f);
    case NotifyLevel::Announcement: return ImVec4(235.f / 255.f, 150.f / 255.f, 60.f / 255.f, 1.f);
    default:                         return ImVec4(0.83f, 0.83f, 0.83f, 1.f);
    }
}

static const char* level_icon(NotifyLevel t)
{
    switch (t)
    {
    case NotifyLevel::Success:      return ICON_MDI_CHECK_CIRCLE;
    case NotifyLevel::Info:         return ICON_MDI_INFORMATION;
    case NotifyLevel::Warn:         return ICON_MDI_ALERT;
    case NotifyLevel::Error:        return ICON_MDI_CLOSE_CIRCLE;
    case NotifyLevel::Announcement: return ICON_MDI_BULLHORN;
    default:                         return ICON_MDI_INFORMATION;
    }
}

void ImNotify::Print(NotifyLevel type, const char* text, ...)
{
    if (!text || !text[0]) return;
    std::string full = text;
    std::string title, desc;
    size_t nl = full.find('\n');
    if (nl != std::string::npos)
    {
        title = full.substr(0, nl);
        desc = full.substr(nl + 1);
        while (!desc.empty() && (desc.back() == '\n' || desc.back() == '\r')) desc.pop_back();
    }
    if (title.empty())
    {
        notify::Type mt = level_type(type);
        title = notify::manager().typeHeader(mt);
        desc = full;
    }
    else if (desc.empty())
    {
        desc = title;
    }
    notify::manager().addCustom(title, desc, level_color(type), level_icon(type), level_type(type));
}

void ImNotify::Update()
{
}

void ImNotify::Render()
{
    notify::manager().render();
    notify::manager().renderHistory();
}

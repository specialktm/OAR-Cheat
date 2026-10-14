#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <cstdint>

#include "../imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "../imgui_internal.h"

#include "../../fonts/mdi_glyphs.h"

enum class NotifyLevel
{
    Success,
    Info,
    Warn,
    Error,
    Announcement,
    None,
};

namespace notify {

enum class Type : int
{
    Dev,
    Debug,
    Info,
    Success,
    Warning,
    Error,
    Critical,
    Announcement
};

struct Notification
{
    uint32_t    id = 0;
    std::string header;
    std::string message;
    std::string icon;
    Type        type = Type::Info;
    ImVec4      color = ImVec4(1, 1, 1, 1);
    float       lifetime = 3.0f;
    float       elapsed = 0.0f;
    float       fadeIn = 0.2f;
    float       fadeOut = 0.5f;
    int         duplicateCount = 1;
    float       progressValue = -1.0f;
    bool        dying = false;

    float lerpedHeight = 0.0f;
    float slideProgress = 0.0f;
    float slideOut = 1.0f;
    bool  heightInit = false;

    float getAlpha() const;
    float getTimerRemaining() const;
    bool  isExpired() const;
    bool  isProgress() const;
};

struct HistoryEntry
{
    std::string header;
    std::string message;
    std::string icon;
    Type        type = Type::Info;
    ImVec4      color = ImVec4(1, 1, 1, 1);
    int         duplicateCount = 1;
};

class NotificationManager
{
public:
    void     add(const std::string& message, Type type = Type::Info, float duration = -1.0f);
    void     addCustom(const std::string& header, const std::string& message, const ImVec4& color, const char* icon = ICON_MDI_INFORMATION, Type type = Type::Info, float duration = -1.0f);
    uint32_t addProgress(const std::string& header, const std::string& message, Type type = Type::Info);
    void     updateProgress(uint32_t id, float value);
    void     completeProgress(uint32_t id, float lingerDuration = 2.0f);
    void     render();
    void     renderHistory();
    void     clear();
    size_t   count() const;
    std::string typeHeader(Type type) const;
    std::string typeIcon(Type type) const;

    int    anchor = 1;
    ImVec2 margin = { 14.0f, 14.0f };
    float  spacing = 6.0f;
    float  defaultDuration = 3.0f;
    float  fadeInTime = 0.2f;
    float  fadeOutTime = 0.5f;
    float  maxWidthFraction = 0.22f;
    float  referenceHeight = 1080.0f;
    float  rounding = 2.0f;
    float  borderThickness = 1.0f;
    size_t maxHistory = 64;
    bool   historyOpen = false;

private:
    std::vector<Notification> m_Active;
    std::vector<Notification> m_Pending;
    std::vector<HistoryEntry> m_History;
    mutable std::mutex        m_Mutex;
    uint32_t                  m_NextId = 1;

    void        pushHistory(const Notification& n);
    void        drainPending(float availableH, float scale, ImFont* font, float fontSize, float headerFontSize, float iconFontSize, float padY, float lineThick, float headerGap, float iconGap, float wrapW);
    float       estimateHeight(const Notification& n, ImFont* font, float fontSize, float headerFontSize, float iconFontSize, float padY, float lineThick, float headerGap, float iconGap, float wrapW, float progressBarH) const;
    float       getScale() const;
    void        mergeDuplicates();
    ImVec4      defaultColor(Type type) const;
};

inline NotificationManager& manager()
{
    static NotificationManager m;
    return m;
}

}

class ImNotify
{
public:
    static void Print(NotifyLevel type, const char* text, ...);
    static void Update();
    static void Render();
};

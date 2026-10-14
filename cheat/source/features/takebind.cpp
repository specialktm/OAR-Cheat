#include "pch/pch.h"
#include "features/takebind.hpp"
#include "config/config.hpp"
#include "render/dx11_hook.hpp"
#include "../../vendor/imgui/imgui.h"
#include <map>
#include <string>

bool keybind::is_active(keybind_t& kb)
{
    if (kb.key == 0)
    {
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    bool menu_open = g_cfg.menuOpen.load() || io.WantCaptureMouse || io.WantCaptureKeyboard;

    HWND foreground_window = GetForegroundWindow();
    if (foreground_window != (HWND)dx11::window())
    {
        if (kb.mode == activation_mode::toggle)
        {
            kb.was_pressed = false;
        }
        return false;
    }

    bool is_pressed = (GetAsyncKeyState(kb.key) & 0x8000) != 0;

    if (kb.mode == activation_mode::always)
    {
        if (menu_open)
        {
            return false;
        }
        return true;
    }
    else if (kb.mode == activation_mode::hold)
    {
        if (menu_open)
        {
            return false;
        }
        return is_pressed;
    }
    else if (kb.mode == activation_mode::toggle)
    {
        if (menu_open)
        {
            if (is_pressed)
            {
                kb.was_pressed = true;
            }
            else if (kb.was_pressed)
            {
                kb.was_pressed = false;
            }
            return kb.state;
        }

        if (is_pressed && !kb.was_pressed)
        {
            kb.state = !kb.state;
            kb.was_pressed = true;
        }
        else if (!is_pressed)
        {
            kb.was_pressed = false;
        }

        return kb.state;
    }

    return false;
}

std::string keybind::get_key_name(int vk_code)
{
    if (vk_code == 0)
    {
        return "None";
    }

    if (vk_code >= 'A' && vk_code <= 'Z')
    {
        return std::string(1, static_cast<char>(vk_code));
    }

    if (vk_code >= '0' && vk_code <= '9')
    {
        return std::string(1, static_cast<char>(vk_code));
    }

    switch (vk_code)
    {
    case VK_LBUTTON: return "LMB";
    case VK_RBUTTON: return "RMB";
    case VK_MBUTTON: return "MMB";
    case VK_XBUTTON1: return "X1";
    case VK_XBUTTON2: return "X2";
    case VK_BACK: return "Backspace";
    case VK_TAB: return "Tab";
    case VK_RETURN: return "Enter";
    case VK_SHIFT: return "Shift";
    case VK_CONTROL: return "Ctrl";
    case VK_MENU: return "Alt";
    case VK_PAUSE: return "Pause";
    case VK_CAPITAL: return "Caps";
    case VK_ESCAPE: return "Esc";
    case VK_SPACE: return "Space";
    case VK_PRIOR: return "Page Up";
    case VK_NEXT: return "Page Down";
    case VK_END: return "End";
    case VK_HOME: return "Home";
    case VK_LEFT: return "Left";
    case VK_UP: return "Up";
    case VK_RIGHT: return "Right";
    case VK_DOWN: return "Down";
    case VK_INSERT: return "Insert";
    case VK_DELETE: return "Delete";
    case VK_LWIN: return "LWin";
    case VK_RWIN: return "RWin";
    case VK_NUMPAD0: return "Numpad 0";
    case VK_NUMPAD1: return "Numpad 1";
    case VK_NUMPAD2: return "Numpad 2";
    case VK_NUMPAD3: return "Numpad 3";
    case VK_NUMPAD4: return "Numpad 4";
    case VK_NUMPAD5: return "Numpad 5";
    case VK_NUMPAD6: return "Numpad 6";
    case VK_NUMPAD7: return "Numpad 7";
    case VK_NUMPAD8: return "Numpad 8";
    case VK_NUMPAD9: return "Numpad 9";
    case VK_MULTIPLY: return "Numpad *";
    case VK_ADD: return "Numpad +";
    case VK_SUBTRACT: return "Numpad -";
    case VK_DECIMAL: return "Numpad .";
    case VK_DIVIDE: return "Numpad /";
    case VK_F1: return "F1";
    case VK_F2: return "F2";
    case VK_F3: return "F3";
    case VK_F4: return "F4";
    case VK_F5: return "F5";
    case VK_F6: return "F6";
    case VK_F7: return "F7";
    case VK_F8: return "F8";
    case VK_F9: return "F9";
    case VK_F10: return "F10";
    case VK_F11: return "F11";
    case VK_F12: return "F12";
    default:
    {
        char key_name[256];
        if (GetKeyNameTextA(MapVirtualKeyA(vk_code, MAPVK_VK_TO_VSC) << 16, key_name, sizeof(key_name)))
        {
            return std::string(key_name);
        }
        return "Key " + std::to_string(vk_code);
    }
    }
}

bool keybind::keybind_selector(int& key, activation_mode& mode, const char* unique_id)
{
    bool changed = false;
    std::string key_name = get_key_name(key);

    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5.0f);

    std::string popup_id;
    std::string mode_popup_id;

    if (unique_id)
    {
        popup_id = std::string("keybind_popup_") + unique_id;
        mode_popup_id = std::string("keybind_mode_popup_") + unique_id;
    }
    else
    {
        popup_id = "keybind_popup_" + std::to_string(reinterpret_cast<std::uintptr_t>(&key));
        mode_popup_id = "keybind_mode_popup_" + std::to_string(reinterpret_cast<std::uintptr_t>(&key));
    }

    if (unique_id)
    {
        ImGui::PushID(unique_id);
    }
    else
    {
        ImGui::PushID(&key);
    }

    if (ImGui::Button(key_name.c_str(), ImVec2(70, 0)))
    {
        ImGui::OpenPopup(popup_id.c_str());
    }

    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
    {
        ImGui::OpenPopup(mode_popup_id.c_str());
    }

    static std::map<std::string, int> popup_frame_count;

    if (ImGui::BeginPopup(popup_id.c_str()))
    {
        ImGui::Text("Press any key...");
        ImGui::Separator();

        std::string popup_key = popup_id;
        if (popup_frame_count.find(popup_key) == popup_frame_count.end())
        {
            popup_frame_count[popup_key] = 0;
        }
        popup_frame_count[popup_key]++;

        if (popup_frame_count[popup_key] > 2)
        {
        for (int i = 1; i < 256; i++)
        {
            if (GetAsyncKeyState(i) & 0x8000)
            {
                key = i;
                changed = true;
                ImGui::CloseCurrentPopup();
                    popup_frame_count.erase(popup_key);
                break;
                }
            }
        }

        if (ImGui::Button("Clear"))
        {
            key = 0;
            changed = true;
            ImGui::CloseCurrentPopup();
            popup_frame_count.erase(popup_key);
        }

        ImGui::EndPopup();
    }
    else
    {
        std::string popup_key = popup_id;
        popup_frame_count.erase(popup_key);
    }

    if (ImGui::BeginPopup(mode_popup_id.c_str()))
    {
        if (ImGui::Selectable("Toggle", mode == activation_mode::toggle))
        {
            mode = activation_mode::toggle;
            changed = true;
        }
        if (ImGui::Selectable("Hold", mode == activation_mode::hold))
        {
            mode = activation_mode::hold;
            changed = true;
        }
        if (ImGui::Selectable("Always", mode == activation_mode::always))
        {
            mode = activation_mode::always;
            changed = true;
        }
        ImGui::EndPopup();
    }

    ImGui::PopID();

    return changed;
}

ImGuiKey keybind::vk_to_imgui_key(int vk_code)
{
    if (vk_code == 0) return ImGuiKey_None;

    if (vk_code >= 'A' && vk_code <= 'Z')
        return (ImGuiKey)(ImGuiKey_A + (vk_code - 'A'));

    if (vk_code >= '0' && vk_code <= '9')
        return (ImGuiKey)(ImGuiKey_0 + (vk_code - '0'));

    if (vk_code == VK_LBUTTON) return ImGuiKey_MouseLeft;
    if (vk_code == VK_RBUTTON) return ImGuiKey_MouseRight;
    if (vk_code == VK_MBUTTON) return ImGuiKey_MouseMiddle;
    if (vk_code == VK_XBUTTON1) return ImGuiKey_MouseX1;
    if (vk_code == VK_XBUTTON2) return ImGuiKey_MouseX2;

    if (vk_code == VK_TAB) return ImGuiKey_Tab;
    if (vk_code == VK_LEFT) return ImGuiKey_LeftArrow;
    if (vk_code == VK_RIGHT) return ImGuiKey_RightArrow;
    if (vk_code == VK_UP) return ImGuiKey_UpArrow;
    if (vk_code == VK_DOWN) return ImGuiKey_DownArrow;
    if (vk_code == VK_PRIOR) return ImGuiKey_PageUp;
    if (vk_code == VK_NEXT) return ImGuiKey_PageDown;
    if (vk_code == VK_HOME) return ImGuiKey_Home;
    if (vk_code == VK_END) return ImGuiKey_End;
    if (vk_code == VK_INSERT) return ImGuiKey_Insert;
    if (vk_code == VK_DELETE) return ImGuiKey_Delete;
    if (vk_code == VK_BACK) return ImGuiKey_Backspace;
    if (vk_code == VK_SPACE) return ImGuiKey_Space;
    if (vk_code == VK_RETURN) return ImGuiKey_Enter;
    if (vk_code == VK_ESCAPE) return ImGuiKey_Escape;
    if (vk_code == VK_LCONTROL || vk_code == VK_RCONTROL) return ImGuiKey_LeftCtrl;
    if (vk_code == VK_LSHIFT || vk_code == VK_RSHIFT) return ImGuiKey_LeftShift;
    if (vk_code == VK_LMENU || vk_code == VK_RMENU) return ImGuiKey_LeftAlt;
    if (vk_code == VK_LWIN || vk_code == VK_RWIN) return ImGuiKey_LeftSuper;
    if (vk_code == VK_MENU) return ImGuiKey_Menu;

    if (vk_code >= VK_F1 && vk_code <= VK_F24)
        return (ImGuiKey)(ImGuiKey_F1 + (vk_code - VK_F1));

    return ImGuiKey_None;
}

int keybind::imgui_key_to_vk(ImGuiKey imgui_key)
{
    if (imgui_key == ImGuiKey_None) return 0;

    if (imgui_key >= ImGuiKey_A && imgui_key <= ImGuiKey_Z)
        return 'A' + (imgui_key - ImGuiKey_A);

    if (imgui_key >= ImGuiKey_0 && imgui_key <= ImGuiKey_9)
        return '0' + (imgui_key - ImGuiKey_0);

    if (imgui_key == ImGuiKey_MouseLeft) return VK_LBUTTON;
    if (imgui_key == ImGuiKey_MouseRight) return VK_RBUTTON;
    if (imgui_key == ImGuiKey_MouseMiddle) return VK_MBUTTON;
    if (imgui_key == ImGuiKey_MouseX1) return VK_XBUTTON1;
    if (imgui_key == ImGuiKey_MouseX2) return VK_XBUTTON2;

    if (imgui_key == ImGuiKey_Tab) return VK_TAB;
    if (imgui_key == ImGuiKey_LeftArrow) return VK_LEFT;
    if (imgui_key == ImGuiKey_RightArrow) return VK_RIGHT;
    if (imgui_key == ImGuiKey_UpArrow) return VK_UP;
    if (imgui_key == ImGuiKey_DownArrow) return VK_DOWN;
    if (imgui_key == ImGuiKey_PageUp) return VK_PRIOR;
    if (imgui_key == ImGuiKey_PageDown) return VK_NEXT;
    if (imgui_key == ImGuiKey_Home) return VK_HOME;
    if (imgui_key == ImGuiKey_End) return VK_END;
    if (imgui_key == ImGuiKey_Insert) return VK_INSERT;
    if (imgui_key == ImGuiKey_Delete) return VK_DELETE;
    if (imgui_key == ImGuiKey_Backspace) return VK_BACK;
    if (imgui_key == ImGuiKey_Space) return VK_SPACE;
    if (imgui_key == ImGuiKey_Enter) return VK_RETURN;
    if (imgui_key == ImGuiKey_Escape) return VK_ESCAPE;
    if (imgui_key == ImGuiKey_LeftCtrl || imgui_key == ImGuiKey_RightCtrl) return VK_LCONTROL;
    if (imgui_key == ImGuiKey_LeftShift || imgui_key == ImGuiKey_RightShift) return VK_LSHIFT;
    if (imgui_key == ImGuiKey_LeftAlt || imgui_key == ImGuiKey_RightAlt) return VK_LMENU;
    if (imgui_key == ImGuiKey_LeftSuper || imgui_key == ImGuiKey_RightSuper) return VK_LWIN;
    if (imgui_key == ImGuiKey_Menu) return VK_MENU;

    if (imgui_key >= ImGuiKey_F1 && imgui_key <= ImGuiKey_F24)
        return VK_F1 + (imgui_key - ImGuiKey_F1);

    return 0;
}

std::string keybind::truncate_text_left(const std::string& text, float max_width)
{
    float text_width = ImGui::CalcTextSize(text.c_str()).x;
    if (text_width <= max_width)
        return text;

    const char* ellipsis = "...";
    float ellipsis_width = ImGui::CalcTextSize(ellipsis).x;
    float available_width = max_width - ellipsis_width;

    if (available_width <= 0)
        return ellipsis;

    std::string truncated = text;
    while (truncated.length() > 0)
    {
        float truncated_width = ImGui::CalcTextSize(truncated.c_str()).x;
        if (truncated_width <= available_width)
            break;
        truncated.erase(0, 1);
    }

    return ellipsis + truncated;
}

bool keybind::custom_keybind(const char* str_id, int& key, float max_width)
{
    ImGuiKey imgui_key = vk_to_imgui_key(key);
    bool changed = false;
    int original_key = key;

    ImVec2 size_arg = ImVec2(0, 0);
    if (max_width > 0.0f)
    {
        size_arg = ImVec2(max_width, 0);
    }

    if (ImAdd::KeyBind(str_id, &imgui_key, size_arg))
    {
        key = imgui_key_to_vk(imgui_key);
        if (key != original_key)
            changed = true;
    }

    return changed;
}

bool keybind::keybind_with_mode(const char* label, int& key, activation_mode& mode, const char* unique_id, float max_width)
{
    bool changed = false;

    ImGui::PushID(unique_id);

    if (custom_keybind(label, key, max_width))
    {
        changed = true;
    }

    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
    {
        ImGui::OpenPopup((std::string("keybind_mode_") + unique_id).c_str());
    }

    ImGui::SetNextWindowSize(ImVec2(160, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    if (ImGui::BeginPopup((std::string("keybind_mode_") + unique_id).c_str()))
    {
        if (ImAdd::SelectableLabel("Hold", mode == activation_mode::hold, true))
        {
            mode = activation_mode::hold;
            changed = true;
            ImGui::CloseCurrentPopup();
        }
        if (ImAdd::SelectableLabel("Toggle", mode == activation_mode::toggle, true))
        {
            mode = activation_mode::toggle;
            changed = true;
            ImGui::CloseCurrentPopup();
        }
        if (ImAdd::SelectableLabel("Always", mode == activation_mode::always, true))
        {
            mode = activation_mode::always;
            changed = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::Separator();
        if (ImAdd::SelectableLabel("Clear", false, true))
        {
            key = 0;
            changed = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(4);

    ImGui::PopID();
    return changed;
}

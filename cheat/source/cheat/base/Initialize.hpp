#pragma once
#include "pch/pch.h"

class Initializer
{
public:
    Initializer() = default;
    ~Initializer() = default;

public:
    template <typename T>
    void AddClassInterface(T** classInterface)
    {
        *classInterface = new T();
        m_class_interface.push_back((Baseclass*)*classInterface);
    }

    void Initialize()
    {
        if (m_class_interface.empty())
            return;

        for (const auto class_interface : m_class_interface)
        {
            g_logger->send(levels::debug, "Created class -> {}", class_interface->ClassName());
        }

        for (const auto class_interface : m_class_interface)
        {
            class_interface->Initialize();
        }

        for (const auto class_interface : m_class_interface)
        {
            if (IS_VALID_PTR(class_interface))
            {
                PrintClassInterface(class_interface);
            }
            else
            {
                g_logger->send(levels::error, "{} is Invalid", class_interface->ClassName());
            }
        }

        g_logger->send(levels::success,"Successfully loaded Cheat");
    }

    void Uninitialize()
    {
        if (m_class_interface.empty())
            return;

        for (const auto class_interface : m_class_interface)
        {
            if (class_interface)
            {
                class_interface->Uninitialize();
                delete class_interface;
            }
        }

        m_class_interface.clear();
    }

private:
    void PrintClassInterface(Baseclass* class_interface)
    {
        size_t maxClassNameLength = 0;
        std::string className = class_interface->ClassName();
        maxClassNameLength = std::max(maxClassNameLength, class_interface->ClassName().length());

        std::string paddedClassName = className + std::string(maxClassNameLength - className.length(), ' ');
        g_logger->send(levels::info, "Created Class -> {} [{:#X}]\n", paddedClassName, uintptr_t(class_interface));
    }

    std::vector<Baseclass*> m_class_interface;
};

inline Initializer* g_Initializer;
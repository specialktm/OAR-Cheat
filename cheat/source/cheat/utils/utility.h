#pragma once
#include "framework/framework.h"
#include <vector>

class utility
{
public:
    utility() = default;
    ~utility() = default;

    template<typename T>
    inline static std::vector<T*> FindActors()
    {
        std::vector<T*> foundActors;

        for (int i = 0; i < SDK::UObject::GObjects->Num(); i++)
        {
            SDK::UObject* obj = SDK::UObject::GObjects->GetByIndex(i);

            if (obj && obj->IsA(T::StaticClass()))
            {
                T* actor = static_cast<T*>(obj);
                foundActors.push_back(actor);
            }
        }

        return foundActors;
    }
};

extern utility g_util;

#pragma once
#include <cstdint>
#include "../other/memory.hpp"
#include "../other/vector3.h"

// Совместимость типов
using uintptr_t = uint64_t;

// Заглушки для process
struct ProcessMock {
    template<typename T>
    T Read(uint64_t address) {
        return rpm<T>(address);
    }
    
    template<typename T>
    void Write(uint64_t address, T value) {
        wpm<T>(address, value);
    }
};

// Используем статический объект вместо динамического
static ProcessMock process_instance;
#define process (&process_instance)

// Заглушки для vars
namespace vars {
    namespace exploits {
        namespace server {
            inline bool score = false;
            inline bool cancel = false;
        }
        namespace local {
            inline bool infinity_ammo = false;
            namespace visual {
                inline bool aspect_ratio = false;
                inline float aspect_value = 1.0f;
            }
            namespace weapon {
                inline bool rcs = false;
                inline float RCS_value1 = 0.0f;
                inline float RCS_value2 = 0.0f;
                inline float RCS_xValue = 0.0f;
                inline float RCS_yValue = 0.0f;
                inline float RCS_zValue = 0.0f;
                inline float RCS_VecValue1 = 0.0f;
                inline float RCS_VecValue2 = 0.0f;
            }
        }
    }
}
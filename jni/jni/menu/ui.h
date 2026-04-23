#pragma once
#include "../includes/uses.h"
#include "../includes/imgui/imgui/imgui_internal.h"
#include "../includes/fonts.h"
#include <map>
#include <atomic>
#include <vector>

class g_menu
{
private:
    struct Snowflake {
        float x, y;
        float size;
        float speed;
        float oscillation;
        float phase;
    };
    
    std::vector<Snowflake> snowflakes;
    void InitializeSnow();
    void UpdateSnow();
    void RenderSnow();

public:
    std::atomic<bool> should_exit;
    void render();
    void ResetAllSettingsToDefault(); // Функция сброса всех настроек к значениям по умолчанию
};
inline g_menu menu;
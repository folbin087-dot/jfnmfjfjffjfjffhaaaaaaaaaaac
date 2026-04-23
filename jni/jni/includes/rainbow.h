#pragma once
#include "imgui/imgui/imgui.h"

class RainbowManager {
private:
    static RainbowManager* instance;
    float hue = 0.0f;
    
public:
    static RainbowManager& Get();
    
    // Основная переменная с текущим радужным цветом
    ImU32 currentRainbowColor;
    float currentRainbowColorArray[4];
    
    // Обновление цвета (вызывать каждый кадр)
    void Update(float speed);
    
    // Получить цвет со смещением
    ImU32 GetRainbowColorWithOffset(float speed, float offset = 0.0f);
    void GetRainbowColorArrayWithOffset(float color[4], float speed, float offset = 0.0f);
};
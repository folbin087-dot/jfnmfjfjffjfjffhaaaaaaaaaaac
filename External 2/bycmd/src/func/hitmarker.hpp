#pragma once

#include "../other/vector3.h"
#include "imgui.h"
#include <vector>
#include <chrono>

// Структура для хранения данных хитмаркера
struct HitMarker {
    Vector3 position;      // 3D позиция попадания (если hitmarker_3d)
    float damage;          // Урон
    std::chrono::steady_clock::time_point timestamp;
    bool is3D;
    bool isHeadshot;
    float screenX, screenY; // 2D позиция на экране (для 3D хитмаркеров)
};

class HitMarkerSystem {
public:
    std::vector<HitMarker> markers;
    
    // Добавить хитмаркер
    void AddHit(float damage, bool isHeadshot = false);
    void AddHit3D(const Vector3& position, float damage, bool isHeadshot = false);
    
    // Обновление (удаление старых)
    void Update();
    
    // Отрисовка
    void Draw();
    
    // Очистка всех
    void Clear();
    
private:
    void DrawX(float x, float y, float size, float thickness, ImColor color);
    void DrawDamage(float x, float y, float damage, bool isHeadshot, ImColor color);
};

// Глобальный экземпляр
extern HitMarkerSystem g_hitmarker;

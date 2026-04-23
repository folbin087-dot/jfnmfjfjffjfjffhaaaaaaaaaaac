#pragma once
#include "uses.h"
#include "imgui/imgui/imgui.h"
#include <vector>
#include <chrono>
#include <mutex>
#include <cmath>

// Структура для хранения данных хитмаркера
struct HitMarker {
    Vector3 startPos;       // Позиция игрока (для направления текста)
    Vector3 hitPos;         // Позиция попадания (где показывать текст)
    int damage;             // Нанесённый урон
    std::chrono::steady_clock::time_point timestamp;
    float duration;         // Время отображения в секундах
    bool isHeadshot;        // Было ли попадание в голову
    bool isKill;            // Было ли убийство
};

// Общие настройки для всех маркеров
struct HitMarkerSettings {
    bool enabled = false;
    int markerType = 0;                 // 0 = "x", 1 = damage number
    float displayTime = 1.5f;           // Время отображения
    float fadeTime = 0.5f;              // Время исчезновения
    
    // Использовать разные цвета для разных типов попаданий
    bool useHitTypeColors = false;
    
    // Цвета
    float hitColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };       // Белый для обычного хита
    float headshotColor[4] = { 1.0f, 0.8f, 0.0f, 1.0f };  // Жёлтый для хедшота
    float killColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };      // Красный для убийства
    
    int maxMarkers = 20;
};

// Глобальные настройки
inline HitMarkerSettings hitMarkerSettings;

class HitMarkerSystem {
public:
    // Добавить новый хитмаркер
    void AddMarker(const Vector3& start, const Vector3& hit, int damage, bool isHeadshot = false, bool isKill = false);
    
    // Обновление (удаление устаревших)
    void Update();
    
    // Отрисовка всех активных маркеров
    void Render(const Matrix& viewMatrix);
    
    // Отрисовка только обычных маркеров ("x")
    void RenderMarkers(const Matrix& viewMatrix);
    
    // Отрисовка только дамага (числа)
    void RenderDamage(const Matrix& viewMatrix);
    
    // Очистить все маркеры
    void Clear();
    
private:
    std::vector<HitMarker> markers;
    std::mutex markersMutex;
    
    void RemoveExpiredMarkers();
};

// Глобальный экземпляр
inline HitMarkerSystem hitMarker;

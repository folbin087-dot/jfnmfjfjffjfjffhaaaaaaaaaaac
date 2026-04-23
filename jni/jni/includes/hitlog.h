#pragma once
#include "uses.h"
#include <string>
#include <vector>
#include <chrono>
#include <mutex>
#include <deque>

// Структура для хранения информации о попадании
struct HitLogEntry {
    std::string attackerName;      // Имя атакующего
    std::string victimName;        // Имя жертвы
    int damage;                    // Нанесенный урон
    std::string weaponName;        // Название оружия
    bool isHeadshot;               // Попадание в голову
    bool isKill;                   // Убийство
    std::chrono::steady_clock::time_point timestamp; // Время события
    float displayDuration;         // Время отображения на экране (секунды)
};

class HitLogger {
public:
    // Настройки хитлогов
    bool enabled = false;              // Включены ли хитлоги
    bool showHeadshotIcon = false;     // Показывать иконку хедшота
    bool showKillIcon = false;         // Показывать иконку убийства
    bool showWeapon = false;           // Показывать оружие
    bool showDamage = false;           // Показывать урон
    
    float displayTime = 3.0f;          // Время отображения записи (секунды)
    float fadeDelay = 2.0f;            // Задержка перед началом исчезновения (секунды)
    int maxEntries = 10;               // Максимальное количество записей на экране
    
    // Цвета
    float textColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float damageColor[4] = { 1.0f, 0.3f, 0.3f, 1.0f };
    float headshotColor[4] = { 1.0f, 0.8f, 0.0f, 1.0f };
    float killColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
    float weaponColor[4] = { 0.7f, 0.7f, 0.7f, 1.0f };
    float backgroundColor[4] = { 0.0f, 0.0f, 0.0f, 0.5f };
    
    // Позиция на экране (0-100, проценты)
    float positionX = 45.0f;           // 50% от левого края
    float positionY = 50.0f;           // 50% от верха
    
    // Методы
    void LogHit(const std::string& attackerName, 
                const std::string& victimName, 
                int damage, 
                const std::string& weaponName,
                bool isHeadshot = false,
                bool isKill = false);
    
    void Update();                     // Обновление (удаление старых записей)
    void Render();                     // Отрисовка на экране
    
    // Получение записей
    const std::deque<HitLogEntry>& GetEntries() const { return entries; }
    
private:
    std::deque<HitLogEntry> entries;
    std::mutex entriesMutex;
    
    void RemoveExpiredEntries();
};

// Глобальный экземпляр
inline HitLogger hitLogger;

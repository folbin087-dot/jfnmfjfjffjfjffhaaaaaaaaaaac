#pragma once
#include <cstdint>

class GrenadePrediction {
private:
    bool is_enabled = false;
    bool is_initialized = false;
    bool was_applied = false;  // Флаг для отслеживания, был ли уже применён код
    uint64_t grenade_practice_service_address = 0;
    
    // Поиск GrenadePracticeController через GrenadeTracerManager
    bool FindGrenadePracticeController();
    
    // Включение/отключение
    void EnableGrenadePractice();
    void DisableGrenadePractice();

public:
    GrenadePrediction() = default;
    ~GrenadePrediction() = default;
    
    // Основные методы
    void Initialize();
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return is_enabled; }
    
    // Статистика
    bool IsSystemFound() const { return grenade_practice_service_address != 0; }
    bool IsInitialized() const { return is_initialized; }
    
    // Дебаг информация
    uint64_t GetGameModeSettingsAddress() const { return grenade_practice_service_address; }
};

extern GrenadePrediction g_grenadePrediction;

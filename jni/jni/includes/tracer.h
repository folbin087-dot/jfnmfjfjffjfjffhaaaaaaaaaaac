#pragma once
#include "uses.h"
#include "imgui/imgui/imgui.h"
#include <vector>
#include <chrono>
#include <mutex>
#include <cmath>

// Структура для хранения данных трассера выстрела
struct BulletTrace {
    Vector3 startPos;       // Позиция начала выстрела
    Vector3 hitPos;         // Позиция попадания (raycast)
    Vector3 enemyHitPos;    // Позиция попадания по противнику (если было)
    std::chrono::steady_clock::time_point timestamp;
    float duration;         // Время отображения в секундах
    bool isActive;
    bool hitEnemy;          // Было ли попадание по противнику
};

class BulletTracer {
public:
    // Настройки
    bool enabled = false;
    float displayTime = 1.5f;           // Время отображения трассера
    float fadeTime = 0.5f;              // Время анимации исчезновения
    float lineThickness = 2.0f;         // Толщина линии
    bool stopOnHit = false;             // Останавливать трассер на месте попадания по противнику
    
    // Цвета
    bool useGradient = false;           // Использовать градиент
    float color1[4] = { 1.0f, 0.3f, 0.3f, 1.0f };  // Цвет начала (или единственный цвет)
    float color2[4] = { 1.0f, 1.0f, 0.3f, 1.0f };  // Цвет конца (для градиента)
    
    // Точка попадания
    bool showHitPoint = false;          // Показывать точку попадания (по умолчанию выкл)
    float hitPointRadius = 4.0f;        // Радиус точки попадания
    float hitPointColor[4] = { 0.0f, 1.0f, 0.0f, 1.0f }; // Цвет точки попадания
    
    int maxTraces = 10;                 // Максимум трассеров на экране
    
    // Добавить новый трассер
    void AddTrace(const Vector3& start, const Vector3& hit);
    
    // Обновить последний трассер с точкой попадания по противнику
    void UpdateLastTraceWithEnemyHit(const Vector3& enemyHitPos);
    
    // Обновление (удаление устаревших)
    void Update();
    
    // Отрисовка всех активных трассеров
    void Render(const Matrix& viewMatrix);
    
    // Очистить все трассеры
    void Clear();
    
    // Получить количество активных трассеров
    int GetActiveCount() const { return static_cast<int>(traces.size()); }
    
private:
    std::vector<BulletTrace> traces;
    std::mutex tracesMutex;
    
    void RemoveExpiredTraces();
};

// Глобальный экземпляр
inline BulletTracer bulletTracer;

// Оффсеты для получения данных о выстреле
namespace TracerOffsets {
    // GHGBBBHHEDCDFEH -> BBADDBBHGEFAEFA (raycast result)
    inline int raycastResult = 0x10;
    
    // BBADDBBHGEFAEFA fields
    inline int resultHitList = 0x20;      // List<BAEAGFHFECFFEBE>
    inline int resultStartPos = 0x28;     // Vector3 start position
    inline int resultDirection = 0x34;    // Vector3 direction
    
    // BAEAGFHFECFFEBE (hit data from raycast)
    inline int hitDataPoint = 0x10;       // Vector3 HitPoint
    
    // List<T> internals
    inline int listItems = 0x10;
    inline int listCount = 0x18;
    
    // Array internals
    inline int arrayFirstElement = 0x20;
}

// Получить позицию localPlayer
inline Vector3 GetLocalPlayerPosition(uint64_t localPlayer) {
    if (!localPlayer) return Vector3::Zero();
    
    uint64_t ptr1 = mem.read<uint64_t>(localPlayer + offsets::pos_ptr1);
    if (!ptr1) return Vector3::Zero();
    
    uint64_t ptr2 = mem.read<uint64_t>(ptr1 + offsets::pos_ptr2);
    if (!ptr2) return Vector3::Zero();
    
    return mem.read<Vector3>(ptr2 + offsets::pos_ptr3);
}

// Структура для хранения последнего известного попадания
struct LastHitData {
    Vector3 startPos;
    Vector3 hitPoint;
    int lastShotCount = -1;
    uint64_t lastWeaponController = 0;  // Для отслеживания смены оружия
    bool isValid = false;
};

inline LastHitData lastHitData;

// Получить счётчик выстрелов (сумма всех оффсетов)
inline int GetShotCounter(uint64_t localPlayer, uint64_t* outWeaponController = nullptr) {
    if (!localPlayer) return -1;
    
    uint64_t weaponryController = mem.read<uint64_t>(localPlayer + offsets::weaponryController);
    if (!weaponryController) return -1;
    
    uint64_t weaponController = mem.read<uint64_t>(weaponryController + offsets::currentWeaponController);
    if (!weaponController) return -1;
    
    if (outWeaponController) {
        *outWeaponController = weaponController;
    }
    
    // Суммируем все три оффсета для надёжной детекции
    int val_110 = mem.read<int>(weaponController + 0x110);
    int val_118 = mem.read<int>(weaponController + 0x118);
    int val_A0 = mem.read<int>(weaponController + 0xA0);
    
    return val_110 + val_118 + val_A0;
}

// Получить реальную точку попадания из GunController raycast result
inline bool GetRealHitPoint(uint64_t localPlayer, Vector3& outHitPoint, Vector3& outStartPos) {
    if (!localPlayer) return false;
    
    // PlayerController -> WeaponryController -> GunController
    uint64_t weaponryController = mem.read<uint64_t>(localPlayer + offsets::weaponryController);
    if (!weaponryController) return false;
    
    uint64_t gunController = mem.read<uint64_t>(weaponryController + offsets::currentWeaponController);
    if (!gunController) return false;
    
    // GunController -> GHGBBBHHEDCDFEH (raycast helper) offset 0xF8
    uint64_t raycastHelper = mem.read<uint64_t>(gunController + 0xF8);
    if (!raycastHelper) return false;
    
    // GHGBBBHHEDCDFEH -> BBADDBBHGEFAEFA (raycast result)
    uint64_t raycastResult = mem.read<uint64_t>(raycastHelper + TracerOffsets::raycastResult);
    if (!raycastResult) return false;
    
    // Читаем начальную позицию
    outStartPos = mem.read<Vector3>(raycastResult + TracerOffsets::resultStartPos);
    
    // Читаем список попаданий
    uint64_t hitList = mem.read<uint64_t>(raycastResult + TracerOffsets::resultHitList);
    if (!hitList) return false;
    
    int hitCount = mem.read<int>(hitList + TracerOffsets::listCount);
    if (hitCount <= 0) {
        // Нет попаданий - используем направление
        Vector3 direction = mem.read<Vector3>(raycastResult + TracerOffsets::resultDirection);
        if (direction == Vector3::Zero() || outStartPos == Vector3::Zero()) return false;
        
        float maxDist = 500.0f;
        outHitPoint = Vector3(
            outStartPos.x + direction.x * maxDist,
            outStartPos.y + direction.y * maxDist,
            outStartPos.z + direction.z * maxDist
        );
        return true;
    }
    
    // Получаем массив элементов
    uint64_t hitArray = mem.read<uint64_t>(hitList + TracerOffsets::listItems);
    if (!hitArray) return false;
    
    // Читаем первый элемент (ближайшее попадание)
    uint64_t firstHit = mem.read<uint64_t>(hitArray + TracerOffsets::arrayFirstElement);
    if (!firstHit) return false;
    
    // BAEAGFHFECFFEBE -> HitPoint
    outHitPoint = mem.read<Vector3>(firstHit + TracerOffsets::hitDataPoint);
    
    return true;
}

// Проверить новый выстрел и получить данные
inline bool CheckNewShot(uint64_t localPlayer, Vector3& outStartPos, Vector3& outHitPoint) {
    uint64_t currentWeaponController = 0;
    int currentShotCount = GetShotCounter(localPlayer, &currentWeaponController);
    
    // Проверяем смену оружия - сбрасываем счётчик
    if (currentWeaponController != lastHitData.lastWeaponController) {
        lastHitData.lastWeaponController = currentWeaponController;
        lastHitData.lastShotCount = currentShotCount;
        return false;  // Не считаем это выстрелом
    }
    
    // Если счётчик увеличился - был выстрел
    // Также проверяем что счётчик не уменьшился (защита от переполнения/сброса)
    bool shotFired = (lastHitData.lastShotCount >= 0 && 
                      currentShotCount > lastHitData.lastShotCount &&
                      currentShotCount - lastHitData.lastShotCount < 100);  // Защита от резких скачков
    
    // Обновляем счётчик
    if (currentShotCount >= 0) {
        lastHitData.lastShotCount = currentShotCount;
    }
    
    if (!shotFired) {
        return false;
    }
    
    // Получаем данные из raycast result
    Vector3 currentStart, currentHit;
    if (GetRealHitPoint(localPlayer, currentHit, currentStart)) {
        if (currentStart != Vector3::Zero() && currentHit != Vector3::Zero()) {
            outStartPos = currentStart;
            outHitPoint = currentHit;
            return true;
        }
    }
    
    // Fallback - используем позицию игрока
    Vector3 playerPos = GetLocalPlayerPosition(localPlayer);
    if (playerPos == Vector3::Zero()) return false;
    
    outStartPos = playerPos;
    outStartPos.y += 1.6f; // уровень глаз
    
    // Без данных о направлении не можем определить точку попадания
    return false;
}

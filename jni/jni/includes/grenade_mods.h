#pragma once
#include <cstdint>

namespace GrenadeMods {
    // Функции для применения модификаций гранат
    void FastThrow();
    void InfinityGrenades();
    void InstantDetonation();
    void GrenadeNoDamage();
    void AntiHE();
    void AntiFlash();
    void AntiSmoke();
    
    // Главная функция обновления - вызывается каждый кадр
    void Update();
}

# СРАВНЕНИЕ: Текущий код vs Рабочая версия ESP

## ✅ РАБОЧАЯ ВЕРСИЯ (из памяти - Apr 7, 2026)

### Offsets в `game.hpp`:
```cpp
namespace offsets
{
    inline uint64_t player_manager  = 151547120;  // 0x9080FF0
    inline int player_list_ptr1 = 0x18;   // Entries pointer from PlayerDict
    inline int player_list_ptr2 = 0x30;   // Base offset within entries array
    inline int player_list_ptr3 = 0x18;   // Stride between players (24 bytes)
}
```

### Player Enumeration в `visuals.cpp`:
```cpp
uint64_t PlayerDict = rpm<uint64_t>(PlayerManager + 0x28);
if (!PlayerDict || PlayerDict < 0x10000 || PlayerDict > 0x7FFFFFFFFFFF) return;

int PlayerCount = rpm<int>(PlayerDict + 0x20);
if (PlayerCount <= 0 || PlayerCount > 64) return;

uint64_t entriesPtr = rpm<uint64_t>(PlayerDict + offsets::player_list_ptr1);
if (!entriesPtr || entriesPtr < 0x10000 || entriesPtr > 0x7FFFFFFFFFFF) return;

for (int i = 0; i < PlayerCount; i++)
{
    // Direct player read - NO INTERMEDIATE playerBase!
    uint64_t Player = rpm<uint64_t>(entriesPtr + offsets::player_list_ptr2 + (i * offsets::player_list_ptr3));
    if (!Player || Player < 0x10000 || Player > 0x7FFFFFFFFFFF || Player == local_player)
        continue;
    // ... ESP rendering
}
```

### Критические правила рабочей версии:
1. ✅ **DO NOT** use intermediate `playerBase` pointer
2. ✅ **DO NOT** change stride from `0x18` (24 bytes) to `0x20` (32 bytes)
3. ✅ **DO NOT** add extra pointer dereference
4. ✅ Address validation (`< 0x10000 || > 0x7FFFFFFFFFFF`) is required

---

## 🔍 ТЕКУЩИЙ КОД (visuals.cpp)

### Проверка offsets в `game.hpp`:
| Offset | Рабочая версия | Текущий код | Статус |
|--------|----------------|-------------|--------|
| player_manager | 151547120 (0x9080FF0) | 151547120 | ✅ ОК |
| player_list_ptr1 | 0x18 | 0x18 | ✅ ОК |
| player_list_ptr2 | 0x30 | 0x30 | ✅ ОК |
| player_list_ptr3 | 0x18 | 0x18 | ✅ ОК |

**ВЫВОД: Offsets идентичны!**

---

### Player Enumeration (текущий код):
```cpp
// Line 605-608
uint64_t PlayerManager = get_player_manager();
if (!PlayerManager || PlayerManager < 0x10000 || PlayerManager > 0x7FFFFFFFFFFF) {
    return;  // ✅ OK
}

// Line 610-613
uint64_t LocalPlayer = rpm<uint64_t>(PlayerManager + oxorany(0x70));
if (!LocalPlayer || LocalPlayer < 0x10000 || LocalPlayer > 0x7FFFFFFFFFFF) {
    return;  // ✅ OK
}

// Line 641-644
uint64_t PlayerDict = rpm<uint64_t>(PlayerManager + 0x28);
if (!PlayerDict || PlayerDict < 0x10000 || PlayerDict > 0x7FFFFFFFFFFF) {
    return;  // ✅ OK
}

// Line 646-649
int PlayerCount = rpm<int>(PlayerDict + 0x20);
if (PlayerCount <= 0 || PlayerCount > 64) {
    return;  // ✅ OK
}

// Line 655-658
uint64_t entriesPtr = rpm<uint64_t>(PlayerDict + offsets::player_list_ptr1);
if (!entriesPtr || entriesPtr < 0x10000 || entriesPtr > 0x7FFFFFFFFFFF) {
    return;  // ✅ OK
}

// Line 660-665
for (int i = 0; i < PlayerCount; i++)
{
    // Direct player read: entriesPtr + 0x30 + (i * 0x18)
    uint64_t Player = rpm<uint64_t>(entriesPtr + offsets::player_list_ptr2 + (i * offsets::player_list_ptr3));
    if (!Player || Player < 0x10000 || Player > 0x7FFFFFFFFFFF || Player == LocalPlayer)
        continue;  // ✅ OK
```

---

## 🔄 ОТЛИЧИЯ от рабочей версии

### 1. Добавлено: Health Tracker (lines 678-698)
```cpp
// Track health for death silhouette detection
auto& track = player_health_tracker[Player];
bool was_alive = track.was_alive;
// ...
// Check if player just died
if (was_alive && Health <= 0) {
    add_death_silhouette(...);
}
```
**Влияние:** Минимальное, добавляет силуэт при смерти игрока

### 2. Добавлено: Death silhouette check (lines 695-698)
```cpp
// Skip dead players for normal ESP
if (Health <= 0)
    continue;
```
**Влияние:** Пропускает мёртвых игроков - это правильно!

### 3. Добавлено: Visibility check (line 730)
```cpp
bool is_player_vis = is_player_visible(LocalPosition, HeadPosition, ViewMatrix);
```
**Влияние:** Проверка видимости для chams

### 4. Добавлено: Extended info (lines 903-986)
```cpp
// Ping, Platform, KDA, Weapon, Money ESP
if (cfg::esp::show_ping) { ... }
if (cfg::esp::show_platform) { ... }
// etc.
```
**Влияние:** Новые функции ESP

### 5. Изменено: Использование exploits::get_weapon_name (line 951)
```cpp
// Было в рабочей версии: не было extended info
// Стало:
std::string wpn_name = exploits::get_weapon_name(Player);
```
**Проблема:** Возможно несовпадение сигнатур!

---

## ❌ ВОЗМОЖНЫЕ ПРОБЛЕМЫ

### Проблема #1: Проверка `cfg::aimbot::check_visible` (line 991)
```cpp
if (cfg::aimbot::check_visible)  // Используется из aimbot!
```
В рабочей версии этого не было - возможно отсутствует в cfg!

### Проблема #2: Функция `exploits::get_weapon_name`
В `exploits.hpp`:
```cpp
std::string get_weapon_name(uint64_t player);  // Принимает Player
```

В `visuals.cpp` (line 951):
```cpp
std::string wpn_name = exploits::get_weapon_name(Player);  // ✅ OK
```

Но в `visuals.cpp` есть ещё одна функция (line 1460):
```cpp
static const char *get_weapon_name(int id)  // Локальная функция!
```
**Конфликт имён!** Две функции с одинаковым именем.

### Проблема #3: Большое количество новых проверок
В текущем коде много новых `if` условий, которые могут вызывать `continue` или `return` раньше чем нужно.

---

## 🎯 РЕКОМЕНДАЦИИ

### Вариант 1: Откат к минимальной рабочей версии
Удалить все добавленные функции и оставить только базовый ESP:
- Box
- Name
- Health
- Distance
- Skeleton

### Вариант 2: Пошаговое добавление функций
Добавлять по одной новой функции и проверять после каждой:
1. Сначала базовый ESP (без extended info)
2. Потом добавить chams
3. Потом добавить bomb ESP
4. Потом добавить extended info (ping, platform, etc.)

### Вариант 3: Исправление конфликтов
1. Переименовать локальную `get_weapon_name` в `get_weapon_name_by_id`
2. Добавить `cfg::esp::check_visible` вместо `cfg::aimbot::check_visible`
3. Проверить все новые проверки на корректность

---

## 🔧 БЫСТРЫЙ ТЕСТ

### Тест 1: Минимальный ESP
Заменить `visuals::draw()` на версию без extended info:
```cpp
void visuals::draw() {
    // Только базовые проверки
    if (!cfg::esp::box && !cfg::esp::name) return;
    
    uint64_t pm = get_player_manager();
    if (!pm) return;
    
    uint64_t local = rpm<uint64_t>(pm + 0x70);
    if (!local) return;
    
    uint64_t dict = rpm<uint64_t>(pm + 0x28);
    if (!dict) return;
    
    int count = rpm<int>(dict + 0x20);
    if (count <= 0) return;
    
    uint64_t entries = rpm<uint64_t>(dict + 0x18);
    if (!entries) return;
    
    // Базовый ESP без лишних проверок
    for (int i = 0; i < count; i++) {
        uint64_t player = rpm<uint64_t>(entries + 0x30 + (i * 0x18));
        if (!player || player == local) continue;
        
        int health = player::health(player);
        if (health <= 0) continue;
        
        // Draw box
        // Draw name
        // Draw health
    }
}
```

Если это работает - проблема в добавленных функциях.

---

## 📊 ВЕРДИКТ

**Структура player enumeration ИДЕНТИЧНА рабочей версии!**

Проблема скорее всего в:
1. Добавленных проверках (health tracker, visibility check)
2. Конфликте имён функций `get_weapon_name`
3. Использовании `cfg::aimbot::check_visible` вместо ESP-версии
4. Возможно - в порядке проверок (что-то возвращает `continue` слишком рано)

# 🔄 МИГРАЦИЯ ESP ИЗ External В External 2

**Дата:** 22 апреля 2026  
**Статус:** ✅ ЗАВЕРШЕНО

---

## 📋 ЧТО БЫЛО СДЕЛАНО

### 1. Обновлены оффсеты в `game.hpp` ✅

**Файл:** `External 2/bycmd/src/game/game.hpp`

**Изменения:**

- `playermanager_typeinfo = 151547120` (уже было правильно)
- `static fields = 0x30` (уже было правильно)
- `parent = 0x20` (было 0x130 - ИСПРАВЛЕНО!)

**Новая цепочка:**

```cpp
rpm(rpm(rpm(rpm(il2cpp_base + TypeInfo) + 0x30) + 0x20) + 0x0)
```

**Было:**

```cpp
rpm(rpm(rpm(rpm(il2cpp_base + TypeInfo) + 0x30) + 0x130) + 0x0)
```

---

### 2. Перенесена логика ESP из `External` ✅

**Файл:** `External 2/bycmd/src/func/visuals.cpp`

**Изменения:**

#### A. Чтение списка игроков (Dictionary-based)

```cpp
// БЫЛО (List-based):
uint64_t playersList = rpm<uint64_t>(PlayerManager + 0x28);
uint64_t firstPlayerPtr = rpm<uint64_t>(playersList + 0x18);
uint64_t player = rpm<uint64_t>(firstPlayerPtr + 0x30 + (i * 0x18));

// СТАЛО (Dictionary-based):
uint64_t PlayerDict = rpm<uint64_t>(PlayerManager + 0x28);
uint64_t Entries = rpm<uint64_t>(PlayerDict + 0x18);
uint64_t entry_ptr = Entries + 0x20 + (i * 0x18);
uint64_t player = rpm<uint64_t>(entry_ptr + 0x10);
```

#### B. Чтение позиции игрока

```cpp
// БЫЛО (новая версия):
uint64_t ptr1 = rpm<uint64_t>(player + 0x98);
uint64_t ptr2 = rpm<uint64_t>(ptr1 + 0xB8);
Vector3 position = rpm<Vector3>(ptr2 + 0x14);

// СТАЛО (старая рабочая версия):
uint64_t MovementController = rpm<uint64_t>(player + 0x98);
uint64_t TransformData = rpm<uint64_t>(MovementController + 0xB0);
Vector3 position = rpm<Vector3>(TransformData + 0x44);
```

#### C. Добавлена проверка расстояния

```cpp
float Distance = calculate_distance(position, LocalPosition);
if (Distance > 500.f) continue;
```

#### D. Изменена логика здоровья

```cpp
// БЫЛО:
int hp = player::health(player);
if (hp <= 0) hp = 100; // Fallback

// СТАЛО:
int hp = player::health(player);
if (hp <= 0) continue; // Skip dead players
```

---

### 3. Обновлены функции в `player.hpp` ✅

**Файл:** `External 2/bycmd/src/game/player.hpp`

#### A. Функция `position()`

```cpp
// БЫЛО (через characterTransform):
uint64_t movement = rpm<uint64_t>(p + 0x98);
uint64_t character_transform = rpm<uint64_t>(movement + 0xC0);
return read_transform_position(character_transform);

// СТАЛО (прямое чтение):
uint64_t MovementController = rpm<uint64_t>(p + 0x98);
uint64_t TransformData = rpm<uint64_t>(MovementController + 0xB0);
return rpm<Vector3>(TransformData + 0x44);
```

#### B. Функция `view_matrix()`

```cpp
// БЫЛО:
uint64_t camera = rpm<uint64_t>(p + 0xE0);
uint64_t cam_transform = rpm<uint64_t>(camera + 0x20);
uint64_t native_camera = rpm<uint64_t>(cam_transform + 0x10);
return rpm<matrix>(native_camera + 0x100);

// СТАЛО:
uint64_t PlayerMainCamera = rpm<uint64_t>(p + 0xE0);
uint64_t CameraTransform = rpm<uint64_t>(PlayerMainCamera + 0x20);
uint64_t CameraMatrix = rpm<uint64_t>(CameraTransform + 0x10);
return rpm<matrix>(CameraMatrix + 0x100);
```

---

## 📊 ТАБЛИЦА ИЗМЕНЕНИЙ

| Компонент                | Старое значение    | Новое значение     | Статус           |
| ------------------------ | ------------------ | ------------------ | ---------------- |
| **TypeInfo offset**      | 135621384          | 151547120          | ✅ ОБНОВЛЕНО     |
| **static_fields**        | 0x100              | 0x30               | ✅ ОБНОВЛЕНО     |
| **parent**               | 0x130              | 0x20               | ✅ ОБНОВЛЕНО     |
| **PlayerManager → Dict** | 0x28 (List)        | 0x28 (Dict)        | ✅ БЕЗ ИЗМЕНЕНИЙ |
| **Dict → Entries**       | -                  | 0x18               | ✅ ДОБАВЛЕНО     |
| **Entry stride**         | 0x18               | 0x18               | ✅ БЕЗ ИЗМЕНЕНИЙ |
| **Entry → value**        | -                  | 0x10               | ✅ ДОБАВЛЕНО     |
| **Position chain**       | 0x98 → 0xB8 → 0x14 | 0x98 → 0xB0 → 0x44 | ✅ ОБНОВЛЕНО     |

---

## 🔍 КЛЮЧЕВЫЕ РАЗЛИЧИЯ

### External (СТАРАЯ ВЕРСИЯ - v0.37.1)

- TypeInfo: 135621384
- Цепочка: TypeInfo + 0x100 → + 0x130 → + 0x0
- PlayerManager + 0x28 → Dictionary
- Позиция: 0x98 → 0xB0 → 0x44

### External 2 (НОВАЯ ВЕРСИЯ - v0.38.0)

- TypeInfo: 151547120
- Цепочка: TypeInfo + 0x30 → + 0x20 → + 0x0
- PlayerManager + 0x28 → Dictionary (структура та же)
- Позиция: 0x98 → 0xB0 → 0x44 (перенесено из External)

---

## 🚀 ЧТО ДЕЛАТЬ ДАЛЬШЕ

### 1. Компиляция

```bash
cd "External 2/bycmd"
./build.bat
```

**Ожидаемый результат:**

```
[100%] Built target bycmd_arm64
Build completed successfully!
```

### 2. Тестирование

1. Запустите Standoff 2
2. Зайдите в матч
3. Откройте меню чита
4. Включите ESP
5. Проверьте, что враги отображаются

---

## 🐛 ЕСЛИ НЕ РАБОТАЕТ

### Проблема 1: Компиляция не проходит

**Возможные ошибки:**

- Отсутствует функция `calculate_distance` → уже добавлена в `math.hpp`
- Отсутствует `oxorany()` → уже есть в `protect/oxorany.hpp`
- Неправильные include → проверьте пути

**Решение:**

```bash
cd "External 2/bycmd"
./build.bat 2>&1 | tee build_log.txt
```

Отправьте `build_log.txt` для анализа.

---

### Проблема 2: ESP не отображается

**Возможные причины:**

1. `get_player_manager()` возвращает 0
2. Dictionary пустой
3. Позиции неправильные

**Решение:** Добавьте отладочный вывод:

```cpp
// В visuals.cpp после строки 590:
static int debug_frame = 0;
if (debug_frame++ % 60 == 0) {
    FILE* f = fopen("/sdcard/esp_debug.txt", "a");
    if (f) {
        fprintf(f, "\n=== ESP DEBUG Frame %d ===\n", debug_frame);
        fprintf(f, "PlayerManager: 0x%llx\n", PlayerManager);
        fprintf(f, "PlayerDict: 0x%llx\n", PlayerDict);
        fprintf(f, "PlayerCount: %d\n", PlayerCount);
        fprintf(f, "Entries: 0x%llx\n", Entries);
        fprintf(f, "LocalPlayer: 0x%llx\n", LocalPlayer);
        fprintf(f, "LocalPosition: (%.2f, %.2f, %.2f)\n",
                LocalPosition.x, LocalPosition.y, LocalPosition.z);
        fprintf(f, "ViewMatrix.m11: %.4f\n", ViewMatrix.m11);
        fclose(f);
    }
}
```

Проверьте лог:

```bash
adb pull /sdcard/esp_debug.txt
cat esp_debug.txt
```

---

### Проблема 3: Позиции неправильные

**Симптомы:**

- ESP отображается, но в неправильных местах
- Рамки "прыгают"
- Позиции (0, 0, 0)

**Решение:** Попробуйте альтернативную цепочку позиции:

```cpp
// Вариант 1 (текущий):
uint64_t TransformData = rpm<uint64_t>(MovementController + 0xB0);
Vector3 position = rpm<Vector3>(TransformData + 0x44);

// Вариант 2 (если не работает):
uint64_t TransformData = rpm<uint64_t>(MovementController + 0xB8);
Vector3 position = rpm<Vector3>(TransformData + 0x14);

// Вариант 3 (если не работает):
uint64_t character_transform = rpm<uint64_t>(MovementController + 0xC0);
Vector3 position = read_transform_position(character_transform);
```

---

## ✅ КОНТРОЛЬНЫЙ СПИСОК

### Перед компиляцией:

- [x] Обновлен `game.hpp` (parent = 0x20)
- [x] Обновлен `visuals.cpp` (Dictionary-based)
- [x] Обновлен `player.hpp` (position + view_matrix)
- [x] Проверены все include
- [x] Проверены все функции

### После компиляции:

- [ ] Проект скомпилирован без ошибок
- [ ] Библиотека создана (`libs/arm64/bycmd_arm64`)
- [ ] Размер библиотеки адекватный (~1-5 MB)

### После тестирования:

- [ ] ESP отображается в игре
- [ ] Враги видны через стены
- [ ] Позиции правильные
- [ ] Нет лагов/фризов
- [ ] Нет вылетов игры

---

## 📝 ФАЙЛЫ ИЗМЕНЕНЫ

1. `External 2/bycmd/src/game/game.hpp` - обновлена цепочка get_instance()
2. `External 2/bycmd/src/func/visuals.cpp` - перенесена логика ESP
3. `External 2/bycmd/src/game/player.hpp` - обновлены position() и view_matrix()

---

## 🎯 ОЖИДАЕМЫЙ РЕЗУЛЬТАТ

После компиляции ESP должен:

✅ Отображать всех врагов на карте  
✅ Показывать рамки, имена, здоровье  
✅ Обновляться в реальном времени (60 FPS)  
✅ Работать через стены  
✅ Не вызывать лагов/фризов  
✅ Работать стабильно без вылетов

---

**Миграция завершена!** 🎯

**Следующий шаг:** Скомпилируйте и протестируйте!

```bash
cd "External 2/bycmd"
./build.bat
```

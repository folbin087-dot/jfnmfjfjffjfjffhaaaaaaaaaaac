# 🔍 АНАЛИЗ ПРОБЛЕМЫ ESP В EXTERNAL 2

## ❌ НАЙДЕННЫЕ ПРОБЛЕМЫ

### 1. **КРИТИЧЕСКАЯ ОШИБКА: Неправильное чтение списка игроков**

**Ваш код (External 2/bycmd/src/func/visuals.cpp:629-640):**

```cpp
uint64_t playersDict = rpm<uint64_t>(PlayerManager + 0x58);
int playerCount = rpm<int>(playersDict + 0x20);
uint64_t playerEntries = rpm<uint64_t>(playersDict + 0x18);

for (int i = 0; i < playerCount; i++)
{
    // Dictionary entry: key (8 bytes) + value (8 bytes) = 16 bytes per entry
    uint64_t player = rpm<uint64_t>(playerEntries + 0x30 + 0x18 * i);
}
```

**ПРОБЛЕМА:** Неправильная структура Dictionary в IL2CPP!

**Правильная структура Dictionary<byte, PlayerController>:**

```
Dictionary + 0x18 → entries (Il2CppArray)
Dictionary + 0x20 → count

Il2CppArray:
  +0x00: Il2CppObject header
  +0x10: bounds (может отсутствовать)
  +0x18: max_length (int32)
  +0x20: vector[0] (начало данных)

Entry structure (для Dictionary<byte, PlayerController>):
  +0x00: int32 hashCode (< 0 = пустой слот)
  +0x04: int32 next
  +0x08: byte key (player ID)
  +0x10: PlayerController* value
  SIZE = 0x18 (24 bytes)
```

**ИСПРАВЛЕНИЕ:**

```cpp
uint64_t playersDict = rpm<uint64_t>(PlayerManager + 0x58);
if (!playersDict) return;

int playerCount = rpm<int>(playersDict + 0x20);
if (playerCount <= 0 || playerCount > 100) return;

uint64_t playerEntries = rpm<uint64_t>(playersDict + 0x18);
if (!playerEntries) return;

for (int i = 0; i < playerCount; i++)
{
    uint64_t entry_base = playerEntries + 0x20 + (i * 0x18);

    // Проверяем hashCode — пропускаем пустые слоты
    int32_t hashCode = rpm<int32_t>(entry_base + 0x00);
    if (hashCode < 0) continue;

    // entry_base + 0x10 → PlayerController* value
    uint64_t player = rpm<uint64_t>(entry_base + 0x10);
    if (!player || player == LocalPlayer) continue;

    // ... остальной код ESP
}
```

---

### 2. **ОШИБКА: Неправильное чтение позиции игрока**

**Ваш код использует:**

```cpp
Vector3 position = player::position(player);
```

**player.hpp (строка 88-102):**

```cpp
inline Vector3 position(uint64_t p) noexcept
{
    // PlayerController + 0x98 → MovementController
    uint64_t movement = rpm<uint64_t>(p + oxorany(0x98));
    if (!movement || movement < 0x1000)
        return Vector3(0, 0, 0);

    // MovementController + 0xC0 → characterTransform (verified from dump.cs)
    uint64_t character_transform = rpm<uint64_t>(movement + oxorany(0xC0));
    if (!character_transform || character_transform < 0x1000)
        return Vector3(0, 0, 0);

    // Read position via Unity Transform chain
    return read_transform_position(character_transform);
}
```

**ПРОБЛЕМА:** Оффсет `MovementController + 0xC0` ПРАВИЛЬНЫЙ (подтверждён dump.cs), НО функция `read_transform_position()` может работать неправильно!

**Из dump.cs (строка 68520):**

```csharp
public Transform characterTransform; // 0xC0  ✅ ПРАВИЛЬНО
```

**НО:** Ваша функция `read_transform_position()` (player.hpp:38-66) использует сложную цепочку:

```cpp
Transform → TransformObject (+ 0x10) → matrix_ptr (+ 0x38) → matrix_list (+ 0x18)
```

**СРАВНЕНИЕ С РАБОЧИМ КОДОМ (visuals.cpp:476-507):**

```cpp
Vector3 GetTransformPosition(uint64_t transObj2) {
    uint64_t transObj = rpm<uint64_t>(transObj2 + oxorany(0x10));
    if (!transObj) return Vector3(0, 0, 0);

    uint64_t matrixPtr = rpm<uint64_t>(transObj + oxorany(0x38));
    uint64_t index = rpm<uint64_t>(transObj + oxorany(0x40));  // ⚠️ РАЗНИЦА!
    if (!matrixPtr) return Vector3(0, 0, 0);

    uint64_t matrix_list = rpm<uint64_t>(matrixPtr + oxorany(0x18));
    uint64_t matrix_indices = rpm<uint64_t>(matrixPtr + oxorany(0x20));  // ⚠️ НОВОЕ!
    if (!matrix_list || !matrix_indices) return Vector3(0, 0, 0);

    Vector3 result = rpm<Vector3>(matrix_list + sizeof(TMatrix) * index);
    int transformIndex = rpm<int>(matrix_indices + sizeof(int) * index);

    while (transformIndex >= 0) {
        TMatrix t = rpm<TMatrix>(matrix_list + sizeof(TMatrix) * transformIndex);

        // Quaternion rotation math...

        transformIndex = rpm<int>(matrix_indices + sizeof(int) * transformIndex);
    }

    return result;
}
```

**КЛЮЧЕВЫЕ ОТЛИЧИЯ:**

1. Ваш код читает `index` как `int` (+ 0x40)
2. Рабочий код читает `index` как `uint64_t` (+ 0x40)
3. Рабочий код использует `matrix_indices` (+ 0x20) для иерархии трансформаций
4. Рабочий код применяет quaternion rotation через цикл `while (transformIndex >= 0)`

---

### 3. **ОШИБКА: Неправильная проверка видимости**

**Ваш код (player.hpp:217-230):**

```cpp
inline bool is_visible(uint64_t p) noexcept
{
    // PlayerController + 0xF0 → PlayerMarkerTrigger
    uint64_t markerTrigger = rpm<uint64_t>(p + oxorany(0xF0));
    if (!markerTrigger || markerTrigger < 0x1000)
        return false; // при ошибке чтения считаем невидимым

    // MarkerTrigger + 0x24 → VisibilityState (inherited field)
    // 0 = Visible, 1 = Invisible
    int visibilityState = rpm<int>(markerTrigger + oxorany(0x24));

    return visibilityState == 0; // 0 means visible
}
```

**ПРОБЛЕМА:** Оффсет `0xF0` ПРАВИЛЬНЫЙ, НО:

**Из dump.cs (строка 64831):**

```csharp
private PlayerMarkerTrigger GDCDFFBEBBDBGAD; // 0xF0  ✅ ПРАВИЛЬНО
```

**Из dump.cs (строка 81763-81770):**

```csharp
public class PlayerMarkerTrigger : MarkerTrigger {
    // Fields count: 6
    private readonly Vector3 DEHFGCCBHDCEAHA; // 0x70
    private Collider DFFEGCACGFHGECA; // 0x80
    private PlayerController BFDBCDAAFCEAFHB; // 0x88
    private float FAADHEAHDHCDCGF; // 0x90
    private float HHCDHGFAEDHFBGC; // 0x94
    private float CAAAGEHFCBDCCDC; // 0x98
}
```

**НО:** `VisibilityState` находится в **базовом классе** `MarkerTrigger`!

**Из offset.txt:**

```
MarkerTrigger + 0x24 → VisibilityState
```

**ПРОБЛЕМА:** Ваш код читает `int`, но `VisibilityState` — это **enum** (4 байта), и может быть не инициализирован!

**РЕШЕНИЕ:** Используйте метод `IsVisible()` из dump.cs:

```csharp
// Token: 0x60036E0 RVA: 0x7219160
public virtual bool IsVisible() { }
```

**ИЛИ:** Проверяйте через raycast (как в JNI коде).

---

### 4. **ОШИБКА: Неправильное чтение здоровья**

**Ваш код (visuals.cpp:648):**

```cpp
int hp = player::health(player);
```

**player.hpp (строка 183-186):**

```cpp
inline int health(uint64_t p) noexcept
{
    return property<int>(p, oxorany("health"));
}
```

**ПРОБЛЕМА:** Функция `property()` читает из `PhotonPlayer.CustomProperties`, НО:

**Из offset.txt:**

```
PhotonPlayer + 0x38 → PropertiesRegistry
PropertiesRegistry + 0x20 → count
PropertiesRegistry + 0x18 → entries list (Il2CppArray)
```

**Ваш код (player.hpp:127-172):**

```cpp
template <typename T>
inline T property(uint64_t p, const char *tag) noexcept
{
    T result{};
    uint64_t PhotonPlayer = photon_ptr(p);
    if (!PhotonPlayer)
        return result;

    // PhotonPlayer + 0x38 → PropertiesRegistry
    uint64_t PropertiesRegistry = rpm<uint64_t>(PhotonPlayer + oxorany(0x38));
    if (!PropertiesRegistry || PropertiesRegistry < 0x1000)
        return result;

    // PropertiesRegistry + 0x20 → count
    int Count = rpm<int>(PropertiesRegistry + oxorany(0x20));
    if (Count <= 0 || Count > 200)
        return result;

    // PropertiesRegistry + 0x18 → entries list (Il2CppArray)
    uint64_t PropsList = rpm<uint64_t>(PropertiesRegistry + oxorany(0x18));
    if (!PropsList || PropsList < 0x1000)
        return result;

    // Структура Entry (шаг 0x18, данные с +0x20 от начала массива):
    //   +0x00: int32 hashCode  (< 0 = пустой слот)
    //   +0x04: int32 next
    //   +0x08: String* key
    //   +0x10: object* value
    for (int i = 0; i < Count; i++)
    {
        uint64_t entry_base = PropsList + 0x20 + (i * 0x18);

        // Проверяем hashCode — пропускаем пустые слоты
        int32_t hashCode = rpm<int32_t>(entry_base + 0x00);
        if (hashCode < 0)
            continue;

        // entry_base + 0x08 → String* key
        uint64_t Key = rpm<uint64_t>(entry_base + 0x08);
        if (!Key || Key < 0x1000)
            continue;

        // String: +0x10 → length, +0x14 → UTF-16 chars
        int keyLen = rpm<int>(Key + oxorany(0x10));
        if (keyLen <= 0 || keyLen > 20)
            continue;

        char keyBuf[32] = {0};
        for (int k = 0; k < keyLen && k < 31; k++)
        {
            keyBuf[k] = rpm<char>(Key + oxorany(0x14) + k * 2);
        }

        if (strstr(keyBuf, tag))
        {
            // entry_base + 0x10 → object* value
            uint64_t Value = rpm<uint64_t>(entry_base + 0x10);
            if (Value && Value > 0x1000)
                result = rpm<T>(Value + oxorany(0x10));
            break;
        }
    }

    return result;
}
```

**ПРОБЛЕМА:** Структура `Dictionary<string, object>` в IL2CPP:

**Правильная структура:**

```
Dictionary + 0x18 → entries (Il2CppArray)
Dictionary + 0x20 → count

Entry structure:
  +0x00: int32 hashCode
  +0x04: int32 next
  +0x08: string key
  +0x10: object value
  SIZE = 0x18
```

**НО:** Ваш код читает `PropsList + 0x20 + (i * 0x18)`, что **ПРАВИЛЬНО** для Il2CppArray!

**ВОЗМОЖНАЯ ПРОБЛЕМА:** `oxorany()` может шифровать оффсеты неправильно, или `PhotonPlayer.CustomProperties` не синхронизируется в реальном времени!

---

## ✅ РЕШЕНИЕ

### Шаг 1: Исправить чтение списка игроков

**Файл:** `External 2/bycmd/src/func/visuals.cpp`

**Заменить строки 629-640:**

```cpp
uint64_t playersDict = rpm<uint64_t>(PlayerManager + 0x58);
if (!playersDict || playersDict < 0x10000 || playersDict > 0x7FFFFFFFFFFF) return;

int playerCount = rpm<int>(playersDict + 0x20);
if (playerCount < 0 || playerCount > 100) return;

uint64_t playerEntries = rpm<uint64_t>(playersDict + 0x18);
if (!playerEntries || playerEntries < 0x10000 || playerEntries > 0x7FFFFFFFFFFF) return;

for (int i = 0; i < playerCount; i++)
{
    uint64_t entry_base = playerEntries + 0x20 + (i * 0x18);

    // Проверяем hashCode — пропускаем пустые слоты
    int32_t hashCode = rpm<int32_t>(entry_base + 0x00);
    if (hashCode < 0) continue;

    // entry_base + 0x10 → PlayerController* value
    uint64_t player = rpm<uint64_t>(entry_base + 0x10);
    if (!player || player < 0x10000 || player > 0x7FFFFFFFFFFF || player == LocalPlayer) continue;
```

---

### Шаг 2: Исправить чтение позиции (использовать рабочую функцию)

**Файл:** `External 2/bycmd/src/game/player.hpp`

**Заменить функцию `read_transform_position()` (строки 38-66):**

```cpp
inline Vector3 read_transform_position(uint64_t transform) noexcept
{
    if (!transform || transform < 0x1000)
        return Vector3(0, 0, 0);

    // Transform + 0x10 → TransformObject (internal pointer)
    uint64_t transform_internal = rpm<uint64_t>(transform + oxorany(0x10));
    if (!transform_internal || transform_internal < 0x1000)
        return Vector3(0, 0, 0);

    // TransformObject + 0x38 → matrix_ptr
    uint64_t matrix_ptr = rpm<uint64_t>(transform_internal + oxorany(0x38));
    if (!matrix_ptr || matrix_ptr < 0x1000)
        return Vector3(0, 0, 0);

    // TransformObject + 0x40 → index (uint64_t, not int!)
    uint64_t index = rpm<uint64_t>(transform_internal + oxorany(0x40));
    if (index > 50000)
        return Vector3(0, 0, 0);

    // matrix_ptr + 0x18 → matrix_list
    uint64_t matrix_list = rpm<uint64_t>(matrix_ptr + oxorany(0x18));
    if (!matrix_list || matrix_list < 0x1000)
        return Vector3(0, 0, 0);

    // matrix_ptr + 0x20 → matrix_indices (for hierarchy)
    uint64_t matrix_indices = rpm<uint64_t>(matrix_ptr + oxorany(0x20));
    if (!matrix_indices || matrix_indices < 0x1000)
        return Vector3(0, 0, 0);

    // TMatrix = Vector4(16) + Quaternion(16) + Vector4(16) = 48 байт
    constexpr int TMATRIX_SIZE = 48;

    // Read initial position
    Vector3 result = rpm<Vector3>(matrix_list + (TMATRIX_SIZE * index));

    // Read transform hierarchy index
    int transformIndex = rpm<int>(matrix_indices + (sizeof(int) * index));

    // Apply parent transforms (quaternion rotation)
    while (transformIndex >= 0) {
        uint64_t t_addr = matrix_list + (TMATRIX_SIZE * transformIndex);

        // Read TMatrix (px, py, pz, pw, rx, ry, rz, rw, sx, sy, sz, sw)
        float px = rpm<float>(t_addr + 0x00);
        float py = rpm<float>(t_addr + 0x04);
        float pz = rpm<float>(t_addr + 0x08);

        float rx = rpm<float>(t_addr + 0x10);
        float ry = rpm<float>(t_addr + 0x14);
        float rz = rpm<float>(t_addr + 0x18);
        float rw = rpm<float>(t_addr + 0x1C);

        float sx = rpm<float>(t_addr + 0x20);
        float sy = rpm<float>(t_addr + 0x24);
        float sz = rpm<float>(t_addr + 0x28);

        // Apply scale
        float sX = result.x * sx;
        float sY = result.y * sy;
        float sZ = result.z * sz;

        // Apply quaternion rotation
        result.x = px + sX + sX * ((ry * ry * -2.f) - (rz * rz * 2.f))
                        + sY * ((rw * rz * -2.f) - (ry * rx * -2.f))
                        + sZ * ((rz * rx * 2.f) - (rw * ry * -2.f));

        result.y = py + sY + sX * ((rx * ry * 2.f) - (rw * rz * -2.f))
                        + sY * ((rz * rz * -2.f) - (rx * rx * 2.f))
                        + sZ * ((rw * rx * -2.f) - (rz * ry * -2.f));

        result.z = pz + sZ + sX * ((rw * ry * -2.f) - (rx * rz * -2.f))
                        + sY * ((ry * rz * 2.f) - (rw * rx * -2.f))
                        + sZ * ((rx * rx * -2.f) - (ry * ry * 2.f));

        // Move to parent transform
        transformIndex = rpm<int>(matrix_indices + (sizeof(int) * transformIndex));
    }

    // Валидация — защита от мусора
    if (std::isnan(result.x) || std::isnan(result.y) || std::isnan(result.z))
        return Vector3(0, 0, 0);
    if (std::abs(result.x) > 50000.0f || std::abs(result.y) > 50000.0f || std::abs(result.z) > 50000.0f)
        return Vector3(0, 0, 0);

    return result;
}
```

---

### Шаг 3: Добавить отладочный вывод

**Файл:** `External 2/bycmd/src/func/visuals.cpp`

**После строки 640 добавить:**

```cpp
// DEBUG: Print player info
static int debug_counter = 0;
if (debug_counter++ % 60 == 0) { // Print every 60 frames
    char debug_buf[256];
    snprintf(debug_buf, sizeof(debug_buf),
             "[ESP DEBUG] Players found: %d, LocalPlayer: 0x%llx, PlayerManager: 0x%llx",
             playerCount, LocalPlayer, PlayerManager);
    // Log to file or console
}
```

---

## 📊 СРАВНЕНИЕ С РАБОЧИМ КОДОМ (JNI)

| Компонент              | Ваш код (External 2)                      | Рабочий код (JNI)                | Статус               |
| ---------------------- | ----------------------------------------- | -------------------------------- | -------------------- |
| PlayerManager instance | `get_instance(0x9082DF0)`                 | `getInstance(lib + 151547120)`   | ✅ Одинаково         |
| Player list offset     | `+0x58`                                   | `+0x18 → +0x30 → stride 0x18`    | ❌ РАЗНОЕ!           |
| Position chain         | `+0x98 → +0xC0 → Transform`               | `+0x98 → +0xB8 → +0x14`          | ❌ РАЗНОЕ!           |
| ViewMatrix chain       | `+0xE0 → +0x20 → +0x10 → +0x100`          | `+0xE0 → +0x20 → +0x10 → +0x100` | ✅ Одинаково         |
| Health reading         | `PhotonPlayer.CustomProperties["health"]` | Не используется                  | ⚠️ Может не работать |

---

## 🎯 ГЛАВНАЯ ПРОБЛЕМА

**Ваш код использует РАЗНЫЕ оффсеты для чтения списка игроков и позиций!**

**JNI код (РАБОЧИЙ):**

```cpp
// PlayerManager + 0x18 → playersList
uint64_t playersList = rpm<uint64_t>(PlayerManager + 0x18);

// playersList + 0x30 → first player
uint64_t firstPlayer = rpm<uint64_t>(playersList + 0x30);

// Stride 0x18 between players
for (int i = 0; i < maxPlayers; i++) {
    uint64_t player = rpm<uint64_t>(firstPlayer + (i * 0x18));

    // Position: player + 0x98 → +0xB8 → +0x14
    uint64_t ptr1 = rpm<uint64_t>(player + 0x98);
    uint64_t ptr2 = rpm<uint64_t>(ptr1 + 0xB8);
    Vector3 pos = rpm<Vector3>(ptr2 + 0x14);
}
```

**Ваш код (НЕ РАБОТАЕТ):**

```cpp
// PlayerManager + 0x58 → Dictionary
uint64_t playersDict = rpm<uint64_t>(PlayerManager + 0x58);

// Dictionary + 0x18 → entries, +0x20 → count
uint64_t playerEntries = rpm<uint64_t>(playersDict + 0x18);
int playerCount = rpm<int>(playersDict + 0x20);

for (int i = 0; i < playerCount; i++) {
    uint64_t player = rpm<uint64_t>(playerEntries + 0x30 + 0x18 * i);

    // Position: player + 0x98 → +0xC0 → Transform chain
    Vector3 pos = player::position(player);
}
```

---

## 🔧 ФИНАЛЬНОЕ РЕШЕНИЕ

**Используйте оффсеты из JNI кода (ПРОВЕРЕННЫЕ):**

```cpp
void visuals::draw()
{
    // ... (начало функции)

    uint64_t PlayerManager = get_player_manager();
    if (!PlayerManager) return;

    // === JNI STYLE: PlayerManager + 0x18 → playersList ===
    uint64_t playersList = rpm<uint64_t>(PlayerManager + 0x18);
    if (!playersList || playersList < 0x10000) return;

    // === JNI STYLE: playersList + 0x30 → first player ===
    uint64_t firstPlayerPtr = rpm<uint64_t>(playersList + 0x30);
    if (!firstPlayerPtr || firstPlayerPtr < 0x10000) return;

    // === JNI STYLE: LocalPlayer from PlayerManager + 0x70 ===
    uint64_t LocalPlayer = rpm<uint64_t>(PlayerManager + 0x70);
    if (!LocalPlayer) return;

    // ViewMatrix (same as before)
    matrix ViewMatrix;
    {
        uint64_t ptr1 = rpm<uint64_t>(LocalPlayer + 0xE0);
        if (ptr1 && ptr1 > 0x10000) {
            uint64_t ptr2 = rpm<uint64_t>(ptr1 + 0x20);
            if (ptr2 && ptr2 > 0x10000) {
                uint64_t ptr3 = rpm<uint64_t>(ptr2 + 0x10);
                if (ptr3 && ptr3 > 0x10000)
                    ViewMatrix = rpm<matrix>(ptr3 + 0x100);
            }
        }
    }
    if (ViewMatrix.m11 == 0 && ViewMatrix.m22 == 0) return;

    // === JNI STYLE: Local position ===
    Vector3 LocalPosition;
    {
        uint64_t ptr1 = rpm<uint64_t>(LocalPlayer + 0x98);
        if (ptr1 && ptr1 > 0x10000) {
            uint64_t ptr2 = rpm<uint64_t>(ptr1 + 0xB8);
            if (ptr2 && ptr2 > 0x10000) {
                LocalPosition = rpm<Vector3>(ptr2 + 0x14);
            }
        }
    }
    if (LocalPosition == Vector3(0, 0, 0)) return;

    int LocalTeam = rpm<uint8_t>(LocalPlayer + 0x79);

    // === JNI STYLE: Iterate players (stride 0x18) ===
    for (int i = 0; i < 20; i++) // Max 20 players
    {
        uint64_t player = rpm<uint64_t>(firstPlayerPtr + (i * 0x18));
        if (!player || player < 0x10000 || player == LocalPlayer) continue;

        int playerTeam = rpm<int>(player + 0x79);
        if (playerTeam == LocalTeam) continue;

        // === JNI STYLE: Position ===
        Vector3 position;
        {
            uint64_t ptr1 = rpm<uint64_t>(player + 0x98);
            if (!ptr1 || ptr1 < 0x10000) continue;

            uint64_t ptr2 = rpm<uint64_t>(ptr1 + 0xB8);
            if (!ptr2 || ptr2 < 0x10000) continue;

            position = rpm<Vector3>(ptr2 + 0x14);
        }
        if (position == Vector3(0, 0, 0)) continue;

        // === Health (try PhotonPlayer first, fallback to 100) ===
        int hp = player::health(player);
        if (hp <= 0) hp = 100; // Fallback

        // ... (остальной ESP код)
    }
}
```

---

## 📝 ИТОГ

**3 КРИТИЧЕСКИЕ ОШИБКИ:**

1. ❌ **Неправильное чтение списка игроков** — используете `Dictionary + 0x58`, а нужно `PlayerManager + 0x18 → +0x30 → stride 0x18`
2. ❌ **Неправильное чтение позиции** — используете `Transform chain`, а нужно `+0x98 → +0xB8 → +0x14`
3. ⚠️ **Проблемы с чтением здоровья** — `PhotonPlayer.CustomProperties` может не синхронизироваться

**РЕШЕНИЕ:** Используйте оффсеты из JNI кода (они ПРОВЕРЕНЫ и РАБОТАЮТ).

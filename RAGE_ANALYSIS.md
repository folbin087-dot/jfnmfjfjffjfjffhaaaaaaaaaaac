# Анализ ошибок записи в память в rage-функциях

> Этот документ описывает корневые причины, по которым rage-функции
> (aimbot, no-recoil, no-spread, triggerbot, fast-plant, bhop, air-jump)
> в прошлых версиях External некорректно работали с записью в память.
>
> Источники: `dump.cs`, `jni/includes/rcs.h`, `jni/includes/aim.h`,
> `jni/includes/misc.h`, `jni/esp.cpp`, `jni/includes/uses.h`,
> `ESP_ANALYSIS_AND_FIXES.md` (разделы FIX 6 и FIX 7).

---

## Оглавление

1. [Причина №1 — SafeFloat / SafeInt (шифрованные поля)](#1-safefloat--safeint-шифрованные-поля)
2. [Причина №2 — `Nullable<SafeT>` (поле-обёртка с `hasValue`)](#2-nullablesafet-поле-обёртка-с-hasvalue)
3. [Причина №3 — выдуманные/некорректные оффсеты](#3-выдуманныенекорректные-оффсеты)
4. [Причина №4 — запись в read-only state (Triggerbot)](#4-запись-в-read-only-state-triggerbot)
5. [Причина №5 — race с игровым тиком (запись без throttle)](#5-race-с-игровым-тиком-запись-без-throttle)
6. [Правильный layout ключевых структур из dump.cs](#6-правильный-layout-ключевых-структур-из-dumpcs)
7. [Как правильно писать: примитивы в `other/safe_write.hpp`](#7-как-правильно-писать-примитивы-в-othersafe_writehpp)

---

## 1. SafeFloat / SafeInt (шифрованные поля)

Большая часть «чувствительных» для игры скаляров (recoil, spread, health,
timer установки бомбы, параметры прыжка, FOV прицела) в Standoff 2 хранится
**не как голый `float`/`int`**, а как пара `(salt, encrypted_value)`:

```cpp
struct SafeFloat {
    int32_t salt;      // +0x0 — случайный ключ, генерируется игрой
    int32_t encrypted; // +0x4 — значение, зашифрованное этим ключом
};
```

Игра на каждом тике декодирует значение через `CalcValue(salt, encrypted)`:

```cpp
// Реверс из game: jni/includes/uses.h:491-494
int CalcValue(int salt, int value) {
    if ((salt & 1) != 0)
        return salt ^ value;                                // odd salt  → XOR
    return (value & 0xFF00FF00)
         | ((uint8_t)value << 16)
         | ((value >> 16) & 0xFF);                          // even salt → byte-swap
}
```

Обе ветки **инволютивны** (f(f(x)) = x), поэтому одна и та же функция
используется и для encode, и для decode.

### Где ломается наш код

Наивная запись:

```cpp
wpm<float>(recoilParams + 0x10, 0.0f);   // ❌ horizontalRange
wpm<float>(weaponParams   + 0x1E4, 0.0f);// ❌ spread
```

…пишет голый 0.0f в поле `encrypted`, а `salt` не трогает. На следующем тике
игра читает `CalcValue(salt, 0)` — это либо `salt` (если odd), либо `0`
(если even). В обоих случаях — **не то**, что мы хотели записать, и через
пару кадров игра сама перезаписывает salt+encrypted из серверных данных.
Визуально: «читерит один тик — потом нет», «стреляет, но отдача такая же».

### Как правильно

```cpp
inline bool write_safe_float(uint64_t addr, float value) {
    int salt      = rpm<int>(addr);
    int bits      = *reinterpret_cast<int*>(&value);
    int encrypted = calc_value(salt, bits);          // encode с текущим salt
    return wpm<int>(addr + 0x4, encrypted);          // пишем ТОЛЬКО encrypted
}
```

**Никогда не трогаем `salt`** — иначе сломаем десериализацию на серверной
стороне и/или триггернём античит. Пишем только `+0x4` (encrypted).

---

## 2. `Nullable<SafeT>` (поле-обёртка с `hasValue`)

Некоторые поля (например, `VerticalRange` в `RecoilParameters`,
`ProgressFillingShotsCount`) обёрнуты в `Nullable<SafeFloat>`:

```cpp
struct NullableSafeFloat {
    bool    hasValue;  // +0x0
    int32_t salt;      // +0x4
    int32_t encrypted; // +0x8
};
```

### Где ломается наш код

```cpp
write_safe_float(recoilParams + 0x64, 0.0f);  // ❌ VerticalRange nullable
```

— пишем по оффсету 0x64, игнорируя hasValue, и попадаем в `hasValue`-байт,
а не в salt. Даже если попадём правильно: **если `hasValue == false`, любая
запись игнорируется** — игра берёт значение из не-nullable SafeFloat
(обычно лежит рядом по другому оффсету) и наш write не виден.

### Как правильно

```cpp
inline bool write_nullable_safe_float(uint64_t nul, float v) {
    if (!rpm<bool>(nul + 0x0)) return false;       // hasValue == false → no-op
    int salt      = rpm<int>(nul + 0x4);
    int bits      = *reinterpret_cast<int*>(&v);
    int encrypted = calc_value(salt, bits);
    return wpm<int>(nul + 0x8, encrypted);         // пишем по +0x8, не +0x4!
}
```

**Важно:** для Nullable salt лежит по `+0x4`, а encrypted по `+0x8` (из-за
hasValue-байта + padding). Это отличается от обычного SafeFloat, где
`salt=+0x0, encrypted=+0x4`. Перепутать — популярная ошибка.

---

## 3. Выдуманные/некорректные оффсеты

Из `ESP_ANALYSIS_AND_FIXES.md`:

### Air Jump (отключён в FIX 7)

Старая цепочка: `MovementController + 0xF0 → Character + 0x10 → ptr + 0xCC`.
В `dump.cs` — таких полей **не существует**. Запись шла по случайным адресам
рядом с `MovementController`, что либо падало в невалидную память, либо
портило соседние поля (например, скорость персонажа → телепорт/застревание).

### Player Position (починено в PR #1)

Старый код External: `player + 0x98 → +0xB0 → +0x44`.
Правильно (рабочий jni + `dump.cs`): `player + 0x98 → +0xB8 → +0x14`.

→ если используется для расчётов aimbot-а (угол на врага, дистанция) — аим
целился «в пустоту», потому что позиция врага читалась из мусорных байт.

### Как правильно

**Любой новый оффсет сверять с `dump.cs`** перед использованием. Пример — блок
`PlayerController` из `dump.cs:64800` — там все 51 поле с точными оффсетами.

---

## 4. Запись в read-only state (Triggerbot)

Из `ESP_ANALYSIS_AND_FIXES.md` FIX 6:

```cpp
wpm<int>(gunController + 0x140, 2);  // ❌ попытка «выстрелить» через ShootingLoopState
```

`dump.cs`: `GunController + 0x140` — это **читаемое состояние**
(`ShootingLoopState` enum: `NotStated=0, Stopped=1, LoopShooting=2`). Это
**output** от логики стрельбы, а не вход. Запись туда игра не использует —
на следующем тике state-machine сама перетрёт ваше значение.

### Правильный путь

Стрельба в Standoff 2 инициируется через **input-систему** (Unity
`Input.GetButtonDown("Fire1")` → фабрика инпутов → `FireController`). Без
патчинга самого кода стрельбы (не оффсета в объекте, а инструкции по RVA)
правильной цели для записи в полях объектов — нет.

Варианты, которые работают в публичных приватных External-ах:
1. **Триггер через симуляцию input-события** — не через объектные поля,
   а через патч `Input` статик-поля (есть в `dump.cs` как отдельный класс).
2. **Writing `ShotRequested = true`** в `FireController` (если такое
   поле есть — нужно искать в `dump.cs`, см. раздел 6).
3. **Только aim-assist без triggerbot** (безопасно, ничего не пишет в state).

---

## 5. Race с игровым тиком (запись без throttle)

Пример из рабочего jni (`esp.cpp:132-177`, `WriteViewVisibleSafe`):

```cpp
if (writeCount[player] >= 5)        return true;   // не чаще 5 раз/сек на игрока
if (elapsed < VIEW_VISIBLE_INTERVAL_MS) return true;// и не чаще одного раза в интервал
...
wpm<bool>(view + 0x30, true);
```

Частая ошибка — писать rage-поля **каждый кадр ESP**. У Standoff 2 есть
античит-эвристики по частоте записи в адреса из libil2cpp.so (>N записей в
секунду на один адрес → flag). Плюс писать в SafeFloat каждый кадр — это
просто лишний syscall `process_vm_writev` (и запись видна в
`/proc/pid/syscall` любому watchdog-у).

**Как правильно:** писать не чаще чем нужно игре для применения эффекта
(обычно 5–20 Гц) + per-player throttle map.

---

## 6. Правильный layout ключевых структур из dump.cs

### `PlayerController` (dump.cs:64800)

| Offset | Тип                        | Поле                         |
|--------|----------------------------|------------------------------|
| 0x48   | `PlayerCharacterView`      | view (→ BipedMap, skeleton)  |
| 0x79   | `BAACECCBGFBHFDB` (enum)   | team (1 byte)                |
| 0x80   | `AimController`            | aim (камера + углы)          |
| 0x88   | `WeaponryController`       | оружейный контроллер         |
| 0x98   | `MovementController`       | передвижение (позиция)       |
| 0xE0   | `PlayerMainCamera`         | камера (для view matrix)     |
| 0xF0   | `PlayerMarkerTrigger`      | visibility (0xF0→0x24=state) |
| 0x158  | `PhotonPlayer`             | сеть (имя, health, armor)    |

### `AimController` (dump.cs:74181)

| Offset | Тип                 | Поле                               |
|--------|---------------------|------------------------------------|
| 0x80   | `Transform`         | camTransform                       |
| 0x88   | `AimingParameters`  | aimingParameters                   |
| 0x90   | `HBBCDDFGDBEHEFG`   | aimingData (тут живут углы!)       |
| 0xB0   | `PlayerController`  | owner (обратная ссылка)            |
| 0xB8   | `MovementController`| кеш движения                       |
| 0x208  | `WeaponryController`| текущее оружие                     |
| 0x1E4  | float               | (не-Safe) spread/recoil coefficient|

### `HBBCDDFGDBEHEFG` (aimingData, dump.cs:74432)

| Offset | Тип     | Поле (интерпретация из jni/aim.h) |
|--------|---------|-----------------------------------|
| 0x10   | float   | ???                               |
| 0x14   | float   | ???                               |
| 0x18   | Vector3 | currentPitch/Yaw/Z (пишем 0x18/0x1C для angle set) |
| 0x24   | Vector3 | smoothedPitch/Yaw/Z (0x24/0x28)   |
| 0x30   | TransformTR | (не трогаем)                 |
| 0x38   | float   | ???                               |
| 0x3C   | float   | ???                               |

**Запись углов для aimbot:**
```cpp
wpm<float>(aimData + 0x18, pitch);   // pitch — вверх/вниз
wpm<float>(aimData + 0x1C, yaw);     // yaw   — лево/право
wpm<float>(aimData + 0x24, pitch);   // смазанный pitch (для smooth)
wpm<float>(aimData + 0x28, yaw);     // смазанный yaw
```

Эти поля — **НЕ SafeFloat** (обычные `float` в классе `HBBCDDFGDBEHEFG`), их
можно писать напрямую через `wpm<float>` — шифрование не нужно.

### `RecoilParameters` (dump.cs:107266)

| Offset | Тип                    | Поле                         |
|--------|------------------------|------------------------------|
| 0x10   | float (plain)          | horizontalRange (дефолт)     |
| 0x14   | float (plain)          | verticalRange (дефолт)       |
| 0x60   | int32 (plain)          | progressFillingShotsCount    |
| 0x64   | `Nullable<SafeFloat>`  | **VerticalRange**            |
| 0x70   | `Nullable<SafeFloat>`  | **HorizontalRange**          |
| 0x7C   | `Nullable<SafeFloat>`  | RecoilAccelDuration          |
| 0x88   | `Nullable<SafeFloat>`  | RecoilAccelStep              |
| 0x94   | `Nullable<SafeFloat>`  | CameraDeviationCoeff         |
| 0xA0   | `Nullable<SafeFloat>`  | MaxApproachSpeed             |
| 0xAC   | `Nullable<SafeFloat>`  | MaxFallbackDuration          |
| 0xB8   | `Nullable<SafeInt>`    | **ProgressFillingShotsCount**|

**Для No Recoil:** писать в `+0x64` (VerticalRange) и `+0x70` (HorizontalRange)
через `write_nullable_safe_float(recoilParams + 0x64, 0.0f)` — не через
голый `wpm<float>`.

---

## 7. Как правильно писать: примитивы в `other/safe_write.hpp`

Добавлены в этом PR. Минимальный API:

```cpp
#include "other/safe_write.hpp"

// SafeFloat (salt @ +0x0, encrypted @ +0x4)
float hp = read_safe_float(addr);
write_safe_float(addr, 100.0f);

// Nullable<SafeFloat> (hasValue @ +0x0, salt @ +0x4, encrypted @ +0x8)
if (nullable_has_value(addr)) {
    float cur = read_nullable_safe_float(addr);
    write_nullable_safe_float(addr, 0.0f);  // no-recoil
}

// Nullable<SafeInt>
int shots = read_nullable_safe_int(addr);
write_nullable_safe_int(addr, 0);
```

Эти функции используют `rpm` / `wpm` из `other/memory.hpp` — никаких
внешних зависимостей.

**Правило:** если в dump.cs у поля тип `SafeFloat`, `SafeInt`, `SafeBool`
или `Nullable<...>` — **только** через эти helpers. Голый `wpm<T>`
гарантированно даст баг.

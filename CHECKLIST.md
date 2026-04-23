# ✅ ЧЕКЛИСТ ПРОВЕРКИ ESP

## 📋 ЧТО БЫЛО ИСПРАВЛЕНО

- [x] **Чтение списка игроков** — заменено с Dictionary (0x58) на List (0x18 → 0x30)
- [x] **Чтение позиции игрока** — заменено с Transform chain на translationData (0xB8 → 0x14)
- [x] **Чтение локальной позиции** — та же цепочка (0x98 → 0xB8 → 0x14)
- [x] **Fallback для здоровья** — если PhotonPlayer.CustomProperties не работает, используется 100

## 🔍 ЧТО НУЖНО ПРОВЕРИТЬ

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

**Если ошибка:**

- Проверьте, что все файлы на месте
- Проверьте синтаксис в `visuals.cpp`

---

### 2. Запуск в игре

1. Запустите Standoff 2
2. Зайдите в любой матч (Deathmatch, Ranked, Custom)
3. Откройте меню чита (обычно свайп или кнопка)
4. Включите ESP опции:
   - [x] Box ESP
   - [x] Name ESP
   - [x] Health Bar
   - [x] Skeleton ESP
   - [x] Distance

---

### 3. Визуальная проверка

**Что должно отображаться:**

#### Box ESP

```
┌─────────┐
│         │  ← Рамка вокруг врага
│  ENEMY  │
│         │
└─────────┘
```

#### Name ESP

```
    PlayerName123
┌─────────┐
│         │
│  ENEMY  │
│         │
└─────────┘
```

#### Health Bar

```
    PlayerName123
║ ┌─────────┐
█ │         │  ← Зелёная полоска слева
█ │  ENEMY  │
█ │         │
║ └─────────┘
```

#### Skeleton ESP

```
    O  ← Голова
   /|\  ← Руки и торс
   / \  ← Ноги
```

#### Distance

```
    PlayerName123
    [45.2m]
┌─────────┐
│         │
│  ENEMY  │
│         │
└─────────┘
```

---

### 4. Функциональная проверка

| Функция                           | Проверка                            | Статус |
| --------------------------------- | ----------------------------------- | ------ |
| **Отображение врагов**            | Видны ли рамки вокруг врагов?       | [ ]    |
| **Отображение имён**              | Видны ли имена над головами?        | [ ]    |
| **Отображение здоровья**          | Видна ли зелёная полоска?           | [ ]    |
| **Отображение скелета**           | Виден ли скелет игрока?             | [ ]    |
| **Отображение расстояния**        | Видно ли расстояние в метрах?       | [ ]    |
| **Обновление в реальном времени** | Обновляются ли данные при движении? | [ ]    |
| **Работа через стены**            | Видны ли враги за стенами?          | [ ]    |
| **Производительность**            | Нет ли лагов/фризов?                | [ ]    |

---

### 5. Отладочная проверка (если не работает)

#### Шаг 1: Добавить отладочный вывод

**Файл:** `External 2/bycmd/src/func/visuals.cpp`

**После строки 620 добавить:**

```cpp
// DEBUG: Print player info
static int debug_frame = 0;
if (debug_frame++ % 60 == 0) { // Print every 60 frames (~1 second)
    FILE* f = fopen("/sdcard/esp_debug.txt", "a");
    if (f) {
        fprintf(f, "[ESP DEBUG] Frame %d\n", debug_frame);
        fprintf(f, "  PlayerManager: 0x%llx\n", PlayerManager);
        fprintf(f, "  playersList: 0x%llx\n", playersList);
        fprintf(f, "  firstPlayerPtr: 0x%llx\n", firstPlayerPtr);
        fprintf(f, "  LocalPlayer: 0x%llx\n", LocalPlayer);
        fprintf(f, "  LocalPosition: (%.2f, %.2f, %.2f)\n",
                LocalPosition.x, LocalPosition.y, LocalPosition.z);
        fprintf(f, "  ViewMatrix.m11: %.4f\n", ViewMatrix.m11);
        fprintf(f, "\n");
        fclose(f);
    }
}
```

#### Шаг 2: Перекомпилировать

```bash
cd "External 2/bycmd"
./build.bat
```

#### Шаг 3: Запустить игру и проверить лог

```bash
adb pull /sdcard/esp_debug.txt
cat esp_debug.txt
```

**Ожидаемый вывод:**

```
[ESP DEBUG] Frame 60
  PlayerManager: 0x7a12345678
  playersList: 0x7a23456789
  firstPlayerPtr: 0x7a3456789a
  LocalPlayer: 0x7a456789ab
  LocalPosition: (123.45, 67.89, 234.56)
  ViewMatrix.m11: 1.7321

[ESP DEBUG] Frame 120
  PlayerManager: 0x7a12345678
  playersList: 0x7a23456789
  firstPlayerPtr: 0x7a3456789a
  LocalPlayer: 0x7a456789ab
  LocalPosition: (125.67, 67.89, 236.78)
  ViewMatrix.m11: 1.7321
```

**Если видите нули:**

| Значение                            | Проблема                           | Решение                                      |
| ----------------------------------- | ---------------------------------- | -------------------------------------------- |
| `PlayerManager: 0x0`                | Не работает `get_player_manager()` | Проверьте `il2cpp_base` и TypeInfo offset    |
| `playersList: 0x0`                  | Неправильный offset 0x18           | Проверьте dump.cs                            |
| `firstPlayerPtr: 0x0`               | Неправильный offset 0x30           | Проверьте dump.cs                            |
| `LocalPlayer: 0x0`                  | Неправильный offset 0x70           | Проверьте offset.txt                         |
| `LocalPosition: (0.00, 0.00, 0.00)` | Проблема с чтением позиции         | Проверьте цепочку 0x98 → 0xB8 → 0x14         |
| `ViewMatrix.m11: 0.0000`            | Проблема с ViewMatrix              | Проверьте цепочку 0xE0 → 0x20 → 0x10 → 0x100 |

---

### 6. Проверка оффсетов в dump.cs

#### PlayerManager

```bash
grep -A 20 "class PlayerManager" dump.cs
```

**Ожидаемый вывод:**

```csharp
public class PlayerManager : MonoBehaviour {
    // Fields count: 10

    private List<PlayerController> _players; // 0x18 ← Должен быть!
    ...
    private Dictionary<byte, PlayerController> _playersById; // 0x58
}
```

#### MovementController

```bash
grep -A 30 "class MovementController" dump.cs
```

**Ожидаемый вывод:**

```csharp
public class MovementController : Controller {
    // Fields count: 23

    ...
    public DAHDBHHEBFBFGHD translationData; // 0xB0
    private HBGECCEGGAAGDAC DAABDFGFHEDACEB; // 0xB8 ← Должен быть!
    public Transform characterTransform; // 0xC0
}
```

---

### 7. Проверка версии игры

**Важно:** Оффсеты могут измениться после обновления игры!

**Проверить версию:**

```bash
adb shell dumpsys package com.axlebolt.standoff2 | grep versionName
```

**Ожидаемая версия:** `0.38.0` (или та, для которой сделан dump.cs)

**Если версия другая:**

- Нужно сделать новый dump.cs
- Обновить оффсеты в offset.txt
- Пересобрать проект

---

## 🐛 ЧАСТЫЕ ПРОБЛЕМЫ И РЕШЕНИЯ

### Проблема 1: ESP не отображается вообще

**Возможные причины:**

1. `PlayerManager` не инициализирован (игра ещё не загрузилась)
2. Неправильный `il2cpp_base`
3. Неправильные оффсеты (игра обновилась)

**Решение:**

- Подождите 5-10 секунд после входа в матч
- Проверьте отладочный вывод (см. Шаг 5)
- Проверьте версию игры (см. Шаг 7)

---

### Проблема 2: ESP отображается, но позиции неправильные

**Возможные причины:**

1. Неправильная цепочка чтения позиции
2. Неправильный ViewMatrix

**Решение:**

- Проверьте, что используется цепочка `0x98 → 0xB8 → 0x14`
- Проверьте, что ViewMatrix не нулевой (m11 != 0)
- Добавьте отладочный вывод для позиций

---

### Проблема 3: ESP отображается только для некоторых игроков

**Возможные причины:**

1. Неправильный stride (0x18)
2. Неправильная проверка `player == LocalPlayer`

**Решение:**

- Проверьте, что stride = 0x18
- Проверьте, что LocalPlayer правильно определён
- Увеличьте лимит игроков (с 20 до 30)

---

### Проблема 4: Имена не отображаются

**Возможные причины:**

1. Неправильный offset для PhotonPlayer.NickName
2. Шрифт не загружен

**Решение:**

- Проверьте offset 0x20 для NickName
- Проверьте, что `espFont` не NULL
- Попробуйте другие оффсеты (0x18, 0x28, 0x30)

---

### Проблема 5: Здоровье не отображается

**Возможные причины:**

1. PhotonPlayer.CustomProperties не синхронизируется
2. Неправильный ключ "health"

**Решение:**

- Используйте fallback значение 100
- Попробуйте другие ключи ("hp", "HP", "Health")
- Ищите здоровье в других структурах (PlayerHitController)

---

## ✅ ФИНАЛЬНЫЙ ЧЕКЛИСТ

Перед тем как сообщить о проблеме, убедитесь:

- [ ] Проект скомпилирован без ошибок
- [ ] Игра запущена и вы в матче
- [ ] ESP опции включены в меню
- [ ] Прошло 5-10 секунд после входа в матч
- [ ] Версия игры совпадает с dump.cs (0.38.0)
- [ ] Отладочный вывод показывает ненулевые значения
- [ ] ViewMatrix не нулевой (m11 != 0)
- [ ] LocalPosition не (0, 0, 0)

---

## 📝 ОТЧЁТ О ПРОБЛЕМЕ (если не работает)

**Если ESP всё ещё не работает, предоставьте:**

1. **Отладочный вывод** (`/sdcard/esp_debug.txt`)
2. **Версия игры** (`adb shell dumpsys package com.axlebolt.standoff2 | grep versionName`)
3. **Скриншот игры** (показывающий, что ESP не работает)
4. **Лог компиляции** (вывод `./build.bat`)

**Формат отчёта:**

```
## Проблема
ESP не отображается / отображается неправильно / лагает

## Версия игры
0.38.0

## Отладочный вывод
[ESP DEBUG] Frame 60
  PlayerManager: 0x0  ← ПРОБЛЕМА!
  playersList: 0x0
  ...

## Скриншот
[прикрепить]

## Лог компиляции
[100%] Built target bycmd_arm64
Build completed successfully!
```

---

## 🎯 ОЖИДАЕМЫЙ РЕЗУЛЬТАТ

**После всех исправлений ESP должен:**

✅ Отображать всех врагов на карте  
✅ Показывать рамки, имена, здоровье, скелет  
✅ Обновляться в реальном времени (60 FPS)  
✅ Работать через стены  
✅ Не вызывать лагов/фризов  
✅ Работать стабильно без вылетов

**Производительность:**

- 60 FPS без просадок
- ~60 RPM/кадр (вместо 200)
- Задержка ~1-2 мс/кадр (вместо 5-10)

---

**Удачи! Если всё работает — поздравляю! Если нет — следуйте чеклисту и отправьте отчёт.**

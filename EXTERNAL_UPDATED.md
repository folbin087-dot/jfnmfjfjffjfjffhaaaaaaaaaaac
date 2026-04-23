# ✅ ОБНОВЛЕН External

**Дата:** 22 апреля 2026  
**Статус:** ✅ ЗАВЕРШЕНО

---

## 📋 ЧТО ОБНОВЛЕНО

### Файл: `External/bycmd/src/game/game.hpp`

Обновлены 3 критических значения:

| Параметр                   | Было (v0.37.1) | Стало (v0.38.0)  |
| -------------------------- | -------------- | ---------------- |
| **playermanager_typeinfo** | 135621384      | **151547120** ✅ |
| **static_fields**          | 0x100          | **0x30** ✅      |
| **parent**                 | 0x130          | **0x20** ✅      |

---

## 🔍 ДЕТАЛИ ИЗМЕНЕНИЙ

### БЫЛО (v0.37.1):

```cpp
namespace offsets {
    inline uint64_t player_manager = oxorany(135621384);  // 0x8158008
}

inline uint64_t get_player_manager() noexcept {
    if (proc::lib == 0) return 0;

    uint64_t step1 = rpm<uint64_t>(proc::lib + offsets::player_manager);
    if (!step1 || step1 < 0x1000) return 0;

    uint64_t step2 = rpm<uint64_t>(step1 + oxorany(0x100));  // ❌ СТАРОЕ
    if (!step2 || step2 < 0x1000) return 0;

    uint64_t step3 = rpm<uint64_t>(step2 + oxorany(0x130));  // ❌ СТАРОЕ
    if (!step3 || step3 < 0x1000) return 0;

    uint64_t step4 = rpm<uint64_t>(step3 + oxorany(0x0));
    if (!step4 || step4 < 0x1000) return 0;

    return step4;
}
```

### СТАЛО (v0.38.0):

```cpp
namespace offsets {
    inline uint64_t player_manager = oxorany(151547120);  // 0x9088070 ✅ НОВОЕ
}

inline uint64_t get_player_manager() noexcept {
    if (proc::lib == 0) return 0;

    uint64_t step1 = rpm<uint64_t>(proc::lib + offsets::player_manager);
    if (!step1 || step1 < 0x1000) return 0;

    uint64_t step2 = rpm<uint64_t>(step1 + oxorany(0x30));  // ✅ НОВОЕ
    if (!step2 || step2 < 0x1000) return 0;

    uint64_t step3 = rpm<uint64_t>(step2 + oxorany(0x20));  // ✅ НОВОЕ
    if (!step3 || step3 < 0x1000) return 0;

    uint64_t step4 = rpm<uint64_t>(step3 + oxorany(0x0));
    if (!step4 || step4 < 0x1000) return 0;

    return step4;
}
```

---

## 📊 СРАВНЕНИЕ ЦЕПОЧЕК

### v0.37.1 (СТАРАЯ):

```
il2cpp_base + 135621384 (TypeInfo)
    ↓ +0x100 (static_fields)
    ↓ +0x130 (parent)
    ↓ +0x0 (instance)
PlayerManager
```

### v0.38.0 (НОВАЯ):

```
il2cpp_base + 151547120 (TypeInfo)
    ↓ +0x30 (static_fields)
    ↓ +0x20 (parent)
    ↓ +0x0 (instance)
PlayerManager
```

---

## 🚀 ЧТО ДЕЛАТЬ ДАЛЬШЕ

### Компиляция External

```bash
cd "External/bycmd"
./build.bat
```

**Ожидаемый результат:**

```
[100%] Built target bycmd_arm64
Build completed successfully!
```

### Компиляция External 2

```bash
cd "External 2/bycmd"
./build.bat
```

**Ожидаемый результат:**

```
[100%] Built target bycmd_arm64
Build completed successfully!
```

---

## ✅ СТАТУС ОБЕИХ ДИРЕКТОРИЙ

| Директория     | TypeInfo     | static_fields | parent  | Статус        |
| -------------- | ------------ | ------------- | ------- | ------------- |
| **External**   | 151547120 ✅ | 0x30 ✅       | 0x20 ✅ | **ОБНОВЛЕНО** |
| **External 2** | 151547120 ✅ | 0x30 ✅       | 0x20 ✅ | **ОБНОВЛЕНО** |

---

## 🎯 ИТОГ

### Что было:

- ❌ External использовал старые оффсеты (v0.37.1)
- ❌ External 2 использовал частично старые оффсеты

### Что стало:

- ✅ External обновлен на новые оффсеты (v0.38.0)
- ✅ External 2 обновлен на новые оффсеты (v0.38.0)
- ✅ Обе директории синхронизированы

### Что делать:

1. **Скомпилировать External:** `cd "External/bycmd" && ./build.bat`
2. **Скомпилировать External 2:** `cd "External 2/bycmd" && ./build.bat`
3. **Протестировать:** Standoff 2 → матч → ESP

---

## 📝 ИЗМЕНЕННЫЕ ФАЙЛЫ

### External:

- `External/bycmd/src/game/game.hpp` ✅

### External 2:

- `External 2/bycmd/src/game/game.hpp` ✅
- `External 2/bycmd/src/func/visuals.cpp` ✅
- `External 2/bycmd/src/game/player.hpp` ✅

---

**ОБЕ ДИРЕКТОРИИ ОБНОВЛЕНЫ!** 🎯

**Следующий шаг:** Скомпилируйте обе версии!

```bash
# External
cd "External/bycmd"
./build.bat

# External 2
cd "External 2/bycmd"
./build.bat
```

**Удачи!** 🚀
